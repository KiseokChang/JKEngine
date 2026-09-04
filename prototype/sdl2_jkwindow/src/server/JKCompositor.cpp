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

    std::lock_guard<std::mutex> lock(layersMutex_);
    auto layer = std::make_unique<JKCompositorLayer>(id, width, height, title);
    layer->SetTexture(texture);
    layer->SetPixels(pixels);
    layer->MarkDirty();
    auto* raw = layer.get();
    layers_.push_back(std::move(layer));
    FocusLayer(id);
    return raw;
}

void JKCompositor::RemoveLayer(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    auto it = std::remove_if(layers_.begin(), layers_.end(),
                             [id](const std::unique_ptr<JKCompositorLayer>& layer) {
        return layer && layer->Id() == id;
    });
    if (it != layers_.end()) {
        if ((*it)->Texture()) {
            SDL_DestroyTexture((*it)->Texture());
        }
        layers_.erase(it, layers_.end());
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

JKCompositorLayer* JKCompositor::FindLayer(uint32_t id) {
    for (auto& layer : layers_) {
        if (layer && layer->Id() == id) {
            return layer.get();
        }
    }
    return nullptr;
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
    // Stable sort: focused layer moved to the back (rendered last / on top).
    std::stable_sort(layers_.begin(), layers_.end(),
        [this](const std::unique_ptr<JKCompositorLayer>& a,
               const std::unique_ptr<JKCompositorLayer>& b) {
            if (a && b) {
                if (a->Id() == focusedId_) return false;
                if (b->Id() == focusedId_) return true;
            }
            return false;
        });
}

void JKCompositor::Composite() {
    if (!renderer_) {
        return;
    }

    SDL_SetRenderDrawColor(renderer_, 64, 64, 64, 255);
    SDL_RenderClear(renderer_);

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
                layer->X(),
                layer->Y(),
                static_cast<int>(layer->Width() * layer->ScaleX()),
                static_cast<int>(layer->Height() * layer->ScaleY())
            };
            SDL_RenderCopy(renderer_, layer->Texture(), nullptr, &dst);
        }
    }

    SDL_RenderPresent(renderer_);
}

JKCompositorLayer* JKCompositor::HitTest(int x, int y) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        JKCompositorLayer* layer = it->get();
        if (!layer || !layer->IsVisible()) {
            continue;
        }
        const int w = static_cast<int>(layer->Width() * layer->ScaleX());
        const int h = static_cast<int>(layer->Height() * layer->ScaleY());
        if (x >= layer->X() && x < layer->X() + w &&
            y >= layer->Y() && y < layer->Y() + h) {
            return layer;
        }
    }
    return nullptr;
}

} // namespace server
} // namespace jk
