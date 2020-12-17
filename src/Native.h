// Native.h: This file defines the C++ functions that make up the native library.
// They are invoked by the functions in Wrapper.h.

#include <napi.h>
#include <vector>

namespace native
{
  void initialize(Napi::Env env, std::string ffmpegPath);
	std::string open(Napi::Env env, int width, int height, int fps, std::string encoder,
    std::string outputPath);
  int32_t write(Napi::Env env, uint8_t* frame, size_t length, int width, int height);
  std::vector<int32_t> checkCompleted(Napi::Env env);
  void close(Napi::Env env);
}
