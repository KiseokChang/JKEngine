#include <JKCheckBox.h>
#include <theme/JKTheme.h>

namespace jk {

JKCheckBox::JKCheckBox() = default;

JKCheckBox::JKCheckBox(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    const auto& t = jk::theme::current();
    SetBackColor(t.widgetFace.r, t.widgetFace.g, t.widgetFace.b);
    SetTextColor(t.widgetText.r, t.widgetText.g, t.widgetText.b);
    SetFocusable(true);
}

void JKCheckBox::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    const auto& t = jk::theme::current();
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(client);

    // 왼쪽에 16x16 체크 상자를 배치한다.
    JKRect box{ client.x, client.y + (client.h - 16) / 2, 16, 16 };
    dc.Box3D(box, 1,
             t.fieldBg.r, t.fieldBg.g, t.fieldBg.b,
             t.bevelLight.r, t.bevelLight.g, t.bevelLight.b,
             t.bevelDark.r, t.bevelDark.g, t.bevelDark.b);

    if (status_) {
        // 체크 표시 (✓).
        dc.SetColor(t.widgetText.r, t.widgetText.g, t.widgetText.b, 255);
        dc.DrawLine(box.x + 3, box.y + 7, box.x + 6, box.y + 12);
        dc.DrawLine(box.x + 6, box.y + 12, box.x + 12, box.y + 4);
    }

    // 상자 오른쪽에 텍스트를 출력한다.
    const std::string& txt = GetText();
    if (!txt.empty()) {
        JKRect textRect = client;
        textRect.x += 20;
        textRect.w -= 20;
        dc.SetTextColor(textR_, textG_, textB_);
        dc.TextOutX(textRect, txt.c_str(), ADJ_YCENTER | ADJ_LEFT, false);
    }

    JKControl::OnPaintClient(dc);
}

void JKCheckBox::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();
        if (autoControl_) {
            status_ = !status_;
        }
    } else if (ev.type == JKEventType::KeyDown) {
        if (ev.keyCode == SDLK_SPACE) {
            if (autoControl_) status_ = !status_;
        } else {
            JKControl::RespondMessage(ev);
        }
    } else {
        JKControl::RespondMessage(ev);
    }
}

} // namespace jk
