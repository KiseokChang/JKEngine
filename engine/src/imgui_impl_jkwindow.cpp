// Dear ImGui backend for the jkwindow client pipeline (docs/23 §4).
// Event side is a port of imgui_impl_sdl2 fed from JKEvent instead of the SDL
// event pump (a client process receives no OS input — the visible window is
// the server's). Render side is a port of imgui_impl_sdlrenderer2: identical
// SDL_RenderGeometryRaw raster because the client already owns an accelerated
// renderer whose render-target texture IS the committed surface (docs/23 §4.2).
#include <imgui_impl_jkwindow.h>

#include "imgui.h"
#ifndef IMGUI_DISABLE

#include <JKEvent.h>
#include <SDL.h>
#include <stdint.h>
#include <string.h>

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry()
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

// One fused data block for the platform+renderer halves: a jkwindow client is
// a single window / single renderer process by construction (docs/23 §3.2),
// so the split data structs the official backends use would be dead weight.
struct ImGui_ImplJKWindow_Data
{
    SDL_Renderer* Renderer = nullptr;
    char*         ClipboardTextData = nullptr;
};

static ImGui_ImplJKWindow_Data* ImGui_ImplJKWindow_GetBackendData()
{
    return ImGui::GetCurrentContext()
        ? (ImGui_ImplJKWindow_Data*)ImGui::GetIO().BackendPlatformUserData
        : nullptr;
}

// Render state exposed to draw callbacks via platform_io.Renderer_RenderState
// (mirrors ImGui_ImplSDLRenderer2_RenderState; only our callbacks see it).
struct ImGui_ImplJKWindow_RenderState
{
    SDL_Renderer* Renderer;
};

void ImGui_ImplJKWindow_DestroyDeviceObjects();

// ---------------------------------------------------------------------------
// Clipboard (docs/23 §4.1): clients run SDL_INIT_VIDEO, so the OS clipboard is
// reachable directly — no server clipboard bridge exists or is needed. The
// SDL_GetClipboardText buffer must outlive ImGui's read; park it in bd.
static const char* ImGui_ImplJKWindow_GetClipboardText(void* user_data)
{
    ImGui_ImplJKWindow_Data* bd = (ImGui_ImplJKWindow_Data*)user_data;
    if (bd->ClipboardTextData)
        SDL_free(bd->ClipboardTextData);
    bd->ClipboardTextData = SDL_GetClipboardText();
    return bd->ClipboardTextData;
}

static void ImGui_ImplJKWindow_SetClipboardText(void*, const char* text)
{
    SDL_SetClipboardText(text);
}

