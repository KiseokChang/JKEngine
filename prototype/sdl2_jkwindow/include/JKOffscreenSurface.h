#ifndef JKOFFSCREENSURFACE_H
#define JKOFFSCREENSURFACE_H

#include <JKRenderBackend.h>
#include <JKTypes.h>
#include <JKDC.h>
#include <cstdint>

namespace jk {

// A render-target texture that can be drawn into through a temporary JKDC.
// This is the equivalent of a memory DC / off-screen bitmap in the legacy engine.
class JKOffscreenSurface {
public:
    JKOffscreenSurface(JKRenderBackend* backend, int w, int h);
    ~JKOffscreenSurface();

    JKOffscreenSurface(const JKOffscreenSurface&) = delete;
    JKOffscreenSurface& operator=(const JKOffscreenSurface&) = delete;

    bool IsValid() const { return texture_ != JKRenderBackend::InvalidTexture; }
    int Width() const { return w_; }
    int Height() const { return h_; }

    JKRenderBackend::TextureHandle GetTexture() const { return texture_; }

    // Switch the backend render target to this surface and return a DC.
    JKDC* BeginDraw();
    void EndDraw();

    // Convenience: clear the surface.
    void Clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

private:
    JKRenderBackend* backend_ = nullptr;
    JKRenderBackend::TextureHandle texture_ = JKRenderBackend::InvalidTexture;
    int w_ = 0;
    int h_ = 0;
    JKDC dc_;
};

} // namespace jk

#endif // JKOFFSCREENSURFACE_H
