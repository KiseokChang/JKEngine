#include <JKStatic.h>

namespace jk {

JKStatic::JKStatic() = default;

JKStatic::JKStatic(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
}

void JKStatic::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(client);

    const std::string& txt = GetText();
    if (!txt.empty()) {
        dc.SetTextColor(textR_, textG_, textB_);
        dc.TextOutX(client, txt.c_str(), adjustFlag_, false);
    }

    // 기본 JKControl::OnPaintClient가 자식 컨트롤을 그린다.
    JKControl::OnPaintClient(dc);
}

} // namespace jk
