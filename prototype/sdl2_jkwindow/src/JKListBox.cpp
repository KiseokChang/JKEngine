#include <JKListBox.h>
#include <JKEvent.h>
#include <algorithm>

namespace jk {

JKListBox::JKListBox() = default;

JKListBox::JKListBox(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    SetBackColor(255, 255, 255);
    SetTextColor(0, 0, 0);
    SetFocusable(true);

    JKRect sbRect{ rect.x + rect.w - 16, rect.y, 16, rect.h };
    auto sb = std::make_unique<JKScrollBar>(sbRect, 0, ScrollBarDir::Vertical);
    sb->SetRange(0, 0, 1);
    sb->SetOnScroll([this](int32_t pos) { topIndex_ = pos; });
    vScroll_ = sb.get();
    AddControl(std::move(sb));
    UpdateScrollRange();
}

void JKListBox::AddString(const std::string& str) {
    items_.push_back(str);
    UpdateScrollRange();
}

void JKListBox::DeleteString(size_t index) {
    if (index >= items_.size()) return;
    items_.erase(items_.begin() + index);
    if (selectedIndex_ >= static_cast<int32_t>(items_.size()))
        selectedIndex_ = static_cast<int32_t>(items_.size()) - 1;
    UpdateScrollRange();
}

void JKListBox::Clear() {
    items_.clear();
    selectedIndex_ = -1;
    topIndex_ = 0;
    UpdateScrollRange();
}

size_t JKListBox::GetCount() const {
    return items_.size();
}

const std::string& JKListBox::GetString(size_t index) const {
    static const std::string empty;
    if (index >= items_.size()) return empty;
    return items_[index];
}

void JKListBox::SetSelectedIndex(int32_t index) {
    if (index < -1 || index >= static_cast<int32_t>(items_.size())) return;
    selectedIndex_ = index;
    if (onSelect_) onSelect_(selectedIndex_);
}

void JKListBox::UpdateScrollRange() {
    const JKRect client = GetScreenClientRect();
    int32_t visible = std::max(1, client.h / itemHeight_);
    int32_t maxPos = std::max(0, static_cast<int32_t>(items_.size()) - visible);
    if (vScroll_) vScroll_->SetRange(0, maxPos, visible);
}

void JKListBox::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.Box3D(client, 1, 255, 255, 255, 255, 255, 255, 0, 0, 0);

    JKRect inner = client;
    inner.x += 2; inner.y += 2;
    inner.w -= 20; inner.h -= 4;
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(inner);

    int32_t visible = std::max(1, inner.h / itemHeight_);
    for (int32_t i = 0; i < visible && topIndex_ + i < static_cast<int32_t>(items_.size()); ++i) {
        int32_t idx = topIndex_ + i;
        int32_t y = inner.y + i * itemHeight_;
        if (idx == selectedIndex_) {
            dc.SetColor(0, 0, 128, 255);
            dc.FillRect(JKRect{ inner.x, y, inner.w, itemHeight_ });
            dc.SetTextColor(255, 255, 255);
        } else {
            dc.SetTextColor(textR_, textG_, textB_);
        }
        dc.TextOut(JKPoint{ inner.x + 2, y }, items_[idx].c_str());
    }

    JKControl::OnPaintClient(dc);
}

void JKListBox::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();
        const JKRect client = GetScreenClientRect();
        int32_t relY = ev.y - client.y - 2;
        int32_t idx = topIndex_ + relY / itemHeight_;
        if (idx >= 0 && idx < static_cast<int32_t>(items_.size())) {
            SetSelectedIndex(idx);
        }
    }
    JKControl::RespondMessage(ev);
}

} // namespace jk
