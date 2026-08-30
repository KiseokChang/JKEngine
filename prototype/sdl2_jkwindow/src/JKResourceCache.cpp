#include <JKResourceCache.h>
#include <JKHangulManager.h>
#include <SDL.h>
#include <cstdio>

namespace jk {

JKResourceCache::JKResourceCache(JKRenderBackend* backend) : backend_(backend) {
}

JKResourceCache::~JKResourceCache() {
    UnloadAllImages();
    UnregisterAllFonts();
}

bool JKResourceCache::LoadImageBMP(const std::string& key, const std::string& path) {
    if (!backend_) return false;
    UnloadImage(key);

    SDL_Surface* surface = SDL_LoadBMP(path.c_str());
    if (!surface) {
        std::fprintf(stderr, "JKResourceCache: failed to load BMP '%s': %s\n",
                     path.c_str(), SDL_GetError());
        return false;
    }

    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(backend_->GetNativeHandle());
    if (!renderer) {
        SDL_FreeSurface(surface);
        return false;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        std::fprintf(stderr, "JKResourceCache: failed to create texture for '%s': %s\n",
                     path.c_str(), SDL_GetError());
        return false;
    }

    CachedImage img;
    img.texture = texture;
    SDL_QueryTexture(texture, nullptr, nullptr, &img.w, &img.h);
    images_[key] = img;
    return true;
}

bool JKResourceCache::HasImage(const std::string& key) const {
    return images_.find(key) != images_.end();
}

JKRenderBackend::TextureHandle JKResourceCache::GetImage(const std::string& key) const {
    auto it = images_.find(key);
    return (it != images_.end()) ? it->second.texture : JKRenderBackend::InvalidTexture;
}

void JKResourceCache::UnloadImage(const std::string& key) {
    auto it = images_.find(key);
    if (it != images_.end()) {
        if (it->second.texture != JKRenderBackend::InvalidTexture && backend_) {
            backend_->DestroyTexture(it->second.texture);
        }
        images_.erase(it);
    }
}

void JKResourceCache::UnloadAllImages() {
    if (backend_) {
        for (auto& kv : images_) {
            if (kv.second.texture != JKRenderBackend::InvalidTexture) {
                backend_->DestroyTexture(kv.second.texture);
            }
        }
    }
    images_.clear();
}

void JKResourceCache::RegisterFont(const std::string& key, HangulManager* manager) {
    fonts_[key] = manager;
}

bool JKResourceCache::HasFont(const std::string& key) const {
    return fonts_.find(key) != fonts_.end();
}

HangulManager* JKResourceCache::GetFont(const std::string& key) const {
    auto it = fonts_.find(key);
    return (it != fonts_.end()) ? it->second : nullptr;
}

void JKResourceCache::UnregisterFont(const std::string& key) {
    fonts_.erase(key);
}

void JKResourceCache::UnregisterAllFonts() {
    fonts_.clear();
}

} // namespace jk
