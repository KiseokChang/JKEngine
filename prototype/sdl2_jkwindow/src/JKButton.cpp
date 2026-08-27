#include <JKButton.h>
#include <JKApplication.h>

namespace jk {

JKButton::JKButton() = default;

JKButton::JKButton(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    SetAdjustFlag(ADJ_XYCENTER);
    SetBackColor(192, 192, 192);
    SetTextColor(0, 0, 0);
    SetFocusable(true);
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
        SetFocus();
        status_ = true;
        selecting_ = true;
        if (g_currentJKApp) g_currentJKApp->SetCapture(this);
    } else if (ev.type == JKEventType::MouseUp) {
        if (g_currentJKApp) g_currentJKApp->ReleaseCapture();
        if (selecting_) {
            selecting_ = false;
            const JKRect client = GetScreenClientRect();
            status_ = false;
            if (client.Contains(ev.x, ev.y)) {
                OnClick();
            }
        }
    } else if (ev.type == JKEventType::MouseMove) {
        if (selecting_ && g_currentJKApp && g_currentJKApp->GetCapture() == this) {
            const JKRect client = GetScreenClientRect();
            status_ = client.Contains(ev.x, ev.y);
        }
    } else if (ev.type == JKEventType::KeyDown) {
        if (ev.keyCode == SDLK_RETURN || ev.keyCode == SDLK_SPACE) {
            status_ = true;
        } else {
            JKStatic::RespondMessage(ev);
        }
    } else if (ev.type == JKEventType::KeyUp) {
        if ((ev.keyCode == SDLK_RETURN || ev.keyCode == SDLK_SPACE) && status_) {
            status_ = false;
            OnClick();
        } else {
            JKStatic::RespondMessage(ev);
        }
    } else {
        JKStatic::RespondMessage(ev);
    }
}

} // namespace jk
