#include "FrameThread.h"
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#ifdef _WIN32
#else
#  include <unistd.h>
#endif

using namespace std;
using namespace cv;

FrameThread::FrameThread(shared_ptr<FfmpegProcess> process,
    shared_ptr<Queue<FrameWrapper*>> pendingQueue,
    shared_ptr<Queue<FrameWrapper*>> completedQueue, uint32_t wid, uint32_t hgt) :
  Thread("frame"),
  ffmpegProcess(process),
  pendingFrameQueue(pendingQueue),
  completedFrameQueue(completedQueue),
  width(wid),
  height(hgt)
{
}

uint32_t FrameThread::run()
{
  uint32_t frameNumber = 0;
  bool previewChannel = false;
  while (!checkForExit())
  {
    FrameWrapper* wrapper = 0;
    if (!pendingFrameQueue->waitItem(&wrapper, 50))
    {
      continue;
    }

    // Frames captured by the Electron framework are encoded in the BGRA colorspace and
    // may be larger than size of the stimulus window.
    Mat frame(wrapper->height, wrapper->width, CV_8UC4, wrapper->frame);
    if ((wrapper->width != width) || (wrapper->height != height))
    {
      Mat resizedFrame;
      resize(frame, resizedFrame, Size2i(width, height), 0, 0, INTER_AREA);
      frame = resizedFrame;
    }

    // Write the raw frame to the ffmpeg process
    ffmpegProcess->writeStdin(frame.data, frame.total() * frame.elemSize());

    // Create the preview channel if needed
    if (!previewChannel)
    {
      unique_lock<mutex> lock(previewChannelMutex);
      if (!previewChannelName.empty())
      {
        printf("## [FrameThread] Open preview channel: %s\n", previewChannelName.c_str());
        previewChannel = true;
      }

      // Write the following to the named pipe:
      // - Magic number
      // - Frame number
      // - Width
      // - Height
      // - DataLength
      // - Data
    }

    // Add the frame to the completed queue
    completedFrameQueue->addItem(wrapper);
    frameNumber += 1;
  }

  // Close the preview channel
  if (previewChannel)
  {
    unlink(previewChannelName.c_str());
  }
  return 0;
}

void FrameThread::setPreviewChannel(string channelName)
{
  unique_lock<mutex> lock(previewChannelMutex);
  previewChannelName = channelName;
}
