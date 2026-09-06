// Browser client app (docs/23 §11.9): CEF C API on the client's main thread.
// Ported from tools/cefosr/osr_main.c (the standalone OSR demo) with the SDL
// event loop replaced by JKEvent forwarding from PreProcessMessage. The CEF
// integrated message loop runs on this thread, so every CEF callback
// (on_after_created, on_paint) lands here — no locks anywhere.
#include <apps/ClientBrowserApp.h>

#include <imgui_impl_jkwindow.h>
#include <imgui.h>
#include <JKWindow.h>
#include <SDL.h>

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/capi/cef_render_handler_capi.h"
#include "include/capi/cef_display_handler_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/cef_api_hash.h"

namespace jk {

namespace {

// ---------------------------------------------------------------------------
// CEF core state — single browser, browser UI thread == this app's thread.
// ---------------------------------------------------------------------------
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_tex = nullptr;
int g_texW = 0, g_texH = 0;

cef_browser_t* g_browser = nullptr; // set in on_after_created
int g_browserGone = 0;

int g_viewW = 960, g_viewH = 576;   // OSR view == page area (below URL bar)
int g_viewPageY = 80;               // page origin in client coords (= pageY_)
bool g_insideView = false;          // last MouseMove was inside the view
uint32_t g_mouseFlags = 0;          // held-button flags for move/wheel events
std::string g_exeDirSlash;          // exe dir with trailing '/', set in InitCef

// ---------------------------------------------------------------------------
// Ref-counting for plain-C handler objects. Every object is
// { cef_xxx_handler_t h; int refs; } — base is the first member of h, and
// base.size is set to sizeof(cef_xxx_handler_t), so |refs| sits at
// (char*)self + self->size. Same scheme as the osr demo.
// ---------------------------------------------------------------------------
void CEF_CALLBACK base_add_ref(cef_base_ref_counted_t* self) {
    ++*(int*)((char*)self + self->size);
}
int CEF_CALLBACK base_release(cef_base_ref_counted_t* self) {
    int* refs = (int*)((char*)self + self->size);
    if (--*refs == 0) { free(self); return 1; }
    return 0;
}
int CEF_CALLBACK base_has_one_ref(cef_base_ref_counted_t* self) {
    return *(int*)((char*)self + self->size) == 1;
}
int CEF_CALLBACK base_has_at_least_one_ref(cef_base_ref_counted_t* self) {
    return *(int*)((char*)self + self->size) >= 1;
}

// Allocates { cef_xxx_handler_t h; int refs; }: zeroed, base.size wired, ref
// callbacks set, caller's first reference taken. Ref handover rule: CEF's
// CToCpp wrap consumes one reference from every struct handed to it, so
// add_ref once more before each handover; refs=1 after ALLOC is the session
// reference (never released — lives until process exit).
#define ALLOC_HANDLER(obj, handler_type)                                      \
    do {                                                                      \
        (obj) = (__typeof__(obj))calloc(1, sizeof *obj);                      \
        (obj)->h.base.size = sizeof(handler_type);                            \
        (obj)->h.base.add_ref = base_add_ref;                                 \
        (obj)->h.base.release = base_release;                                 \
        (obj)->h.base.has_one_ref = base_has_one_ref;                         \
        (obj)->h.base.has_at_least_one_ref = base_has_at_least_one_ref;       \
        base_add_ref(&(obj)->h.base);                                         \
    } while (0)

// cef_string_utf8_to_utf16 frees any pre-existing content via out->dtor
// before writing — the output string MUST start zeroed or it calls garbage
// (the intermittent 0xC0000005 root-caused in the osr demo).
void MakeString(const char* utf8, cef_string_t* out) {
    memset(out, 0, sizeof *out);
    cef_string_utf8_to_utf16(utf8, strlen(utf8), out);
}

// ---------------------------------------------------------------------------
// Handlers: life span / render / display / client / app
// ---------------------------------------------------------------------------
typedef struct { cef_life_span_handler_t h; int refs; } LifeSpanObj;
typedef struct { cef_render_handler_t h; int refs; } RenderObj;
typedef struct { cef_display_handler_t h; int refs; } DisplayObj;
typedef struct {
    cef_client_t h;
    int refs;
    LifeSpanObj* lifeSpan;
    RenderObj* render;
    DisplayObj* display;
} ClientObj;
typedef struct { cef_app_t h; int refs; } AppObj;

void CEF_CALLBACK ls_on_after_created(struct _cef_life_span_handler_t* /*self*/,
                                      struct _cef_browser_t* browser) {
    g_browser = browser;
    cef_browser_host_t* host = browser->get_host(browser);
    host->set_focus(host, 1);
}

void CEF_CALLBACK ls_on_before_close(struct _cef_life_span_handler_t* /*self*/,
                                     struct _cef_browser_t* /*browser*/) {
    g_browser = NULL;
    g_browserGone = 1;
}

void CEF_CALLBACK rh_get_view_rect(struct _cef_render_handler_t* /*self*/,
                                   struct _cef_browser_t* /*browser*/,
                                   cef_rect_t* rect) {
    rect->x = 0;
    rect->y = 0;
    rect->width = g_viewW;
    rect->height = g_viewH;
}

// on_paint runs on the browser UI thread — the thread pumping
// cef_do_message_loop_work() (RenderOverlay), so touching SDL here is safe.
// |buffer| is BGRA, top-left origin — byte layout == ARGB8888.
void CEF_CALLBACK rh_on_paint(struct _cef_render_handler_t* /*self*/,
                              struct _cef_browser_t* /*browser*/,
                              cef_paint_element_type_t type,
                              size_t dirtyRectsCount,
                              cef_rect_t const* dirtyRects,
                              const void* buffer,
                              int width, int height) {
    (void)dirtyRectsCount;
    (void)dirtyRects;
    if (type != PET_VIEW || !g_renderer || !buffer || width <= 0 || height <= 0)
        return;
    if (!g_tex || width != g_texW || height != g_texH) {
        if (g_tex) SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, width, height);
        g_texW = width;
        g_texH = height;
        if (!g_tex) return;
        SDL_SetTextureScaleMode(g_tex, SDL_ScaleModeLinear);
    }
    void* pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(g_tex, NULL, &pixels, &pitch) == 0) {
        const size_t stride = (size_t)width * 4;
        const uint8_t* src = (const uint8_t*)buffer;
        uint8_t* dst = (uint8_t*)pixels;
        for (int y = 0; y < height; ++y)
            memcpy(dst + (size_t)y * pitch, src + (size_t)y * stride, stride);
        SDL_UnlockTexture(g_tex);
    }
}

