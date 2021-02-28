#include "FfmpegProcess.h"
#include <stdexcept>
#ifdef _WIN32
#else
  #include <unistd.h>
  #include <sys/wait.h>
  #include <signal.h>
  #include <sys/types.h>
  #ifdef __APPLE__
    #include <crt_externs.h>
  #endif
#endif

using namespace std;

#define PIPE_READ 0
#define PIPE_WRITE 1

FfmpegProcess::FfmpegProcess(string exec, uint32_t width, uint32_t height, uint32_t fps,
    string encoder, string outputPath) :
  Thread("ffmpeg"),
  executable(exec)
{
  // Check options using:
  //   ffmpeg -h encoder=h264_videotoolbox
  // Profile and level are both interesting. Experiment witg "-b:v 2000k"

  // ffmpeg -f rawvideo -pix_fmt rgb24 -vf scale=960x720 1920x1080 -framerate 30 -i pipe:0
  // -c:v h264_videotoolbox -profile:v high -pix_fmt yuv420p -y output.mp4

  // Input options
  arguments.push_back("-f");
  arguments.push_back("rawvideo");

  arguments.push_back("-pix_fmt");
  arguments.push_back("bgra");

  arguments.push_back("-video_size");
  arguments.push_back(to_string(width) + "x" + to_string(height));

  arguments.push_back("-framerate");
  arguments.push_back(to_string(fps));

  arguments.push_back("-i");
  arguments.push_back("pipe:0");

  // Output options
  //arguments.push_back("-vf");
  //arguments.push_back("scale=" + to_string(width) + "x" + to_string(height));

  arguments.push_back("-c:v");
  arguments.push_back(encoder);

  arguments.push_back("-profile:v");
  arguments.push_back("high");

  arguments.push_back("-pix_fmt");
  arguments.push_back("yuv420p");

  arguments.push_back("-y");
  arguments.push_back(outputPath);
}

uint32_t FfmpegProcess::run()
{
  if (!startProcess())
  {
    return 0;
  }
  stdoutReader = shared_ptr<PipeReader>(new PipeReader("ffmpeg_stdout",
    processStdout));
  stdoutReader->spawn();
  stderrReader = shared_ptr<PipeReader>(new PipeReader("ffmpeg_stderr",
    processStderr));
  stderrReader->spawn();
  processMutex.lock();
  processStarted = true;
  processMutex.unlock();
  processStartEvent.notify_one();
  while (isProcessRunning())
  {
    if (!stdoutReader->isRunning() ||
      !stderrReader->isRunning())
    {
      printf("ERROR: A process thread has exited unexpectedly\n");
      break;
    }
    if (checkForExit())
    {
      terminateProcess();
      break;
    }
#ifdef _WIN32
  Sleep(10);
#else
  usleep(100000);
#endif
  }
  stdoutReader->terminate();
  stderrReader->terminate();

  // TODO: Iteratively print the output in the loop above
  printf("## Ffmpeg process has exited\n");
  printf("## Stdout: %s\n", stdoutReader->getData().c_str());
  printf("## Stderr: %s\n", stderrReader->getData().c_str());
  cleanUpProcess();
  return 0;
}

