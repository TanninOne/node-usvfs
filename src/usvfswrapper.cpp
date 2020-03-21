#define _WINSOCKAPI_
#include "string_cast.h"
#include <usvfs.h>
#include <nan.h>
#include <sstream>
#include <map>
#include <unordered_map>

using namespace Nan;
using namespace v8;


static std::wstring strerror(DWORD errorno) {
  wchar_t *errmsg = nullptr;

  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorno,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errmsg, 0, nullptr);

  if (errmsg) {
    for (int i = (wcslen(errmsg) - 1);
         (i >= 0) && ((errmsg[i] == '\n') || (errmsg[i] == '\r'));
         --i) {
      errmsg[i] = '\0';
    }

    return errmsg;
  }
  else {
    return L"Unknown error";
  }
}

inline v8::Local<v8::Value> WinApiException(
  DWORD lastError
  , const char *func = nullptr
  , const char* path = nullptr) {

  std::wstring errStr = strerror(lastError);
  std::string err = toMB(errStr.c_str(), CodePage::UTF8, errStr.size());
  return node::WinapiErrnoException(v8::Isolate::GetCurrent(), lastError, func, err.c_str(), path);
}

Local<String> operator "" _n(const char *input, size_t) {
  return Nan::New(input).ToLocalChecked();
}

std::wstring toWC(Isolate *isolate, const Local<Value> &input) {
  if (input->IsNullOrUndefined()) {
    return std::wstring();
  }
  String::Utf8Value temp(isolate, input);
  return toWC(*temp, CodePage::UTF8, temp.length());
}


void initParameters(Local<Context> context, USVFSParameters *target, const Local<Object> &parameters) {
  memset(target, 0x00, sizeof(USVFSParameters));
  memcpy(target->instanceName, *Utf8String(parameters->Get(context, "instanceName"_n).ToLocalChecked()), 64);
  memcpy(target->currentSHMName, *Utf8String(parameters->Get(context, "instanceName"_n).ToLocalChecked()), 60);
  _snprintf(target->currentInverseSHMName, 60, "inv_%s", *Utf8String(parameters->Get(context, "instanceName"_n).ToLocalChecked()));
  target->debugMode = parameters->Get(context, "debugMode"_n).ToLocalChecked()->BooleanValue(context->GetIsolate());
  target->logLevel = static_cast<LogLevel>(parameters->Get(context, "logLevel"_n).ToLocalChecked()->Int32Value(context).ToChecked());
}

NAN_METHOD(CreateVFS) {
  Local<Context> context = Nan::GetCurrentContext();
  Isolate* isolate = context->GetIsolate();

  if (info.Length() != 1) {
    Nan::ThrowError("Expected one parameter (settings)");
    return;
  }

  USVFSParameters param;
  initParameters(context, &param, info[0]->ToObject(context).ToLocalChecked());

  if (!CreateVFS(&param)) {
    isolate->ThrowException(WinApiException(::GetLastError(), "CreateVFS"));
  }
}

NAN_METHOD(ConnectVFS) {
  Local<Context> context = Nan::GetCurrentContext();
  Isolate* isolate = context->GetIsolate();

  if (info.Length() != 1) {
    Nan::ThrowError("Expected one parameter (settings)");
    return;
  }

  USVFSParameters param;
  initParameters(context, &param, info[0]->ToObject(context).ToLocalChecked());
  
  if (!ConnectVFS(&param)) {
    isolate->ThrowException(WinApiException(::GetLastError(), "ConnectVFS"));
  }
}

NAN_METHOD(DisconnectVFS) {
  DisconnectVFS();
}

NAN_METHOD(InitLogging) {
  Isolate* isolate = Isolate::GetCurrent();

  bool toLocal = false;

  if (info.Length() > 0) {
    toLocal = info[0]->BooleanValue(isolate);
  }

  InitLogging(toLocal);
}

NAN_METHOD(ClearVirtualMappings) {
  ClearVirtualMappings();
}

