#include <JKPlatform.h>

#ifdef _WIN32

#include <SDL.h>
#include <SDL_syswm.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>

#ifndef IME_CMODE_NATIVE
#define IME_CMODE_NATIVE 0x00000001
#endif
#ifndef IME_CMODE_ALPHANUMERIC
#define IME_CMODE_ALPHANUMERIC 0x00000000
#endif

namespace jk {

// ---------------------------------------------------------------------------
// Native handle wrapper
// ---------------------------------------------------------------------------
struct PlatformWindow {
    HWND hwnd = nullptr;
};

static HWND GetHwnd(SDL_Window* window) {
    if (!window) return nullptr;
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo) ||
        wmInfo.subsystem != SDL_SYSWM_WINDOWS) {
        return nullptr;
    }
    return wmInfo.info.win.window;
}

static HMONITOR GetMonitor(HWND hwnd) {
    if (!hwnd) return nullptr;
    return MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
}

// ---------------------------------------------------------------------------
// Process DPI awareness
// ---------------------------------------------------------------------------
void JKPlatform::InitializeProcessDpiAwareness() {
    // Try PMv2 first. The API pointer may be absent on older Windows.
    using SetProcessDpiAwarenessContextFn = BOOL (WINAPI*)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto fn = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (fn) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4
            if (fn(reinterpret_cast<DPI_AWARENESS_CONTEXT>(
                    reinterpret_cast<void*>(
                        static_cast<intptr_t>(-4))))) {
                return;
            }
        }
    }
    // Fallback to system DPI awareness.
    SetProcessDPIAware();
}

// ---------------------------------------------------------------------------
// Display enumeration
// ---------------------------------------------------------------------------
int JKPlatform::GetDisplayCount() {
    return SDL_GetNumVideoDisplays();
}

bool JKPlatform::GetDisplayInfo(int displayIndex, DisplayInfo& out) {
    if (displayIndex < 0 || displayIndex >= SDL_GetNumVideoDisplays()) {
        return false;
    }
    out.index = displayIndex;
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(displayIndex, &bounds) != 0) return false;
    out.x = bounds.x;
    out.y = bounds.y;
    out.width = bounds.w;
    out.height = bounds.h;

    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(displayIndex, &usable) == 0) {
        out.workX = usable.x;
        out.workY = usable.y;
        out.workWidth = usable.w;
        out.workHeight = usable.h;
    } else {
        out.workX = bounds.x;
        out.workY = bounds.y;
        out.workWidth = bounds.w;
        out.workHeight = bounds.h;
    }

    float ddpi = 0.0f;
    SDL_GetDisplayDPI(displayIndex, &ddpi, nullptr, nullptr);
    if (ddpi <= 0.0f) ddpi = 96.0f;
    out.dpi = ddpi;
    out.scale = ddpi / 96.0f;
    out.primary = (displayIndex == 0);
    return true;
}

int JKPlatform::GetWindowDisplayIndex(SDL_Window* window) {
    if (!window) return 0;
    int idx = SDL_GetWindowDisplayIndex(window);
    return (idx >= 0) ? idx : 0;
}

// ---------------------------------------------------------------------------
// Native window / frame metrics
// ---------------------------------------------------------------------------
PlatformWindow* JKPlatform::GetNativeWindow(SDL_Window* window) {
    // We return a transient wrapper; callers should not store it.
    static thread_local PlatformWindow wrapper;
    wrapper.hwnd = GetHwnd(window);
    return wrapper.hwnd ? &wrapper : nullptr;
}

bool JKPlatform::GetWindowFrameMetrics(SDL_Window* window, FrameMetrics& out) {
    HWND hwnd = GetHwnd(window);
    if (!hwnd) return false;

    HMONITOR monitor = GetMonitor(hwnd);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    RECT frame{};
    if (!monitor || !GetMonitorInfoW(monitor, &mi) || !GetWindowRect(hwnd, &frame)) {
        return false;
    }

    out.frameX = frame.left;
    out.frameY = frame.top;
    out.frameW = frame.right - frame.left;
    out.frameH = frame.bottom - frame.top;

    RECT client{};
    GetClientRect(hwnd, &client);
    POINT clientOrigin{0, 0};
    ClientToScreen(hwnd, &clientOrigin);
    out.clientX = clientOrigin.x;
    out.clientY = clientOrigin.y;
    out.clientW = client.right - client.left;
    out.clientH = client.bottom - client.top;

    out.borderLeft = out.clientX - out.frameX;
    out.borderTop = out.clientY - out.frameY;

    UINT dpi = GetDpiForWindow(hwnd);
    out.dpiScale = (dpi > 0) ? (dpi / 96.0f) : 1.0f;
    return true;
}