bool FfmpegProcess::startProcess()
{
#ifdef _WIN32
  // Set the bInheritHandle flag so pipe handles are inherited.
  SECURITY_ATTRIBUTES saAttr;
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;
  
  // Create a pipe for the child process's stdout and ensure the read handle to the pipe is not inherited.
  HANDLE childStdoutRd = NULL, childStdoutWr = NULL;
  if (!CreatePipe(&childStdoutRd, &childStdoutWr, &saAttr, 0))
  {
    printf("ERROR: Failed to create stdout pipes\n");
    return false;
  }
  if (!SetHandleInformation(childStdoutRd, HANDLE_FLAG_INHERIT, 0))
  {
    printf("ERROR: Failed to set stdout pipe flag\n");
    return false;
  }

  // Create a pipe for the child process's stderr and ensure the read handle to the pipe is not inherited.
  HANDLE childStderrRd = NULL, childStderrWr = NULL;
  if (!CreatePipe(&childStderrRd, &childStderrWr, &saAttr, 0))
  {
    printf("ERROR: Failed to create stderr pipes\n");
    return false;
  }
  if (!SetHandleInformation(childStderrRd, HANDLE_FLAG_INHERIT, 0))
  {
    printf("ERROR: Failed to set stderr pipe flag\n");
    return false;
  }
  
  // Create a pipe for the child process's stdin and ensure the write handle is not inherited. 
  HANDLE childStdinRd = NULL, childStdinWr = NULL;
  if (!CreatePipe(&childStdinRd, &childStdinWr, &saAttr, 0))
  {
    printf("ERROR: Failed to create stdin pipes\n");
    return false;
  }
  if (!SetHandleInformation(childStdinWr, HANDLE_FLAG_INHERIT, 0))
  {
    printf("ERROR: Failed to set stdin pipe flag\n");
    return false;
  }

  // Set up members of the PROCESS_INFORMATION structure. 
  PROCESS_INFORMATION piProcInfo; 
  ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
  STARTUPINFO siStartInfo;
  ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
  siStartInfo.cb = sizeof(STARTUPINFO); 
  siStartInfo.hStdError = childStderrWr;
  siStartInfo.hStdOutput = childStdoutWr;
  siStartInfo.hStdInput = childStdinRd;
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
 
  // Format the executable and arguments into a single string.
  string commandLine = executable;
  for (auto it = arguments.begin(); it != arguments.end(); ++it)
  {
    commandLine += " ";
    commandLine += *it;
  }
    
  // Create the child process.
  LPSTR cmdLineStr = strdup(commandLine.c_str());
  if (!CreateProcess(NULL, cmdLineStr, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL,
	&siStartInfo, &piProcInfo))
  {
    free(cmdLineStr);
    printf("ERROR: Failed to create ffmpeg process\n");
    return false;
  }
  free(cmdLineStr);
  
  // Close the handles to the child process thread and the stdin, stdout, and stderr pipes no longer
  // needed by the child process.
  CloseHandle(piProcInfo.hThread);
  CloseHandle(childStdoutWr);
  CloseHandle(childStderrWr);
  CloseHandle(childStdinRd);
  
  // Remember the handles.
  processPid = piProcInfo.hProcess;
  processStdin = childStdinWr;
  processStdout = childStdoutRd;
  processStderr = childStderrRd;
  return true;
#else
  vector<string> environment;
  char** env;
  #ifdef __APPLE__
    env = *_NSGetEnviron();
  #else
    env = environ;
  #endif
  for (int i = 0; *(env + i) != 0; i++)
  {
    environment.push_back(*(env + i));
  }
  int stdinPipe[2], stdoutPipe[2], stderrPipe[2];
  if ((pipe(stdinPipe) < 0) || (pipe(stdoutPipe) < 0) || (pipe(stderrPipe) < 0))
  {
    printf("ERROR: Failed to allocate pipes\n");
    return false;
  }
  processPid = fork();
  if (processPid == 0)
  {
    fflush(stdout);
    if (dup2(stdinPipe[PIPE_READ], STDIN_FILENO) == -1)
    {
      fflush(stdout);
      exit(errno);
    }
    if (dup2(stdoutPipe[PIPE_WRITE], STDOUT_FILENO) == -1)
    {
      fflush(stdout);
      exit(errno);
    }
    if (dup2(stderrPipe[PIPE_WRITE], STDERR_FILENO) == -1)
    {
      fflush(stdout);
      exit(errno);
    }
    fflush(stdout);
    close(stdinPipe[PIPE_READ]);
    close(stdinPipe[PIPE_WRITE]); 
    close(stdoutPipe[PIPE_READ]);
    close(stdoutPipe[PIPE_WRITE]); 
    close(stderrPipe[PIPE_READ]);
    close(stderrPipe[PIPE_WRITE]);
    vector<char*> args, env;
    args.push_back(&executable[0]);
    fflush(stdout);
    for (vector<string>::iterator it = arguments.begin(); it != arguments.end();
      ++it)
    {
      args.push_back(&(*it)[0]);
      fflush(stdout);
    }
    args.push_back(NULL);
    for (vector<string>::iterator envIterator = environment.begin();
      envIterator != environment.end(); ++envIterator)
    {
      env.push_back(const_cast<char*>((*envIterator).c_str()));
      fflush(stdout);
     }
    env.push_back(NULL);
    execve(executable.c_str(), args.data(), env.data());
    exit(-1);
  }
  else if (processPid > 0)
  {
    processStdin = stdinPipe[PIPE_WRITE];
    processStdout = stdoutPipe[PIPE_READ];
    processStderr = stderrPipe[PIPE_READ];
    close(stdinPipe[PIPE_READ]); 
    close(stdoutPipe[PIPE_WRITE]); 
    close(stderrPipe[PIPE_WRITE]);
    return true;
  }
  else
  {
    close(stdinPipe[PIPE_READ]);
    close(stdinPipe[PIPE_WRITE]);
    close(stdoutPipe[PIPE_READ]);
    close(stdoutPipe[PIPE_WRITE]);
    close(stderrPipe[PIPE_READ]);
    close(stderrPipe[PIPE_WRITE]);
    printf("ERROR: Failed to fork child\n");
    return false;
  }
#endif
}

