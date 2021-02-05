#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#ifdef _WIN32
#  include <Windows.h>
#endif

class Thread
{
public:
  Thread(std::string name);
  virtual ~Thread() {};

  std::string spawn();
  bool isRunning();
  std::string terminate();

protected:
  void signalExit();
  bool checkForExit();
  void signalComplete();
  bool waitForCompletion(uint32_t timeout);

public:
  uint32_t runStart();
  virtual uint32_t run() = 0;

#ifdef _WIN32
  static DWORD runHelper(void* context);
#else
  static void* runHelper(void* context);
#endif

protected:
  std::string threadName;
#ifdef _WIN32
  HANDLE threadId = 0;
#else
  pthread_t threadId = 0;
#endif

private:
  bool threadRunning = false;
  bool threadExit = false;
  std::mutex threadMutex;
  std::condition_variable completeEvent;
};
