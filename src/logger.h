#pragma once

#include "common.h"

namespace remix::log {

void Initialize(const std::wstring& path);
void AddMirror(const std::wstring& path);
void Shutdown();
void Info(const char* fmt, ...);
void Error(const char* fmt, ...);

}  // namespace remix::log
