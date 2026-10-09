#ifndef JKIMAGELOADER_H
#define JKIMAGELOADER_H

#include <cstdint>
#include <string>
#include <vector>

namespace jk {

// Decoded RGBA8 image (row-major, w*h*4 bytes).
struct LoadedImage {
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
};

// Decode a PNG (or JPEG) file from disk into RGBA8 via stb_image.
// Returns false if the file cannot be read or decoded.
bool LoadImageFile(const std::string& path, LoadedImage& out);

// Decode an in-memory PNG/JPEG (e.g. an ICON entry of a .jkx container).
bool LoadImageMemory(const uint8_t* data, size_t size, LoadedImage& out);

// Encode RGBA8 to PNG via stb_image_write (T3 gallery thumbnail disk cache).
// The stb_image_write implementation lives here (jkcore) — define it once with
// STB_IMAGE_WRITE_STATIC so every binary that links jkcore gets a private
// copy; jkwinserver's own static definition (screenshot path) stays untouched
// and no app module TU may redefine the implementation (link breakage).
// Returns false on degenerate input (w/h <= 0 or empty pixels) or encode error.
bool SaveImageFile(const std::string& path, const LoadedImage& img);

// Resolve an "assets/..." style relative path: first against the executable
// directory (SDL_GetBasePath), then the current working directory. Returns
// the first candidate that exists, or the exe-relative candidate so error
// messages stay meaningful.
std::string ResolveAssetPath(const std::string& relative);

} // namespace jk

#endif // JKIMAGELOADER_H