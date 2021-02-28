#include "PreviewThread.h"
#include "FrameHeader.h"
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
bool ReadData(int fd, uint8_t* data, uint32_t length)
{
  uint32_t bytesRead = 0;
  while (bytesRead < length)
  {
    ssize_t ret = read(fd, data + bytesRead, length - bytesRead);
    if (ret == -1)
    {
      return false;
    }
    bytesRead += ret;
  }
  return true;
}

PreviewThread::PreviewThread(string name, shared_ptr<Queue<cv::Mat*>> queue) :
  Thread("preview"),
  channelName(name),
  previewQueue(queue)
{
}

uint32_t PreviewThread::run()
{
  printf("## Starting preview thread\n");

  // Open the named pipe
#ifdef _WIN32
#else
  int namedPipe = open(channelName.c_str(), O_RDONLY);
  if (namedPipe == -1)
  {
    printf("## Failed to open named pipe\n");
    return 1;
  }
#endif

  uint8_t frameHeader[FRAME_HEADER_SIZE];
  uint8_t* buffer = 0;
  uint32_t bufferSize = 0;
  uint32_t number, width, height, length;
  while (!checkForExit())
  {
    // Read the frame header from the named pipe and extract the fields
    if (!ReadData(namedPipe, &(frameHeader[0]), FRAME_HEADER_SIZE))
    {
      printf("[PreviewThread] Failed to read from named pipe\n");
      return 1;
    }
    if (!frameheader::parse(frameHeader, number, width, height, length))
    {
      printf("[PreviewThread] Failed to parse frame header\n");
      return 1;
    }

    // Read the frame
    if (bufferSize < length)
    {
      if (bufferSize != 0)
      {
        delete [] buffer;
      }
      bufferSize = length;
      buffer = new uint8_t[bufferSize];
    }
    if (!ReadData(namedPipe, buffer, length))
    {
      printf("[PreviewThread] Failed to read from named pipe\n");
      return 1;
    }

    // Wrap the frame as an OpenCV matrix and add it to the preview queue
    Mat wrapped(height, width, CV_8UC4, buffer);
    Mat* copy = new Mat;
    wrapped.copyTo(*copy);
    previewQueue->addItem(copy);
  }

  printf("## Stopping preview thread\n");
  return 0;
}
