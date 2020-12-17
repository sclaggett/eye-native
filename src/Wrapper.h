// Wrapper.h: This file defines the functions that translate the JavaScript
// parameters and return values to and from C++. Everything is set up in the
// Init() function, called via index.js, and invoke the C++ functions
// from Native.h.

#include <napi.h>

namespace wrapper
{
  Napi::Object Init(Napi::Env env, Napi::Object exports);

  void initialized(const Napi::CallbackInfo& info);
  Napi::String open(const Napi::CallbackInfo& info);
  Napi::Number write(const Napi::CallbackInfo& info);
  Napi::Int32Array checkCompleted(const Napi::CallbackInfo& info);
  void close(const Napi::CallbackInfo& info);
}
