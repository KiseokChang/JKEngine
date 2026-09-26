#include <JKComboBox.h>
#include <JKEvent.h>
#include <JKTextAtlas.h>
#include <theme/JKTheme.h>
#include <algorithm>

namespace jk {

JKComboBox::JKComboBox() = default;

JKComboBox::JKComboBox(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    const auto& t = jk::theme::current();
    SetBackColor(t.fieldBg.r, t.fieldBg.g, t.fieldBg.b);
    SetTextColor(t.widgetText.r, t.widgetText.g, t.widgetText.b);
    SetFocusable(true);
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

void JKComboBox::SelectItem(int32_t index) {
    if (index < -1 || index >= static_cast<int32_t>(items_.size())) return;
    if (selectedIndex_ == index) return;  // 무변화 — 콜백 재발화 루프 금지
    selectedIndex_ = index;
    if (onSelectionChanged_) onSelectionChanged_(index);
}

void JKComboBox::ToggleDropDown() {
    if (readOnly_) return;
    if (dropped_) { CloseDropDown(); return; }
    if (items_.empty()) return;

    const JKRect rc = GetRect();
    // 폰트 메트릭 vs 레이아웃 여백 (docs/63 §6): 항목 수×cellH는 행 높이
    // (메트릭), +4(상하 여백)·최대 120은 레이아웃이라 그대로.
    int32_t h = static_cast<int32_t>(items_.size()) * text::GetCellMetrics().cellH + 4;
    h = std::min(h, 120);
    // Attach the popup to our PARENT so its rect lives in the same
    // parent-client coordinate space as GetRect() (a child-of-combo rect
    // would be interpreted as combo-local and end up double-offset by
    // (rc.x, rc.y) on screen). Being the parent's last child also paints and
    // hit-tests above the combo's siblings.
    JKControl* parent = GetParent();
    if (!parent) return;
    JKRect popupRect{ rc.x, rc.y + rc.h, rc.w, h };
    auto list = std::make_unique<JKListBox>(popupRect, 0);
    for (const auto& s : items_) list->AddString(s);
    list->SetSelectedIndex(selectedIndex_);
    list->SetOnSelect([this](int32_t idx) {
        SelectItem(idx);
        CloseDropDown();
    });
    popup_ = list.get();
    parent->AddControl(std::move(list));
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
    const auto& t = jk::theme::current();
    dc.Box3D(client, 1,
             t.fieldBg.r, t.fieldBg.g, t.fieldBg.b,
             t.bevelLight.r, t.bevelLight.g, t.bevelLight.b,
             t.bevelDark.r, t.bevelDark.g, t.bevelDark.b);

    JKRect inner = client;
    inner.x += 2; inner.y += 2;
    inner.w -= 20; inner.h -= 4;
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(inner);

    dc.SetTextColor(textR_, textG_, textB_);
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int32_t>(items_.size())) {
        // 텍스트 수직 중앙 배치 — 16은 셀 높이(폰트 메트릭, docs/63 §6);
        // inner.x+2px 좌측 인셋은 레이아웃 여백.
        dc.TextOut(JKPoint{ inner.x + 2,
                            inner.y + (inner.h - text::GetCellMetrics().cellH) / 2 },
                   items_[selectedIndex_].c_str());
    }

    JKRect btn{ client.x + client.w - 18, client.y + 2, 16, client.h - 4 };
    dc.Box3D(btn, 1,
             t.widgetFace.r, t.widgetFace.g, t.widgetFace.b,
             t.bevelLight.r, t.bevelLight.g, t.bevelLight.b,
             t.bevelMid.r, t.bevelMid.g, t.bevelMid.b);
    dc.SetColor(t.widgetText.r, t.widgetText.g, t.widgetText.b, 255);
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
        if (!readOnly_ && ev.x >= client.x + client.w - 18) ToggleDropDown();
    } else if (ev.type == JKEventType::KeyDown) {
        if (readOnly_) {
            // Read-only combo only allows copy-like inspection; ignore all nav.
            JKControl::RespondMessage(ev);
            return;
        }
        if (dropped_ && popup_) {
            // Let the popup listbox handle Up/Down/Enter/Escape navigation.
            popup_->RespondMessage(ev);
            if (ev.keyCode == SDLK_ESCAPE) CloseDropDown();
            return;
        }
        switch (ev.keyCode) {
            case SDLK_UP:
                if (selectedIndex_ > 0) SelectItem(selectedIndex_ - 1);
                return;
            case SDLK_DOWN:
                if (selectedIndex_ + 1 < static_cast<int32_t>(items_.size()))
                    SelectItem(selectedIndex_ + 1);
                return;
            case SDLK_RETURN:
            case SDLK_SPACE:
            case SDLK_F4:
                ToggleDropDown();
                return;
        }
        JKControl::RespondMessage(ev);
        return;
    }
    JKControl::RespondMessage(ev);
}

// ctor가 fieldBg를 캡처하는 위젯군 (docs/52 — 근거는 JKEdit.cpp ApplyTheme).
void JKComboBox::ApplyTheme() {
    const auto& t = jk::theme::current();
    SetBackColor(t.fieldBg.r, t.fieldBg.g, t.fieldBg.b);
    SetTextColor(t.widgetText.r, t.widgetText.g, t.widgetText.b);
    for (auto& c : children_) c->ApplyTheme();
    InvalidateRect(JKRect{ 0, 0, rect_.w, rect_.h });
}

} // namespace jk
