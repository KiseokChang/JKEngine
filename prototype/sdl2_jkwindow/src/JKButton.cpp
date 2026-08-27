#include <JKButton.h>

namespace jk {

JKButton::JKButton() = default;

JKButton::JKButton(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    SetAdjustFlag(ADJ_XYCENTER);
    SetBackColor(192, 192, 192);
    SetTextColor(0, 0, 0);
}

void JKButton::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    if (!status_) {
        dc.Rectangle3D(client, depth_);
    } else {
        // 눌린 상태: 하이라이트/섀도우를 반전시켜 표현한다.
        dc.Rectangle3D(client, -depth_);
    }

    JKRect inner = client;
    inner.x += depth_;
    inner.y += depth_;
    inner.w -= 2 * depth_;
    inner.h -= 2 * depth_;
    if (!inner.IsEmpty()) {
        dc.SetColor(backR_, backG_, backB_, 255);
        dc.FillRect(inner);
    }

    const std::string& txt = GetText();
    if (!txt.empty() && !inner.IsEmpty()) {
        dc.SetTextColor(textR_, textG_, textB_);
        dc.TextOutX(inner, txt.c_str(), adjustFlag_, false);
    }

    JKControl::OnPaintClient(dc);
}

void JKButton::RespondMessage(const JKEvent& ev) {
    if (!enabled_) {
        JKStatic::RespondMessage(ev);
        return;
    }

    if (ev.type == JKEventType::MouseDown) {
        status_ = true;
        selecting_ = true;
    } else if (ev.type == JKEventType::MouseUp) {
        if (selecting_) {
            status_ = false;
            selecting_ = false;
            OnClick();
        }
    } else if (ev.type == JKEventType::MouseMove) {
        // TODO: 마우스가 버튼 영역 밖으로 나가면 눌림 상태를 해제한다.
    } else {
        JKStatic::RespondMessage(ev);
    }
}

} // namespace jk