bool FfmpegProcess::isProcessRunning()
{
  std::unique_lock<std::mutex> lock(processMutex);
  if (processPid == 0)
  {
    return false;
  }
#ifdef _WIN32
  DWORD exitCode = 0;
  if (!GetExitCodeProcess(processPid, &exitCode))
  {
    printf("ERROR: Failed to check if child process is running\n");
    return false;
  }
  return (exitCode == STILL_ACTIVE);
#else
  int status;
  int res = waitpid(processPid, &status, WNOHANG);
  if (res == 0)
  {
    return true;
  }
  else if (res == processPid)
  {
    return false;
  }
  else if ((res == -1) && (errno == ECHILD))
  {
    return false;
  }
  else
  {
    printf("ERROR: Failed to check if child process is running\n");
    return false;
  }
#endif
}

void FfmpegProcess::waitForExit()
{
  if (processStdin != 0)
  {
#ifdef _WIN32
    CloseHandle(processStdin);
#else
    close(processStdin);
#endif
    processStdin = 0;
  }
  while (isRunning())
  {
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10000);
#endif
  }
}

void FfmpegProcess::writeStdin(uint8_t* data, uint32_t length)
{
  if (processStdin == 0)
  {
    printf("ERROR: Stdin has been closed\n");
    return;
  }
  bool success = false;
#ifdef _WIN32  
  DWORD dwWritten = 0;
  success = WriteFile(processStdin, data, length, &dwWritten, NULL);
  success &= (dwWritten == length);
#else
  ssize_t ret = write(processStdin, data, length);
  success = (ret == (int32_t)length);
#endif
  if (!success)
  {
    printf("ERROR: Failed to write to process stdin\n");
  }
}

string FfmpegProcess::readStdout()
{
  return stdoutReader->getData();
}

string FfmpegProcess::readStderr()
{
  return stderrReader->getData();
}

void FfmpegProcess::terminateProcess()
{
#ifdef _WIN32
  TerminateProcess(processPid, 1);
#else
  kill(processPid, SIGKILL);
#endif
}

void FfmpegProcess::cleanUpProcess()
{
  if (processStdin != 0)
  {
#ifdef _WIN32
    CloseHandle(processStdin);
#else
    close(processStdin);
#endif
    processStdin = 0;
  }
  if (processStdout != 0)
  {
#ifdef _WIN32
    CloseHandle(processStdout);
#else
    close(processStdout);
#endif
    processStdout = 0;
  }
  if (processStderr != 0)
  {
#ifdef _WIN32
    CloseHandle(processStderr);
#else
    close(processStderr);
#endif
    processStderr = 0;
  }
}
