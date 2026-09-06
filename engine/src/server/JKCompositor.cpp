#include <server/JKCompositor.h>

#include <algorithm>
#include <cstdio>

namespace jk {
namespace server {

JKCompositor::JKCompositor(SDL_Renderer* renderer) : renderer_(renderer) {
}

JKCompositor::~JKCompositor() {
    std::lock_guard<std::mutex> lock(layersMutex_);
    for (auto& layer : layers_) {
        if (layer && layer->Texture()) {
            SDL_DestroyTexture(layer->Texture());
            layer->SetTexture(nullptr);
        }
    }
    layers_.clear();
}

JKCompositorLayer* JKCompositor::AddLayer(uint32_t id,
                                            int width, int height,
                                            const std::string& title,
                                            uint8_t* pixels) {
    if (!renderer_ || width <= 0 || height <= 0) {
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer_,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             width, height);
    if (!texture) {
        std::fprintf(stderr, "JKCompositor::AddLayer: SDL_CreateTexture failed: %s\n",
                     SDL_GetError());
        return nullptr;
    }

    JKCompositorLayer* raw = nullptr;
    {
        std::lock_guard<std::mutex> lock(layersMutex_);
        auto layer = std::make_unique<JKCompositorLayer>(id, width, height, title);
        layer->SetTexture(texture);
        layer->SetPixels(pixels);
        layer->MarkDirty();
        raw = layer.get();
        layers_.push_back(std::move(layer));
    }
    // Focus is applied by the caller to avoid a nested layersMutex_ lock.
    return raw;
}

void JKCompositor::RemoveLayer(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    // Plain erase loop, NOT remove_if+erase: remove_if moves kept unique_ptrs
    // forward, leaving MOVED-FROM (null) unique_ptrs in the tail — the old
    // code then dereferenced (*it)->Texture() on a null element whenever the
    // removed layer was not the vector's last element (killing a non-focused
    // client, e.g. three at once from Task Manager, crashed the server).
    for (auto it = layers_.begin(); it != layers_.end(); ++it) {
        if (*it && (*it)->Id() == id) {
            if ((*it)->Texture()) {
                SDL_DestroyTexture((*it)->Texture());
            }
            layers_.erase(it);
            break;
        }
    }
    if (focusedId_ == id) {
        focusedId_ = 0;
    }
}

void JKCompositor::SetLayerPosition(uint32_t id, int x, int y) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        layer->SetPosition(x, y);
    }
}

void JKCompositor::SetLayerScale(uint32_t id, float sx, float sy) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        layer->SetScale(sx, sy);
    }
}

void JKCompositor::SetLayerAlpha(uint32_t id, uint8_t alpha) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        layer->SetAlpha(alpha);
    }
}

void JKCompositor::SetLayerShell(uint32_t id, bool shell) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        layer->SetShell(shell);
        // Keep the shell topmost right away (full sort policy in SortLayers).
        SortLayers();
    }
}

int JKCompositor::ShellReserveHeight() {
    std::lock_guard<std::mutex> lock(layersMutex_);
    for (auto& layer : layers_) {
        if (layer && layer->IsShell() && layer->IsVisible()) {
            return static_cast<int>(layer->Height() * layer->ScaleY());
        }
    }
    return 0;
}

void JKCompositor::SetLayerVisible(uint32_t id, bool visible) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        layer->SetVisible(visible);
    }
}

bool JKCompositor::IsLayerVisible(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    JKCompositorLayer* layer = FindLayer(id);
    return layer ? layer->IsVisible() : false;
}

bool JKCompositor::ResizeLayer(uint32_t id, int width, int height, uint8_t* pixels) {
    if (!renderer_ || width <= 0 || height <= 0) {
        return false;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer_,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             width, height);
    if (!texture) {
        std::fprintf(stderr, "JKCompositor::ResizeLayer: SDL_CreateTexture failed: %s\n",
                     SDL_GetError());
        return false;
    }
    std::lock_guard<std::mutex> lock(layersMutex_);
    JKCompositorLayer* layer = FindLayer(id);
    if (!layer) {
        SDL_DestroyTexture(texture);
        return false;
    }
    if (layer->Texture()) {
        SDL_DestroyTexture(layer->Texture());
    }
    layer->SetTexture(texture);
    layer->SetPixels(pixels);
    layer->SetSize(width, height);
    // The stretch preview during a resize drag uses Scale; reset it here so
    // the new texture is drawn at its native size.
    layer->SetScale(1.0f, 1.0f);
    layer->MarkDirty();
    return true;
}

void JKCompositor::MarkDirty(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        layer->MarkDirty();
    }
}

void JKCompositor::SetOutput(const JKCompositorOutput& output) {
    output_ = output;
}

void JKCompositor::FocusLayer(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (!FindLayer(id)) {
        return;
    }
    focusedId_ = id;
    SortLayers();
}

uint32_t JKCompositor::TopmostLayerId() {
    std::lock_guard<std::mutex> lock(layersMutex_);
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        // The shell never receives focus fallback (docs/28) — keyboard input
        // must stay with app windows.
        if (*it && (*it)->IsVisible() && !(*it)->IsShell()) {
            return (*it)->Id();
        }
    }
    return 0;
}

JKCompositorLayer* JKCompositor::FindLayer(uint32_t id) {
    for (auto& layer : layers_) {
        if (layer && layer->Id() == id) {
            return layer.get();
        }
    }
    return nullptr;
}

