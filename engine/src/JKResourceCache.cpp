#include <JKResourceCache.h>
#include <JKImageLoader.h>
#include <JKHangulManager.h>
#include <SDL.h>
#include <cstdio>
#include <cstring>

namespace jk {

JKResourceCache::JKResourceCache(JKRenderBackend* backend) : backend_(backend) {
}

JKResourceCache::~JKResourceCache() {
    bool hasWork = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        hasWork = !images_.empty() || !pending_.empty() ||
                  !retained_.empty() || !pendingDestroys_.empty();
    }
    if (hasWork) {
        FlushUploads(backend_);
    }
    // 보존 소스의 소유 SDL_Surface 자유(rgba는 vector 자유) — 소유자 재확정.
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : retained_) {
        if (kv.second.surface) {
            SDL_FreeSurface(kv.second.surface);
        }
    }
    retained_.clear();
}

bool JKResourceCache::LoadImageBMP(const std::string& key, const std::string& path) {
    SDL_Surface* surface = SDL_LoadBMP(path.c_str());
    if (!surface) {
        std::fprintf(stderr, "JKResourceCache: failed to load BMP '%s': %s\n",
                     path.c_str(), SDL_GetError());
        return false;
    }

    UnloadImage(key);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        PendingUpload pending;
        pending.surface = surface;
        pending.w = surface->w;
        pending.h = surface->h;
        pending_[key] = std::move(pending);
        images_[key] = CachedImage{ JKRenderBackend::InvalidTexture, surface->w, surface->h };
    }
    return true;
}

bool JKResourceCache::LoadImagePNG(const std::string& key, const std::string& path) {
    LoadedImage img;
    if (!LoadImageFile(ResolveAssetPath(path), img)) {
        return false;
    }
    return CreateImageFromRGBA(key, img.w, img.h, img.rgba);
}

bool JKResourceCache::CreateImageFromRGBA(const std::string& key,
                                          int w, int h,
                                          const std::vector<uint8_t>& rgba) {
    if (!backend_ || w <= 0 || h <= 0 ||
        static_cast<size_t>(w * h * 4) != rgba.size()) {
        return false;
    }

    UnloadImage(key);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        PendingUpload pending;
        pending.rgba = rgba;
        pending.w = w;
        pending.h = h;
        pending_[key] = std::move(pending);
        images_[key] = CachedImage{ JKRenderBackend::InvalidTexture, w, h };
    }
    return true;
}

bool JKResourceCache::HasImage(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return images_.find(key) != images_.end();
}

JKRenderBackend::TextureHandle JKResourceCache::GetImage(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = images_.find(key);
    return (it != images_.end()) ? it->second.texture : JKRenderBackend::InvalidTexture;
}

JKPoint JKResourceCache::GetImageSize(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = images_.find(key);
    if (it != images_.end()) {
        return JKPoint{ it->second.w, it->second.h };
    }
    return JKPoint{ 0, 0 };
}

void JKResourceCache::UnloadImage(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = images_.find(key);
    if (it != images_.end()) {
        if (it->second.texture != JKRenderBackend::InvalidTexture) {
            pendingDestroys_.push_back(it->second.texture);
        }
        images_.erase(it);
    }
    auto pit = pending_.find(key);
    if (pit != pending_.end()) {
        if (pit->second.surface) {
            SDL_FreeSurface(pit->second.surface);
        }
        pending_.erase(pit);
    }
    auto rit = retained_.find(key);
    if (rit != retained_.end()) {
        if (rit->second.surface) {
            SDL_FreeSurface(rit->second.surface);
        }
        retained_.erase(rit);
    }
}

void JKResourceCache::UnloadAllImages() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : images_) {
        if (kv.second.texture != JKRenderBackend::InvalidTexture) {
            pendingDestroys_.push_back(kv.second.texture);
        }
    }
    images_.clear();
    for (auto& kv : pending_) {
        if (kv.second.surface) {
            SDL_FreeSurface(kv.second.surface);
        }
    }
    pending_.clear();
    for (auto& kv : retained_) {
        if (kv.second.surface) {
            SDL_FreeSurface(kv.second.surface);
        }
    }
    retained_.clear();
}

