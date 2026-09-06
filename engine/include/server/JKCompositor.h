#ifndef JKCOMPOSITOR_H
#define JKCOMPOSITOR_H

#include <server/JKCompositorLayer.h>
#include <server/JKCompositorOutput.h>
#include <SDL.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace jk {
namespace server {

// Server-side window chrome geometry (logical points, surface-local).
// MUST stay in sync with the frame painted by the client inside its surface:
// src/JKWindow.cpp OnRectChanged (kBorder=2, kTitle=24) and
// GetCloseButtonRect (20x20 at 2px inset from the top-right).
constexpr int kChromeTitleBar = 24;
constexpr int kChromeBorder = 2;
constexpr int kChromeCloseSize = 20;
constexpr int kChromeCloseMargin = 2;
constexpr int kResizeHotspot = 6;

// Server compositor: owns SDL textures for client surfaces and draws them into
// a single renderer output. For Phase 2 there is exactly one output.
class JKCompositor {
public:
    explicit JKCompositor(SDL_Renderer* renderer);
    ~JKCompositor();

    // Add or remove layers.
    JKCompositorLayer* AddLayer(uint32_t id,
                                int width, int height,
                                const std::string& title,
                                uint8_t* pixels);
    void RemoveLayer(uint32_t id);

    // Move/resize a layer.
    void SetLayerPosition(uint32_t id, int x, int y);
    void SetLayerScale(uint32_t id, float sx, float sy);
    void SetLayerAlpha(uint32_t id, uint8_t alpha);
    // Shell role (docs/28): re-sorts so the shell stays topmost.
    void SetLayerShell(uint32_t id, bool shell);

    // Minimize support (docs/28 단계 4): hide/show a layer server-side —
    // the client keeps rendering and committing; only the draw is skipped.
    void SetLayerVisible(uint32_t id, bool visible);
    bool IsLayerVisible(uint32_t id);

    // Display height (logical points) of the shell layer, or 0 when no shell
    // is active. The window server reserves this much of the desktop as the
    // work area (window placement clamps + drag clamps).
    int ShellReserveHeight();

    // Mark a layer dirty and request texture update.
    void MarkDirty(uint32_t id);

    // Update output bounds (for now a single output covering the SDL window).
    void SetOutput(const JKCompositorOutput& output);
    float OutputScale() const { return output_.Scale(); }

    // Sort layers by a z-order policy. Phase 2: focused client on top.
    void FocusLayer(uint32_t id);

    // Id of the topmost visible layer (the focused one, or the last sorted
    // layer when focus is unset) — used to re-focus keyboard input after the
    // focused client disconnects. Returns 0 when no layers exist.
    uint32_t TopmostLayerId();

    // Replace a layer's texture and pixel source with a resized one. All
    // fields (texture, w/h, pixels, scale reset) swap atomically under the
    // layers mutex so Composite/HitTest never observe a mismatched pitch.
    // Main-thread only; returns false if the layer does not exist.
    bool ResizeLayer(uint32_t id, int width, int height, uint8_t* pixels);

    // Composite all layers to the renderer.
    void Composite();

    // Hit test in output coordinates returns the topmost layer at (x,y).
    JKCompositorLayer* HitTest(int x, int y);

    // Public lookup for the window server's chrome drag state (layer
    // pointers die on RemoveLayer, so callers store ids and re-resolve).
    JKCompositorLayer* FindLayerById(uint32_t id);

private:
    SDL_Renderer* renderer_ = nullptr;
    JKCompositorOutput output_{0, JKRect{0, 0, 0, 0}, 1.0f};

    std::mutex layersMutex_;
    std::vector<std::unique_ptr<JKCompositorLayer>> layers_;
    uint32_t focusedId_ = 0;

    JKCompositorLayer* FindLayer(uint32_t id);
    void UpdateLayerTexture(JKCompositorLayer& layer);
    void SortLayers();
    void DrawCloseOverlay(const JKCompositorLayer& layer, float scale);
};

} // namespace server
} // namespace jk

#endif // JKCOMPOSITOR_H