int CEF_CALLBACK dh_on_console_message(struct _cef_display_handler_t* /*self*/,
                                       struct _cef_browser_t* /*browser*/,
                                       cef_log_severity_t /*level*/,
                                       const cef_string_t* message,
                                       const cef_string_t* /*source*/,
                                       int line) {
    char msg[512];
    size_t n = message && message->str
                   ? (message->length < 511 ? message->length : 511) : 0;
    for (size_t i = 0; i < n; ++i) msg[i] = (char)message->str[i];
    msg[n] = 0;
    fprintf(stderr, "[browser console] %s (line %d)\n", msg, line);
    return 0;
}

struct _cef_life_span_handler_t* CEF_CALLBACK cl_get_life_span(
    struct _cef_client_t* self) {
    ClientObj* c = (ClientObj*)self;
    c->lifeSpan->h.base.add_ref(&c->lifeSpan->h.base);
    return &c->lifeSpan->h;
}
struct _cef_render_handler_t* CEF_CALLBACK cl_get_render(
    struct _cef_client_t* self) {
    ClientObj* c = (ClientObj*)self;
    c->render->h.base.add_ref(&c->render->h.base);
    return &c->render->h;
}
struct _cef_display_handler_t* CEF_CALLBACK cl_get_display(
    struct _cef_client_t* self) {
    ClientObj* c = (ClientObj*)self;
    c->display->h.base.add_ref(&c->display->h.base);
    return &c->display->h;
}