void JKResourceCache::ReinstateImages() {
    std::lock_guard<std::mutex> lock(mutex_);
    // pendingDestroys_ = 구 렌더러 핸들의 해제 큐 — 구 렌더러 소거가 텍스처를
    // 이미 파괴했으므로(포인터는 이미 자유상태) 여기서 건드리면 dangling
    // 파괴 = UAF.
    pendingDestroys_.clear();
    for (auto& kv : images_) {
        auto rit = retained_.find(kv.first);
        if (rit == retained_.end()) continue;  // 소스 부재 — 재업로드 불가(스킵)
        pending_[kv.first] = std::move(rit->second);
        retained_.erase(rit);
    }
    retained_.clear();
    // images_의 핸들 = 죽은 지문 — flush 전 타 경로의 BlitTexture가 죽은
    // 핸들을 쓰지 않게 무효화한다(GetImage가 InvalidTexture를 주면 블릿
    // no-op — 실그리기는 flush 후 복원).
    for (auto& kv : images_) {
        kv.second.texture = JKRenderBackend::InvalidTexture;
    }
}

void JKResourceCache::FlushUploads(JKRenderBackend* backend) {
    if (!backend) return;

    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(backend->GetNativeHandle());
    if (!renderer) return;

    std::lock_guard<std::mutex> lock(mutex_);

    for (auto handle : pendingDestroys_) {
        backend->DestroyTexture(handle);
    }
    pendingDestroys_.clear();

    std::vector<std::string> succeeded;
    for (auto& kv : pending_) {
        SDL_Texture* texture = nullptr;
        if (kv.second.surface) {
            texture = SDL_CreateTextureFromSurface(renderer, kv.second.surface);
            // ReinstateImages 소스 보존(2026-11-04 — 렌더러 교체 재업로드
            // 원료): 구 계약(업로드 즉시 SDL_FreeSurface)의 소비를 철수 —
            // surface는 retained_로 이동해 다음 재결합 플러시가 다시 쓴다.
        } else if (!kv.second.rgba.empty()) {
            SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
                const_cast<uint8_t*>(kv.second.rgba.data()),
                kv.second.w, kv.second.h, 32, kv.second.w * 4,
                0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
            if (surface) {
                texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
            }
        }

        if (!texture) {
            std::fprintf(stderr, "JKResourceCache: failed to upload '%s': %s\n",
                         kv.first.c_str(), SDL_GetError());
            continue;  // 비성립은 pending_에 남는다 — 다음 flush 재시도(기존)
        }

        CachedImage& img = images_[kv.first];
        img.texture = texture;
        SDL_QueryTexture(texture, nullptr, nullptr, &img.w, &img.h);
        succeeded.push_back(kv.first);
    }
    // 소스 보존 이동 — 언맵 순회 중 erase 방지(UB 아님을 두 번째 패스로 보장).
    for (const std::string& key : succeeded) {
        retained_[key] = std::move(pending_[key]);
        pending_.erase(key);
    }
}

void JKResourceCache::RegisterFont(const std::string& key, HangulManager* manager) {
    std::lock_guard<std::mutex> lock(mutex_);
    fonts_[key] = manager;
}

bool JKResourceCache::HasFont(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fonts_.find(key) != fonts_.end();
}

HangulManager* JKResourceCache::GetFont(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fonts_.find(key);
    return (it != fonts_.end()) ? it->second : nullptr;
}

void JKResourceCache::UnregisterFont(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    fonts_.erase(key);
}

void JKResourceCache::UnregisterAllFonts() {
    std::lock_guard<std::mutex> lock(mutex_);
    fonts_.clear();
}

} // namespace jk
