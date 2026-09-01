#include <JKResourceCache.h>
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
        hasWork = !images_.empty() || !pending_.empty() || !pendingDestroys_.empty();
    }
    if (hasWork) {
        FlushUploads(backend_);
    }
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

    for (auto& kv : pending_) {
        SDL_Texture* texture = nullptr;
        if (kv.second.surface) {
            texture = SDL_CreateTextureFromSurface(renderer, kv.second.surface);
            SDL_FreeSurface(kv.second.surface);
            kv.second.surface = nullptr;
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
            continue;
        }

        CachedImage& img = images_[kv.first];
        img.texture = texture;
        SDL_QueryTexture(texture, nullptr, nullptr, &img.w, &img.h);
    }
    pending_.clear();
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
