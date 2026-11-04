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
    // Load a PNG file (see JKImageLoader) and register it under `key`.
    // `path` is relative to the assets root, e.g. "assets/icons/mine@1x.png";
    // it is resolved against the exe directory, then the working directory.
    // Returns false (nothing registered) if the file is missing or undecodable.
    bool LoadImagePNG(const std::string& key, const std::string& path);
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

    // 렌더러 교체(클라 리사이즈 재결합 훅 — 8b81569 계열) 대응(2026-11-04):
    // SDL_DestroyRenderer는 렌더러에 얹혔던 **모든** 텍스처를 통째로 파괴하므
    // 로 캐시 핸들 = 죽은 지문(블릿 = 눈엔 아이콘 소실·UAF — DeX 최대화 후
    // 깃발 소실+클라 Segmentation fault 실측 원문). 수형 = 핸들을 버리고,
    // 업로드 성립 후에도 보존해 둔 소스(PendingUpload)를 다시 pending_으로
    // 옮겨 다음 FlushUploads가 재업로드하게 한다. pendingDestroys_(구 렌더러
    // 핸들의 파괴 큐)는 그 핸들이 이미 구 렌더러 소거 때 죽었으므로 새
    // 렌더러에서 건드리면 dangling 파괴 — 소각한다. 소자 큺 이유로 소스
    // 보존은 이 캐시의 아이콘 규모(16-96px)라 비용 미미(갤러리 썸네일은
    // 별도 LRU 소유).
    void ReinstateImages();

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
    // 업로드 성립 후에도 보존하는 소스(ReinstateImages 재업로드 원료 —
    // FlushUploads는 자유시키지 않고 소각만 한다).
    std::unordered_map<std::string, PendingUpload> retained_;
    std::vector<JKRenderBackend::TextureHandle> pendingDestroys_;

    std::unordered_map<std::string, HangulManager*> fonts_;
};

} // namespace jk

#endif // JKRESOURCECACHE_H
