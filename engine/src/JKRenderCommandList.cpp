#include <JKRenderCommandList.h>
#include <cstring>

namespace jk {

namespace {

template <typename T>
void AppendBytes(std::vector<uint8_t>& out, const T* data, size_t count) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + count * sizeof(T));
}

template <typename T>
bool ReadBytes(const uint8_t*& p, size_t& remaining, T* out, size_t count) {
    const size_t needed = count * sizeof(T);
    if (remaining < needed) return false;
    std::memcpy(out, p, needed);
    p += needed;
    remaining -= needed;
    return true;
}

} // anonymous namespace

JKRenderCommandList::JKRenderCommandList() = default;

void* JKRenderCommandList::GetNativeHandle() const {
    return nullptr;
}

void JKRenderCommandList::SetScale(float sx, float sy) {
    (void)sx;
    (void)sy;
    // Scale is applied by the render thread based on the logical/physical size,
    // so application-side SetScale calls are intentionally not recorded.
}

void JKRenderCommandList::GetOutputSize(int& w, int& h) {
    w = 0;
    h = 0;
}

void JKRenderCommandList::SetDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Command cmd;
    cmd.type = Type::SetDrawColor;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.a = a;
    Append(cmd);
}

void JKRenderCommandList::Clear() {
    Command cmd;
    cmd.type = Type::Clear;
    Append(cmd);
}

void JKRenderCommandList::Present() {
    // Present is executed by the render thread after replaying the frame.
}

void JKRenderCommandList::SetClipRect(const JKRect* rect) {
    Command cmd;
    cmd.type = Type::SetClipRect;
    cmd.hasSrcRect = rect != nullptr;
    if (rect) {
        cmd.rect = *rect;
    }
    Append(cmd);
}

void JKRenderCommandList::DrawRect(const JKRect& rect) {
    Command cmd;
    cmd.type = Type::DrawRect;
    cmd.rect = rect;
    Append(cmd);
}

void JKRenderCommandList::FillRect(const JKRect& rect) {
    Command cmd;
    cmd.type = Type::FillRect;
    cmd.rect = rect;
    Append(cmd);
}

void JKRenderCommandList::DrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    Command cmd;
    cmd.type = Type::DrawLine;
    cmd.x1 = x1;
    cmd.y1 = y1;
    cmd.x2 = x2;
    cmd.y2 = y2;
    Append(cmd);
}

void JKRenderCommandList::DrawPixel(int32_t x, int32_t y) {
    Command cmd;
    cmd.type = Type::DrawPixel;
    cmd.x1 = x;
    cmd.y1 = y;
    Append(cmd);
}

void JKRenderCommandList::DrawPoints(const JKPoint* points, size_t count) {
    if (!points || count == 0) return;
    Command cmd;
    cmd.type = Type::DrawPoints;
    AppendPoints(points, count, cmd.pointOffset);
    cmd.pointCount = count;
    Append(cmd);
}

void JKRenderCommandList::DrawPolygon(const std::vector<JKPoint>& points) {
    if (points.size() < 2) return;
    Command cmd;
    cmd.type = Type::DrawPolygon;
    AppendPoints(points.data(), points.size(), cmd.pointOffset);
    cmd.pointCount = points.size();
    Append(cmd);
}

JKRenderBackend::TextureHandle JKRenderCommandList::CreateTargetTexture(int w, int h) {
    (void)w;
    (void)h;
    // Offscreen targets are not supported through the command list in Phase 1.
    return InvalidTexture;
}

void JKRenderCommandList::DestroyTexture(TextureHandle texture) {
    (void)texture;
}

void JKRenderCommandList::SetRenderTarget(TextureHandle texture) {
    (void)texture;
}

void JKRenderCommandList::BlitTexture(TextureHandle texture,
                                     const JKRect* src,
                                     const JKRect& dst,
                                     uint8_t alpha) {
    if (!texture || dst.IsEmpty()) return;
    Command cmd;
    cmd.type = Type::BlitTexture;
    cmd.texture = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(texture));
    cmd.rect = dst;
    cmd.hasSrcRect = src != nullptr;
    if (src) {
        cmd.srcRect = *src;
    }
    cmd.alpha = alpha;
    Append(cmd);
}

