#include <JKOffscreenSurface.h>

namespace jk {

JKOffscreenSurface::JKOffscreenSurface(JKRenderBackend* backend, int w, int h)
    : backend_(backend), w_(w), h_(h), dc_(backend_) {
    if (backend_) {
        texture_ = backend_->CreateTargetTexture(w, h);
    }
}

JKOffscreenSurface::~JKOffscreenSurface() {
    if (backend_ && texture_ != JKRenderBackend::InvalidTexture) {
        backend_->DestroyTexture(texture_);
        texture_ = JKRenderBackend::InvalidTexture;
    }
}

JKDC* JKOffscreenSurface::BeginDraw() {
    if (!backend_ || !IsValid()) return nullptr;
    backend_->SetRenderTarget(texture_);
    return &dc_;
}

void JKOffscreenSurface::EndDraw() {
    if (backend_) {
        backend_->SetRenderTarget(nullptr);
    }
}

void JKOffscreenSurface::Clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    JKDC* dc = BeginDraw();
    if (dc) {
        dc->SetColor(r, g, b, a);
        dc->Clear();
        EndDraw();
    }
}

} // namespace jk
