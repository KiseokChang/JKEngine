#ifndef JKRENDERCOMMANDLIST_H
#define JKRENDERCOMMANDLIST_H

#include <JKRenderBackend.h>
#include <JKTypes.h>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>

namespace jk {

// Thread-safe intermediate representation of a single frame's rendering.
// JKApplication paints the window hierarchy into a JKRenderCommandList on the
// application thread; the serialized command list is then replayed by
// JKRenderThread on the SDL renderer thread.
class JKRenderCommandList : public JKRenderBackend {
public:
    JKRenderCommandList();
    ~JKRenderCommandList() override = default;

    // JKRenderBackend interface (records commands instead of executing them).
    void* GetNativeHandle() const override;
    void SetScale(float sx, float sy) override;
    void GetOutputSize(int& w, int& h) override;
    void SetDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) override;
    void Clear() override;
    void Present() override;
    void SetClipRect(const JKRect* rect) override;
    void DrawRect(const JKRect& rect) override;
    void FillRect(const JKRect& rect) override;
    void DrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2) override;
    void DrawPixel(int32_t x, int32_t y) override;
    void DrawPoints(const JKPoint* points, size_t count) override;
    void DrawPolygon(const std::vector<JKPoint>& points) override;
    TextureHandle CreateTargetTexture(int w, int h) override;
    void DestroyTexture(TextureHandle texture) override;
    void SetRenderTarget(TextureHandle texture) override;
    void BlitTexture(TextureHandle texture,
                     const JKRect* src,
                     const JKRect& dst,
                     uint8_t alpha = 255) override;

    // Encode the recorded commands into a byte buffer for the message bus.
    std::vector<uint8_t> Serialize() const;

    // Decode a byte buffer back into a command list for replay.
    static std::unique_ptr<JKRenderCommandList> Deserialize(
        const std::vector<uint8_t>& data);

    // Execute the recorded commands against a real backend.
    void Replay(JKRenderBackend* target) const;

private:
    enum class Type : uint8_t {
        SetScale,
        SetClipRect,
        SetDrawColor,
        Clear,
        Present,
        DrawRect,
        FillRect,
        DrawLine,
        DrawPixel,
        DrawPoints,
        DrawPolygon,
        BlitTexture
    };

    struct Command {
        Type type = Type::DrawPixel;
        uint8_t r = 0, g = 0, b = 0, a = 255;
        float sx = 1.0f, sy = 1.0f;
        int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        JKRect rect;
        JKRect srcRect;
        bool hasSrcRect = false;
        uint8_t alpha = 255;
        uint64_t texture = 0; // uintptr_t encoded for serialization.
        size_t pointOffset = 0;
        size_t pointCount = 0;
    };

    std::vector<Command> commands_;
    std::vector<JKPoint> points_;

    void Append(const Command& cmd);
    void AppendPoints(const JKPoint* points, size_t count, size_t& offset);
};

} // namespace jk

#endif // JKRENDERCOMMANDLIST_H