uint32_t convertFlags(Local<Context> context, const Local<Object> flagsDict) {
  Isolate *isolate = context->GetIsolate();

  static const std::map<std::string, uint32_t> LINK_FLAGS = {
    { "failIfExists", LINKFLAG_FAILIFEXISTS },
    { "monitorChanges", LINKFLAG_MONITORCHANGES },
    { "createTarget", LINKFLAG_CREATETARGET },
    { "recursive", LINKFLAG_RECURSIVE }
  };

  uint32_t flags = 0;
  Local<Array> keys = flagsDict->GetPropertyNames(context).ToLocalChecked();

  for (uint32_t i = 0; i < keys->Length(); ++i) {
    Local<Value> key = keys->Get(context, i).ToLocalChecked();
    if (flagsDict->Get(context, key).ToLocalChecked()->BooleanValue(isolate)) {
      auto iter = LINK_FLAGS.find(*Utf8String(keys->Get(context, i).ToLocalChecked()->ToString(context).ToLocalChecked()));
      flags |= iter->second;
    }
  }

  return flags;
}

class LogWorker : public AsyncProgressWorker {
  static const int BUFFER_SIZE = 1024;
public:
  LogWorker(Callback *logCB, Callback *callback)
    : AsyncProgressWorker(callback)
    , m_Progress(logCB)
    , m_Buffer(new char[BUFFER_SIZE]) {}

  ~LogWorker() {
    delete m_Progress;
  }

  virtual void Execute(const AsyncProgressWorker::ExecutionProgress &progress) override {
    while (m_Loop) {
      GetLogMessages(m_Buffer.get(), BUFFER_SIZE, true);
      progress.Send(m_Buffer.get(), strlen(m_Buffer.get()));
    }
  }

  virtual void HandleProgressCallback(const char *data, size_t size) override {
    Local<Context> context = Nan::GetCurrentContext();
    Nan::HandleScope scope;

    Local<Value> argv[] = {
      Nan::New<String>(data, size).ToLocalChecked()
    };

    v8::MaybeLocal<v8::Value> res = Nan::Call(*m_Progress, 1, argv);
    if (!res.ToLocalChecked()->BooleanValue(context->GetIsolate())) {
      m_Loop = false;
    }
  }

  void HandleOKCallback () {
    Nan::HandleScope scope;

    Local<Value> argv[] = {
        Null()
    };

    callback->Call(1, argv, async_resource);
  }

private:
  Callback *m_Progress;
  bool m_Loop { true };
  std::unique_ptr<char[]> m_Buffer;
};

NAN_METHOD(PollLogMessages) {
  Callback *logCB = new Callback(To<Function>(info[0]).ToLocalChecked());
  Callback *callback = new Callback(To<Function>(info[1]).ToLocalChecked());
  AsyncQueueWorker(new LogWorker(logCB, callback));
}

NAN_METHOD(GetLogMessage) {
  Local<Context> context = Nan::GetCurrentContext();
  Isolate *isolate = context->GetIsolate();

  bool blocking = false;

  if (info.Length() > 0) {
    blocking = info[0]->BooleanValue(isolate);
  }

  static char buffer[1024];
  if (GetLogMessages(buffer, 1024, blocking)) {
    info.GetReturnValue().Set(New<String>(buffer).ToLocalChecked());
  }
  else {
    info.GetReturnValue().SetNull();
  }
}

NAN_METHOD(VirtualLinkFile) {
  Local<Context> context = Nan::GetCurrentContext();
  Isolate* isolate = context->GetIsolate();

  if (info.Length() != 3) {
    Nan::ThrowError("Expected three parameters (source, destination, flags)");
    return;
  }

  std::wstring source = toWC(isolate, info[0]);
  std::wstring destination = toWC(isolate, info[1]);
  uint32_t flags = convertFlags(context, info[2]->ToObject(context).ToLocalChecked());

  if (!VirtualLinkFile(source.c_str(), destination.c_str(), flags)) {
    isolate->ThrowException(WinApiException(::GetLastError(), "VirtualLinkFile"));
  }
}

