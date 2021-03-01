#include "PipeReader.h"
#include "Platform.h"

using namespace std;

PipeReader::PipeReader(string name, uint32_t f) :
  Thread(name),
  file(f)
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
  printf("[PipeReader] ## Spawning pipe reader, checkForExit = %i\n", checkForExit());

  char buffer[1024];
  while (!checkForExit())
  {
    printf("[PipeReader] ## A\n");

    // Wait for data to become available to read and continue around the loop if nothing
    // arrives within 100 ms
    int32_t ret = platform::waitForData(file, 100);
    if (ret == -1)
    {
      printf("[PipeReader] Failed to read from pipe\n");
      return 0;
    }
    else if (ret == 0)
    {
      continue;
    }

    printf("[PipeReader] ## B\n");

    // Read data from the pipe and append it to the data string
    ret = platform::read(file, (uint8_t*)&(buffer[0]), 1023);
    if (ret == -1)
    {
      printf("[PipeReader] Failed to read from pipe\n");
      return 0;
    }
    else if (ret == 0)
    {
      break;
    }
    else if (ret > 0)
    {
      buffer[ret] = 0;
      unique_lock<mutex> lock(dataMutex);
      data += buffer;
    }

    printf("[PipeReader] ## C\n");
  }

  printf("[PipeReader] ## Pipe reader exiting\n");
  return 0;
}