// ---------------------------------------------------------------------------
// Input forwarding — JKEvent coords are client-relative; the CEF view is the
// page area starting at g_viewPageY, so y is translated before every send.
// ---------------------------------------------------------------------------
uint32_t SdlModsToCef(int kmod) {
    uint32_t m = 0;
    if (kmod & KMOD_LSHIFT) m |= EVENTFLAG_SHIFT_DOWN;
    if (kmod & KMOD_RSHIFT) m |= EVENTFLAG_SHIFT_DOWN | EVENTFLAG_IS_RIGHT;
    if (kmod & KMOD_LCTRL) m |= EVENTFLAG_CONTROL_DOWN;
    if (kmod & KMOD_RCTRL) m |= EVENTFLAG_CONTROL_DOWN | EVENTFLAG_IS_RIGHT;
    if (kmod & KMOD_LALT) m |= EVENTFLAG_ALT_DOWN;
    if (kmod & KMOD_RALT) m |= EVENTFLAG_ALT_DOWN | EVENTFLAG_IS_RIGHT;
    return m;
}

// SDL_Keycode -> Windows VK (demo scope: printable ASCII + navigation keys).
int SdlKeyToVk(int k) {
    if (k >= 'a' && k <= 'z') return k - 'a' + 0x41;
    if (k >= '0' && k <= '9') return k - '0' + 0x30;
    switch (k) {
        case SDLK_SPACE: return 0x20;
        case SDLK_RETURN: return 0x0D;
        case SDLK_BACKSPACE: return 0x08;
        case SDLK_TAB: return 0x09;
        case SDLK_ESCAPE: return 0x1B;
        case SDLK_LEFT: return 0x25;
        case SDLK_UP: return 0x26;
        case SDLK_RIGHT: return 0x27;
        case SDLK_DOWN: return 0x28;
        case SDLK_DELETE: return 0x2E;
        case SDLK_HOME: return 0x24;
        case SDLK_END: return 0x23;
        case SDLK_PAGEUP: return 0x21;
        case SDLK_PAGEDOWN: return 0x22;
        case SDLK_F1: return 0x70;
        case SDLK_F5: return 0x74;
        case SDLK_F12: return 0x7B;
        default: return 0;
    }
}

void SendMouseMotion(int x, int y) {
    if (!g_browser) return;
    const int vx = x;
    const int vy = y - g_viewPageY;
    const bool inside = vx >= 0 && vx < g_viewW && vy >= 0 && vy < g_viewH;
    if (!inside && !g_insideView) return; // was and stays outside
    cef_browser_host_t* host = g_browser->get_host(g_browser);
    cef_mouse_event_t me;
    memset(&me, 0, sizeof me);
    me.x = vx;
    me.y = vy;
    me.modifiers = SdlModsToCef(SDL_GetModState()) | g_mouseFlags;
    host->send_mouse_move_event(host, &me, inside ? 0 : 1);
    g_insideView = inside;
}

// |btn|: JKEvent detail — 1=left, 2=middle, 3=right (SDL numbering).
void SendMouseButton(int x, int y, int btn, bool up) {
    if (!g_browser) return;
    const int vy = y - g_viewPageY;
    if (vy < 0 || vy >= g_viewH || x < 0 || x >= g_viewW) return;
    cef_browser_host_t* host = g_browser->get_host(g_browser);
    cef_mouse_event_t me;
    memset(&me, 0, sizeof me);
    me.x = x;
    me.y = vy;
    cef_mouse_button_type_t mbt = MBT_LEFT;
    uint32_t flag = EVENTFLAG_LEFT_MOUSE_BUTTON;
    if (btn == 3) { mbt = MBT_RIGHT;  flag = EVENTFLAG_RIGHT_MOUSE_BUTTON; }
    else if (btn == 2) { mbt = MBT_MIDDLE; flag = EVENTFLAG_MIDDLE_MOUSE_BUTTON; }
    if (up) g_mouseFlags &= ~flag;
    else    g_mouseFlags |= flag;
    me.modifiers = SdlModsToCef(SDL_GetModState()) | g_mouseFlags;
    host->send_mouse_click_event(host, &me, mbt, up ? 1 : 0, 1);
}