JKCompositorLayer* JKCompositor::FindLayerById(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    return FindLayer(id);
}

void JKCompositor::UpdateLayerTexture(JKCompositorLayer& layer) {
    if (!layer.Texture() || !layer.Pixels()) {
        return;
    }
    SDL_UpdateTexture(layer.Texture(), nullptr, layer.Pixels(),
                      layer.Width() * 4);
    layer.ClearDirty();
}

void JKCompositor::SortLayers() {
    // Stable sort: non-focused layers come before the focused layer so it is
    // rendered last (on top). Null entries are sorted to the back. Shell
    // layers (docs/28) outrank everything — the focused window never covers
    // the taskbar.
    std::stable_sort(layers_.begin(), layers_.end(),
        [this](const std::unique_ptr<JKCompositorLayer>& a,
               const std::unique_ptr<JKCompositorLayer>& b) {
            const bool aValid = a != nullptr;
            const bool bValid = b != nullptr;
            if (!aValid || !bValid) {
                return !aValid < bValid;
            }
            if (a->IsShell() != b->IsShell()) {
                return !a->IsShell() && b->IsShell();
            }
            const bool aFocused = (a->Id() == focusedId_);
            const bool bFocused = (b->Id() == focusedId_);
            return !aFocused && bFocused;
        });
}

void JKCompositor::Composite() {
    if (!renderer_) {
        return;
    }

    // The caller is responsible for clearing the framebuffer (e.g. with the
    // launcher/desktop background) before calling Composite(). We only draw
    // client layers on top so the background is preserved.
    //
    // All drawing is in physical pixels. Layer positions/sizes are stored in
    // SDL logical points, so we multiply by the output scale (physW/logW) to
    // get the physical-pixel destination rect. The mouse hit-test uses the
    // same physical-pixel space, so what you see is what you click.

    const float outputScale = output_.Scale();

    {
        std::lock_guard<std::mutex> lock(layersMutex_);
        for (auto& layer : layers_) {
            if (!layer || !layer->IsVisible() || !layer->Texture()) {
                continue;
            }
            if (layer->IsDirty()) {
                UpdateLayerTexture(*layer);
            }

            uint8_t alpha = layer->Alpha();
            SDL_SetTextureAlphaMod(layer->Texture(), alpha);

            SDL_Rect dst{
                static_cast<int>(layer->X() * outputScale),
                static_cast<int>(layer->Y() * outputScale),
                static_cast<int>(layer->Width() * layer->ScaleX() * outputScale),
                static_cast<int>(layer->Height() * layer->ScaleY() * outputScale)
            };
            SDL_RenderCopy(renderer_, layer->Texture(), nullptr, &dst);

            // The client paints the title bar inside its surface, but a
            // parentless main window paints no close button — the server
            // draws the close overlay at the top-right of every layer.
            // Shell layers have no chrome at all (docs/28).
            if (!layer->IsShell()) {
                DrawCloseOverlay(*layer, outputScale);
            }
        }
    }

    SDL_RenderPresent(renderer_);
}

void JKCompositor::DrawCloseOverlay(const JKCompositorLayer& layer, float scale) {
    if (!renderer_) {
        return;
    }
    // Mirrors JKWindow::GetCloseButtonRect / close-button painting
    // (src/JKWindow.cpp:171-229): 20x20 at 2px inset from the top-right,
    // grey fill, black outline, white X with a 5px pad.
    // The rect lives in SURFACE px like the chrome hit-test zones
    // (JKWindowServer::TryChromeGrab), so it shrinks proportionally on
    // fit-scaled layers — Width()*ScaleX() is the on-screen width.
    const SDL_Rect btn{
        static_cast<int>((layer.X() + (layer.Width() - kChromeCloseSize - kChromeCloseMargin) * layer.ScaleX()) * scale),
        static_cast<int>((layer.Y() + kChromeCloseMargin * layer.ScaleY()) * scale),
        static_cast<int>(kChromeCloseSize * layer.ScaleX() * scale),
        static_cast<int>(kChromeCloseSize * layer.ScaleY() * scale)
    };
    const int pad = static_cast<int>(5 * layer.ScaleX() * scale);

    SDL_SetRenderDrawColor(renderer_, 192, 192, 192, 255);
    SDL_RenderFillRect(renderer_, &btn);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer_, &btn);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer_, btn.x + pad, btn.y + pad,
                       btn.x + btn.w - pad - 1, btn.y + btn.h - pad - 1);
    SDL_RenderDrawLine(renderer_, btn.x + btn.w - pad - 1, btn.y + pad,
                       btn.x + pad, btn.y + btn.h - pad - 1);
}

JKCompositorLayer* JKCompositor::HitTest(int x, int y) {
    // (x, y) are physical client pixels. Layer positions/sizes are stored in
    // SDL logical points, so multiply by the output scale before comparing.
    const float s = output_.Scale();
    std::lock_guard<std::mutex> lock(layersMutex_);
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        JKCompositorLayer* layer = it->get();
        if (!layer || !layer->IsVisible()) {
            continue;
        }
        const int lx = static_cast<int>(layer->X() * s);
        const int ly = static_cast<int>(layer->Y() * s);
        const int w = static_cast<int>(layer->Width() * layer->ScaleX() * s);
        const int h = static_cast<int>(layer->Height() * layer->ScaleY() * s);
        if (x >= lx && x < lx + w &&
            y >= ly && y < ly + h) {
            return layer;
        }
    }
    return nullptr;
}

} // namespace server
} // namespace jk
