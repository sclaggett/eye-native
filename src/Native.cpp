#include "Native.h"
#include "FfmpegProcess.h"
#include "FrameThread.h"
#include "Wrapper.h"
#include <stdio.h>

using namespace std;

// Global variables
string gFfmpegPath;
bool gInitialized = false, gRecording = false;
uint32_t gNextFrameId = 0;
shared_ptr<Queue<FrameWrapper*>> gPendingFrameQueue(new Queue<FrameWrapper*>());
shared_ptr<Queue<FrameWrapper*>> gCompletedFrameQueue(new Queue<FrameWrapper*>());
shared_ptr<FfmpegProcess> gFfmpegProcess(nullptr);
shared_ptr<FrameThread> gFrameThread(nullptr);

void native::initialize(Napi::Env env, string ffmpegPath)
{
  // Remember the location of ffmpeg
  gFfmpegPath = ffmpegPath;
  gInitialized = true;
}

string native::open(Napi::Env env, int width, int height, int fps, string encoder,
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

int32_t native::write(Napi::Env env, uint8_t* frame, size_t length, int width,
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

vector<int32_t> native::checkCompleted(Napi::Env env)
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

void native::close(Napi::Env env)
{
  printf("## Closing\n");
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
