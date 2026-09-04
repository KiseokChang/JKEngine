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

    // Mark a layer dirty and request texture update.
    void MarkDirty(uint32_t id);

    // Update output bounds (for now a single output covering the SDL window).
    void SetOutput(const JKCompositorOutput& output);

    // Sort layers by a z-order policy. Phase 2: focused client on top.
    void FocusLayer(uint32_t id);

    // Composite all layers to the renderer.
    void Composite();

    // Hit test in output coordinates returns the topmost layer at (x,y).
    JKCompositorLayer* HitTest(int x, int y);

private:
    SDL_Renderer* renderer_ = nullptr;
    JKCompositorOutput output_{0, JKRect{0, 0, 0, 0}, 1.0f};

    std::mutex layersMutex_;
    std::vector<std::unique_ptr<JKCompositorLayer>> layers_;
    uint32_t focusedId_ = 0;

    JKCompositorLayer* FindLayer(uint32_t id);
    void UpdateLayerTexture(JKCompositorLayer& layer);
    void SortLayers();
};

} // namespace server
} // namespace jk

#endif // JKCOMPOSITOR_H
