#include "PipeReader.h"
#ifdef _WIN32
#else
  #include <unistd.h>
#endif

using namespace std;

#ifdef _WIN32
PipeReader::PipeReader(string name, HANDLE f) :
#else
PipeReader::PipeReader(string name, int f) :
#endif
  Thread(name),
  fd(f)
{
}

string PipeReader::getData()
{
  unique_lock<mutex> lock(dataMutex);
  string ret = data;
  data.clear();
  return ret;
}

uint32_t PipeReader::run()
{
  char buffer[1024];
  while (!checkForExit())
  {
#ifdef _WIN32
    DWORD dwRead = 0;
    if (!ReadFile(fd, buffer, 1023, &dwRead, NULL))
    {
      printf("ERROR: Failed to read from pipe\n");
      return 0;
    }
    if (dwRead == 0)
    {
      break;
    }
    else
    {
      buffer[dwRead] = 0;
      unique_lock<mutex> lock(dataMutex);
      data += buffer;
    }
#else
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    int rv = select(fd + 1, &set, NULL, NULL, &timeout);
    if (rv == -1)
    {
      printf("ERROR: Failed to wait for pipe\n");
      return 0;
    }
    if (rv == 0)
    {
      continue;
    }
    ssize_t readCount = read(fd, buffer, 1023);
    if (readCount == -1)
    {
      printf("ERROR: Failed to read from pipe\n");
      return 0;
    }
    if (readCount == 0)
    {
      break;
    }
    if (readCount > 0)
    {
      buffer[readCount] = 0;
      unique_lock<mutex> lock(dataMutex);
      data += buffer;
    }
#endif
  }
  return 0;
}
