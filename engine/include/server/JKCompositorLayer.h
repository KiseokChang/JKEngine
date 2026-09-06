#ifndef JKCOMPOSITORLAYER_H
#define JKCOMPOSITORLAYER_H

#include <SDL.h>
#include <cstdint>
#include <string>
#include <string>

namespace jk {
namespace server {

// Server-side representation of one client surface in the compositor.
class JKCompositorLayer {
public:
    JKCompositorLayer(uint32_t id,
                      int width, int height,
                      const std::string& title)
        : id_(id), width_(width), height_(height), title_(title) {
    }

    uint32_t Id() const { return id_; }
    int Width() const { return width_; }
    int Height() const { return height_; }
    const std::string& Title() const { return title_; }

    int X() const { return x_; }
    int Y() const { return y_; }
    void SetPosition(int x, int y) { x_ = x; y_ = y; }

    float ScaleX() const { return scaleX_; }
    float ScaleY() const { return scaleY_; }
    void SetScale(float sx, float sy) { scaleX_ = sx; scaleY_ = sy; }

    uint8_t Alpha() const { return alpha_; }
    void SetAlpha(uint8_t a) { alpha_ = a; }

    bool IsDirty() const { return dirty_; }
    void MarkDirty() { dirty_ = true; }
    void ClearDirty() { dirty_ = false; }

    SDL_Texture* Texture() const { return texture_; }
    void SetTexture(SDL_Texture* t) { texture_ = t; }

    uint8_t* Pixels() const { return pixels_; }
    void SetPixels(uint8_t* p) { pixels_ = p; }

    bool IsVisible() const { return visible_; }
    void SetVisible(bool v) { visible_ = v; }

private:
    uint32_t id_ = 0;
    int width_ = 0;
    int height_ = 0;
    std::string title_;
    int x_ = 0;
    int y_ = 0;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    uint8_t alpha_ = 255;
    bool dirty_ = true;
    bool visible_ = true;
    SDL_Texture* texture_ = nullptr;
    uint8_t* pixels_ = nullptr;
};

} // namespace server
} // namespace jk

#endif // JKCOMPOSITORLAYER_H