void SendMouseWheel(int dx, int dy, int x, int y) {
    if (!g_browser) return;
    // The wheel arrives with the last hover coords; clamp into the page view
    // rather than dropping — stale (0,0) hover should still scroll.
    int vx = x;
    int vy = y - g_viewPageY;
    if (vx < 0) vx = 0;
    if (vx >= g_viewW) vx = g_viewW - 1;
    if (vy < 0) vy = 0;
    if (vy >= g_viewH) vy = g_viewH - 1;
    cef_browser_host_t* host = g_browser->get_host(g_browser);
    cef_mouse_event_t me;
    memset(&me, 0, sizeof me);
    me.x = vx;
    me.y = vy;
    me.modifiers = SdlModsToCef(SDL_GetModState()) | g_mouseFlags;
    // SDL reports notches (±1); Chromium wants WHEEL_DELTA (120) per notch.
    host->send_mouse_wheel_event(host, &me, dx * 120, dy * 120);
}

void SendKey(int sym, int mods, bool down) {
    if (!g_browser) return;
    const int vk = SdlKeyToVk(sym);
    if (vk == 0 && (sym < ' ' || sym > '~')) return; // unmapped
    cef_browser_host_t* host = g_browser->get_host(g_browser);
    cef_key_event_t ke;
    memset(&ke, 0, sizeof ke);
    ke.size = sizeof ke;
    ke.type = down ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
    ke.windows_key_code = vk;
    ke.modifiers = SdlModsToCef(mods);
    if (vk >= 0x20 && vk < 0x7F)
        ke.character = (char16_t)vk;
    ke.unmodified_character = ke.character;
    host->send_key_event(host, &ke);
}

// Generated characters (JKEvent::Char text is UTF-8). ASCII path only —
// same limitation as the osr demo; Korean input is a later step.
void SendCharText(const char* text) {
    if (!g_browser) return;
    cef_browser_host_t* host = g_browser->get_host(g_browser);
    for (; *text; ++text) {
        if ((unsigned char)*text < 0x80 && *text != '\r' && *text != '\n') {
            cef_key_event_t ke;
            memset(&ke, 0, sizeof ke);
            ke.size = sizeof ke;
            ke.type = KEYEVENT_CHAR;
            ke.character = (char16_t)(unsigned char)*text;
            ke.windows_key_code = (int)(unsigned char)*text;
            host->send_key_event(host, &ke);
        }
    }
}

void ShutdownBrowser() {
    if (!g_browser) return;
    cef_browser_host_t* host = g_browser->get_host(g_browser);
    host->close_browser(host, 1);
    // Keep pumping so the close pipeline (on_before_close) actually runs.
    for (int i = 0; i < 50 && !g_browserGone; ++i) {
        cef_do_message_loop_work();
        Sleep(10);
    }
}

// Paints the dark clear color so the surface never flashes white behind the
// ImGui panels (docs/23 §11 app idiom).
class BrowserRoot : public JKWindow {
public:
    explicit BrowserRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        dc.SetColor(24, 24, 28, 255);
        dc.FillRect(GetClientRect());
    }
};

} // namespace

ClientBrowserApp::~ClientBrowserApp() = default;

void ClientBrowserApp::OnInit() {
    auto main = std::make_unique<BrowserRoot>("Browser");
    main->SetWindowRect(JKRect{ 0, 0, 960, 640 });
    main->SetAttrFlags(WA_CHROMELESS); // server close button only (docs/23 §9)
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence (CEF pump + repaint clock)

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    lastFrame_ = std::chrono::steady_clock::now();
}

