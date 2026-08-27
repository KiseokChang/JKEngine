#include <JKComboBox.h>
#include <JKEvent.h>
#include <algorithm>

namespace jk {

JKComboBox::JKComboBox() = default;

JKComboBox::JKComboBox(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    SetBackColor(255, 255, 255);
    SetTextColor(0, 0, 0);
}

void JKComboBox::AddString(const std::string& str) {
    items_.push_back(str);
    if (popup_) popup_->AddString(str);
}

void JKComboBox::Clear() {
    items_.clear();
    selectedIndex_ = -1;
    if (popup_) popup_->Clear();
}

size_t JKComboBox::GetCount() const {
    return items_.size();
}

void JKComboBox::SetSelectedIndex(int32_t index) {
    if (index < -1 || index >= static_cast<int32_t>(items_.size())) return;
    selectedIndex_ = index;
}

const std::string& JKComboBox::GetSelectedString() const {
    static const std::string empty;
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int32_t>(items_.size())) return empty;
    return items_[selectedIndex_];
}

void JKComboBox::ToggleDropDown() {
    if (dropped_) { CloseDropDown(); return; }
    if (items_.empty()) return;

    const JKRect rc = GetScreenRect();
    int32_t h = static_cast<int32_t>(items_.size()) * 16 + 4;
    h = std::min(h, 120);
    JKRect popupRect{ rc.x, rc.y + rc.h, rc.w, h };
    auto list = std::make_unique<JKListBox>(popupRect, 0);
    for (const auto& s : items_) list->AddString(s);
    list->SetSelectedIndex(selectedIndex_);
    list->SetOnSelect([this](int32_t idx) {
        selectedIndex_ = idx;
        CloseDropDown();
    });
    popup_ = list.get();
    AddControl(std::move(list));
    dropped_ = true;
}

void JKComboBox::CloseDropDown() {
    if (popup_) {
        popup_->RequestClose();
        popup_ = nullptr;
    }
    dropped_ = false;
}

void JKComboBox::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.Box3D(client, 1, 255, 255, 255, 255, 255, 255, 0, 0, 0);

    JKRect inner = client;
    inner.x += 2; inner.y += 2;
    inner.w -= 20; inner.h -= 4;
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(inner);

    dc.SetTextColor(textR_, textG_, textB_);
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int32_t>(items_.size())) {
        dc.TextOut(JKPoint{ inner.x + 2, inner.y + (inner.h - 16) / 2 },
                   items_[selectedIndex_].c_str());
    }

    JKRect btn{ client.x + client.w - 18, client.y + 2, 16, client.h - 4 };
    dc.Box3D(btn, 1, 192, 192, 192, 255, 255, 255, 128, 128, 128);
    dc.SetColor(0, 0, 0, 255);
    int32_t cx = btn.x + btn.w / 2;
    int32_t cy = btn.y + btn.h / 2;
    dc.DrawLine(cx - 3, cy - 1, cx + 3, cy - 1);
    dc.DrawLine(cx - 2, cy, cx + 2, cy);
    dc.DrawLine(cx - 1, cy + 1, cx + 1, cy + 1);

    JKControl::OnPaintClient(dc);
}

void JKComboBox::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();
        const JKRect client = GetScreenClientRect();
        if (ev.x >= client.x + client.w - 18) ToggleDropDown();
    }
    JKControl::RespondMessage(ev);
}

} // namespace jk
