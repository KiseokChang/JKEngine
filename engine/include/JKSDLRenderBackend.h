#ifndef JKSDLRENDERBACKEND_H
#define JKSDLRENDERBACKEND_H

#include <JKRenderBackend.h>
#include <SDL.h>

namespace jk {

class JKSDLRenderBackend : public JKRenderBackend {
public:
    explicit JKSDLRenderBackend(SDL_Renderer* renderer);
    ~JKSDLRenderBackend() override = default;

    void* GetNativeHandle() const override { return renderer_; }
    void SetScale(float sx, float sy) override;
    void GetOutputSize(int& w, int& h) override;

    void SetDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) override;
    void Clear() override;
    void Present() override;

    void SetClipRect(const JKRect* rect) override;

    void DrawRect(const JKRect& rect) override;
    void FillRect(const JKRect& rect) override;
    void DrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2) override;
    void DrawPixel(int32_t x, int32_t y) override;
    void DrawPoints(const JKPoint* points, size_t count) override;
    void DrawPolygon(const std::vector<JKPoint>& points) override;

    TextureHandle CreateTargetTexture(int w, int h) override;
    void DestroyTexture(TextureHandle texture) override;
    void SetRenderTarget(TextureHandle texture) override;
    void BlitTexture(TextureHandle texture,
                     const JKRect* src,
                     const JKRect& dst,
                     uint8_t alpha = 255) override;

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Rect ToSDL(const JKRect& r) const;
};

} // namespace jk

#endif // JKSDLRENDERBACKEND_H
