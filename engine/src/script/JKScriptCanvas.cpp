#include <JKScriptCanvas.h>

#include <JKApplicationHost.h>
#include <JKDC.h>
#include <theme/JKTheme.h>

#include <cstdio>

namespace jk {

JKScriptCanvas::JKScriptCanvas(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    // 포커스 가능 — 키/휠 이벤트는 포커스 컨트롤로만 온다(JKWindow::RespondMessage).
    SetFocusable(true);
    const auto& t = jk::theme::current();
    SetBackColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b);
}

void JKScriptCanvas::AddOp(Op op) {
    if (ops_.size() >= kMaxOps) {
        // Ring behavior (2026-09-24 폰 실전): trail-style scripts never call
        // canvasClear, so at the cap they used to lose their NEWEST ops — the
        // ball froze while old frames kept replaying. Evict the oldest op
        // instead: trails keep rendering, canvasClear-per-frame scenes are
        // unchanged (they never reach the cap), and a malformed static scene
        // degrades the same way it already did (oldest strokes go first).
        if (!warnedCap_) {
            warnedCap_ = true;
            std::printf("[canvas] op cap %zu reached - evicting oldest ops "
                        "(trail scripts: fine; static scenes: too many ops)\n",
                        kMaxOps);
            std::fflush(stdout);
        }
        ops_.erase(ops_.begin());
    }
    ops_.push_back(std::move(op));
    Invalidate();
}

void JKScriptCanvas::Clear(uint8_t r, uint8_t g, uint8_t b) {
    ops_.clear();
    SetBackColor(r, g, b);
    Invalidate();
}

void JKScriptCanvas::DrawRectOp(int32_t x, int32_t y, int32_t w, int32_t h,
                                uint8_t r, uint8_t g, uint8_t b, bool filled) {
    Op op;
    op.kind = 0;
    op.rect = JKRect{ x, y, w, h };
    op.filled = filled;
    op.cr = r; op.cg = g; op.cb = b;
    AddOp(std::move(op));
}

void JKScriptCanvas::DrawPixel(int32_t x, int32_t y,
                               uint8_t r, uint8_t g, uint8_t b) {
    Op op;
    op.kind = 1;
    op.rect = JKRect{ x, y, 1, 1 };
    op.cr = r; op.cg = g; op.cb = b;
    AddOp(std::move(op));
}

void JKScriptCanvas::DrawLineOp(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                uint8_t r, uint8_t g, uint8_t b) {
    Op op;
    op.kind = 2;
    op.rect = JKRect{ x1, y1, x2, y2 };  // rect.x/y = 시작, w/h = 끝 (관례 주석)
    op.cr = r; op.cg = g; op.cb = b;
    AddOp(std::move(op));
}

void JKScriptCanvas::DrawCircle(int32_t cx, int32_t cy, int32_t radius,
                                uint8_t r, uint8_t g, uint8_t b, bool filled) {
    Op op;
    op.kind = 3;
    op.point = JKPoint{ cx, cy };
    op.radius = radius;
    op.filled = filled;
    op.cr = r; op.cg = g; op.cb = b;
    AddOp(std::move(op));
}

void JKScriptCanvas::DrawText(int32_t x, int32_t y, const std::string& kssmText,
                              uint8_t r, uint8_t g, uint8_t b) {
    Op op;
    op.kind = 4;
    op.point = JKPoint{ x, y };
    op.text = kssmText;
    op.cr = r; op.cg = g; op.cb = b;
    AddOp(std::move(op));
}

