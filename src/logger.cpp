#include "logger.h"

#include <cstdarg>
#include <ctime>

namespace remix::log {
namespace {

HANDLE g_files[2] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
CRITICAL_SECTION g_lock{};
bool g_lockReady = false;

void WriteLine(const char* level, const char* fmt, va_list args) {
    if (g_files[0] == INVALID_HANDLE_VALUE && g_files[1] == INVALID_HANDLE_VALUE) {
        return;
    }

    char message[2048]{};
    vsnprintf_s(message, sizeof(message), _TRUNCATE, fmt, args);

    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);

    char line[2300]{};
    snprintf(line, sizeof(line), "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\r\n",
             local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
             local.tm_hour, local.tm_min, local.tm_sec, level, message);

    EnterCriticalSection(&g_lock);
    for (HANDLE file : g_files) {
        if (file == INVALID_HANDLE_VALUE) {
            continue;
        }
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
    }
    LeaveCriticalSection(&g_lock);
}

}  // namespace

void Initialize(const std::wstring& path) {
    if (!g_lockReady) {
        InitializeCriticalSection(&g_lock);
        g_lockReady = true;
    }

    if (g_files[0] != INVALID_HANDLE_VALUE) {
        CloseHandle(g_files[0]);
        g_files[0] = INVALID_HANDLE_VALUE;
    }

    g_files[0] = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    Info("remix_trainer log started");
}

void AddMirror(const std::wstring& path) {
    if (!g_lockReady) {
        InitializeCriticalSection(&g_lock);
        g_lockReady = true;
    }

    if (g_files[1] != INVALID_HANDLE_VALUE) {
        CloseHandle(g_files[1]);
        g_files[1] = INVALID_HANDLE_VALUE;
    }

    g_files[1] = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    Info("Mirror log path: %s", WideToUtf8(path).c_str());
}

void Shutdown() {
    for (HANDLE& file : g_files) {
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
        }
    }
    if (g_lockReady) {
        DeleteCriticalSection(&g_lock);
        g_lockReady = false;
    }
}

void Info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    WriteLine("INFO", fmt, args);
    va_end(args);
}

void Error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    WriteLine("ERROR", fmt, args);
    va_end(args);
}

}  // namespace remix::log
