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
#if defined(__ANDROID__)
    // jkdesktop-apk — r30 실측 원문: SDL_SW 렌더러의 TARGET 텍스처 경로(SRT rc=0
    // +fmt RGBA8888 정상)에서 그림이 미착지(계량기 전 시리즈). 클라 렌더를
    // 윈도우 표면 일직선으로 우회: 타깃을 생성하지 않고 더미 핸들만 돌려
    // 상위 파이프라인을 산다(SetRenderTarget도 NO-OP — 아래).
    return reinterpret_cast<TextureHandle>(static_cast<uintptr_t>(1));
#endif
    SDL_Texture* tex = SDL_CreateTexture(renderer_,
                                         SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET,
                                         w, h);
    return tex;
}

void JKSDLRenderBackend::DestroyTexture(TextureHandle texture) {
    if (texture) {
#if defined(__ANDROID__)
        if (reinterpret_cast<uintptr_t>(texture) == 1u) return;  // 더미 핸들
#endif
        SDL_DestroyTexture(static_cast<SDL_Texture*>(texture));
    }
}

void JKSDLRenderBackend::SetRenderTarget(TextureHandle texture) {
    if (!renderer_) return;
#if defined(__ANDROID__)
    // jkdesktop-apk — 더미 핸들(윈도우 표면 우회)이 들어오면 바인딩을 무시한다
    // — SW 렌더러가 기본 결합 상태(윈도우 표면)로 그리고 ReadPixels가 같은
    // 표면을 읽는다(r30 원문 — SDL SW 타깃 경로 미착지 회피).
    if (reinterpret_cast<uintptr_t>(texture) == 1u) return;
#endif
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
