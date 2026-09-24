#ifndef JKSCRIPTCANVAS_H
#define JKSCRIPTCANVAS_H

// Retained-mode drawing canvas for script apps (docs/60 §10 — 캔버스 API v5,
// 게임·토이 층). The script draws through the host bindings
// (createCanvas/canvasClear/canvasRect/...); each call appends an Op that
// OnPaintClient replays with the screen-rect origin offset. The control owns
// its scene so repaints reproduce it — the same retained contract JKButton's
// OnPaintClient follows.
//
// Input: the canvas is focusable. Mouse events land here via the window's
// HitTest routing (MouseDown takes focus + capture so moves keep arriving
// during a drag — JKButton precedent); key/wheel events arrive through the
// focusChild_ route (JKWindow::RespondMessage). Events are translated to
// canvas-local pixels and handed to the input sink — JKScriptHost turns them
// into onMouse/onWheel/onKey calls.
//
// Threading: draw calls and paints both run on the main/UI thread (docs/27
// §3.2) — no locking.

#include <JKControl.h>
#include <JKEvent.h>
#include <functional>
#include <string>
#include <vector>

namespace jk {

class JKScriptCanvas : public JKControl {
public:
    // kind: 0=down, 1=up, 2=move, 3=wheel (detail = wheel delta, dy>0 up),
    // 4=KeyDown/5=KeyUp (detail = SDL keycode; x/y unused). Coordinates are
    // canvas-local pixels; key events carry no coordinates.
    struct InputEvent {
        int kind = 0;      // 0 down, 1 up, 2 move, 3 wheel
        int32_t x = 0;
        int32_t y = 0;
        int32_t detail = 0;
        int32_t button = 0;  // down/up: SDL button (1 left, 2 middle, 3 right;
                             // ev.detail convention). 0 on move/wheel/key —
                             // 2026-09-24 폰 실전: 좌/우 구분 API 부재로 LLM이
                             // 6턴 소모 (onMouse 5번째 인자로 노출).
    };
    using InputSink = std::function<void(const InputEvent&)>;

    JKScriptCanvas(const JKRect& rect, uint16_t controlId);

    // --- drawing (retained op list) ---------------------------------------
    void Clear(uint8_t r, uint8_t g, uint8_t b);
    void DrawRectOp(int32_t x, int32_t y, int32_t w, int32_t h,
                    uint8_t r, uint8_t g, uint8_t b, bool filled);
    void DrawPixel(int32_t x, int32_t y, uint8_t r, uint8_t g, uint8_t b);
    void DrawLineOp(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                    uint8_t r, uint8_t g, uint8_t b);
    void DrawCircle(int32_t cx, int32_t cy, int32_t radius,
                    uint8_t r, uint8_t g, uint8_t b, bool filled);
    void DrawText(int32_t x, int32_t y, const std::string& kssmText,
                  uint8_t r, uint8_t g, uint8_t b);

    void SetInputSink(InputSink sink) { sink_ = std::move(sink); }
    void ClearInputSink() { sink_ = nullptr; }

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

    // Ops beyond this cap evict the oldest (ring behavior — trail scripts
    // that never canvasClear keep rendering their newest ops; 2026-09-24 폰
    // 실전에서 drop-new가 잔상을 죽이는 함정으로 확인). Animations still keep
    // the list bounded by canvasClear() per frame (docs/60 §10).
    static constexpr size_t kMaxOps = 4096;

private:
    struct Op {
        uint8_t kind = 0;  // 0 rect, 1 pixel, 2 line, 3 circle, 4 text
        JKRect rect;       // rect / pixel point / line endpoints (x1,y1,x2,y2)
        JKPoint point{};   // circle center / text origin
        int32_t radius = 0;
        bool filled = false;
        uint8_t cr = 0, cg = 0, cb = 0;
        std::string text;  // KSSM-encoded (docs/60: 위젯 텍스트 경로)
    };

    void DispatchLocal(const JKEvent& ev, int kind);
    JKPoint ToLocal(int32_t screenX, int32_t screenY) const;
    void AddOp(Op op);

    std::vector<Op> ops_;
    InputSink sink_;
    JKPoint lastMouse_{};  // local coords (wheel events carry no coordinates)
    bool warnedCap_ = false;
};

} // namespace jk

#endif // JKSCRIPTCANVAS_H