// ---------------------------------------------------------------------------
// Key mapping — ImGui_ImplSDL2_KeyEventToImGuiKey minus the scancode fallback:
// JKEvent carries no scancode (the server relays keysym.sym only, docs/23
// §4.1), so punctuation keys are resolved from their ASCII-valued keycodes
// here instead.
static ImGuiKey ImGui_ImplJKWindow_KeycodeToImGuiKey(SDL_Keycode keycode)
{
    switch (keycode)
    {
        case SDLK_TAB: return ImGuiKey_Tab;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_PAGEUP: return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
        case SDLK_HOME: return ImGuiKey_Home;
        case SDLK_END: return ImGuiKey_End;
        case SDLK_INSERT: return ImGuiKey_Insert;
        case SDLK_DELETE: return ImGuiKey_Delete;
        case SDLK_BACKSPACE: return ImGuiKey_Backspace;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        case SDLK_COMMA: return ImGuiKey_Comma;
        case SDLK_MINUS: return ImGuiKey_Minus;
        case SDLK_PERIOD: return ImGuiKey_Period;
        case SDLK_SLASH: return ImGuiKey_Slash;
        case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
        case SDLK_EQUALS: return ImGuiKey_Equal;
        case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
        case SDLK_BACKSLASH: return ImGuiKey_Backslash;
        case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        case SDLK_BACKQUOTE: return ImGuiKey_GraveAccent;
        case SDLK_QUOTE: return ImGuiKey_Apostrophe;
        case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
        case SDLK_PAUSE: return ImGuiKey_Pause;
        case SDLK_KP_0: return ImGuiKey_Keypad0;
        case SDLK_KP_1: return ImGuiKey_Keypad1;
        case SDLK_KP_2: return ImGuiKey_Keypad2;
        case SDLK_KP_3: return ImGuiKey_Keypad3;
        case SDLK_KP_4: return ImGuiKey_Keypad4;
        case SDLK_KP_5: return ImGuiKey_Keypad5;
        case SDLK_KP_6: return ImGuiKey_Keypad6;
        case SDLK_KP_7: return ImGuiKey_Keypad7;
        case SDLK_KP_8: return ImGuiKey_Keypad8;
        case SDLK_KP_9: return ImGuiKey_Keypad9;
        case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
        case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
        case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
        case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
        case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return ImGuiKey_LeftShift;
        case SDLK_LALT: return ImGuiKey_LeftAlt;
        case SDLK_LGUI: return ImGuiKey_LeftSuper;
        case SDLK_RCTRL: return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT: return ImGuiKey_RightShift;
        case SDLK_RALT: return ImGuiKey_RightAlt;
        case SDLK_RGUI: return ImGuiKey_RightSuper;
        case SDLK_APPLICATION: return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0;
        case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2;
        case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4;
        case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6;
        case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8;
        case SDLK_9: return ImGuiKey_9;
        case SDLK_a: return ImGuiKey_A;
        case SDLK_b: return ImGuiKey_B;
        case SDLK_c: return ImGuiKey_C;
        case SDLK_d: return ImGuiKey_D;
        case SDLK_e: return ImGuiKey_E;
        case SDLK_f: return ImGuiKey_F;
        case SDLK_g: return ImGuiKey_G;
        case SDLK_h: return ImGuiKey_H;
        case SDLK_i: return ImGuiKey_I;
        case SDLK_j: return ImGuiKey_J;
        case SDLK_k: return ImGuiKey_K;
        case SDLK_l: return ImGuiKey_L;
        case SDLK_m: return ImGuiKey_M;
        case SDLK_n: return ImGuiKey_N;
        case SDLK_o: return ImGuiKey_O;
        case SDLK_p: return ImGuiKey_P;
        case SDLK_q: return ImGuiKey_Q;
        case SDLK_r: return ImGuiKey_R;
        case SDLK_s: return ImGuiKey_S;
        case SDLK_t: return ImGuiKey_T;
        case SDLK_u: return ImGuiKey_U;
        case SDLK_v: return ImGuiKey_V;
        case SDLK_w: return ImGuiKey_W;
        case SDLK_x: return ImGuiKey_X;
        case SDLK_y: return ImGuiKey_Y;
        case SDLK_z: return ImGuiKey_Z;
        case SDLK_F1: return ImGuiKey_F1;
        case SDLK_F2: return ImGuiKey_F2;
        case SDLK_F3: return ImGuiKey_F3;
        case SDLK_F4: return ImGuiKey_F4;
        case SDLK_F5: return ImGuiKey_F5;
        case SDLK_F6: return ImGuiKey_F6;
        case SDLK_F7: return ImGuiKey_F7;
        case SDLK_F8: return ImGuiKey_F8;
        case SDLK_F9: return ImGuiKey_F9;
        case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11;
        case SDLK_F12: return ImGuiKey_F12;
        case SDLK_F13: return ImGuiKey_F13;
        case SDLK_F14: return ImGuiKey_F14;
        case SDLK_F15: return ImGuiKey_F15;
        case SDLK_F16: return ImGuiKey_F16;
        case SDLK_F17: return ImGuiKey_F17;
        case SDLK_F18: return ImGuiKey_F18;
        case SDLK_F19: return ImGuiKey_F19;
        case SDLK_F20: return ImGuiKey_F20;
        case SDLK_F21: return ImGuiKey_F21;
        case SDLK_F22: return ImGuiKey_F22;
        case SDLK_F23: return ImGuiKey_F23;
        case SDLK_F24: return ImGuiKey_F24;
        case SDLK_AC_BACK: return ImGuiKey_AppBack;
        case SDLK_AC_FORWARD: return ImGuiKey_AppForward;
        default: break;
    }
    return ImGuiKey_None;
}

