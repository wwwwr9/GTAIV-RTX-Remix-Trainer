#include "common.h"
#include "config.h"
#include "logger.h"
#include "memory.h"
#include "native_invoker.h"
#include "overlay.h"
#include "trainer.h"

namespace {

HMODULE g_module = nullptr;
LONG g_started = 0;
remix::uptr g_gameTickReturn = 0;
LONG g_firstTickLogged = 0;

void __stdcall TrainerTickBridge() {
    if (InterlockedCompareExchange(&g_firstTickLogged, 1, 0) == 0) {
        remix::log::Info("First game tick reached");
    }
    remix::Trainer::Instance().Tick();
}

extern "C" __declspec(naked) void GameTickHook() {
    __asm {
        pushfd
        pushad
        call TrainerTickBridge
        popad
        popfd
        lea eax, [esp + 10h]
        push eax
        jmp dword ptr [g_gameTickReturn]
    }
}

DWORD WINAPI InitializeThread(void*) {
    const std::wstring exeDir = remix::ParentDirectory(remix::GetModulePath(nullptr));
    const std::wstring asiDir = remix::ParentDirectory(remix::GetModulePath(g_module));

    remix::log::Initialize(exeDir + L"\\remix_trainer.log");
    if (_wcsicmp(exeDir.c_str(), asiDir.c_str()) != 0) {
        remix::log::AddMirror(asiDir + L"\\remix_trainer.log");
    }
    remix::log::Info("ASI directory: %s", remix::WideToUtf8(asiDir).c_str());
    remix::log::Info("Game directory: %s", remix::WideToUtf8(exeDir).c_str());
    remix::overlay::Start(g_module);

    Sleep(1500);

    remix::Trainer::Instance().Configure(remix::LoadConfig(asiDir + L"\\remix_trainer.ini"));

    remix::memory::Module gta{};
    if (!remix::memory::LoadMainModule(&gta)) {
        remix::log::Error("Failed to inspect main module; trainer disabled");
        return 0;
    }

    if (!remix::native::Initialize(gta)) {
        remix::log::Error("Native initialization failed; trainer disabled");
        return 0;
    }

    const remix::uptr tick =
        remix::memory::FindPattern(gta, ".text", "8D 44 24 10 50 FF D5 8D 44 24 10");
    if (!tick) {
        remix::log::Error("Game tick pattern not found; trainer disabled");
        return 0;
    }

    g_gameTickReturn = tick + 5;
    if (!remix::memory::InstallJump(tick, reinterpret_cast<void*>(&GameTickHook), 5)) {
        remix::log::Error("Failed to install game tick hook; trainer disabled");
        return 0;
    }

    remix::log::Info("Game tick hook installed at 0x%p -> return 0x%p",
                     reinterpret_cast<void*>(tick), reinterpret_cast<void*>(g_gameTickReturn));
    remix::log::Info("remix_trainer initialized");
    return 0;
}

void StartOnce() {
    if (InterlockedCompareExchange(&g_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(nullptr, 0, InitializeThread, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    }
}

}  // namespace

extern "C" __declspec(dllexport) void InitializeASI() {
    StartOnce();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        StartOnce();
    }
    return TRUE;
}
