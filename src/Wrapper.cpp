#include "Wrapper.h"
#include "FfmpegProcess.h"
#include "FrameThread.h"
#include "Native.h"
#include <stdio.h>

using namespace std;

Napi::Object wrapper::Init(Napi::Env env, Napi::Object exports)
{
  exports.Set("initialize", Napi::Function::New(env, wrapper::initialized));
  exports.Set("open", Napi::Function::New(env, wrapper::open));
  exports.Set("write", Napi::Function::New(env, wrapper::write));
  exports.Set("checkCompleted", Napi::Function::New(env, wrapper::checkCompleted));
  exports.Set("close", Napi::Function::New(env, wrapper::close));
  return exports;
}

void wrapper::initialized(const Napi::CallbackInfo& info)
{
  Napi::Env env = info.Env();
  if ((info.Length() != 1) || !info[0].IsString())
  {
    Napi::TypeError::New(env, "Incorrect parameter type").ThrowAsJavaScriptException();
    return;
  }
  Napi::String ffmpegPath = info[0].As<Napi::String>();  
  native::initialize(env, ffmpegPath);
}

Napi::String wrapper::open(const Napi::CallbackInfo& info)
{
  Napi::Env env = info.Env();
  if ((info.Length() != 5) ||
    !info[0].IsNumber() ||
    !info[1].IsNumber() ||
    !info[2].IsNumber() ||
    !info[3].IsString() ||
    !info[4].IsString())
  {
    Napi::TypeError::New(env, "Incorrect parameter type").ThrowAsJavaScriptException();
    return Napi::String();
  }
  Napi::Number width = info[0].As<Napi::Number>();
  Napi::Number height = info[1].As<Napi::Number>();
  Napi::Number fps = info[2].As<Napi::Number>();
  Napi::String encoder = info[3].As<Napi::String>();
  Napi::String outputPath = info[4].As<Napi::String>();
  Napi::String returnValue = Napi::String::New(env,
    native::open(env, width, height, fps, encoder, outputPath));
  return returnValue;  
}

Napi::Number wrapper::write(const Napi::CallbackInfo& info)
{
  Napi::Env env = info.Env();
  if ((info.Length() != 3) ||
    !info[0].IsBuffer() ||
    !info[1].IsNumber() ||
    !info[2].IsNumber())
  {
    Napi::TypeError::New(env, "Incorrect parameter type").ThrowAsJavaScriptException();
    return Napi::Number::New(env, -1);
  }
  Napi::TypedArray typedArray = info[0].As<Napi::TypedArray>();
  if (typedArray.TypedArrayType() != napi_uint8_array)
  {
    Napi::TypeError::New(env, "Unexpected buffer type").ThrowAsJavaScriptException();
    return Napi::Number::New(env, -1);
  }
  Napi::Buffer<uint8_t> frame = info[0].As<Napi::Buffer<uint8_t>>();
  Napi::Number width = info[1].As<Napi::Number>();
  Napi::Number height = info[2].As<Napi::Number>();
  Napi::Number returnValue = Napi::Number::New(env,
    native::write(env, frame.Data(), frame.Length(), width, height));
  return returnValue;
}

Napi::Int32Array wrapper::checkCompleted(const Napi::CallbackInfo& info)
{
  Napi::Env env = info.Env();
  vector<int> completedIds = native::checkCompleted(env);
  Napi::Int32Array returnValue = Napi::Int32Array::New(env, completedIds.size());
  memcpy(returnValue.Data(), completedIds.data(), sizeof(int32_t) * completedIds.size());
  return returnValue;
}

void wrapper::close(const Napi::CallbackInfo& info)
{
  Napi::Env env = info.Env();
  native::close(env);
}