bool JKPlatform::ComputeCenteredPlacement(SDL_Window* window,
                                          int clientPtW, int clientPtH,
                                          Placement& out) {
    if (!window) return false;

    FrameMetrics fm{};
    if (!GetWindowFrameMetrics(window, fm)) return false;

    HMONITOR monitor = GetMonitor(GetHwnd(window));
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!monitor || !GetMonitorInfoW(monitor, &mi)) return false;

    const int workX = mi.rcWork.left;
    const int workY = mi.rcWork.top;
    const int workW = mi.rcWork.right - mi.rcWork.left;
    const int workH = mi.rcWork.bottom - mi.rcWork.top;

    // If the frame is larger than the work area, clamp to the work-area origin.
    const int outerPxX = workX + ((workW > fm.frameW) ? (workW - fm.frameW) / 2 : 0);
    const int outerPxY = workY + ((workH > fm.frameH) ? (workH - fm.frameH) / 2 : 0);

    out.targetClientPxX = outerPxX + fm.borderLeft;
    out.targetClientPxY = outerPxY + fm.borderTop;
    out.clientPtX = static_cast<int>(out.targetClientPxX / fm.dpiScale + 0.5f);
    out.clientPtY = static_cast<int>(out.targetClientPxY / fm.dpiScale + 0.5f);
    (void)clientPtW;
    (void)clientPtH;
    return true;
}

bool JKPlatform::IsPerMonitorDpiAware(SDL_Window* window) {
    HWND hwnd = GetHwnd(window);
    if (!hwnd) return false;
    DPI_AWARENESS_CONTEXT ctx = GetWindowDpiAwarenessContext(hwnd);
    // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4
    auto pmv2 = reinterpret_cast<DPI_AWARENESS_CONTEXT>(
        reinterpret_cast<void*>(static_cast<intptr_t>(-4)));
    return AreDpiAwarenessContextsEqual(ctx, pmv2) != FALSE;
}

// ---------------------------------------------------------------------------
// Mouse input
// ---------------------------------------------------------------------------
bool JKPlatform::GetPhysicalMousePos(SDL_Window* window, int& outX, int& outY) {
    if (!window) return false;
    int gx = 0, gy = 0;
    SDL_GetGlobalMouseState(&gx, &gy);
    int wx = 0, wy = 0;
    SDL_GetWindowPosition(window, &wx, &wy);
    outX = gx - wx;
    outY = gy - wy;
    return true;
}

// ---------------------------------------------------------------------------
// Synthetic input
// ---------------------------------------------------------------------------
bool JKPlatform::SendSyntheticKey(uint32_t vkOrScancode, bool extended, bool keyUp) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(vkOrScancode & 0xFFFF);
    input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    if (extended) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0;

    return SendInput(1, &input, sizeof(input)) == 1;
}

bool JKPlatform::SendSyntheticChar(wchar_t ch) {
    INPUT input[2]{};
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wScan = static_cast<WORD>(ch);
    input[0].ki.dwFlags = KEYEVENTF_UNICODE;
    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wScan = static_cast<WORD>(ch);
    input[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    return SendInput(2, input, sizeof(input[0])) == 2;
}

// ---------------------------------------------------------------------------
// IME
// ---------------------------------------------------------------------------
static HWND GetImeHwnd(SDL_Window* window) {
    if (!window) return nullptr;
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo) ||
        wmInfo.subsystem != SDL_SYSWM_WINDOWS) {
        return nullptr;
    }
    return wmInfo.info.win.window;
}