void ClientBrowserApp::OnClose() {
    if (cefReady_)
        ShutdownBrowser(); // close + pump before any ImGui teardown
    if (g_tex) {
        SDL_DestroyTexture(g_tex);
        g_tex = nullptr;
    }
    g_texW = g_texH = 0;
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
    if (cefReady_) {
        cef_shutdown();
        cefReady_ = false;
    }
}

bool ClientBrowserApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer)
        frameDirty_ = true; // frame clock (docs/23 §11.5 lesson 5)

    if (!cefReady_)
        return true;

    ImGuiIO& io = ImGui::GetIO();
    switch (ev.type) {
        case JKEventType::MouseMove:
            lastMouseX_ = ev.x;
            lastMouseY_ = ev.y;
            // Ungated on purpose: motion is hover-only, and the leave event
            // for "pointer entered the URL bar row" must still reach CEF.
            SendMouseMotion(ev.x, ev.y);
            break;
        case JKEventType::MouseDown:
            // Geometric gate only. WantCaptureMouse is wrong here: imgui sets
            // it true while ANY button is down (mouse_any_down), so gating
            // UPs by it drops the UP, leaves CEF stuck pressed, and freezes
            // our g_mouseFlags. Pair DOWN/UP by ourselves instead.
            if (ev.y >= pageY_) {
                SendMouseButton(ev.x, ev.y, ev.detail, false);
                cefMouseDown_ = true;
            }
            break;
        case JKEventType::MouseUp:
            if (ev.y >= pageY_ || cefMouseDown_) {
                SendMouseButton(ev.x, ev.y, ev.detail, true);
                cefMouseDown_ = false;
            }
            break;
        case JKEventType::MouseWheel:
            // Same geometric reasoning; coords come from the last MouseMove.
            if (lastMouseY_ >= pageY_)
                SendMouseWheel(ev.dx, ev.dy, lastMouseX_, lastMouseY_);
            break;
        case JKEventType::KeyDown:
        case JKEventType::KeyUp:
            if (!io.WantCaptureKeyboard)
                SendKey(ev.keyCode, ev.option, ev.type == JKEventType::KeyDown);
            break;
        case JKEventType::Char:
            if (!io.WantTextInput)
                SendCharText(ev.text);
            break;
        default:
            break;
    }
    return true;
}

void ClientBrowserApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientBrowserApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }
    renderer_ = renderer;

    // Lazy CEF init: first frame, when surface + pipe are already live.
    if (!cefReady_ && initError_.empty())
        InitCef();

    const int pw = w;
    int ph = h - pageY_;
    if (ph < 1) ph = 1;
    if (pw != pageW_ || ph != pageH_) {
        pageW_ = pw;
        pageH_ = ph;
        viewSizeDirty_ = true;
    }
    if (cefReady_ && viewSizeDirty_) {
        g_viewW = pageW_;
        g_viewH = pageH_;
        if (g_browser) {
            cef_browser_host_t* host = g_browser->get_host(g_browser);
            host->was_resized(host);
        }
        viewSizeDirty_ = false;
    }

    if (cefReady_) {
        g_renderer = renderer;
        cef_do_message_loop_work(); // may fire on_paint -> g_tex update
    }

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(
        now - lastFrame_).count();
    lastFrame_ = now;

    ImGui_ImplJKWindow_NewFrame(dt, w, h);
    ImGui::NewFrame();
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientBrowserApp::InitCef() {
    // CEF 133+ API versioning: the platform API hash must be checked before
    // any other CEF call, in every process (cef_execute_process included).
    const char* hash = cef_api_hash(CEF_API_VERSION, 0);
    if (!hash || strcmp(hash, CEF_API_HASH_PLATFORM) != 0) {
        initError_ = std::string("CEF api hash mismatch (dll=") +
                     (hash ? hash : "(null)") + ")";
        return;
    }

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string dir = exePath;
    const size_t slash = dir.find_last_of('\\');
    if (slash != std::string::npos)
        dir = dir.substr(0, slash);
    g_exeDirSlash = dir;
    for (char& c : g_exeDirSlash)
        if (c == '\\') c = '/';
    g_exeDirSlash += '/';

    cef_main_args_t mainArgs;
    memset(&mainArgs, 0, sizeof mainArgs);
    mainArgs.instance = GetModuleHandleW(NULL);

    // Subprocess re-entry — should never trigger here (--client carries no
    // --type=); subprocesses run cefosr.exe instead.
    AppObj* appObj;
    ALLOC_HANDLER(appObj, cef_app_t);
    appObj->h.base.add_ref(&appObj->h.base);
    const int sub = cef_execute_process(&mainArgs, &appObj->h, NULL);
    if (sub >= 0) {
        initError_ = "unexpected CEF subprocess re-entry";
        return;
    }

    // Subprocess path must exist — a missing cefosr.exe fails cryptically
    // (subprocess launches crash the browser process seconds later).
    const std::string subExe = dir + "\\cefosr.exe";
    if (GetFileAttributesA(subExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        initError_ = "cefosr.exe not found next to jkdesktop.exe "
                     "(see third_party/cef/README.md)";
        return;
    }

    cef_settings_t settings;
    memset(&settings, 0, sizeof settings);
    settings.size = sizeof settings;
    settings.windowless_rendering_enabled = 1;
    settings.multi_threaded_message_loop = 0;
    // Sandbox requires a manifest-embedded policy set that a MinGW exe does
    // not have; subprocess launches crash without this (osr demo lesson).
    settings.no_sandbox = 1;
    settings.log_severity = LOGSEVERITY_WARNING;
    MakeString(subExe.c_str(), &settings.browser_subprocess_path);
    MakeString((dir + "\\cef_browser.log").c_str(), &settings.log_file);
    // NOTE: settings strings are intentionally not freed — CEF may retain
    // the pointers for its lifetime (same as the osr demo).

    appObj->h.base.add_ref(&appObj->h.base);
    if (!cef_initialize(&mainArgs, &settings, &appObj->h, NULL)) {
        initError_ = "cef_initialize failed";
        return;
    }

    ClientObj* clientObj;
    ALLOC_HANDLER(clientObj, cef_client_t);
    clientObj->h.get_life_span_handler = cl_get_life_span;
    clientObj->h.get_render_handler = cl_get_render;
    clientObj->h.get_display_handler = cl_get_display;

    LifeSpanObj* lifeSpan;
    ALLOC_HANDLER(lifeSpan, cef_life_span_handler_t);
    lifeSpan->h.on_after_created = ls_on_after_created;
    lifeSpan->h.on_before_close = ls_on_before_close;
    clientObj->lifeSpan = lifeSpan;

    RenderObj* render;
    ALLOC_HANDLER(render, cef_render_handler_t);
    render->h.get_view_rect = rh_get_view_rect;
    render->h.on_paint = rh_on_paint;
    clientObj->render = render;

    DisplayObj* display;
    ALLOC_HANDLER(display, cef_display_handler_t);
    display->h.on_console_message = dh_on_console_message;
    clientObj->display = display;

    char url[MAX_PATH * 2];
    snprintf(url, sizeof url, "file:///%sassets/browser_home.html",
             g_exeDirSlash.c_str());
    cef_string_t urlStr;
    MakeString(url, &urlStr);

    cef_window_info_t winInfo;
    memset(&winInfo, 0, sizeof winInfo);
    winInfo.size = sizeof winInfo;
    winInfo.windowless_rendering_enabled = 1;

    cef_browser_settings_t bsettings;
    memset(&bsettings, 0, sizeof bsettings);
    bsettings.size = sizeof bsettings;
    bsettings.windowless_frame_rate = 60;

    g_viewW = pageW_;
    g_viewH = pageH_;
    g_viewPageY = pageY_; // keep CEF-space translation in sync with the UI row
    clientObj->h.base.add_ref(&clientObj->h.base);
    const int r = cef_browser_host_create_browser(&winInfo, &clientObj->h,
                                                  &urlStr, &bsettings, NULL, NULL);
    cef_string_utf16_clear(&urlStr); // create_browser copies
    cefReady_ = true;                // pump+shutdown path active from here
    if (r != 0) {
        initError_ = "cef_browser_host_create_browser failed";
        fprintf(stderr, "[browser] create_browser returned %d\n", r);
    }
}

void ClientBrowserApp::Navigate(const char* url) {
    if (!g_browser || !url || !url[0])
        return;
    char buf[2048];
    if (!strstr(url, "://"))
        snprintf(buf, sizeof buf, "https://%s", url); // bare host -> https
    else
        snprintf(buf, sizeof buf, "%s", url);
    cef_string_t us;
    MakeString(buf, &us);
    cef_frame_t* frame = g_browser->get_main_frame(g_browser);
    if (frame)
        frame->load_url(frame, &us);
    cef_string_utf16_clear(&us); // load_url copies; our scratch is done
}

void ClientBrowserApp::BuildUi(int w, int h) {
    // Page host: full-surface NoInputs window, so hovering the page keeps
    // io.WantCaptureMouse false and mouse/keys fall through to CEF.
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    const ImGuiWindowFlags pageFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("browser_page", nullptr, pageFlags);
    // Zero padding: the CEF texture must land at exactly (0, pageY) — the
    // default WindowPadding(8,8) drew it inset, hiding the top of the page
    // under the bar and leaving a black band at the bottom.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::SetCursorPos(ImVec2(0.0f, (float)pageY_));
    if (g_tex) {
        ImGui::Image((ImTextureID)g_tex, ImVec2((float)pageW_, (float)pageH_));
    } else if (!initError_.empty()) {
        ImGui::SetCursorPos(ImVec2(0.0f, (float)pageY_));
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                           initError_.c_str());
    } else {
        ImGui::SetCursorPos(ImVec2(0.0f, (float)pageY_));
        ImGui::TextUnformatted("starting Chromium...");
    }
    ImGui::PopStyleVar();
    ImGui::End();

    // URL bar row. The server reserves the top 24pt of every surface as
    // window chrome — clicks there never reach this app, so the row starts
    // at y=30 (docs/23 §11 app idiom).
    ImGui::SetNextWindowPos(ImVec2(0, 30));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)(pageY_ - 30)));
    const ImGuiWindowFlags barFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
    // 50pt row: pad so the ~25px widgets sit vertically centered.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 6.0f));
    ImGui::Begin("browser_bar", nullptr, barFlags);

    // Back / Forward — direct cef_browser_t calls, dimmed when unavailable.
    if (g_browser && g_browser->can_go_back(g_browser)) {
        if (ImGui::Button("<"))
            g_browser->go_back(g_browser);
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("<");
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (g_browser && g_browser->can_go_forward(g_browser)) {
        if (ImGui::Button(">"))
            g_browser->go_forward(g_browser);
    } else {
        ImGui::BeginDisabled();
        ImGui::Button(">");
        ImGui::EndDisabled();
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100);
    const bool go = ImGui::InputTextWithHint("##url", "https://  (Enter to load)",
                                             urlBuf_, sizeof(urlBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Go") || go)
        Navigate(urlBuf_);
    ImGui::SameLine();
    if (ImGui::Button("Home")) {
        char home[MAX_PATH * 2];
        snprintf(home, sizeof home, "file:///%sassets/browser_home.html",
                 g_exeDirSlash.c_str());
        snprintf(urlBuf_, sizeof(urlBuf_), "%s", home);
        Navigate(urlBuf_);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace jk