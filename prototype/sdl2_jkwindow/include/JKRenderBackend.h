#ifndef JKRENDERBACKEND_H
#define JKRENDERBACKEND_H

#include <JKTypes.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace jk {

// Low-level rendering abstraction used by JKDC.
// The SDL2 prototype provides an SDL-based implementation; future ports
// can plug in a different backend without touching the UI code.
class JKRenderBackend {
public:
    using TextureHandle = void*;
    static constexpr TextureHandle InvalidTexture = nullptr;

    virtual ~JKRenderBackend() = default;

    // Native handle for optional backend-specific operations (e.g. SDL_Renderer*).
    virtual void* GetNativeHandle() const = 0;

    // Coordinate / output
    virtual void SetScale(float sx, float sy) = 0;
    virtual void GetOutputSize(int& w, int& h) = 0;

    // Color / clearing
    virtual void SetDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) = 0;
    virtual void Clear() = 0;
    virtual void Present() = 0;

    // Clipping: nullptr disables.
    virtual void SetClipRect(const JKRect* rect) = 0;

    // Primitives
    virtual void DrawRect(const JKRect& rect) = 0;
    virtual void FillRect(const JKRect& rect) = 0;
    virtual void DrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2) = 0;
    virtual void DrawPixel(int32_t x, int32_t y) = 0;
    virtual void DrawPoints(const JKPoint* points, size_t count) = 0;
    virtual void DrawPolygon(const std::vector<JKPoint>& points) = 0;

    // Render-target textures
    virtual TextureHandle CreateTargetTexture(int w, int h) = 0;
    virtual void DestroyTexture(TextureHandle texture) = 0;
    virtual void SetRenderTarget(TextureHandle texture) = 0;
    virtual void BlitTexture(TextureHandle texture,
                             const JKRect* src,
                             const JKRect& dst,
                             uint8_t alpha = 255) = 0;
};

} // namespace jk

#endif // JKRENDERBACKEND_H
