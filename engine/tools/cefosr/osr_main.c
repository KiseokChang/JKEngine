// CEF off-screen-rendering (OSR) demo — single exe, browser + subprocess.
// Proves the CEF C API works from a MinGW (ucrt64 gcc) build: no
// libcef_dll_wrapper, GNU ld links libcef.dll directly (-l:libcef.dll).
//
// Usage: osr.exe [url]
//   url defaults to demo.html sitting next to the exe (file:// form).
// The exe must live in a directory containing the CEF runtime set
// (libcef.dll, chrome_elf.dll, icudtl.dat, v8_context_snapshot.bin,
// *.pak, locales/, libEGL/libGLESv2/d3dcompiler_47...).
//
// Flow:
//   1. cef_execute_process() — renderer/GPU subprocesses launch this same
//      exe (browser_subprocess_path = this exe) and never return from here.
//   2. SDL window + cef_initialize(windowless) +
//      cef_browser_host_create_browser.
//   3. Main loop: SDL events forwarded to the browser, ~10 ms pump of
//      cef_do_message_loop_work(), present on_paint BGRA via SDL texture.
//   4. On window close: force-close the browser, pump until the life-span
//      handler signals it is gone, then cef_shutdown().

#include <SDL.h>
#include <windows.h>
#include <uchar.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/capi/cef_render_handler_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/cef_api_hash.h"

static const int kViewW = 960;
static const int kViewH = 640;

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;
static int g_texW = 0, g_texH = 0;
static int g_textureDirty = 0;

static cef_browser_t* g_browser = NULL; // UI thread only (set in on_after_created)
static int g_browserGone = 0;

// ---------------------------------------------------------------------------
// Ref-counting for plain-C handler objects. Every object is
// { cef_xxx_handler_t h; int refs; } — base is the first member of h, and
// base.size is set to sizeof(cef_xxx_handler_t), so |refs| sits at
// (char*)self + self->size.
// ---------------------------------------------------------------------------
static void CEF_CALLBACK base_add_ref(cef_base_ref_counted_t* self) {
    ++*(int*)((char*)self + self->size);
}
static int CEF_CALLBACK base_release(cef_base_ref_counted_t* self) {
    int* refs = (int*)((char*)self + self->size);
    if (--*refs == 0) { free(self); return 1; }
    return 0;
}
static int CEF_CALLBACK base_has_one_ref(cef_base_ref_counted_t* self) {
    return *(int*)((char*)self + self->size) == 1;
}
static int CEF_CALLBACK base_has_at_least_one_ref(cef_base_ref_counted_t* self) {
    return *(int*)((char*)self + self->size) >= 1;
}

// Allocates a { cef_xxx_handler_t h; int refs; } object: zeroes it, sets
// base.size to the handler-struct size (this is also where refs lives),
// wires the ref callbacks and takes the caller's first reference. Wire the
// real callbacks and pass &obj->h to CEF afterwards.
#define ALLOC_HANDLER(obj, handler_type)                                      \
    do {                                                                      \
        (obj) = (typeof(obj))calloc(1, sizeof *obj);                          \
        (obj)->h.base.size = sizeof(handler_type);                            \
        (obj)->h.base.add_ref = base_add_ref;                                 \
        (obj)->h.base.release = base_release;                                 \
        (obj)->h.base.has_one_ref = base_has_one_ref;                         \
        (obj)->h.base.has_at_least_one_ref = base_has_at_least_one_ref;       \
        base_add_ref(&(obj)->h.base);                                         \
    } while (0)

// ---------------------------------------------------------------------------
// cef_life_span_handler_t
// ---------------------------------------------------------------------------
typedef struct {
    cef_life_span_handler_t h;
    int refs;
} LifeSpanObj;

static void CEF_CALLBACK ls_on_after_created(struct _cef_life_span_handler_t* self,
                                             struct _cef_browser_t* browser) {
    g_browser = browser;
    cef_browser_host_t* host = browser->get_host(browser);
    host->set_focus(host, 1);
}

static void CEF_CALLBACK ls_on_before_close(struct _cef_life_span_handler_t* self,
                                            struct _cef_browser_t* browser) {
    g_browser = NULL;
    g_browserGone = 1;
}

// ---------------------------------------------------------------------------
// cef_render_handler_t — view size + BGRA on_paint
// ---------------------------------------------------------------------------
typedef struct {
    cef_render_handler_t h;
    int refs;
} RenderObj;

static void CEF_CALLBACK rh_get_view_rect(struct _cef_render_handler_t* self,
                                          struct _cef_browser_t* browser,
                                          cef_rect_t* rect) {
    rect->x = 0;
    rect->y = 0;
    rect->width = kViewW;
    rect->height = kViewH;
}

