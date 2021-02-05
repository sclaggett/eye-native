#pragma once

#include <memory>
#include <mutex>
#include <string>
#include "Thread.h"

class PipeReader : public Thread
{
public:
#ifdef _WIN32
  PipeReader(std::string name, HANDLE fd);
#else
  PipeReader(std::string name, int fd);
#endif
  virtual ~PipeReader() {};

public:
  std::string getData();
  uint32_t run();

private:
#ifdef _WIN32
  HANDLE fd = NULL;
#else
  int fd = 0;
#endif
  std::string data;
  std::mutex dataMutex;
};

