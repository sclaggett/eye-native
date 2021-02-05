#include "Thread.h"

#ifdef _WIN32
#else
#  include <unistd.h>
#  include <sys/syscall.h>
#endif

using namespace std;

Thread::Thread(string name) :
  threadName(name)
{
}

std::string Thread::spawn()
{
  if (threadId != 0)
  {
    return "An instance of the thread is already running";
  }
  bool success = false;
#ifdef _WIN32
  DWORD dwThreadId = 0;
  threadId = CreateThread(NULL, 0, &Thread::runHelper, this, 0, &dwThreadId);
  success = (threadId != NULL);
#else
  int retVal = pthread_create(&threadId, nullptr, &Thread::runHelper, this);
  success = (retVal == 0);
#endif
  if (!success)
  {
    return "Failed to spawn thread";
  }
  threadRunning = true;
  return "";
}

bool Thread::isRunning()
{
  if (threadId == 0)
  {
    return false;
  }
  return threadRunning;
}

std::string Thread::terminate()
{
  if (threadId == 0)
  {
    return "";
  }
  signalExit();
  if (waitForCompletion(100))
  {
    return "";
  }
  bool success = false;
#ifdef _WIN32
  success = TerminateThread(threadId, 1);
#else
  int retVal = pthread_cancel(threadId);
  success = (retVa == 0);
#endif
  if (!success)
  {
    return "Unable to cancel thread";
  }
  threadRunning = false;
  return "";
}

void Thread::signalExit()
{
  unique_lock<mutex> lock(threadMutex);
  threadExit = true;
}

bool Thread::checkForExit()
{
  unique_lock<mutex> lock(threadMutex);
  return threadExit;
}

void Thread::signalComplete()
{
  threadRunning = false;
  completeEvent.notify_one();
}

bool Thread::waitForCompletion(uint32_t timeout)
{
  unique_lock<mutex> lock(threadMutex);
  if (threadRunning)
  {
    completeEvent.wait_for(lock, chrono::milliseconds(timeout));
  }
  return !threadRunning;
}

uint32_t Thread::runStart()
{
  uint32_t retVal = run();
  signalComplete();
  return retVal;
}

#ifdef _WIN32
  DWORD Thread::runHelper(void* context)
  {
    return (DWORD)((Thread*)context)->runStart();
  }
#else
  void* Thread::runHelper(void* context)
  {
    return (void*)((Thread*)context)->runStart();
  }
#endif
