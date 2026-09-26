#include <JKImeHook.h>

#include <SDL.h>
#include <SDL_syswm.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>

// 한/영 전환키 + 한자키 저수준 훅 (docs/61 §16, docs/66 — O1 한자 변환).
// IME가 VK_HANGUL과 VK_HANJA(0x19)를 삼켜 앱에 키 이벤트가 도달하지 않으므로
// WH_KEYBOARD_LL로 IME 이전의 원시 키를 관측한다. 훅 콜백은 설치 스레드의
// 메시지 펌프 중에 실행된다 — 서버 Run 루프의 SDL_PollEvent가 Windows 메시지를
// 계속 펌프하므로 거기서 배출된다.

static HHOOK  g_hook        = nullptr;
static Uint32 g_toggleEvent = 0;
static Uint32 g_hanjaEvent  = 0;
static HWND   g_watchHwnd   = nullptr;
// 키 반복 가드: LL 훅에 키다운 반복이 그대로 도달한다 — 누르고 있으면 토글/
// 한자 이벤트가 연발한다. down에서만 발화하고 keyup에서 플래그를 푼다
// (VK_HANGUL·VK_HANJA 동시 봉합, docs/66 위험 4).
static bool g_hangulDown = false;
static bool g_hanjaDown  = false;

static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    // LL 훅의 wParam은 메시지 종류(WM_KEYDOWN 등)다 — 가상키는 vkCode에 있다.
    // 옛 코드가 wParam == VK_HANGUL(0x15)로 검사해 WM_KEYDOWN(0x100)과 절대
    // 일치하지 않았고, 훅은 영원히 발화하지 않았다(docs/61 §19).
    if (code >= 0) {
        const KBDLLHOOKSTRUCT* info =
            reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        const bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        const bool up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
        if ((down || up) && info &&
            (!g_watchHwnd || GetForegroundWindow() == g_watchHwnd)) {
            if (info->vkCode == VK_HANGUL) {
                // 토글은 다운 엣지 1회당 1번 — keyup에서 플래그 해제.
                if (down && !g_hangulDown && !(info->flags & LLKHF_UP)) {
                    g_hangulDown = true;
                    SDL_Event ev{};
                    ev.type = g_toggleEvent;
                    SDL_PushEvent(&ev);
                } else if (up) {
                    g_hangulDown = false;
                }
            } else if (info->vkCode == VK_HANJA) {
                // 한자키(docs/66): 다운 엣지에서 후보 진입 이벤트.
                if (down && !g_hanjaDown && !(info->flags & LLKHF_UP)) {
                    g_hanjaDown = true;
                    SDL_Event ev{};
                    ev.type = g_hanjaEvent;
                    SDL_PushEvent(&ev);
                } else if (up) {
                    g_hanjaDown = false;
                }
                // 원시 키 차단: 실물 한자키는 OS IME가 삼켜 SDL에 도달하지
                // 않는다(docs/61:313). 합성 입력(SendInput)만 삼킴을 우회해
                // 새어 들어오고, 그 원시 키다운이 팝업의 '그 밖 키=취소'
                // 규칙으로 방금 연 팝업을 닫아버린다(라이브 e2e 실측 —
                // 이 리그에선 스캔 0x1D로 번역돼 도착). 실물과 합성의 동작을
                // 일치시키기 위해 훅에서 down/up 통째로 삼킨다.
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hook, code, wParam, lParam);
}

bool JkInstallImeKeyHook(SDL_Window* watchWindow) {
    if (g_hook) return true;
    if (g_toggleEvent == 0 || g_hanjaEvent == 0) {
        // 토글+한자 두 이벤트를 연속 번호로 등록한다(순서 보장).
        if (g_toggleEvent == 0) {
            g_toggleEvent = SDL_RegisterEvents(1);
            if (g_toggleEvent == 0xFFFFFFFF) return false;
        }
        if (g_hanjaEvent == 0) {
            g_hanjaEvent = SDL_RegisterEvents(1);
            if (g_hanjaEvent == 0xFFFFFFFF) return false;
        }
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

uint32_t JkImeHanjaEventType() {
    return g_hanjaEvent;
}