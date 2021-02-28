#include "FrameThread.h"
#include "FrameHeader.h"
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#ifdef _WIN32
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

using namespace std;
using namespace cv;

// This should go in a class that hides the cross-platform messiness
bool WriteData(int fd, const uint8_t* data, uint32_t length)
{
  uint32_t bytesWritten = 0;
  while (bytesWritten < length)
  {
    ssize_t ret = write(fd, data + bytesWritten, length - bytesWritten);
    if (ret == -1)
    {
      return false;
    }
    bytesWritten += ret;
  }
  return true;
}

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
  int namedPipe = 0;
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
    uint32_t frameLength = frame.total() * frame.elemSize();
    ffmpegProcess->writeStdin(frame.data, frameLength);

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
          printf("[FrameThread] Failed to create named pipe\n");
          channelState = CHANNEL_ERROR;
          continue;
        }

        // Set the state to opening while we wait for the remote process to connect
        printf("## [FrameThread] Pipe created, waiting for renderer process to connect\n");
        channelState = CHANNEL_OPENING;
#else
        // Attempt to create the named pipe in nonblocking mode. This will only succeed
        // if the remote process has already opened the pipe for reading. We use this
        // approach so this thread doesn't hang forever waiting on the remote process
        namedPipe = open(previewChannelName.c_str(), O_WRONLY | O_NONBLOCK);
        if (namedPipe == -1)
        {
          // A response of ENXIO is expected and means the other end of the pipe hasn't
          // been opened for reading, any other error code means something else went wrong
          if (errno != ENXIO)
          {
            printf("[FrameThread] Failed to create named pipe\n");
            channelState = CHANNEL_ERROR;
          }
          continue;
        }

        // Switch the named pipe to blocking mode now that the other end is connected
        int flags = fcntl(namedPipe, F_GETFL, 0);
        flags &= ~O_NONBLOCK;
        fcntl(namedPipe, F_SETFL, flags);

        // Skip the opening state and go straight to open
        channelState = CHANNEL_OPEN;
#endif
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
        printf("[FrameThread] Named pipe connetion failed: %i\n", err);
        channelState = CHANNEL_ERROR;
        continue;
      }
#else
#endif
    }
    
    // Write the frame to the named pipe once the connection is established
    if (channelState == CHANNEL_OPEN)
    {
      string header = frameheader::format(frameNumber, width, height, frameLength);
#ifdef _WIN32
#else
      // Write the frame to the named pipe
      if (!WriteData(namedPipe, (const uint8_t*)header.data(), header.size()) ||
        !WriteData(namedPipe, (const uint8_t*)frame.data, frameLength))
      {
        printf("[FrameThread] Failed to write frame to pipe\n");
        channelState = CHANNEL_ERROR;
        continue;
      }
#endif
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
