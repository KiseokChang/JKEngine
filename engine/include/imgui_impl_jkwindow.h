#ifndef IMGUI_IMPL_JKWINDOW_H
#define IMGUI_IMPL_JKWINDOW_H

// Dear ImGui backend for the jkwindow client pipeline (docs/23 §4).
// Fuses imgui_impl_sdl2 (events, ported to JKEvent) and imgui_impl_sdlrenderer2
// (SDL_RenderGeometry raster): a jkwindow client does not own a visible SDL
// window — the server does — so there is no SDL event pump to hook. Instead the
// app forwards every JKEvent it receives, and the draw output is rendered into
// the client's offscreen target texture before the shm readback.

#include <imgui.h>

struct SDL_Renderer;
namespace jk { struct JKEvent; }

// Call after ImGui::CreateContext(). The renderer must be the client's hidden
// accelerated renderer; draw output lands on whatever render target is bound
// when RenderDrawData runs (the client's offscreen target texture, scale 1:1).
bool ImGui_ImplJKWindow_Init(SDL_Renderer* renderer);
void ImGui_ImplJKWindow_Shutdown();

// Feed one JKEvent (any type; unhandled kinds are ignored). Call for EVERY
// event the app receives, before the next NewFrame consumes the input queue.
// Modifiers come from the event's `option` field (the server-relayed
// keysym.mod) — SDL_GetModState() in a client process is always empty because
// the visible window belongs to the server (docs/23 §4.1).
void ImGui_ImplJKWindow_ProcessJKEvent(const jk::JKEvent& ev);

// display size is the client surface design size, not the server desktop —
// the server applies the fit scale when compositing (docs/23 §5.2-5).
void ImGui_ImplJKWindow_NewFrame(float dt, int display_w, int display_h);

// Renders the current draw data onto the render target bound to `renderer`.
// Call between the scene replay and SDL_RenderReadPixels in the client commit
// path — i.e. from the JKClientApplication::RenderOverlay hook (docs/23 §5.2-4).
void ImGui_ImplJKWindow_RenderDrawData(ImDrawData* draw_data, SDL_Renderer* renderer);

#endif // IMGUI_IMPL_JKWINDOW_H