NAN_METHOD(VirtualLinkDirectoryStatic) {
  Local<Context> context = Nan::GetCurrentContext();
  Isolate* isolate = context->GetIsolate();

  if (info.Length() != 3) {
    Nan::ThrowError("Expected three parameters (source, destination, flags)");
    return;
  }

  std::wstring source = toWC(isolate, info[0]);
  std::wstring destination = toWC(isolate, info[1]);
  uint32_t flags = convertFlags(context, info[2]->ToObject(context).ToLocalChecked());

  if (!VirtualLinkDirectoryStatic(source.c_str(), destination.c_str(), flags)) {
    isolate->ThrowException(WinApiException(::GetLastError(), "VirtualLinkDirectoryStatic"));
  }
}

NAN_METHOD(CreateProcessHooked) {
  Local<Context> context = Nan::GetCurrentContext();
  Isolate* isolate = context->GetIsolate();

  if (info.Length() != 4) {
    Nan::ThrowError("Expected four parameters (applicationName, commandLine, currentDirectory, environment)");
    return;
  }

  std::wstring applicationName = toWC(isolate, info[0]);
  std::wstring commandLine = toWC(isolate, info[1]);
  std::wstring currentDirectory = toWC(isolate, info[2]);
  
  std::wstringstream str;
  if (!info[3]->IsNullOrUndefined()) {
    Local<Object> envObj = info[3]->ToObject(context).ToLocalChecked();
    Local<Array> keys = envObj->GetOwnPropertyNames(context).ToLocalChecked();
    for (uint32_t i = 0; i < keys->Length(); ++i) {
      auto key = keys->Get(context, i).ToLocalChecked();
      auto value = envObj->Get(context, key).ToLocalChecked();
      str << toWC(isolate, key) << L"=" << toWC(isolate, value);
      str.put('\0');
    }
    str.put('\0');
  }

  std::wstring environment = str.str();

  STARTUPINFO startupInfo;
  ::ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
  startupInfo.cb = sizeof(STARTUPINFO);

  PROCESS_INFORMATION processInformation;

  if (!CreateProcessHooked(applicationName.length() == 0 ? nullptr : applicationName.c_str(),
          commandLine.length() == 0 ? nullptr : &commandLine[0],
          nullptr, nullptr,
          false, CREATE_UNICODE_ENVIRONMENT,
          environment.length() == 0 ? nullptr : &environment[0],
          currentDirectory.length() == 0 ? nullptr : currentDirectory.c_str(),
          &startupInfo, &processInformation)) {
    isolate->ThrowException(WinApiException(::GetLastError(), "CreateProcessHooked"));
  }
}

NAN_MODULE_INIT(Init) {
  Nan::Set(target, "CreateVFS"_n, GetFunction(New<FunctionTemplate>(CreateVFS)).ToLocalChecked());
  Nan::Set(target, "ConnectVFS"_n, GetFunction(New<FunctionTemplate>(ConnectVFS)).ToLocalChecked());
  Nan::Set(target, "DisconnectVFS"_n, GetFunction(New<FunctionTemplate>(DisconnectVFS)).ToLocalChecked());
  Nan::Set(target, "ClearVirtualMappings"_n, GetFunction(New<FunctionTemplate>(ClearVirtualMappings)).ToLocalChecked());
  Nan::Set(target, "VirtualLinkFile"_n, GetFunction(New<FunctionTemplate>(VirtualLinkFile)).ToLocalChecked());
  Nan::Set(target, "VirtualLinkDirectoryStatic"_n, GetFunction(New<FunctionTemplate>(VirtualLinkDirectoryStatic)).ToLocalChecked());
  Nan::Set(target, "CreateProcessHooked"_n, GetFunction(New<FunctionTemplate>(CreateProcessHooked)).ToLocalChecked());
  Nan::Set(target, "InitLogging"_n, GetFunction(New<FunctionTemplate>(InitLogging)).ToLocalChecked());
  Nan::Set(target, "GetLogMessage"_n, GetFunction(New<FunctionTemplate>(GetLogMessage)).ToLocalChecked());
  Nan::Set(target, "PollLogMessages"_n, GetFunction(New<FunctionTemplate>(PollLogMessages)).ToLocalChecked());
}

NODE_MODULE(winapi, Init)