static void ImGui_ImplJKWindow_UpdateKeyModifiers(SDL_Keymod sdl_key_mods)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, (sdl_key_mods & KMOD_CTRL) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (sdl_key_mods & KMOD_SHIFT) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (sdl_key_mods & KMOD_ALT) != 0);
    io.AddKeyEvent(ImGuiMod_Super, (sdl_key_mods & KMOD_GUI) != 0);
}

// ---------------------------------------------------------------------------
// Event feeding (docs/23 §4.1 table).

void ImGui_ImplJKWindow_ProcessJKEvent(const jk::JKEvent& ev)
{
    ImGuiIO& io = ImGui::GetIO();

    switch (ev.type)
    {
        case jk::JKEventType::MouseMove:
            // Surface-local design px already; server capture keeps sending
            // out-of-surface coords during drags — ImGui accepts those
            // (docs/23 §8-5).
            io.AddMousePosEvent((float)ev.x, (float)ev.y);
            break;

        case jk::JKEventType::MouseDown:
        case jk::JKEventType::MouseUp:
        {
            // Client convention (JKClientSurface.cpp deserialization):
            // ev.detail = SDL button number 1..5.
            int button = -1;
            if (ev.detail == SDL_BUTTON_LEFT)   button = 0;
            if (ev.detail == SDL_BUTTON_RIGHT)  button = 1;
            if (ev.detail == SDL_BUTTON_MIDDLE) button = 2;
            if (ev.detail == SDL_BUTTON_X1)     button = 3;
            if (ev.detail == SDL_BUTTON_X2)     button = 4;
            if (button != -1)
                io.AddMouseButtonEvent(button, ev.type == jk::JKEventType::MouseDown);
            break;
        }

        case jk::JKEventType::MouseWheel:
            // Same sign flip as imgui_impl_sdl2: x inverted, y as-is.
            io.AddMouseWheelEvent(-(float)ev.dx, (float)ev.dy);
            break;

        case jk::JKEventType::KeyDown:
        case jk::JKEventType::KeyUp:
            // Modifiers MUST come from the relayed keysym.mod (ev.option) —
            // SDL_GetModState() is dead in a client process. Repeat events
            // (ev.detail) are fed through like the official backend does.
            ImGui_ImplJKWindow_UpdateKeyModifiers((SDL_Keymod)ev.option);
        {
            const ImGuiKey key = ImGui_ImplJKWindow_KeycodeToImGuiKey((SDL_Keycode)ev.keyCode);
            if (key != ImGuiKey_None)
                io.AddKeyEvent(key, ev.type == jk::JKEventType::KeyDown);
        }
            break;

        case jk::JKEventType::Char:
            // UTF-8 from SDL_TEXTINPUT relayed by the server (ev.text).
            io.AddInputCharactersUTF8(ev.text);
            break;

        default:
            // TextEditing (IME pre-edit) is deliberately unmapped in Phase 1
            // (docs/23 §4.1, §8-7): ImGui does not render composition strings.
            break;
    }
}

// ---------------------------------------------------------------------------
// Renderer side — port of imgui_impl_sdlrenderer2.

