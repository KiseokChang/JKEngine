#ifndef JKCOMPOSITOR_H
#define JKCOMPOSITOR_H

#include <server/JKCompositorLayer.h>
#include <server/JKCompositorOutput.h>
#include <SDL.h>
#include <cstdint>
#include <functional>
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
// docs/39: maximize/restore button — same 20x20 box as the close X, sitting
// kChromeMaximizeGap px to its left in the title bar.
constexpr int kChromeMaximizeSize = 20;
constexpr int kChromeMaximizeGap = 2;
constexpr int kResizeHotspot = 6;

// docs/35: the rubber-band capture overlay's surface title. The server treats
// a client with this title as chromeless fullscreen: no close overlay, no
// title-bar drag, and the spawn placement resizes it to the whole desktop.
// Must match ClientSnapApp's JKAppMeta title.
constexpr const char* kCaptureOverlayTitle = "Region Capture";

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
    // Logical size of the single output (docs/35 capture_region clamping).
    int OutputWidth() const { return output_.Bounds().w; }
    int OutputHeight() const { return output_.Bounds().h; }

    // Sort layers by a z-order policy. Phase 2: focused client on top.
    void FocusLayer(uint32_t id);

    // Draw all visible layers into the framebuffer; present when asked.
    // capture_region (docs/35) draws without presenting to read pixels back.
    void Composite(bool present);

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

    // 승인 대상 시각화 훅 (스펙 2026-09-19-app-tool-hub §5 1단): Composite가
    // 모든 레이어(크롬 오버레이 포함)를 그린 뒤 present 직전에 한 번 부른다 —
    // DrawCloseOverlay와 같은 컴포지트 패스의 "레이어 위" 드로잉 단계. 서버만
    // pendingApprovals_를 알므로 컴포지터는 함수만 받는다(의존성 역전 —
    // DrawCloseOverlay의 면제 규칙은 훅 구현자가 동일 적용한다). Composite(false)
    // (capture_region 리드백)에서도 그려진다 — 화면에 보이는 것이 곧 캡처다.
    using OverlayHook = std::function<void(float outputScale)>;
    void SetOverlayHook(OverlayHook hook) { overlayHook_ = std::move(hook); }

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
    // docs/39: maximize/restore button left of the close X. The glyph
    // (maximize vs. restore) follows the layer's Maximized() flag — the
    // server owns the state, the compositor only mirrors it for drawing.
    void DrawMaximizeButton(const JKCompositorLayer& layer, float scale);
    OverlayHook overlayHook_;
};

} // namespace server
} // namespace jk

#endif // JKCOMPOSITOR_H