void JKScriptCanvas::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    if (client.IsEmpty()) return;

    // 바탕 + 테두리 (비어 있어도 캔버스 실존이 눈에 보이게)
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(client);
    dc.SetColor(90, 90, 90, 255);
    dc.DrawRect(client);

    // 옵 리플레이 — 유지 모드(docs/60 §10): 저장된 장면을 매 paint 재현.
    for (const Op& op : ops_) {
        dc.SetColor(op.cr, op.cg, op.cb, 255);
        switch (op.kind) {
            case 0:
                if (op.filled) dc.FillRect(JKRect{ client.x + op.rect.x,
                    client.y + op.rect.y, op.rect.w, op.rect.h });
                else dc.DrawRect(JKRect{ client.x + op.rect.x,
                    client.y + op.rect.y, op.rect.w, op.rect.h });
                break;
            case 1:
                dc.DrawPixel(client.x + op.rect.x, client.y + op.rect.y);
                break;
            case 2:
                dc.DrawLine(client.x + op.rect.x, client.y + op.rect.y,
                            client.x + op.rect.w, client.y + op.rect.h);
                break;
            case 3:
                if (op.filled) {
                    // 스캔라인 근사 — JKDC에 채운 원이 없다. r=0이면 픽셀 하나.
                    const int32_t r2 = op.radius * op.radius;
                    for (int32_t dy = -op.radius; dy <= op.radius; ++dy) {
                        const int32_t dy2 = dy * dy;
                        if (dy2 > r2) continue;
                        const int32_t span = static_cast<int32_t>(
                            sqrt(static_cast<double>(r2 - dy2)));
                        dc.FillRect(JKRect{ client.x + op.point.x - span,
                            client.y + op.point.y + dy, span * 2 + 1, 1 });
                    }
                } else {
                    dc.Circle(JKPoint{ client.x + op.point.x,
                                       client.y + op.point.y }, op.radius);
                }
                break;
            case 4:
                if (!op.text.empty()) {
                    dc.SetTextColor(op.cr, op.cg, op.cb);
                    dc.TextOut(JKPoint{ client.x + op.point.x,
                                        client.y + op.point.y },
                               op.text.c_str());
                }
                break;
            default:
                break;
        }
    }
}

JKPoint JKScriptCanvas::ToLocal(int32_t screenX, int32_t screenY) const {
    const JKRect client = GetScreenClientRect();
    return JKPoint{ screenX - client.x, screenY - client.y };
}

void JKScriptCanvas::DispatchLocal(const JKEvent& ev, int kind) {
    const JKPoint p = ToLocal(ev.x, ev.y);
    lastMouse_ = p;
    if (sink_) {
        InputEvent in;
        in.kind = kind;
        in.x = p.x;
        in.y = p.y;
        in.detail = ev.dy;  // wheel delta; mouse events pass 0
        // down/up carry the SDL button (ev.detail convention — single-process
        // TranslateSDLEvent and the client wire remap both put it there).
        in.button = (kind <= 1) ? static_cast<int32_t>(ev.detail) : 0;
        sink_(in);
    }
}

void JKScriptCanvas::RespondMessage(const JKEvent& ev) {
    switch (ev.type) {
        case JKEventType::MouseDown:
            SetFocus();
            if (g_jkAppHost) g_jkAppHost->SetCapture(this);
            DispatchLocal(ev, 0);
            break;
        case JKEventType::MouseUp:
            if (g_jkAppHost) g_jkAppHost->ReleaseCapture();
            DispatchLocal(ev, 1);
            break;
        case JKEventType::MouseMove:
            DispatchLocal(ev, 2);
            break;
        case JKEventType::MouseWheel:
            // 휠은 좌표가 없어 포커스 경로로 온다(JKWindow::RespondMessage) —
            // 마지막 마우스 위치를 대신 전달한다.
            if (sink_) {
                InputEvent in;
                in.kind = 3;
                in.x = lastMouse_.x;
                in.y = lastMouse_.y;
                in.detail = ev.dy;  // dy>0 = 위 (docs/61 §2 관례)
                sink_(in);
            }
            break;
        case JKEventType::KeyDown:
        case JKEventType::KeyUp:
            if (sink_) {
                InputEvent in;
                in.kind = (ev.type == JKEventType::KeyDown) ? 4 : 5;
                in.detail = static_cast<int32_t>(ev.keyCode);
                sink_(in);
            }
            break;
        default:
            JKControl::RespondMessage(ev);
            break;
    }
}

} // namespace jk