bool ImGui_ImplJKWindow_Init(SDL_Renderer* renderer)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendPlatformUserData == nullptr &&
              io.BackendRendererUserData == nullptr && "Already initialized a backend!");
    IM_ASSERT(renderer != nullptr && "SDL_Renderer not initialized!");

    ImGui_ImplJKWindow_Data* bd = IM_NEW(ImGui_ImplJKWindow_Data)();
    bd->Renderer = renderer;
    io.BackendPlatformUserData = (void*)bd;
    io.BackendRendererUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_jkwindow";
    io.BackendRendererName = "imgui_impl_jkwindow";
    // RendererHasTextures: 1.92 dynamic font protocol — texture
    // create/update/destroy requests arrive via draw_data->Textures.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    // No ImGuiBackendFlags_HasMouseCursors: the server draws the one cursor
    // and does not track shape changes (docs/23 §8-6).

    io.SetClipboardTextFn = ImGui_ImplJKWindow_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplJKWindow_GetClipboardText;
    io.ClipboardUserData = (void*)bd;

    return true;
}

void ImGui_ImplJKWindow_Shutdown()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGui_ImplJKWindow_Data* bd = ImGui_ImplJKWindow_GetBackendData();
    IM_ASSERT(bd != nullptr && "No backend to shutdown, or already shutdown?");

    ImGui_ImplJKWindow_DestroyDeviceObjects();

    if (bd->ClipboardTextData)
        SDL_free(bd->ClipboardTextData);

    io.BackendPlatformName = nullptr;
    io.BackendRendererName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset |
                         ImGuiBackendFlags_RendererHasTextures);
    platform_io.ClearPlatformHandlers();
    platform_io.ClearRendererHandlers();
    IM_DELETE(bd);
}

void ImGui_ImplJKWindow_NewFrame(float dt, int display_w, int display_h)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(ImGui_ImplJKWindow_GetBackendData() != nullptr &&
              "Backend not initialized! Did you call ImGui_ImplJKWindow_Init()?");

    io.DisplaySize = ImVec2((float)display_w, (float)display_h);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;
}

// Draw callbacks
static void ImGui_ImplJKWindow_DrawCallback_ResetRenderState(const ImDrawList*, const ImDrawCmd*) {}

static void ImGui_ImplJKWindow_SetupRenderState(SDL_Renderer* renderer)
{
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetClipRect(renderer, nullptr);
}