// on_paint runs on the browser UI thread — the same thread that pumps
// cef_do_message_loop_work(), so touching SDL from here is safe. |buffer|
// is BGRA, top-left origin — byte layout equals SDL_PIXELFORMAT_ARGB8888.
static void CEF_CALLBACK rh_on_paint(struct _cef_render_handler_t* self,
                                     struct _cef_browser_t* browser,
                                     cef_paint_element_type_t type,
                                     size_t dirtyRectsCount,
                                     cef_rect_t const* dirtyRects,
                                     const void* buffer,
                                     int width, int height) {
    if (type != PET_VIEW || !g_renderer || !buffer || width <= 0 || height <= 0)
        return;
    if (!g_texture || width != g_texW || height != g_texH) {
        if (g_texture) SDL_DestroyTexture(g_texture);
        g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, width, height);
        g_texW = width;
        g_texH = height;
        if (!g_texture) return;
    }
    void* pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(g_texture, NULL, &pixels, &pitch) == 0) {
        const size_t stride = (size_t)width * 4;
        const uint8_t* src = (const uint8_t*)buffer;
        uint8_t* dst = (uint8_t*)pixels;
        int y;
        for (y = 0; y < height; ++y)
            memcpy(dst + (size_t)y * pitch, src + (size_t)y * stride, stride);
        SDL_UnlockTexture(g_texture);
        g_textureDirty = 1;
    }
}

// ---------------------------------------------------------------------------
// cef_display_handler_t — page console.log to stderr (debugging aid)
// ---------------------------------------------------------------------------
typedef struct {
    cef_display_handler_t h;
    int refs;
} DisplayObj;

static int CEF_CALLBACK dh_on_console_message(struct _cef_display_handler_t* self,
                                              struct _cef_browser_t* browser,
                                              cef_log_severity_t level,
                                              const cef_string_t* message,
                                              const cef_string_t* source,
                                              int line) {
    char msg[512];
    size_t n = message && message->str
                   ? (message->length < 511 ? message->length : 511) : 0;
    size_t i;
    for (i = 0; i < n; ++i) msg[i] = (char)message->str[i];
    msg[n] = 0;
    fprintf(stderr, "[console] %s (line %d)\n", msg, line);
    return 0;
}

// ---------------------------------------------------------------------------
// cef_client_t — returns our three sub-handlers
// ---------------------------------------------------------------------------
typedef struct {
    cef_client_t h;
    int refs;
    LifeSpanObj* lifeSpan;
    RenderObj* render;
    DisplayObj* display;
} ClientObj;

static struct _cef_life_span_handler_t* CEF_CALLBACK cl_get_life_span(
    struct _cef_client_t* self) {
    ClientObj* c = (ClientObj*)self;
    c->lifeSpan->h.base.add_ref(&c->lifeSpan->h.base);
    return &c->lifeSpan->h;
}
static struct _cef_render_handler_t* CEF_CALLBACK cl_get_render(
    struct _cef_client_t* self) {
    ClientObj* c = (ClientObj*)self;
    c->render->h.base.add_ref(&c->render->h.base);
    return &c->render->h;
}
static struct _cef_display_handler_t* CEF_CALLBACK cl_get_display(
    struct _cef_client_t* self) {
    ClientObj* c = (ClientObj*)self;
    c->display->h.base.add_ref(&c->display->h.base);
    return &c->display->h;
}

// ---------------------------------------------------------------------------
// cef_app_t — minimal shell shared by browser and subprocess processes
// ---------------------------------------------------------------------------
typedef struct {
    cef_app_t h;
    int refs;
} AppObj;

static void MakeString(const char* utf8, cef_string_t* out);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void MakeString(const char* utf8, cef_string_t* out) {
    // cef_string_utf8_to_utf16 frees any pre-existing content via out->dtor
    // before writing — the output string MUST start zeroed or it calls
    // garbage (this was the intermittent 0xC0000005 at a heap address).
    memset(out, 0, sizeof *out);
    cef_string_utf8_to_utf16(utf8, strlen(utf8), out);
}

static uint32_t SDLModToCef(int kmod) {
    uint32_t m = 0;
    if (kmod & KMOD_LSHIFT) m |= EVENTFLAG_SHIFT_DOWN;
    if (kmod & KMOD_RSHIFT) m |= EVENTFLAG_SHIFT_DOWN | EVENTFLAG_IS_RIGHT;
    if (kmod & KMOD_LCTRL) m |= EVENTFLAG_CONTROL_DOWN;
    if (kmod & KMOD_RCTRL) m |= EVENTFLAG_CONTROL_DOWN | EVENTFLAG_IS_RIGHT;
    if (kmod & KMOD_LALT) m |= EVENTFLAG_ALT_DOWN;
    if (kmod & KMOD_RALT) m |= EVENTFLAG_ALT_DOWN | EVENTFLAG_IS_RIGHT;
    return m;
}

