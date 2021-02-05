#include "PreviewThread.h"
#ifdef _WIN32
#else
  #include <unistd.h>
#endif

using namespace std;
using namespace cv;

PreviewThread::PreviewThread(string name, shared_ptr<Queue<cv::Mat*>> queue) :
  Thread("preview"),
  channelName(name),
  previewQueue(queue)
{
}

uint32_t PreviewThread::run()
{
  printf("## Starting preview thread\n");

  uint32_t temp = 0;
  while (!checkForExit())
  {
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10 * 1000);
#endif

    // Read the following from the named pipe:
    // - Magic number
    // - Frame number
    // - Width
    // - Height
    // - DataLength
    // - Data

    uint32_t width = 1024;
    uint32_t height = 768;
    size_t length = width * height * 4;
    uint8_t* data = new uint8_t[length];
    uint32_t offset = 0;
    for (uint32_t y = 0; y < height; ++y)
    {
      for (uint32_t x = 0; x < width; ++x)
      {
        *(data + offset++) = temp % 255; // B
        *(data + offset++) = (temp + 64) % 255; // G
        *(data + offset++) = (temp + 128 % 255); // R
        *(data + offset++) = 255; // A
      }
    }

    Mat wrapped(height, width, CV_8UC4, data);
    Mat* copy = new Mat;
    wrapped.copyTo(*copy);
    previewQueue->addItem(copy);

    delete [] data;
    temp += 1;
  }

  printf("## Stopping preview thread\n");
  return 0;
}