// 1.92 dynamic font/textures protocol (docs/23 §4.3): create/update/destroy
// SDL textures from ImTextureData before the draw lists sample them.
static void ImGui_ImplJKWindow_UpdateTexture(ImTextureData* tex)
{
    ImGui_ImplJKWindow_Data* bd = ImGui_ImplJKWindow_GetBackendData();

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);

        // (Bilinear sampling is required by default — see the official
        // backend note on ImFontAtlasFlags_NoBakedLines.)
        SDL_Texture* sdl_texture = SDL_CreateTexture(bd->Renderer, SDL_PIXELFORMAT_RGBA32,
                                                     SDL_TEXTUREACCESS_STATIC,
                                                     tex->Width, tex->Height);
        IM_ASSERT(sdl_texture != nullptr && "Backend failed to create texture!");
        SDL_UpdateTexture(sdl_texture, nullptr, tex->GetPixels(), tex->GetPitch());
        SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(sdl_texture, SDL_ScaleModeLinear);

        tex->SetTexID((ImTextureID)(intptr_t)sdl_texture);
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        SDL_Texture* sdl_texture = (SDL_Texture*)(intptr_t)tex->TexID;
        for (ImTextureRect& r : tex->Updates)
        {
            SDL_Rect sdl_r = { r.x, r.y, r.w, r.h };
            SDL_UpdateTexture(sdl_texture, &sdl_r, tex->GetPixelsAt(r.x, r.y), tex->GetPitch());
        }
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy)
    {
        if (tex->TexID != ImTextureID_Invalid)
            if (SDL_Texture* sdl_texture = (SDL_Texture*)(intptr_t)tex->TexID)
                SDL_DestroyTexture(sdl_texture);

        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

void ImGui_ImplJKWindow_DestroyDeviceObjects()
{
    // Destroy all backend-owned textures.
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
        {
            tex->SetStatus(ImTextureStatus_WantDestroy);
            ImGui_ImplJKWindow_UpdateTexture(tex);
        }
}

void ImGui_ImplJKWindow_RenderDrawData(ImDrawData* draw_data, SDL_Renderer* renderer)
{
    // FramebufferScale is (1,1) by construction: the client renders its
    // surface design pixels into a same-size target texture, and the server
    // applies the display fit scale afterwards (docs/23 §5.2-5).
    const int fb_width = (int)(draw_data->DisplaySize.x);
    const int fb_height = (int)(draw_data->DisplaySize.y);
    if (fb_width == 0 || fb_height == 0)
        return;

    // Catch up with texture updates. Most of the times, the list will have 1
    // element with an OK status, aka nothing to do.
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplJKWindow_UpdateTexture(tex);

    // Backup SDL_Renderer state that will be modified to restore it afterwards
    struct BackupSDLRendererState
    {
        SDL_Rect    Viewport;
        bool        ClipEnabled;
        SDL_Rect    ClipRect;
    };
    BackupSDLRendererState old = {};
    old.ClipEnabled = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;
    SDL_RenderGetViewport(renderer, &old.Viewport);
    SDL_RenderGetClipRect(renderer, &old.ClipRect);

    // Setup desired state
    ImGui_ImplJKWindow_SetupRenderState(renderer);

    // Setup render state structure (for callbacks and custom texture bindings)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGui_ImplJKWindow_RenderState render_state;
    render_state.Renderer = renderer;
    platform_io.Renderer_RenderState = &render_state;

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless multi-viewport

    // Render command lists
    for (const ImDrawList* draw_list : draw_data->CmdLists)
    {
        const ImDrawVert* vtx_buffer = draw_list->VtxBuffer.Data;
        const ImDrawIdx* idx_buffer = draw_list->IdxBuffer.Data;

        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                if (pcmd->UserCallback == ImGui_ImplJKWindow_DrawCallback_ResetRenderState)
                    ImGui_ImplJKWindow_SetupRenderState(renderer);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x), (pcmd->ClipRect.y - clip_off.y));
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x), (pcmd->ClipRect.w - clip_off.y));
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > (float)fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > (float)fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                SDL_Rect r = { (int)(clip_min.x), (int)(clip_min.y),
                               (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y) };
                SDL_RenderSetClipRect(renderer, &r);

                const float* xy = (const float*)(const void*)((const char*)(vtx_buffer + pcmd->VtxOffset) + offsetof(ImDrawVert, pos));
                const float* uv = (const float*)(const void*)((const char*)(vtx_buffer + pcmd->VtxOffset) + offsetof(ImDrawVert, uv));
                const SDL_Color* color = (const SDL_Color*)(const void*)((const char*)(vtx_buffer + pcmd->VtxOffset) + offsetof(ImDrawVert, col)); // SDL 2.0.19+

                // Bind texture, Draw
                SDL_Texture* tex = (SDL_Texture*)pcmd->GetTexID();
                SDL_RenderGeometryRaw(renderer, tex,
                    xy, (int)sizeof(ImDrawVert),
                    color, (int)sizeof(ImDrawVert),
                    uv, (int)sizeof(ImDrawVert),
                    draw_list->VtxBuffer.Size - pcmd->VtxOffset,
                    idx_buffer + pcmd->IdxOffset, pcmd->ElemCount, sizeof(ImDrawIdx));
            }
        }
    }
    platform_io.Renderer_RenderState = nullptr;

    // Restore modified SDL_Renderer state
    SDL_RenderSetViewport(renderer, &old.Viewport);
    SDL_RenderSetClipRect(renderer, old.ClipEnabled ? &old.ClipRect : nullptr);
}

#endif // #ifndef IMGUI_DISABLE