static uint32_t g_mouseFlags = 0; // held-button flags for move/wheel events

static void SendMouseEvent(const SDL_Event* ev) {
    if (!g_browser) return;
    cef_browser_host_t* host = g_browser->get_host(g_browser);
    cef_mouse_event_t me;
    memset(&me, 0, sizeof me);

    if (ev->type == SDL_MOUSEMOTION) {
        me.x = ev->motion.x;
        me.y = ev->motion.y;
        me.modifiers = SDLModToCef(SDL_GetModState()) | g_mouseFlags;
        host->send_mouse_move_event(host, &me, 0);
        return;
    }
    if (ev->type == SDL_MOUSEBUTTONDOWN || ev->type == SDL_MOUSEBUTTONUP) {
        cef_mouse_button_type_t btn = MBT_LEFT;
        uint32_t btnFlag = EVENTFLAG_LEFT_MOUSE_BUTTON;
        if (ev->button.button == SDL_BUTTON_RIGHT) {
            btn = MBT_RIGHT;
            btnFlag = EVENTFLAG_RIGHT_MOUSE_BUTTON;
        } else if (ev->button.button == SDL_BUTTON_MIDDLE) {
            btn = MBT_MIDDLE;
            btnFlag = EVENTFLAG_MIDDLE_MOUSE_BUTTON;
        }
        if (ev->type == SDL_MOUSEBUTTONDOWN)
            g_mouseFlags |= btnFlag;
        else
            g_mouseFlags &= ~btnFlag;
        me.x = ev->button.x;
        me.y = ev->button.y;
        me.modifiers = SDLModToCef(SDL_GetModState()) | g_mouseFlags;
        host->send_mouse_click_event(host, &me, btn,
                                     ev->type == SDL_MOUSEBUTTONUP, 1);
        return;
    }
    if (ev->type == SDL_MOUSEWHEEL) {
        me.x = ev->wheel.mouseX;
        me.y = ev->wheel.mouseY;
        me.modifiers = SDLModToCef(SDL_GetModState()) | g_mouseFlags;
        host->send_mouse_wheel_event(host, &me, ev->wheel.x, ev->wheel.y);
    }
}

