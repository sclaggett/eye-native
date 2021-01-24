#pragma once

#include <mutex>
#include <opencv2/core/core.hpp>
#include "Thread.h"
#include "Queue.hpp"

class PreviewThread : public Thread
{
public:
  PreviewThread(std::string channelName, std::shared_ptr<Queue<cv::Mat*>> previewQueue);
  virtual ~PreviewThread() {};

  void* run();

private:
  std::string channelName;
  std::shared_ptr<Queue<cv::Mat*>> previewQueue;
};
