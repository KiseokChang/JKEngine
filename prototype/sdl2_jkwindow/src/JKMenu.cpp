#include <JKMenu.h>
#include <JKApplication.h>
#include <JKEvent.h>
#include <algorithm>

namespace jk {

JKMenu::JKMenu() = default;

JKMenu::JKMenu(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    SetBackColor(192, 192, 192);
    SetTextColor(0, 0, 0);
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
    if (g_currentJKApp) {
        g_currentJKApp->SetModalWindow(popup_.get());
    }
}

void JKMenu::ClosePopup() {
    if (popup_) {
        if (g_currentJKApp && g_currentJKApp->GetModalWindow() == popup_.get()) {
            g_currentJKApp->SetModalWindow(nullptr);
        }
        popup_->RequestClose();
    }
    activeIndex_ = -1;
}

void JKMenu::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(client);

    int32_t x = client.x;
    for (size_t i = 0; i < menus_.size(); ++i) {
        int32_t w = ItemWidth(static_cast<int32_t>(i));
        JKRect itemRect{ x, client.y, w, client.h };
        if (static_cast<int32_t>(i) == activeIndex_) {
            dc.SetColor(0, 0, 128, 255);
            dc.FillRect(itemRect);
            dc.SetTextColor(255, 255, 255);
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

void JKMenu::Popup::SetRect(const JKRect& rect) {
    JKControl::SetRect(rect);
    SetClientRect(rect);
}

void JKMenu::Popup::PaintWindow(JKDC& dc) {
    OnPaintClient(dc);
}

void JKMenu::Popup::OnPaintClient(JKDC& dc) {
    const JKRect screen = GetScreenRect();
    dc.SetColor(240, 240, 240, 255);
    dc.FillRect(screen);
    dc.Rectangle3D(screen, 2);
    JKWindow::OnPaintClient(dc);
}

void JKMenu::Popup::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        const JKRect screen = GetScreenRect();
        if (!screen.Contains(ev.x, ev.y)) {
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
