#include <JKSDLRenderBackend.h>
#include <algorithm>

namespace jk {

JKSDLRenderBackend::JKSDLRenderBackend(SDL_Renderer* renderer) : renderer_(renderer) {
}

SDL_Rect JKSDLRenderBackend::ToSDL(const JKRect& r) const {
    return SDL_Rect{ r.x, r.y, r.w, r.h };
}

void JKSDLRenderBackend::SetScale(float sx, float sy) {
    if (renderer_) {
        SDL_RenderSetScale(renderer_, sx, sy);
    }
}

void JKSDLRenderBackend::GetOutputSize(int& w, int& h) {
    w = 0;
    h = 0;
    if (renderer_) {
        SDL_GetRendererOutputSize(renderer_, &w, &h);
    }
}

void JKSDLRenderBackend::SetDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (renderer_) {
        SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    }
}

void JKSDLRenderBackend::Clear() {
    if (renderer_) {
        SDL_RenderClear(renderer_);
    }
}

void JKSDLRenderBackend::Present() {
    if (renderer_) {
        SDL_RenderPresent(renderer_);
    }
}

void JKSDLRenderBackend::SetClipRect(const JKRect* rect) {
    if (!renderer_) return;
    if (rect && !rect->IsEmpty()) {
        SDL_Rect sr = ToSDL(*rect);
        SDL_RenderSetClipRect(renderer_, &sr);
    } else {
        SDL_RenderSetClipRect(renderer_, nullptr);
    }
}

void JKSDLRenderBackend::DrawRect(const JKRect& rect) {
    if (!renderer_ || rect.IsEmpty()) return;
    SDL_Rect r = ToSDL(rect);
    SDL_RenderDrawRect(renderer_, &r);
}

void JKSDLRenderBackend::FillRect(const JKRect& rect) {
    if (!renderer_ || rect.IsEmpty()) return;
    SDL_Rect r = ToSDL(rect);
#if defined(__ANDROID__)
    // 스파이크-B 첫 FillRect 직착점 계량기 — 채우고 2x2 되읽기로 "이
    // 프리미티브가 타깃에 착지했는가"를 그 지점에서 판정한다(r25 원문:
    // rc=0인데 픽셀 불도착 — 호출 사이 어긋남 탈출).
    {
        static int spikeFills = 3;
        const bool probe = spikeFills > 0;
        const int rc = SDL_RenderFillRect(renderer_, &r);
        if (probe) {
            --spikeFills;
            SDL_ClearError();
            uint8_t cr = 0, cg = 0, cb = 0, ca = 0;
            SDL_GetRenderDrawColor(renderer_, &cr, &cg, &cb, &ca);
            const int cx = (rect.x + 2 < rect.x + rect.w) ? rect.x + rect.w / 2
                                                          : rect.x + 2;
            const int cy = (rect.y + 2 < rect.y + rect.h) ? rect.y + rect.h / 2
                                                          : rect.y + 2;
            uint32_t sample = 0;
            SDL_Rect cell = SDL_Rect{ cx, cy, 2, 2 };
            const int rd = SDL_RenderReadPixels(renderer_, &cell,
                SDL_PIXELFORMAT_RGBA32, &sample, 8);
            std::fprintf(stderr,
                "[spikeB] FillRect rc=%d %dx%d@%d,%d color=%02x%02x%02x%02x "
                "read=%d val=%08x at=%d,%d err=%s\n",
                rc, rect.w, rect.h, rect.x, rect.y, cr, cg, cb, ca,
                rd, sample, cx, cy, SDL_GetError());
            std::fflush(stderr);
        }
        return;
    }
#endif
    SDL_RenderFillRect(renderer_, &r);
}

void JKSDLRenderBackend::DrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    if (!renderer_) return;
    SDL_RenderDrawLine(renderer_, x1, y1, x2, y2);
}

void JKSDLRenderBackend::DrawPixel(int32_t x, int32_t y) {
    if (!renderer_) return;
    SDL_RenderDrawPoint(renderer_, x, y);
}

void JKSDLRenderBackend::DrawPoints(const JKPoint* points, size_t count) {
    if (!renderer_ || !points || count == 0) return;
    std::vector<SDL_Point> sdl;
    sdl.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        sdl.push_back(SDL_Point{ points[i].x, points[i].y });
    }
    SDL_RenderDrawPoints(renderer_, sdl.data(), static_cast<int>(sdl.size()));
}

void JKSDLRenderBackend::DrawPolygon(const std::vector<JKPoint>& points) {
    if (!renderer_ || points.size() < 2) return;
    std::vector<SDL_Point> sdl;
    sdl.reserve(points.size() + 1);
    for (const auto& p : points) {
        sdl.push_back(SDL_Point{ p.x, p.y });
    }
    sdl.push_back(sdl.front());
    SDL_RenderDrawLines(renderer_, sdl.data(), static_cast<int>(sdl.size()));
}

JKRenderBackend::TextureHandle JKSDLRenderBackend::CreateTargetTexture(int w, int h) {
    if (!renderer_ || w <= 0 || h <= 0) {
        return InvalidTexture;
    }
    SDL_Texture* tex = SDL_CreateTexture(renderer_,
                                         SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET,
                                         w, h);
    return tex;
}

void JKSDLRenderBackend::DestroyTexture(TextureHandle texture) {
    if (texture) {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(texture));
    }
}

void JKSDLRenderBackend::SetRenderTarget(TextureHandle texture) {
    if (!renderer_) return;
    SDL_SetRenderTarget(renderer_, static_cast<SDL_Texture*>(texture));
}

void JKSDLRenderBackend::BlitTexture(TextureHandle texture,
                                     const JKRect* src,
                                     const JKRect& dst,
                                     uint8_t alpha) {
    if (!renderer_ || !texture) return;
    SDL_Texture* tex = static_cast<SDL_Texture*>(texture);
    if (alpha < 255) {
        SDL_SetTextureAlphaMod(tex, alpha);
    }
    SDL_Rect dr = ToSDL(dst);
    SDL_Rect* srPtr = nullptr;
    SDL_Rect sr;
    if (src && !src->IsEmpty()) {
        sr = ToSDL(*src);
        srPtr = &sr;
    }
    SDL_RenderCopy(renderer_, tex, srPtr, &dr);
    if (alpha < 255) {
        SDL_SetTextureAlphaMod(tex, 255);
    }
}

} // namespace jk