std::vector<uint8_t> JKRenderCommandList::Serialize() const {
    std::vector<uint8_t> out;
    const uint32_t cmdCount = static_cast<uint32_t>(commands_.size());
    AppendBytes(out, &cmdCount, 1);
    if (!commands_.empty()) {
        AppendBytes(out, commands_.data(), commands_.size());
    }
    const uint32_t pointCount = static_cast<uint32_t>(points_.size());
    AppendBytes(out, &pointCount, 1);
    if (!points_.empty()) {
        AppendBytes(out, points_.data(), points_.size());
    }
    return out;
}

std::unique_ptr<JKRenderCommandList> JKRenderCommandList::Deserialize(
    const std::vector<uint8_t>& data) {
    if (data.empty()) return nullptr;

    const uint8_t* p = data.data();
    size_t remaining = data.size();

    uint32_t cmdCount = 0;
    if (!ReadBytes(p, remaining, &cmdCount, 1)) return nullptr;

    auto list = std::make_unique<JKRenderCommandList>();
    if (cmdCount > 0) {
        list->commands_.resize(cmdCount);
        if (!ReadBytes(p, remaining, list->commands_.data(), cmdCount)) {
            return nullptr;
        }
    }

    uint32_t pointCount = 0;
    if (!ReadBytes(p, remaining, &pointCount, 1)) return nullptr;
    if (pointCount > 0) {
        list->points_.resize(pointCount);
        if (!ReadBytes(p, remaining, list->points_.data(), pointCount)) {
            return nullptr;
        }
    }

    return list;
}

void JKRenderCommandList::Replay(JKRenderBackend* target) const {
    if (!target) return;

    for (const auto& cmd : commands_) {
        switch (cmd.type) {
            case Type::SetScale:
                target->SetScale(cmd.sx, cmd.sy);
                break;
            case Type::SetClipRect:
                if (cmd.hasSrcRect) {
                    target->SetClipRect(&cmd.rect);
                } else {
                    target->SetClipRect(nullptr);
                }
                break;
            case Type::SetDrawColor:
                target->SetDrawColor(cmd.r, cmd.g, cmd.b, cmd.a);
                break;
            case Type::Clear:
                target->Clear();
                break;
            case Type::Present:
                target->Present();
                break;
            case Type::DrawRect:
                target->DrawRect(cmd.rect);
                break;
            case Type::FillRect:
                target->FillRect(cmd.rect);
                break;
            case Type::DrawLine:
                target->DrawLine(cmd.x1, cmd.y1, cmd.x2, cmd.y2);
                break;
            case Type::DrawPixel:
                target->DrawPixel(cmd.x1, cmd.y1);
                break;
            case Type::DrawPoints:
            case Type::DrawPolygon: {
                if (cmd.pointOffset + cmd.pointCount <= points_.size()) {
                    const JKPoint* pts = &points_[cmd.pointOffset];
                    if (cmd.type == Type::DrawPoints) {
                        target->DrawPoints(pts, cmd.pointCount);
                    } else {
                        std::vector<JKPoint> poly(pts, pts + cmd.pointCount);
                        target->DrawPolygon(poly);
                    }
                }
                break;
            }
            case Type::BlitTexture: {
                auto* tex = reinterpret_cast<TextureHandle>(
                    static_cast<uintptr_t>(cmd.texture));
                if (cmd.hasSrcRect) {
                    target->BlitTexture(tex, &cmd.srcRect, cmd.rect, cmd.alpha);
                } else {
                    target->BlitTexture(tex, nullptr, cmd.rect, cmd.alpha);
                }
                break;
            }
        }
    }
}

void JKRenderCommandList::Append(const Command& cmd) {
    commands_.push_back(cmd);
}

void JKRenderCommandList::AppendPoints(const JKPoint* points, size_t count,
                                      size_t& offset) {
    offset = points_.size();
    points_.reserve(offset + count);
    for (size_t i = 0; i < count; ++i) {
        points_.push_back(points[i]);
    }
}

} // namespace jk
