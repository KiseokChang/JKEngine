#include <JKListBox.h>
#include <JKEvent.h>
#include <JKApplication.h>
#include <algorithm>
#include <cmath>

namespace jk {

namespace {

constexpr uint32_t kDoubleClickMs = 300;
constexpr uint32_t kDoubleClickDistanceSq = 25; // 5px squared

uint32_t GetEventTick() {
    return SDL_GetTicks();
}

} // anonymous namespace

JKListBox::JKListBox() = default;

JKListBox::JKListBox(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    SetBackColor(255, 255, 255);
    SetTextColor(0, 0, 0);
    SetFocusable(true);

    // Scrollbar is a child of the listbox: coordinates are relative to the
    // listbox rect (parent client area), not screen coordinates.
    JKRect sbRect{ rect.w - 16, 0, 16, rect.h };
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

int32_t JKListBox::VisibleItemCount() const {
    const JKRect client = GetClientRect();
    return std::max(1, client.h / itemHeight_);
}

void JKListBox::EnsureVisible(int32_t index) {
    if (index < 0 || index >= static_cast<int32_t>(items_.size())) return;
    int32_t visible = VisibleItemCount();
    if (index < topIndex_) {
        topIndex_ = index;
    } else if (index >= topIndex_ + visible) {
        topIndex_ = index - visible + 1;
    }
    topIndex_ = std::max(0, std::min(topIndex_, static_cast<int32_t>(items_.size()) - visible));
    if (vScroll_) vScroll_->SetPos(topIndex_);
}

void JKListBox::MoveSelection(int32_t delta) {
    if (items_.empty()) return;
    int32_t newIndex = selectedIndex_ + delta;
    newIndex = std::max(0, std::min(newIndex, static_cast<int32_t>(items_.size()) - 1));
    if (newIndex != selectedIndex_) {
        SetSelectedIndex(newIndex);
        EnsureVisible(newIndex);
    }
}

void JKListBox::UpdateScrollRange() {
    int32_t visible = VisibleItemCount();
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
            uint32_t now = GetEventTick();
            int32_t dx = ev.x - lastClickX_;
            int32_t dy = ev.y - lastClickY_;
            int32_t distSq = dx * dx + dy * dy;
            bool isDouble = (idx == lastClickIndex_) &&
                            (now - lastClickTime_ <= kDoubleClickMs) &&
                            (distSq <= static_cast<int32_t>(kDoubleClickDistanceSq));
            lastClickIndex_ = idx;
            lastClickTime_ = now;
            lastClickX_ = ev.x;
            lastClickY_ = ev.y;
            if (isDouble && onDoubleClick_) {
                onDoubleClick_(idx);
            }
        }
    } else if (ev.type == JKEventType::KeyDown) {
        switch (ev.keyCode) {
            case SDLK_UP:        MoveSelection(-1); return;
            case SDLK_DOWN:      MoveSelection(1); return;
            case SDLK_HOME:      MoveSelection(-static_cast<int32_t>(items_.size())); return;
            case SDLK_END:       MoveSelection(static_cast<int32_t>(items_.size())); return;
            case SDLK_PAGEUP:    MoveSelection(-VisibleItemCount()); return;
            case SDLK_PAGEDOWN:  MoveSelection(VisibleItemCount()); return;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (selectedIndex_ >= 0 && onActivate_) onActivate_(selectedIndex_);
                return;
        }
    }
    JKControl::RespondMessage(ev);
}

} // namespace jk
