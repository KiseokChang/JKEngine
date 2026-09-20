#include <JKImeHook.h>

#include <SDL.h>
#include <SDL_syswm.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>

// 한/영 전환키 저수준 훅 (docs/61 §16). IME가 VK_HANGUL을 삼켜 앱에 키
// 이벤트가 도달하지 않으므로 WH_KEYBOARD_LL로 IME 이전의 원시 키를 관측한다.
// 훅 콜백은 설치 스레드의 메시지 펌프 중에 실행된다 — 서버 Run 루프의
// SDL_PollEvent가 Windows 메시지를 계속 펌프하므로 거기서 배출된다.

static HHOOK  g_hook        = nullptr;
static Uint32 g_toggleEvent = 0;
static HWND   g_watchHwnd   = nullptr;

static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && wParam == VK_HANGUL) {
        const KBDLLHOOKSTRUCT* info =
            reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        // 키다운만(LLKHF_UP = 키업 플래그) — 토글은 다운 1회당 1번.
        // watchWindow가 전경일 때만 — 유저가 다른 앱에서 누른 한/영 오탐 방지.
        if (info && !(info->flags & LLKHF_UP) &&
            (!g_watchHwnd || GetForegroundWindow() == g_watchHwnd)) {
            SDL_Event ev{};
            ev.type = g_toggleEvent;
            SDL_PushEvent(&ev);
        }
    }
    return CallNextHookEx(g_hook, code, wParam, lParam);
}

bool JkInstallImeKeyHook(SDL_Window* watchWindow) {
    if (g_hook) return true;
    if (g_toggleEvent == 0) {
        g_toggleEvent = SDL_RegisterEvents(1);
        if (g_toggleEvent == 0xFFFFFFFF) return false;
    }
    if (watchWindow) {
        SDL_SysWMinfo wm{};
        SDL_VERSION(&wm.version);
        if (SDL_GetWindowWMInfo(watchWindow, &wm) &&
            wm.subsystem == SDL_SYSWM_WINDOWS) {
            g_watchHwnd = wm.info.win.window;
        }
    }
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                              GetModuleHandle(nullptr), 0);
    return g_hook != nullptr;
}

void JkUninstallImeKeyHook() {
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
}

uint32_t JkImeToggleEventType() {
    return g_toggleEvent;
}