#include <JKImageLoader.h>
#include <SDL.h>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

// T3 gallery thumbnail cache write (SaveImageFile) — the *only* shared
// definition outside jkwinserver's own static copy. STATIC keeps these
// symbols TU-local so jkwinserver (which links both this TU and
// JKWindowServer.cpp) never sees a duplicate symbol. App module TUs
// (jkapp_*.dll) must not redefine the implementation themselves.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

namespace jk {

bool LoadImageFile(const std::string& path, LoadedImage& out) {
    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!pixels) {
        std::fprintf(stderr, "JKImageLoader: failed to load '%s': %s\n",
                     path.c_str(), stbi_failure_reason());
        return false;
    }
    const size_t bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    out.rgba.assign(pixels, pixels + bytes);
    stbi_image_free(pixels);
    out.w = w;
    out.h = h;
    return true;
}

bool LoadImageMemory(const uint8_t* data, size_t size, LoadedImage& out) {
    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &channels, 4);
    if (!pixels) {
        std::fprintf(stderr, "JKImageLoader: failed to decode %zu bytes from memory: %s\n",
                     size, stbi_failure_reason());
        return false;
    }
    const size_t bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    out.rgba.assign(pixels, pixels + bytes);
    stbi_image_free(pixels);
    out.w = w;
    out.h = h;
    return true;
}

bool SaveImageFile(const std::string& path, const LoadedImage& img) {
    if (img.w <= 0 || img.h <= 0 || img.rgba.empty())
        return false;  // 퇴화 입력 — 호출부 placeholder 유지 계약
    return stbi_write_png(path.c_str(), img.w, img.h, 4, img.rgba.data(),
                          img.w * 4) != 0;
}

std::string ResolveAssetPath(const std::string& relative) {
    std::string exeRelative;
    if (char* basePath = SDL_GetBasePath()) {
        exeRelative = std::string(basePath) + relative;
        SDL_free(basePath);
    }

    if (FILE* f = std::fopen(exeRelative.c_str(), "rb")) {
        std::fclose(f);
        return exeRelative;
    }
    if (FILE* f = std::fopen(relative.c_str(), "rb")) {
        std::fclose(f);
        return relative;
    }
    return exeRelative.empty() ? relative : exeRelative;
}

} // namespace jk