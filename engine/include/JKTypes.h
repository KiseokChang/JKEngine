#ifndef JKTYPES_H
#define JKTYPES_H

#include <SDL.h>
#include <cstdint>

namespace jk {

// Text alignment constants used by JKRect::Adjust / JKDC::TextOutX.
constexpr uint8_t ADJ_START  = 1;
constexpr uint8_t ADJ_END    = 2;
constexpr uint8_t ADJ_CENTER = 3;
constexpr uint8_t ADJ_PREV   = 4;
constexpr uint8_t ADJ_NEXT   = 5;

constexpr uint8_t ADJ_LEFT     = 0x01;
constexpr uint8_t ADJ_RIGHT    = 0x02;
constexpr uint8_t ADJ_XCENTER  = 0x03;
constexpr uint8_t ADJ_XPREV    = 0x04;
constexpr uint8_t ADJ_XNEXT    = 0x05;
constexpr uint8_t ADJ_TOP      = 0x10;
constexpr uint8_t ADJ_BOTTOM   = 0x20;
constexpr uint8_t ADJ_YCENTER  = 0x30;
constexpr uint8_t ADJ_YPREV    = 0x40;
constexpr uint8_t ADJ_YNEXT    = 0x50;
constexpr uint8_t ADJ_XYCENTER = ADJ_XCENTER | ADJ_YCENTER;

struct JKPoint {
    int32_t x = 0;
    int32_t y = 0;

    void Set(int32_t x_, int32_t y_) { x = x_; y = y_; }

    // Treat this point as the interval [x, y].
    int32_t Adjust(uint8_t setting, int32_t size) const {
        int32_t width = y - x;
        switch (setting) {
            case ADJ_START:  return x;
            case ADJ_END:    return y - size;
            case ADJ_CENTER: return x + (width - size) / 2;
            case ADJ_PREV:   return x - (size + 1);
            case ADJ_NEXT:   return y + 1;
            default:         return x;
        }
    }
};

struct JKRect {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;

    bool Contains(int32_t px, int32_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    bool IsEmpty() const {
        return w <= 0 || h <= 0;
    }

    SDL_Rect ToSDL() const {
        return SDL_Rect{ x, y, w, h };
    }

    JKRect OffsetBy(int32_t dx, int32_t dy) const {
        return JKRect{ x + dx, y + dy, w, h };
    }

    // Legacy-style helpers (original JKENGINE used left/top/right/bottom).
    int32_t Left() const { return x; }
    int32_t Top() const { return y; }
    int32_t Right() const { return x + w; }
    int32_t Bottom() const { return y + h; }
    int32_t Width() const { return w; }
    int32_t Height() const { return h; }

    JKRect SetLeftTopBy(int32_t left = 0, int32_t top = 0) const {
        return JKRect{ left, top, w, h };
    }
    JKRect SetLeftTopBy(const JKPoint& p) const {
        return JKRect{ p.x, p.y, w, h };
    }

    // Expand/contract all edges by the same amount.
    JKRect Expand(int32_t amount) const {
        return JKRect{ x - amount, y - amount, w + amount * 2, h + amount * 2 };
    }

    // Expand/contract each edge independently.
    JKRect Expand(int32_t left, int32_t top, int32_t right, int32_t bottom) const {
        return JKRect{ x - left, y - top, w + left + right, h + top + bottom };
    }

    // Legacy alias used by original code (e.g. rect.ExpandBy(2)).
    JKRect ExpandBy(int32_t amount) const {
        return Expand(amount);
    }

    JKPoint LeftTop() const { return JKPoint{ x, y }; }

    JKPoint Adjust(uint8_t setting, JKPoint size) const {
        JKPoint p;
        JKPoint tx{x, x + w};
        p.x = tx.Adjust(setting & 0x0f, size.x);
        JKPoint ty{y, y + h};
        p.y = ty.Adjust((setting >> 4) & 0x0f, size.y);
        return p;
    }
};

inline JKRect MakeRect(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    return JKRect{ left, top, right - left, bottom - top };
}

} // namespace jk

#endif // JKTYPES_H
