#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>
#include "PipeReader.h"
#include "Thread.h"

class FfmpegProcess : public Thread
{
public:
  FfmpegProcess(std::string executable, uint32_t width, uint32_t height, uint32_t fps,
    std::string encoder, std::string outputPath);
  virtual ~FfmpegProcess() {};

public:
  bool isProcessRunning();
  void waitForExit();

  void writeStdin(uint8_t* data, uint32_t length);
  std::string readStdout();
  std::string readStderr();

private:
  bool startProcess();
  void terminateProcess();
  void cleanUpProcess();

public:
  uint32_t run();

private:
  std::string executable;
  std::vector<std::string> arguments;
  bool processStarted = false;
  std::mutex processMutex;
  std::condition_variable processStartEvent;
#ifdef _WIN32
  HANDLE processPid = NULL;
  HANDLE processStdin = NULL;
  HANDLE processStdout = NULL;
  HANDLE processStderr = NULL;
#else
  int processPid = 0;
  int processStdin = 0;
  int processStdout = 0;
  int processStderr = 0;
#endif
  std::shared_ptr<PipeReader> stdoutReader;
  std::shared_ptr<PipeReader> stderrReader;
};
