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

std::wstring toWC(const Local<Value> &input) {
  if (input->IsNullOrUndefined()) {
    return std::wstring();
  }
  String::Utf8Value temp(input);
  return toWC(*temp, CodePage::UTF8, temp.length());
}


void initParameters(USVFSParameters *target, const Local<Object> &parameters) {
  memset(target, 0x00, sizeof(USVFSParameters));
  memcpy(target->instanceName, *Utf8String(parameters->Get("instanceName"_n)), 64);
  memcpy(target->currentSHMName, *Utf8String(parameters->Get("instanceName"_n)), 60);
  _snprintf(target->currentInverseSHMName, 60, "inv_%s", *Utf8String(parameters->Get("instanceName"_n)));
  target->debugMode = parameters->Get("debugMode"_n)->BooleanValue();
  target->logLevel = static_cast<LogLevel>(parameters->Get("logLevel"_n)->Int32Value());
}

NAN_METHOD(CreateVFS) {
  Isolate* isolate = Isolate::GetCurrent();

  if (info.Length() != 1) {
    Nan::ThrowError("Expected one parameter (settings)");
    return;
  }

  USVFSParameters param;
  initParameters(&param, info[0]->ToObject());

  if (!CreateVFS(&param)) {
    isolate->ThrowException(WinApiException(::GetLastError(), "CreateVFS"));
  }
}

NAN_METHOD(ConnectVFS) {
  Isolate* isolate = Isolate::GetCurrent();

  if (info.Length() != 1) {
    Nan::ThrowError("Expected one parameter (settings)");
    return;
  }

  USVFSParameters param;
  initParameters(&param, info[0]->ToObject());
  
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
    toLocal = info[0]->BooleanValue();
  }

  InitLogging(toLocal);
}

NAN_METHOD(ClearVirtualMappings) {
  ClearVirtualMappings();
}

uint32_t convertFlags(const Local<Object> flagsDict) {
  static const std::map<std::string, uint32_t> LINK_FLAGS = {
    { "failIfExists", LINKFLAG_FAILIFEXISTS },
    { "monitorChanges", LINKFLAG_MONITORCHANGES },
    { "createTarget", LINKFLAG_CREATETARGET },
    { "recursive", LINKFLAG_RECURSIVE }
  };

  uint32_t flags = 0;
  Local<Array> keys = flagsDict->GetPropertyNames();

  for (uint32_t i = 0; i < keys->Length(); ++i) {
    if (flagsDict->Get(keys->Get(i))->BooleanValue()) {
      auto iter = LINK_FLAGS.find(*Utf8String(keys->Get(i)->ToString()));
      flags |= iter->second;
    }
  }

  return flags;
}

NAN_METHOD(GetLogMessages) {
  Isolate* isolate = Isolate::GetCurrent();

  bool blocking = false;

  if (info.Length() > 0) {
    blocking = info[0]->BooleanValue();
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
  Isolate* isolate = Isolate::GetCurrent();

  if (info.Length() != 3) {
    Nan::ThrowError("Expected three parameters (source, destination, flags)");
    return;
  }

  std::wstring source = toWC(info[0]);
  std::wstring destination = toWC(info[1]);
  uint32_t flags = convertFlags(info[2]->ToObject());

  if (!VirtualLinkFile(source.c_str(), destination.c_str(), flags)) {
    isolate->ThrowException(WinApiException(::GetLastError(), "VirtualLinkFile"));
  }
}

NAN_METHOD(VirtualLinkDirectoryStatic) {
  Isolate* isolate = Isolate::GetCurrent();

  if (info.Length() != 3) {
    Nan::ThrowError("Expected three parameters (source, destination, flags)");
    return;
  }

  std::wstring source = toWC(info[0]);
  std::wstring destination = toWC(info[1]);
  uint32_t flags = convertFlags(info[2]->ToObject());

  if (!VirtualLinkDirectoryStatic(source.c_str(), destination.c_str(), flags)) {
    isolate->ThrowException(WinApiException(::GetLastError(), "VirtualLinkDirectoryStatic"));
  }
}

NAN_METHOD(CreateProcessHooked) {
  Isolate* isolate = Isolate::GetCurrent();

  if (info.Length() != 4) {
    Nan::ThrowError("Expected four parameters (applicationName, commandLine, currentDirectory, environment)");
    return;
  }

  std::wstring applicationName = toWC(info[0]);
  std::wstring commandLine = toWC(info[1]);
  std::wstring currentDirectory = toWC(info[2]);
  
  std::wstringstream str;
  if (!info[3]->IsNullOrUndefined()) {
    Local<Object> envObj = info[3]->ToObject();
    Local<Array> keys = envObj->GetOwnPropertyNames();
    for (uint32_t i = 0; i < keys->Length(); ++i) {
      str << toWC(keys->Get(i)) << L"=" << toWC(envObj->Get(keys->Get(i)));
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
  Nan::Set(target, "GetLogMessages"_n, GetFunction(New<FunctionTemplate>(GetLogMessages)).ToLocalChecked());
}

NODE_MODULE(winapi, Init)
