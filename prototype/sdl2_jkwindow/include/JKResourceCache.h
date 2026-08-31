#ifndef JKRESOURCECACHE_H
#define JKRESOURCECACHE_H

#include <JKRenderBackend.h>
#include <JKTypes.h>
#include <string>
#include <unordered_map>

namespace jk {

class HangulManager;

// Simple image/font cache for the SDL2 prototype.
// Images are stored as render-target textures created from BMP files.
// Fonts are stored as non-owning HangulManager* pointers.
class JKResourceCache {
public:
    explicit JKResourceCache(JKRenderBackend* backend);
    ~JKResourceCache();

    // Images -----------------------------------------------------------------
    bool LoadImageBMP(const std::string& key, const std::string& path);
    bool HasImage(const std::string& key) const;
    JKRenderBackend::TextureHandle GetImage(const std::string& key) const;
    JKPoint GetImageSize(const std::string& key) const;
    // Create a texture from an in-memory RGBA buffer and cache it.
    bool CreateImageFromRGBA(const std::string& key,
                             int w, int h,
                             const std::vector<uint8_t>& rgba);
    void UnloadImage(const std::string& key);
    void UnloadAllImages();

    // Fonts ------------------------------------------------------------------
    void RegisterFont(const std::string& key, HangulManager* manager);
    bool HasFont(const std::string& key) const;
    HangulManager* GetFont(const std::string& key) const;
    void UnregisterFont(const std::string& key);
    void UnregisterAllFonts();

private:
    struct CachedImage {
        JKRenderBackend::TextureHandle texture = JKRenderBackend::InvalidTexture;
        int w = 0;
        int h = 0;
    };

    JKRenderBackend* backend_ = nullptr;
    std::unordered_map<std::string, CachedImage> images_;
    std::unordered_map<std::string, HangulManager*> fonts_;
};

} // namespace jk

#endif // JKRESOURCECACHE_H
