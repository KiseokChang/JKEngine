#ifndef JKRESOURCECACHE_H
#define JKRESOURCECACHE_H

#include <JKRenderBackend.h>
#include <JKTypes.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

struct SDL_Surface;

namespace jk {

class HangulManager;

// Simple image/font cache for the SDL2 prototype.
// Images are stored as render-target textures created from BMP files or
// in-memory RGBA buffers. Texture upload and destruction happen on the render
// thread via FlushUploads(); the application thread only stores CPU-side data.
class JKResourceCache {
public:
    explicit JKResourceCache(JKRenderBackend* backend);
    ~JKResourceCache();

    // Images -----------------------------------------------------------------
    // Load a BMP file on the calling thread (no SDL renderer access).
    bool LoadImageBMP(const std::string& key, const std::string& path);
    // Register a generated RGBA buffer for upload on the render thread.
    bool CreateImageFromRGBA(const std::string& key,
                             int w, int h,
                             const std::vector<uint8_t>& rgba);
    bool HasImage(const std::string& key) const;
    JKRenderBackend::TextureHandle GetImage(const std::string& key) const;
    JKPoint GetImageSize(const std::string& key) const;
    void UnloadImage(const std::string& key);
    void UnloadAllImages();

    // Called from the render thread. Creates any pending textures and destroys
    // textures that were queued for removal from other threads.
    void FlushUploads(JKRenderBackend* backend);

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

    struct PendingUpload {
        // For BMP loads the SDL_Surface is owned here until upload.
        SDL_Surface* surface = nullptr;
        // For generated RGBA icons the pixel buffer is owned here.
        std::vector<uint8_t> rgba;
        int w = 0;
        int h = 0;
    };

    JKRenderBackend* backend_ = nullptr;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, CachedImage> images_;
    std::unordered_map<std::string, PendingUpload> pending_;
    std::vector<JKRenderBackend::TextureHandle> pendingDestroys_;

    std::unordered_map<std::string, HangulManager*> fonts_;
};

} // namespace jk

#endif // JKRESOURCECACHE_H
