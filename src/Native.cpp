#include "Native.h"
#include "FfmpegProcess.h"
#include "FrameThread.h"
#include "Wrapper.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

// Global variables
string gFfmpegPath;
bool gInitialized = false, gRecording = false;
uint32_t gNextFrameId = 0;
shared_ptr<Queue<FrameWrapper*>> gPendingFrameQueue(new Queue<FrameWrapper*>());
shared_ptr<Queue<FrameWrapper*>> gCompletedFrameQueue(new Queue<FrameWrapper*>());
shared_ptr<FfmpegProcess> gFfmpegProcess(nullptr);
shared_ptr<FrameThread> gFrameThread(nullptr);

void native::initializeFfmpeg(Napi::Env env, string ffmpegPath)
{
  // Remember the location of ffmpeg
  gFfmpegPath = ffmpegPath;
  gInitialized = true;
}

string native::createVideoOutput(Napi::Env env, int width, int height, int fps, string encoder,
  string outputPath)
{
  // Make sure we've been initialized and aren't currently recording
  if (!gInitialized)
  {
    return "Library has not been initialized";
  }
  if (gRecording)
  {
    return "Recording already in progress";
  }

  // Spawn the ffmpeg process
  gFfmpegProcess = shared_ptr<FfmpegProcess>(new FfmpegProcess(gFfmpegPath, width, height,
    fps, encoder, outputPath));
  gFfmpegProcess->spawn();

  // Spawn the thread that will feed frames to the ffmpeg process
  gFrameThread = shared_ptr<FrameThread>(new FrameThread(gFfmpegProcess, gPendingFrameQueue,
    gCompletedFrameQueue, width, height));
  gFrameThread->spawn();

  printf("## Native library open(%i, %i, %i, %s)\n", (uint32_t)width, (uint32_t)height,
    fps, outputPath.c_str());

  gRecording = true;
  return "";
}

int32_t native::queueNextFrame(Napi::Env env, uint8_t* frame, size_t length, int width,
  int height)
{
  // Make sure we've been initialized and are recording
  if (!gInitialized)
  {
    return -1;
  }
  if (!gRecording)
  {
    return -1;
  }

  // Wrap the incoming frame and place it in the queue for the thread to process
  FrameWrapper* wrapper = new FrameWrapper;
  wrapper->frame = frame;
  wrapper->length = length;
  wrapper->width = width;
  wrapper->height = height;
  wrapper->id = gNextFrameId++;
  gPendingFrameQueue->addItem(wrapper);
  return wrapper->id;
}

vector<int32_t> native::checkCompletedFrames(Napi::Env env)
{
  // Return an array of all frames that we're done with and free the associated memory
  vector<int32_t> ret;
  FrameWrapper* wrapper;
  while (gCompletedFrameQueue->waitItem(&wrapper, 0))
  {
    ret.push_back(wrapper->id);
    delete wrapper;
  }
  return ret;
}

void native::closeVideoOutput(Napi::Env env)
{
  if (!gRecording)
  {
    return;
  }
  if (gFrameThread != nullptr)
  {
    if (gFrameThread->isRunning())
    {
      gFrameThread->terminate();
    }
    gFrameThread = nullptr;
  }
  if (gFfmpegProcess != nullptr)
  {
    if (gFfmpegProcess->isProcessRunning())
    {
      gFfmpegProcess->waitForExit();
    }
    gFfmpegProcess = nullptr;
  }
  gRecording = false;
}

string native::createPreviewChannel(Napi::Env env, string& channelName)
{
  // Pass the preview channel name to the frame thread
  if (gFrameThread == nullptr)
  {
    return "Create video output before preview channel";
  }

  // Create a temporary file
  char nameBuffer[128];
  snprintf(nameBuffer, 128, "/tmp/eyeNativeXXXXXX");
  int tmpFd = mkstemp(nameBuffer);
  if (tmpFd == -1)
  {
    return "Failed to create temporary file";
  }

  // Append ".fifo" to the temporary file's name to make it unique and create a
  // named pipe
  channelName = string(nameBuffer) + ".fifo";
  if (mkfifo(channelName.c_str(), S_IRUSR | S_IWUSR | S_IWGRP | S_IXGRP | 
    S_IROTH | S_IWOTH) != 0)
  {
    return "Failed to create named pipe";
  }

  // Pass the preview channel name to the frame thread
  gFrameThread->setPreviewChannel(channelName);
  return "";
}

string native::openPreviewChannel(Napi::Env env, string name)
{
  printf("## openPreviewChannel(%s)\n", name.c_str());
  return "openPreviewChannel";
}

bool native::getNextFrame(Napi::Env env, uint8_t** frame, size_t* length,
  int width, int height)
{
  printf("## getNextFrame\n");
  return false;
}

void native::closePreviewChannel(Napi::Env env)
{
}
