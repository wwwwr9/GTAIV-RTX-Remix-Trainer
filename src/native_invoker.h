#pragma once

#include "common.h"
#include "memory.h"

namespace remix::native {

bool Initialize(const memory::Module& module);
bool IsReady();
u32 Joaat(const char* text);

template <typename R, typename... Args>
R Invoke(const char* name, Args... args);

template <typename... Args>
void InvokeVoid(const char* name, Args... args);

}  // namespace remix::native

#include "native_invoker.inl"