JKPlatform::ImeMode JKPlatform::GetCurrentConversionMode(SDL_Window* window) {
    HWND hwnd = GetImeHwnd(window);
    if (!hwnd) return ImeMode::Unknown;

    HIMC himc = ImmGetContext(hwnd);
    if (!himc) return ImeMode::Ascii;

    ImeMode mode = ImeMode::Ascii;
    DWORD conversion = 0, sentence = 0;
    if (ImmGetOpenStatus(himc) && ImmGetConversionStatus(himc, &conversion, &sentence)) {
        if (conversion & IME_CMODE_NATIVE) {
            mode = ImeMode::Hangul;
        }
    }
    ImmReleaseContext(hwnd, himc);
    return mode;
}

void JKPlatform::SetConversionMode(SDL_Window* window, ImeMode mode) {
    HWND hwnd = GetImeHwnd(window);
    if (!hwnd) return;

    HIMC himc = ImmGetContext(hwnd);
    if (!himc) return;

    DWORD conversion = 0, sentence = 0;
    if (ImmGetConversionStatus(himc, &conversion, &sentence)) {
        if (mode == ImeMode::Hangul) {
            conversion |= IME_CMODE_NATIVE;
            conversion &= ~IME_CMODE_ALPHANUMERIC;
        } else if (mode == ImeMode::Ascii) {
            conversion &= ~IME_CMODE_NATIVE;
            conversion |= IME_CMODE_ALPHANUMERIC;
        }
        ImmSetConversionStatus(himc, conversion, sentence);
    }
    ImmReleaseContext(hwnd, himc);
}

void JKPlatform::CompleteComposition(SDL_Window* window) {
    HWND hwnd = GetImeHwnd(window);
    if (!hwnd) return;

    HIMC himc = ImmGetContext(hwnd);
    if (himc) {
        ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
        ImmReleaseContext(hwnd, himc);
    }
}

} // namespace jk

#else // Non-Windows stub implementation

#include <SDL.h>

namespace jk {

void JKPlatform::InitializeProcessDpiAwareness() {}

int JKPlatform::GetDisplayCount() {
    return SDL_GetNumVideoDisplays();
}

bool JKPlatform::GetDisplayInfo(int displayIndex, DisplayInfo& out) {
    if (displayIndex < 0 || displayIndex >= SDL_GetNumVideoDisplays()) return false;
    out.index = displayIndex;
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(displayIndex, &bounds) != 0) return false;
    out.x = bounds.x; out.y = bounds.y; out.width = bounds.w; out.height = bounds.h;
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(displayIndex, &usable) == 0) {
        out.workX = usable.x; out.workY = usable.y;
        out.workWidth = usable.w; out.workHeight = usable.h;
    } else {
        out.workX = bounds.x; out.workY = bounds.y;
        out.workWidth = bounds.w; out.workHeight = bounds.h;
    }
    out.dpi = 96.0f; out.scale = 1.0f; out.primary = (displayIndex == 0);
    return true;
}

int JKPlatform::GetWindowDisplayIndex(SDL_Window* window) {
    if (!window) return 0;
    int idx = SDL_GetWindowDisplayIndex(window);
    return (idx >= 0) ? idx : 0;
}

PlatformWindow* JKPlatform::GetNativeWindow(SDL_Window*) { return nullptr; }
bool JKPlatform::GetWindowFrameMetrics(SDL_Window*, FrameMetrics&) { return false; }
bool JKPlatform::ComputeCenteredPlacement(SDL_Window*, int, int, Placement&) { return false; }
bool JKPlatform::IsPerMonitorDpiAware(SDL_Window*) { return false; }

bool JKPlatform::GetPhysicalMousePos(SDL_Window* window, int& outX, int& outY) {
    if (!window) return false;
    int gx = 0, gy = 0;
    SDL_GetGlobalMouseState(&gx, &gy);
    int wx = 0, wy = 0;
    SDL_GetWindowPosition(window, &wx, &wy);
    outX = gx - wx;
    outY = gy - wy;
    return true;
}

bool JKPlatform::SendSyntheticKey(uint32_t, bool, bool) { return false; }
bool JKPlatform::SendSyntheticChar(wchar_t) { return false; }

JKPlatform::ImeMode JKPlatform::GetCurrentConversionMode(SDL_Window*) {
    return ImeMode::Unknown;
}
void JKPlatform::SetConversionMode(SDL_Window*, ImeMode) {}
void JKPlatform::CompleteComposition(SDL_Window*) {}

} // namespace jk

#endif // _WIN32
