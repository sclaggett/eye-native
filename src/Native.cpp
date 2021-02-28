#include "Native.h"
#include "FfmpegProcess.h"
#include "FrameThread.h"
#include "PreviewThread.h"
#include "Wrapper.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <stdio.h>
#ifdef _WIN32
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

using namespace std;
using namespace cv;

// Global variables
string gFfmpegPath;
bool gInitialized = false, gRecording = false;
uint32_t gNextFrameId = 0;
shared_ptr<Queue<FrameWrapper*>> gPendingFrameQueue(new Queue<FrameWrapper*>());
shared_ptr<Queue<FrameWrapper*>> gCompletedFrameQueue(new Queue<FrameWrapper*>());
shared_ptr<FfmpegProcess> gFfmpegProcess(nullptr);
shared_ptr<FrameThread> gFrameThread(nullptr);
shared_ptr<Queue<Mat*>> gPreviewFrameQueue(new Queue<Mat*>());
shared_ptr<PreviewThread> gPreviewThread(nullptr);

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
  // Make sure the main thread is running
  if (gFrameThread == nullptr)
  {
    return "Create video output before preview channel";
  }

#ifdef _WIN32
  // Create a name for the unique pipe
  UUID pipeId = {0};
  UuidCreate(&pipeId);
  RPC_CSTR pipeIdStr = NULL;
  UuidToString(&pipeId, &pipeIdStr);
  channelName = "\\\\.\\pipe\\";
  channelName += (char*)pipeIdStr;
  RpcStringFree(&pipeIdStr);
#else
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
#endif

  // Pass the preview channel name to the frame thread
  gFrameThread->setPreviewChannel(channelName);
  return "";
}

string native::openPreviewChannel(Napi::Env env, string name)
{
  // Spawn the thread that will read frames from the remote frame thread
  gPreviewThread = shared_ptr<PreviewThread>(new PreviewThread(name,
    gPreviewFrameQueue));
  gPreviewThread->spawn();
  return "";
}

bool native::getNextFrame(Napi::Env env, uint8_t*& frame, size_t& length,
  int maxWidth, int maxHeight)
{
  // Get the next preview frame and return if none are available
  Mat* previewFrame = 0;
  if (!gPreviewFrameQueue->waitItem(&previewFrame, 0))
  {
    return false;
  }

  // Use the standard approach to calculate the scaled size of the preview frame
  double frameRatio = (double)previewFrame->cols / (double)previewFrame->rows;
  double maxRatio = (double)maxWidth / (double)maxHeight;
  uint32_t width, height;
  if (frameRatio > maxRatio)
  {
    width = maxWidth;
    height = (uint32_t)((double)previewFrame->rows * (double)maxWidth /
      (double)previewFrame->cols);
  }
  else
  {
    height = maxHeight;
    width = (uint32_t)((double)previewFrame->cols * (double)maxHeight /
      (double)previewFrame->rows);
  }

  printf("## GetNextFrame, frame = (%i, %i), max = (%i, %i), resized = (%i, %i)\n",
    previewFrame->cols, previewFrame->rows, maxWidth, maxHeight, width, height);

  // Resize the preview frame and export it in the PNG format
  Mat resizedFrame;
  resize(*previewFrame, resizedFrame, Size2i(width, height), 0, 0, INTER_AREA);
  vector<uchar> pngFrame;
  imencode(".png", resizedFrame, pngFrame);

  // Create a copy of the PNG frame data and delete the preview frame
  length = pngFrame.size();
  frame = new uint8_t[length];
  memcpy(frame, &pngFrame[0], length);
  delete previewFrame;
  return true;
}

void native::closePreviewChannel(Napi::Env env)
{
  if (gPreviewThread != nullptr)
  {
    if (gPreviewThread->isRunning())
    {
      gPreviewThread->terminate();
    }
    gPreviewThread = nullptr;
  }
}

void native::deletePreviewFrame(napi_env env, void* finalize_data, void* finalize_hint)
{
  delete[] reinterpret_cast<uint8_t*>(finalize_data);
}
