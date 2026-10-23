#include "overlay.h"

#include "logger.h"

namespace remix::overlay {
namespace {

constexpr wchar_t kClassName[] = L"RemixTrainerMenuOverlay";

struct State {
    HMODULE module = nullptr;
    HWND window = nullptr;
    HWND gameWindow = nullptr;
    CRITICAL_SECTION lock{};
    bool lockReady = false;
    bool visible = false;
    int selected = 0;
    std::vector<std::string> lines;
    std::string status;
    bool showStatus = false;
};

State g_state;

void EnsureLock() {
    if (!g_state.lockReady) {
        InitializeCriticalSection(&g_state.lock);
        g_state.lockReady = true;
    }
}

BOOL CALLBACK EnumGameWindows(HWND hwnd, LPARAM param) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() || hwnd == g_state.window || !IsWindowVisible(hwnd)) {
        return TRUE;
    }

    wchar_t className[128]{};
    GetClassNameW(hwnd, className, static_cast<int>(std::size(className)));
    if (_wcsicmp(className, kClassName) == 0) {
        return TRUE;
    }

    *reinterpret_cast<HWND*>(param) = hwnd;
    return FALSE;
}

HWND FindGameWindow() {
    HWND result = nullptr;
    EnumWindows(EnumGameWindows, reinterpret_cast<LPARAM>(&result));
    return result;
}

struct Snapshot {
    bool visible = false;
    int selected = 0;
    std::vector<std::string> lines;
    std::string status;
    bool showStatus = false;
};

Snapshot GetSnapshot() {
    Snapshot snapshot;
    EnsureLock();
    EnterCriticalSection(&g_state.lock);
    snapshot.visible = g_state.visible;
    snapshot.selected = g_state.selected;
    snapshot.lines = g_state.lines;
    snapshot.status = g_state.status;
    snapshot.showStatus = g_state.showStatus;
    LeaveCriticalSection(&g_state.lock);
    return snapshot;
}

void DrawTextLine(HDC dc, int x, int y, const std::string& text, COLORREF color, bool strong) {
    SetTextColor(dc, color);
    if (strong) {
        RECT glow{x - 1, y - 1, x + 900, y + 24};
        SetTextColor(dc, RGB(0, 0, 0));
        DrawTextA(dc, text.c_str(), static_cast<int>(text.size()), &glow, DT_LEFT | DT_SINGLELINE);
        SetTextColor(dc, color);
    }
    RECT rect{x, y, x + 900, y + 24};
    DrawTextA(dc, text.c_str(), static_cast<int>(text.size()), &rect, DT_LEFT | DT_SINGLELINE);
}

void PaintOverlay(HWND hwnd) {
    const Snapshot snapshot = GetSnapshot();

    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);

    RECT rect{};
    GetClientRect(hwnd, &rect);
    HBRUSH transparentKey = CreateSolidBrush(RGB(1, 1, 1));
    FillRect(dc, &rect, transparentKey);
    DeleteObject(transparentKey);

    SetBkMode(dc, TRANSPARENT);
    HFONT font = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));

    if (snapshot.visible) {
        const int width = 560;
        const int height = 92 + static_cast<int>(snapshot.lines.size()) * 24;
        RECT panel{12, 12, 12 + width, 12 + height};
        HBRUSH panelBrush = CreateSolidBrush(RGB(18, 22, 24));
        FillRect(dc, &panel, panelBrush);
        DeleteObject(panelBrush);

        HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(255, 201, 92));
        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(dc, borderPen));
        HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(dc, GetStockObject(HOLLOW_BRUSH)));
        Rectangle(dc, panel.left, panel.top, panel.right, panel.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(borderPen);

        DrawTextLine(dc, 30, 28, "REMIX TRAINER", RGB(255, 218, 120), true);
        DrawTextLine(dc, 30, 52, "Num8/2 move | Num5/Enter select | Num0/Back back | F1 close",
                     RGB(190, 204, 200), false);

        int y = 86;
        for (size_t i = 0; i < snapshot.lines.size(); ++i) {
            const bool selected = static_cast<int>(i) == snapshot.selected;
            std::string line = selected ? "> " + snapshot.lines[i] : "  " + snapshot.lines[i];
            DrawTextLine(dc, 30, y, line, selected ? RGB(255, 229, 122) : RGB(236, 242, 238),
                         selected);
            y += 24;
        }
        if (snapshot.showStatus && !snapshot.status.empty()) {
            DrawTextLine(dc, 30, panel.bottom + 12, snapshot.status, RGB(255, 229, 122), true);
        }
    } else if (snapshot.showStatus && !snapshot.status.empty()) {
        DrawTextLine(dc, 30, 30, snapshot.status, RGB(255, 229, 122), true);
    }

    SelectObject(dc, oldFont);
    DeleteObject(font);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT:
        PaintOverlay(hwnd);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void UpdateOverlayPosition() {
    if (!g_state.window) {
        return;
    }

    if (!g_state.gameWindow || !IsWindow(g_state.gameWindow)) {
        g_state.gameWindow = FindGameWindow();
    }

    RECT rect{100, 100, 1000, 700};
    if (g_state.gameWindow) {
        GetWindowRect(g_state.gameWindow, &rect);
    }

    const int width = std::max(680, static_cast<int>((rect.right - rect.left) / 2));
    const int height = std::max(360, static_cast<int>((rect.bottom - rect.top) / 2));
    SetWindowPos(g_state.window, HWND_TOPMOST, rect.left + 20, rect.top + 40, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

DWORD WINAPI OverlayThread(void*) {
    EnsureLock();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_state.module;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    g_state.window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, L"Remix Trainer Menu Overlay", WS_POPUP, 100, 100, 680, 360,
        nullptr, nullptr, g_state.module, nullptr);

    if (!g_state.window) {
        log::Error("Failed to create GDI menu overlay window");
        return 0;
    }

    SetLayeredWindowAttributes(g_state.window, RGB(1, 1, 1), 0, LWA_COLORKEY);
    SetTimer(g_state.window, 1, 33, nullptr);
    log::Info("GDI menu overlay initialized");

    MSG msg{};
    while (true) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        UpdateOverlayPosition();
        Sleep(16);
    }
}

}  // namespace

void Start(HMODULE module) {
    EnsureLock();
    g_state.module = module;

    HANDLE thread = CreateThread(nullptr, 0, OverlayThread, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    }
}

void SetMenuState(bool visible, const std::vector<std::string>& lines, int selected,
                  const std::string& status, bool showStatus) {
    EnsureLock();
    EnterCriticalSection(&g_state.lock);
    g_state.visible = visible;
    g_state.lines = lines;
    g_state.selected = selected;
    g_state.status = status;
    g_state.showStatus = showStatus;
    LeaveCriticalSection(&g_state.lock);
}

}  // namespace remix::overlay

