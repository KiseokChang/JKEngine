#include <JKMenu.h>
#include <JKApplication.h>
#include <JKEvent.h>
#include <theme/JKTheme.h>
#include <algorithm>

namespace jk {

JKMenu::JKMenu() = default;

JKMenu::JKMenu(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    const auto& t = jk::theme::current();
    SetBackColor(t.widgetFace.r, t.widgetFace.g, t.widgetFace.b);
    SetTextColor(t.widgetText.r, t.widgetText.g, t.widgetText.b);
}

void JKMenu::AddMenu(const std::string& label, const std::vector<JKMenuItem>& items) {
    menus_.push_back({ label, items });
}

int32_t JKMenu::HitItem(int32_t x) const {
    const JKRect screen = GetScreenRect();
    int32_t relX = x - screen.x;
    if (relX < 0) return -1;
    int32_t pos = 0;
    for (size_t i = 0; i < menus_.size(); ++i) {
        int32_t w = ItemWidth(static_cast<int32_t>(i));
        if (relX >= pos && relX < pos + w) {
            return static_cast<int32_t>(i);
        }
        pos += w;
    }
    return -1;
}

int32_t JKMenu::ItemX(int32_t index) const {
    int32_t x = 0;
    for (int32_t i = 0; i < index && i < static_cast<int32_t>(menus_.size()); ++i) {
        x += ItemWidth(i);
    }
    return x;
}

int32_t JKMenu::ItemWidth(int32_t index) const {
    if (index < 0 || index >= static_cast<int32_t>(menus_.size())) return 0;
    return static_cast<int32_t>(menus_[index].label.size()) * 8 + 16;
}

void JKMenu::OpenPopup(int32_t index) {
    if (index < 0 || index >= static_cast<int32_t>(menus_.size())) return;
    ClosePopup();
    activeIndex_ = index;

    const JKRect screen = GetScreenRect();
    int32_t itemX = ItemX(index);
    int32_t maxLen = 0;
    for (const auto& item : menus_[index].items) {
        maxLen = std::max(maxLen, static_cast<int32_t>(item.label.size()));
    }
    int32_t w = std::max(80, maxLen * 8 + 40);
    int32_t h = static_cast<int32_t>(menus_[index].items.size()) * 16 + 4;
    h = std::max(h, 24);

    JKRect popupRect{ screen.x + itemX, screen.y + screen.h, w, h };
    auto popup = std::make_unique<Popup>(popupRect, menus_[index].items,
                                            [this]() { ClosePopup(); });
    popup_ = std::move(popup);
    if (g_jkAppHost) {
        g_jkAppHost->SetModalWindow(popup_.get());
    }
}

void JKMenu::ClosePopup() {
    if (popup_) {
        if (g_jkAppHost && g_jkAppHost->GetModalWindow() == popup_.get()) {
            g_jkAppHost->SetModalWindow(nullptr);
        }
        popup_->RequestClose();
    }
    activeIndex_ = -1;
}

void JKMenu::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    const auto& t = jk::theme::current();
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(client);

    int32_t x = client.x;
    for (size_t i = 0; i < menus_.size(); ++i) {
        int32_t w = ItemWidth(static_cast<int32_t>(i));
        JKRect itemRect{ x, client.y, w, client.h };
        if (static_cast<int32_t>(i) == activeIndex_) {
            dc.SetColor(t.selectionBg.r, t.selectionBg.g, t.selectionBg.b, 255);
            dc.FillRect(itemRect);
            dc.SetTextColor(t.selectionText.r, t.selectionText.g, t.selectionText.b);
        } else {
            dc.SetTextColor(textR_, textG_, textB_);
        }
        JKRect textRect = itemRect;
        textRect.x += 8;
        textRect.w -= 16;
        dc.TextOutX(textRect, menus_[i].label.c_str(), ADJ_YCENTER | ADJ_LEFT, false);
        x += w;
    }

    JKControl::OnPaintClient(dc);
}

void JKMenu::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();
        int32_t index = HitItem(ev.x);
        if (index >= 0) {
            OpenPopup(index);
            return;
        }
        ClosePopup();
    }
    JKControl::RespondMessage(ev);
}

// ---------------------------------------------------------------------------
// JKMenu::Popup
// ---------------------------------------------------------------------------

JKMenu::Popup::Popup(const JKRect& rect, const std::vector<JKMenuItem>& items,
                     std::function<void()> onClose)
    : items_(items), onClose_(std::move(onClose)) {
    SetWindowRect(rect);
}

void JKMenu::Popup::OnRectChanged(const JKRect& rect) {
    // Popup has no border/title bar: client area occupies the whole window rect.
    // clientRect_ is relative to the window rect, so use (0,0) origin.
    SetClientRect(JKRect{ 0, 0, rect.w, rect.h });
}

void JKMenu::Popup::PaintWindow(JKDC& dc) {
    OnPaintClient(dc);
}

void JKMenu::Popup::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    if (client.IsEmpty()) return;
    const auto& t = jk::theme::current();

    // Menu popup background and raised border.
    dc.SetColor(t.widgetFace.r, t.widgetFace.g, t.widgetFace.b, 255);
    dc.FillRect(client);
    dc.Rectangle3D(client, 2);

    // Menu item text rows (16 px each, 2 px inset).
    constexpr int32_t kItemH = 16;
    for (size_t i = 0; i < items_.size(); ++i) {
        int32_t y = client.y + 2 + static_cast<int32_t>(i) * kItemH;
        if (static_cast<int32_t>(i) == selectedIndex_) {
            dc.SetColor(t.selectionBg.r, t.selectionBg.g, t.selectionBg.b, 255);
            dc.FillRect(JKRect{ client.x + 2, y, client.w - 4, kItemH });
            dc.SetTextColor(t.selectionText.r, t.selectionText.g, t.selectionText.b);
        } else {
            // 팝업 텍스트 리터럴 우회 교정 (스펙 §1c-4): 직접 지정색도
            // widgetText 토큰을 따른다.
            dc.SetTextColor(t.widgetText.r, t.widgetText.g, t.widgetText.b);
        }
        dc.TextOut(JKPoint{ client.x + 4, y }, items_[i].label.c_str());
    }
}

void JKMenu::Popup::RespondMessage(const JKEvent& ev) {
    const JKRect screen = GetScreenRect();
    if (ev.type == JKEventType::MouseMove) {
        const JKRect client = GetScreenClientRect();
        if (client.Contains(ev.x, ev.y)) {
            constexpr int32_t kItemH = 16;
            int32_t idx = (ev.y - client.y - 2) / kItemH;
            if (idx < 0 || idx >= static_cast<int32_t>(items_.size())) idx = -1;
            selectedIndex_ = idx;
        }
    } else if (ev.type == JKEventType::MouseDown) {
        if (!screen.Contains(ev.x, ev.y)) {
            if (onClose_) onClose_();
            return;
        }
        const JKRect client = GetScreenClientRect();
        if (client.Contains(ev.x, ev.y) && selectedIndex_ >= 0 &&
            selectedIndex_ < static_cast<int32_t>(items_.size())) {
            const auto& item = items_[selectedIndex_];
            if (item.onClick) item.onClick();
            if (onClose_) onClose_();
            return;
        }
    } else if (ev.type == JKEventType::KeyDown) {
        if (ev.keyCode == SDLK_ESCAPE) {
            if (onClose_) onClose_();
            return;
        }
    }
    JKWindow::RespondMessage(ev);
}

} // namespace jk