// Minimal SDL_Keycode -> Windows VK mapping (demo scope).
static int SDLKeyToVK(SDL_Keycode k) {
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

static void SendKeyEvent(const SDL_Event* ev) {
    if (!g_browser) return;
    cef_browser_host_t* host = g_browser->get_host(g_browser);
    cef_key_event_t ke;
    memset(&ke, 0, sizeof ke);
    ke.size = sizeof ke;
    const SDL_Keycode k = ev->key.keysym.sym;

    ke.type = (ev->type == SDL_KEYDOWN) ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
    ke.windows_key_code = SDLKeyToVK(k);
    ke.modifiers = SDLModToCef(SDL_GetModState());
    if (ke.windows_key_code >= 0x20 && ke.windows_key_code < 0x7F)
        ke.character = (char16_t)ke.windows_key_code;
    ke.unmodified_character = ke.character;
    host->send_key_event(host, &ke);
}

// SDL_TEXTINPUT -> KEYEVENT_CHAR (generated characters, ASCII path only).
static void SendCharEvent(const char* text) {
    if (!g_browser) return;
    cef_browser_host_t* host = g_browser->get_host(g_browser);
    for (; *text; ++text) {
        if ((unsigned char)*text < 0x80 && *text != '\r' && *text != '\n') {
            cef_key_event_t ke;
            memset(&ke, 0, sizeof ke);
            ke.size = sizeof ke;
            ke.type = KEYEVENT_CHAR;
            ke.character = (char16_t)*text;
            ke.windows_key_code = (int)(unsigned char)*text;
            host->send_key_event(host, &ke);
        }
    }
}

static void Present(void) {
    if (g_textureDirty && g_texture) {
        g_textureDirty = 0;
        SDL_RenderClear(g_renderer);
        SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
        SDL_RenderPresent(g_renderer);
    }
}

static void ShutdownBrowser(void) {
    if (g_browser) {
        cef_browser_host_t* host = g_browser->get_host(g_browser);
        host->close_browser(host, 1);
    }
    // Keep pumping so the close pipeline (on_before_close) actually runs.
    for (int i = 0; i < 50 && !g_browserGone; ++i) {
        cef_do_message_loop_work();
        Sleep(10);
    }
}

int main(int argc, char* argv[]) {
    // CEF 133+ API versioning: cef_api_version() stays -1 (and every wrapper
    // FATALs with "called with invalid version") until the platform API hash
    // has been fetched and checked — in EVERY process, before any other CEF
    // call (cef_execute_process included).
    {
        const char* hash = cef_api_hash(CEF_API_VERSION, 0);
        if (!hash || strcmp(hash, CEF_API_HASH_PLATFORM) != 0) {
            fprintf(stderr, "osr: API hash mismatch (dll=%s header=%s)\n",
                    hash ? hash : "(null)", CEF_API_HASH_PLATFORM);
            return 1;
        }
    }

    cef_main_args_t mainArgs;
    mainArgs.instance = GetModuleHandleW(NULL);

    // Subprocess re-entry: renderer/GPU processes launch this same exe.
    // Ref handover: CEF's CToCpp wrap consumes one reference from every
    // struct handed to it, so add_ref before each handover — refs=1 after
    // ALLOC_HANDLER is our session reference (never released; lives until
    // process exit).
    AppObj* appObj;
    ALLOC_HANDLER(appObj, cef_app_t);
    appObj->h.base.add_ref(&appObj->h.base);
    const int sub = cef_execute_process(&mainArgs, &appObj->h, NULL);
    fprintf(stderr, "osr: execute_process=%d\n", sub);
    if (sub >= 0) return 0; // subprocess finished

    // --- browser process ---
    fprintf(stderr, "osr: sdl init\n");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    g_window = SDL_CreateWindow("cef-osr", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, kViewW, kViewH,
                                SDL_WINDOW_RESIZABLE);
    g_renderer = SDL_CreateRenderer(g_window, -1, 0);
    if (!g_window || !g_renderer) {
        fprintf(stderr, "SDL window/renderer failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_StartTextInput();

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char dir[MAX_PATH];
    strncpy(dir, exePath, MAX_PATH);
    dir[MAX_PATH - 1] = 0;
    char* slash = strrchr(dir, '\\');
    if (slash) *slash = 0;

    cef_settings_t settings;
    memset(&settings, 0, sizeof settings);
    settings.size = sizeof settings;
    settings.windowless_rendering_enabled = 1;
    settings.multi_threaded_message_loop = 0;
    // Sandbox requires a manifest-embedded policy set that a bare MinGW exe
    // does not have; subprocess launches were intermittently crashing the
    // browser process ~1s after initialize without this.
    settings.no_sandbox = 1;
    settings.log_severity = LOGSEVERITY_WARNING;
    MakeString(exePath, &settings.browser_subprocess_path);
    char buf[MAX_PATH * 2];
    snprintf(buf, sizeof buf, "%s\\debug.log", dir);
    MakeString(buf, &settings.log_file);

    appObj->h.base.add_ref(&appObj->h.base);
    const int initOk = cef_initialize(&mainArgs, &settings, &appObj->h, NULL);
    if (!initOk) {
        fprintf(stderr, "osr: initialize failed, exit code %d\n",
                cef_get_exit_code());
        return 1;
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

    // URL: argv[1] or demo.html next to the exe.
    char url[MAX_PATH * 2];
    if (argc > 1) {
        strncpy(url, argv[1], sizeof url - 1);
        url[sizeof url - 1] = 0;
    } else {
        snprintf(url, sizeof url, "file:///%s/demo.html", dir);
        {
            char* p;
            for (p = url; *p; ++p)
                if (*p == '\\') *p = '/';
        }
    }
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

    clientObj->h.base.add_ref(&clientObj->h.base);
    const int r = cef_browser_host_create_browser(&winInfo, &clientObj->h,
                                                  &urlStr, &bsettings, NULL, NULL);
    if (r != 0) fprintf(stderr, "osr: create_browser returned %d\n", r);

    // --- main loop ---
    uint32_t lastPump = 0;
    while (!g_browserGone) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    ShutdownBrowser();
                    break;
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                case SDL_MOUSEWHEEL:
                    SendMouseEvent(&ev);
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    SendKeyEvent(&ev);
                    break;
                case SDL_TEXTINPUT:
                    SendCharEvent(ev.text.text);
                    break;
                default:
                    break;
            }
        }
        const uint32_t now = SDL_GetTicks();
        if (now - lastPump >= 10) {
            lastPump = now;
            cef_do_message_loop_work();
        }
        Present();
        SDL_Delay(1);
    }

    cef_shutdown();
    SDL_Quit();
    return 0;
}