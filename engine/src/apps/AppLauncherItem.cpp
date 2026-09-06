#include <apps/AppLauncherItem.h>

#include <JKApplication.h>
#include <JKResourceCache.h>
#include <JKSoundManager.h>
#include <JKDC.h>
#include <algorithm>
#include <cstring>

namespace jk {

namespace {

constexpr int kLauncherIconSize = 32;
constexpr int kLabelHeight = 16;
constexpr int kMargin = 4;

} // anonymous namespace

AppLauncherItem::AppLauncherItem(const JKRect& rect, const std::string& label,
                                 std::function<void()> onClick)
    : label_(label), onClick_(std::move(onClick)) {
    SetRect(rect);
    SetBackColor(192, 192, 192);
    SetTextColor(0, 0, 0);
    SetFocusable(true);
}

JKPoint AppLauncherItem::MeasureContent() const {
    return JKPoint{ kLauncherIconSize + kMargin * 2,
                    kLauncherIconSize + kMargin + kLabelHeight };
}

void AppLauncherItem::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    if (client.IsEmpty()) return;

    // Fill background with the control's back color.
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(client);

    // Draw a raised or sunken 3D border depending on press state.
    JKRect face = client;
    face.x += 2;
    face.y += 2;
    face.w -= 4;
    face.h -= 4;
    if (pressed_) {
        dc.Box3D(face, 2, 128, 128, 128, 0, 0, 0, 255, 255, 255);
    } else {
        dc.Box3D(face, 2, 192, 192, 192, 255, 255, 255, 0, 0, 0);
    }

    // Icon area centered in the upper portion.
    JKRect iconRect = client;
    iconRect.x += (client.w - kLauncherIconSize) / 2;
    iconRect.y += kMargin + 2;
    iconRect.w = kLauncherIconSize;
    iconRect.h = kLauncherIconSize;

    JKResourceCache* cache = g_currentJKApp ? g_currentJKApp->GetResourceCache() : nullptr;
    if (!iconKey_.empty() && cache && cache->HasImage(iconKey_)) {
        auto tex = cache->GetImage(iconKey_);
        dc.DrawSpriteX(iconRect, tex, kLauncherIconSize, kLauncherIconSize, ADJ_XYCENTER);
    }

    // Label centered below the icon.
    JKRect labelRect = client;
    labelRect.y += kMargin + kLauncherIconSize + 4;
    labelRect.h = kLabelHeight;
    dc.SetTextColor(textR_, textG_, textB_);
    dc.TextOutX(labelRect, label_.c_str(), ADJ_XYCENTER, false);

    JKControl::OnPaintClient(dc);
}

void AppLauncherItem::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        pressed_ = true;
        Invalidate();
        return;
    }
    if (ev.type == JKEventType::MouseUp) {
        pressed_ = false;
        Invalidate();
        const JKRect client = GetScreenClientRect();
        if (client.Contains(ev.x, ev.y) && onClick_) {
            JKSoundManager::GetInstance().PlaySFX("button_click", kAudioBusUI);
            onClick_();
        }
        return;
    }
    if (ev.type == JKEventType::MouseMove) {
        // No hover effect for now.
        return;
    }
    if (ev.type == JKEventType::KeyDown) {
        if (ev.keyCode == SDLK_RETURN || ev.keyCode == SDLK_SPACE) {
            if (onClick_) {
                JKSoundManager::GetInstance().PlaySFX("button_click", kAudioBusUI);
                onClick_();
            }
            return;
        }
    }
    JKControl::RespondMessage(ev);
}

std::vector<uint8_t> CreateMineLauncherIcon() {
    std::vector<uint8_t> data(kLauncherIconSize * kLauncherIconSize * 4, 0);
    auto set = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (x < 0 || x >= kLauncherIconSize || y < 0 || y >= kLauncherIconSize) return;
        int idx = (y * kLauncherIconSize + x) * 4;
        data[idx + 0] = r;
        data[idx + 1] = g;
        data[idx + 2] = b;
        data[idx + 3] = a;
    };
    auto drawCircle = [&](int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x * x + y * y <= radius * radius + radius / 2) {
                    set(cx + x, cy + y, r, g, b, 255);
                }
            }
        }
    };
    auto drawLine = [&](int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b) {
        int dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
        int dy = -std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            set(x1, y1, r, g, b, 255);
            if (x1 == x2 && y1 == y2) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    };

    // Black mine body with grey highlight.
    drawCircle(16, 14, 10, 0, 0, 0);
    drawCircle(12, 10, 3, 192, 192, 192);
    // Spikes.
    drawLine(16, 2, 16, 7, 0, 0, 0);
    drawLine(16, 21, 16, 26, 0, 0, 0);
    drawLine(2, 14, 7, 14, 0, 0, 0);
    drawLine(25, 14, 30, 14, 0, 0, 0);
    drawLine(5, 5, 9, 9, 0, 0, 0);
    drawLine(23, 5, 27, 9, 0, 0, 0);
    drawLine(5, 23, 9, 19, 0, 0, 0);
    drawLine(23, 23, 27, 19, 0, 0, 0);
    return data;
}

std::vector<uint8_t> CreateTetrisLauncherIcon() {
    std::vector<uint8_t> data(kLauncherIconSize * kLauncherIconSize * 4, 0);
    auto set = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (x < 0 || x >= kLauncherIconSize || y < 0 || y >= kLauncherIconSize) return;
        int idx = (y * kLauncherIconSize + x) * 4;
        data[idx + 0] = r;
        data[idx + 1] = g;
        data[idx + 2] = b;
        data[idx + 3] = a;
    };
    auto drawBlock = [&](int bx, int by, uint8_t r, uint8_t g, uint8_t b) {
        for (int y = by; y < by + 7; ++y) {
            for (int x = bx; x < bx + 7; ++x) {
                set(x, y, r, g, b, 255);
            }
        }
        // Highlight.
        for (int x = bx; x < bx + 7; ++x) set(x, by, 255, 255, 255, 255);
        for (int y = by; y < by + 7; ++y) set(bx, y, 255, 255, 255, 255);
        // Shadow.
        for (int x = bx; x < bx + 7; ++x) set(x, by + 6, 0, 0, 0, 255);
        for (int y = by; y < by + 7; ++y) set(bx + 6, y, 0, 0, 0, 255);
    };

    // Draw a Tetris "T" piece using 7x7 blocks.
    uint8_t r = 128, g = 0, b = 128; // purple T piece
    drawBlock(12, 4, r, g, b);
    drawBlock(5, 11, r, g, b);
    drawBlock(12, 11, r, g, b);
    drawBlock(19, 11, r, g, b);
    return data;
}

} // namespace jk
