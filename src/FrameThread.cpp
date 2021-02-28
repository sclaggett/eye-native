#include "FrameThread.h"
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#ifdef _WIN32
#else
  #include <unistd.h>
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

// Define constants to keep track of the three states of the named pipe
#define CHANNEL_CLOSED 0
#define CHANNEL_OPENING 1
#define CHANNEL_OPEN 2
#define CHANNEL_ERROR 3

uint32_t FrameThread::run()
{
  uint32_t frameNumber = 0;
  uint32_t channelState = CHANNEL_CLOSED;
#ifdef _WIN32
  HANDLE namedPipe = 0;
#else
#endif
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

    // Create the preview channel
    if (channelState == CHANNEL_CLOSED)
    {
      unique_lock<mutex> lock(previewChannelMutex);
      if (!previewChannelName.empty())
      {
#ifdef _WIN32
		// Create the named pipe
        printf("## [FrameThread] Creating named pipe: %s\n", previewChannelName.c_str());
		namedPipe = CreateNamedPipe(previewChannelName.c_str(), PIPE_ACCESS_OUTBOUND,
		  PIPE_TYPE_BYTE | PIPE_NOWAIT, 1, 0, 0, 0, NULL);
		if (namedPipe == INVALID_HANDLE_VALUE)
		{
		  printf("Error: Failed to create named pipe\n");
		  channelState = CHANNEL_ERROR;
		  continue;
		}
        printf("## [FrameThread] Pipe created, waiting for renderer process to connect\n");
#else
#endif
        channelState = CHANNEL_OPENING;
      }
	}

    // Check asynchronously if the renderer process has connected to the preview channel
	if (channelState == CHANNEL_OPENING)
	{
#ifdef _WIN32
	  // Wait for the client to connect
	  ConnectNamedPipe(namedPipe, NULL);
	  DWORD err = GetLastError();
	  if (err == ERROR_PIPE_CONNECTED)
	  {
        printf("## [FrameThread] Renderer process has connected\n");
		channelState = CHANNEL_OPEN;
	  }
	  else if (err != ERROR_IO_PENDING)
	  {
		printf("Error: Named pipe connetion failed: %i\n", err);
		channelState = CHANNEL_ERROR;
		continue;
	  }
#else
#endif
	}
	
	// Write the frame to the named pipe once the connection is established
	if (channelState == CHANNEL_OPEN)
	{
      printf("## [FrameThread] Write frame to pipe\n");
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
  if (channelState != CHANNEL_CLOSED)
  {
#ifdef _WIN32
  	CloseHandle(namedPipe);
#else
    unlink(previewChannelName.c_str());
#endif
  }
  

  return 0;
}

void FrameThread::setPreviewChannel(string channelName)
{
  unique_lock<mutex> lock(previewChannelMutex);
  previewChannelName = channelName;
}
