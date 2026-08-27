#include <JKDC.h>
#include <JKHangulManager.h>
#include <JKBitmapFont8x8.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace jk {

JKDC::JKDC(SDL_Renderer* renderer) : renderer_(renderer) {
}

void JKDC::SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (renderer_) {
        SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    }
}

void JKDC::SetTextColor(uint8_t r, uint8_t g, uint8_t b) {
    textR_ = r; textG_ = g; textB_ = b;
}

void JKDC::SetBackColor(uint8_t r, uint8_t g, uint8_t b) {
    backR_ = r; backG_ = g; backB_ = b;
}

void JKDC::Clear() {
    if (renderer_) {
        SDL_RenderClear(renderer_);
    }
}

void JKDC::Present() {
    if (renderer_) {
        SDL_RenderPresent(renderer_);
    }
}

void JKDC::PushClipRect(const JKRect& rect) {
    if (!renderer_) return;
    SavedClip saved;
    saved.enabled = SDL_RenderIsClipEnabled(renderer_) == SDL_TRUE;
    if (saved.enabled) {
        SDL_RenderGetClipRect(renderer_, &saved.rect);
    }
    clipStack_.push_back(saved);

    // 새 클립은 기존 클립과 교차해야 자식 컨트롤이 부모 클라이언트 영역을
    // 벗어나서 그려지지 않는다.
    SDL_Rect r = rect.ToSDL();
    if (saved.enabled) {
        const SDL_Rect& cur = saved.rect;
        const int x1 = std::max(cur.x, r.x);
        const int y1 = std::max(cur.y, r.y);
        const int x2 = std::min(cur.x + cur.w, r.x + r.w);
        const int y2 = std::min(cur.y + cur.h, r.y + r.h);
        if (x2 > x1 && y2 > y1) {
            r.x = x1;
            r.y = y1;
            r.w = x2 - x1;
            r.h = y2 - y1;
        } else {
            // 교차 결과가 없으면 드로잉이 아무것도 안 되도록 0 크기 클립을 설정한다.
            r.w = 0;
            r.h = 0;
        }
    }
    SDL_RenderSetClipRect(renderer_, &r);
}

void JKDC::PopClipRect() {
    if (!renderer_ || clipStack_.empty()) return;
    const SavedClip saved = clipStack_.back();
    clipStack_.pop_back();
    if (saved.enabled) {
        SDL_RenderSetClipRect(renderer_, &saved.rect);
    } else {
        SDL_RenderSetClipRect(renderer_, nullptr);
    }
}

void JKDC::DrawRect(const JKRect& rect) {
    if (!renderer_ || rect.IsEmpty()) return;
    SDL_Rect r = rect.ToSDL();
    SDL_RenderDrawRect(renderer_, &r);
}

void JKDC::FillRect(const JKRect& rect) {
    if (!renderer_ || rect.IsEmpty()) return;
    SDL_Rect r = rect.ToSDL();
    SDL_RenderFillRect(renderer_, &r);
}

void JKDC::DrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    if (!renderer_) return;
    SDL_RenderDrawLine(renderer_, x1, y1, x2, y2);
}

void JKDC::DrawPixel(int32_t x, int32_t y) {
    if (!renderer_) return;
    SDL_RenderDrawPoint(renderer_, x, y);
}

void JKDC::PutEngGlyph8x8(JKPoint p, uint8_t ch) {
    if (!renderer_) return;
    const uint8_t* glyph = FONT_8X8[ch & 0x7f];
    SetColor(textR_, textG_, textB_, 255);
    for (int32_t row = 0; row < 8; ++row) {
        uint8_t bits = glyph[row];
        for (int32_t col = 0; col < 8; ++col) {
            // font8x8: LSB is the leftmost pixel.
            if ((bits >> col) & 1) {
                DrawPixel(p.x + col, p.y + row);
            }
        }
    }
}

void JKDC::PutEngGlyph8x16(JKPoint p, const uint8_t* image) {
    if (!renderer_) return;
    SetColor(textR_, textG_, textB_, 255);
    for (int32_t row = 0; row < 16; ++row) {
        uint8_t bits = image[row];
        for (int32_t col = 0; col < 8; ++col) {
            // english.fnt: MSB is the leftmost pixel.
            if ((bits >> (7 - col)) & 1) {
                DrawPixel(p.x + col, p.y + row);
            }
        }
    }
}

void JKDC::PutHanGlyph16x16(JKPoint p, const uint8_t* buffer) {
    if (!renderer_) return;
    SetColor(textR_, textG_, textB_, 255);
    for (int32_t row = 0; row < 16; ++row) {
        uint8_t left  = buffer[row * 2];
        uint8_t right = buffer[row * 2 + 1];
        for (int32_t col = 0; col < 8; ++col) {
            if ((left >> (7 - col)) & 1) {
                DrawPixel(p.x + col, p.y + row);
            }
        }
        for (int32_t col = 0; col < 8; ++col) {
            if ((right >> (7 - col)) & 1) {
                DrawPixel(p.x + 8 + col, p.y + row);
            }
        }
    }
}

void JKDC::EngPutCh(JKPoint p, uint8_t ch) {
    uint8_t image[16];
    if (fontMan_ && fontMan_->GetEnglishImage(image, ch)) {
        PutEngGlyph8x16(p, image);
    } else {
        PutEngGlyph8x8(p, ch);
    }
}

void JKDC::HanPutCh(JKPoint p, uint8_t first, uint8_t second) {
    uint8_t buffer[32];
    if (fontMan_ && fontMan_->GetWORDImage(buffer, first, second)) {
        PutHanGlyph16x16(p, buffer);
    } else {
        // Fallback: draw an empty rectangle for missing glyph data.
        SetColor(textR_, textG_, textB_, 255);
        DrawRect(JKRect{ p.x, p.y, 16, 16 });
    }
}

void JKDC::PutCh(JKPoint p, uint16_t code) {
    uint8_t first  = static_cast<uint8_t>(code >> 8);
    uint8_t second = static_cast<uint8_t>(code & 0xff);
    if (first) {
        HanPutCh(p, first, second);
    } else {
        EngPutCh(p, second);
    }
}

void JKDC::TextOut(JKPoint p, const char* str) {
    if (!str) return;
    TextOut(p, std::strlen(str), str);
}

void JKDC::TextOut(JKPoint p, size_t n, const char* str) {
    if (!str || !str[0] || n == 0) return;
    size_t len = std::strlen(str);
    if (n > len) n = len;

    size_t i = 0;
    while (i < n && *str) {
        unsigned char c = static_cast<unsigned char>(*str);
        if (i + 1 < n && (c & 0x80)) {
            HanPutCh(p, static_cast<uint8_t>(c), static_cast<uint8_t>(str[1]));
            str += 2;
            p.x += 16;
            i += 2;
        } else {
            EngPutCh(p, static_cast<uint8_t>(c));
            ++str;
            p.x += 8;
            ++i;
        }
    }
}

void JKDC::TextOutInRect(const JKRect& rect, JKPoint p, size_t n, const char* str) {
    if (!n || !str) return;
    const int32_t charW = 8;
    const int32_t charH = 16;
    if (rect.y > p.y || rect.y + rect.h < p.y + charH) return;

    int32_t count1 = (p.x >= rect.x) ? (rect.x - p.x) / charW : (rect.x - p.x) / charW + 1;
    int32_t count2 = (p.x <= rect.x + rect.w) ? (rect.x + rect.w - p.x - 7) / charW
                                                : (rect.x + rect.w - p.x) / charW - 1;
    if (count2 >= static_cast<int32_t>(n)) count2 = static_cast<int32_t>(n) - 1;
    if (count1 < 0) count1 = 0;
    if (count1 > count2) return;
    if (count1 < static_cast<int32_t>(n)) {
        TextOut(JKPoint{ p.x + count1 * charW, p.y },
                static_cast<size_t>(count2 - count1 + 1),
                str + count1);
    }
}

void JKDC::TextOutX(const JKRect& rect, const char* str, uint8_t adjflag, bool wrapping) {
    if (!str || !str[0]) return;

    size_t charsPerLine[100];
    size_t lineCount = 0;
    const char* tempstr = str;
    size_t i = 0;
    while (*tempstr) {
        if (*tempstr == '\n') {
            charsPerLine[lineCount++] = i;
            i = 0;
        } else {
            ++i;
        }
        ++tempstr;
    }
    charsPerLine[lineCount++] = i;

    JKPoint p;
    JKPoint adjY{ rect.y, rect.y + rect.h };
    p.y = adjY.Adjust((adjflag >> 4) & 0x0f, static_cast<int32_t>(lineCount * 16));

    JKPoint adjX{ rect.x, rect.x + rect.w };
    tempstr = str;
    for (i = 0; i < lineCount; ++i) {
        if (charsPerLine[i]) {
            p.x = adjX.Adjust(adjflag & 0x0f, static_cast<int32_t>(8 * charsPerLine[i]));
            if (wrapping) {
                TextOutInRect(rect, JKPoint{ p.x, p.y + static_cast<int32_t>(i * 16) },
                              charsPerLine[i], tempstr);
            } else {
                TextOut(JKPoint{ p.x, p.y + static_cast<int32_t>(i * 16) },
                        charsPerLine[i], tempstr);
            }
            tempstr += charsPerLine[i] + 1;
        } else {
            ++tempstr;
        }
    }
}

void JKDC::SolidBar(const JKRect& rect) {
    FillRect(rect);
}

void JKDC::Circle(JKPoint center, int32_t radius) {
    if (!renderer_ || radius < 0) return;
    int32_t x = 0;
    int32_t y = radius;
    int32_t d = 3 - 2 * radius;
    while (x <= y) {
        DrawPixel(center.x + x, center.y + y);
        DrawPixel(center.x - x, center.y + y);
        DrawPixel(center.x + x, center.y - y);
        DrawPixel(center.x - x, center.y - y);
        DrawPixel(center.x + y, center.y + x);
        DrawPixel(center.x - y, center.y + x);
        DrawPixel(center.x + y, center.y - x);
        DrawPixel(center.x - y, center.y - x);
        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            --y;
        }
        ++x;
    }
}

void JKDC::Ellipse(JKPoint center, int32_t rx, int32_t ry) {
    if (!renderer_ || rx < 0 || ry < 0) return;
    if (rx == ry) {
        Circle(center, rx);
        return;
    }

    int64_t rx2 = static_cast<int64_t>(rx) * rx;
    int64_t ry2 = static_cast<int64_t>(ry) * ry;
    int64_t twoRx2 = 2 * rx2;
    int64_t twoRy2 = 2 * ry2;

    int32_t x = 0;
    int32_t y = ry;
    int64_t dx = 0;
    int64_t dy = twoRx2 * y;
    int64_t d1 = ry2 - rx2 * ry + rx2 / 4;

    while (dx < dy) {
        DrawPixel(center.x + x, center.y + y);
        DrawPixel(center.x - x, center.y + y);
        DrawPixel(center.x + x, center.y - y);
        DrawPixel(center.x - x, center.y - y);
        if (d1 < 0) {
            ++x;
            dx += twoRy2;
            d1 += dx + ry2;
        } else {
            ++x;
            --y;
            dx += twoRy2;
            dy -= twoRx2;
            d1 += dx - dy + ry2;
        }
    }

    int64_t d2 = ry2 * (static_cast<int64_t>(x) * x + x) +
                 rx2 * (static_cast<int64_t>(y) * y - y) -
                 rx2 * ry2;
    while (y >= 0) {
        DrawPixel(center.x + x, center.y + y);
        DrawPixel(center.x - x, center.y + y);
        DrawPixel(center.x + x, center.y - y);
        DrawPixel(center.x - x, center.y - y);
        if (d2 > 0) {
            --y;
            dy -= twoRx2;
            d2 += rx2 - dy;
        } else {
            --y;
            ++x;
            dx += twoRy2;
            d2 += dx + rx2 - dy;
        }
    }
}

void JKDC::Arc(JKPoint center, double startAngle, double endAngle, int32_t radius) {
    if (!renderer_ || radius < 0) return;
    if (startAngle == endAngle) return;
    const int32_t steps = std::max(16, static_cast<int32_t>(radius * 2));
    const double delta = (endAngle - startAngle) / steps;
    double x0 = center.x + radius * std::cos(startAngle);
    double y0 = center.y + radius * std::sin(startAngle);
    for (int i = 1; i <= steps; ++i) {
        double a = startAngle + delta * i;
        double x1 = center.x + radius * std::cos(a);
        double y1 = center.y + radius * std::sin(a);
        DrawLine(static_cast<int32_t>(x0), static_cast<int32_t>(y0),
                 static_cast<int32_t>(x1), static_cast<int32_t>(y1));
        x0 = x1;
        y0 = y1;
    }
}

void JKDC::Pieslice(JKPoint center, double startAngle, double endAngle, int32_t radius) {
    if (!renderer_ || radius <= 0) return;

    // Draw the arc.
    Arc(center, startAngle, endAngle, radius);

    // Lines from center to arc endpoints.
    double x0 = center.x + radius * std::cos(startAngle);
    double y0 = center.y + radius * std::sin(startAngle);
    double x1 = center.x + radius * std::cos(endAngle);
    double y1 = center.y + radius * std::sin(endAngle);
    DrawLine(center.x, center.y, static_cast<int32_t>(x0), static_cast<int32_t>(y0));
    DrawLine(center.x, center.y, static_cast<int32_t>(x1), static_cast<int32_t>(y1));

    // Fill via triangulation of the sector.
    const int32_t steps = std::max(8, static_cast<int32_t>(radius));
    const double delta = (endAngle - startAngle) / steps;
    for (int i = 0; i < steps; ++i) {
        double a0 = startAngle + delta * i;
        double a1 = startAngle + delta * (i + 1);
        std::vector<JKPoint> tri;
        tri.push_back(center);
        tri.push_back(JKPoint{ static_cast<int32_t>(center.x + radius * std::cos(a0)),
                               static_cast<int32_t>(center.y + radius * std::sin(a0)) });
        tri.push_back(JKPoint{ static_cast<int32_t>(center.x + radius * std::cos(a1)),
                               static_cast<int32_t>(center.y + radius * std::sin(a1)) });
        FillPolygon(tri);
    }
}

void JKDC::DrawPolygon(const std::vector<JKPoint>& points) {
    if (!renderer_ || points.size() < 2) return;
    std::vector<SDL_Point> sdlPoints;
    sdlPoints.reserve(points.size() + 1);
    for (const auto& p : points) {
        sdlPoints.push_back(SDL_Point{ p.x, p.y });
    }
    sdlPoints.push_back(sdlPoints.front());
    SDL_RenderDrawLines(renderer_, sdlPoints.data(), static_cast<int>(sdlPoints.size()));
}

void JKDC::FillPolygon(const std::vector<JKPoint>& points) {
    if (!renderer_ || points.size() < 3) return;

    int32_t minY = points[0].y;
    int32_t maxY = points[0].y;
    for (const auto& p : points) {
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    for (int32_t y = minY; y <= maxY; ++y) {
        std::vector<double> xs;
        const size_t n = points.size();
        for (size_t i = 0; i < n; ++i) {
            const JKPoint& a = points[i];
            const JKPoint& b = points[(i + 1) % n];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                if (a.y == b.y) continue;
                double x = a.x + static_cast<double>(y - a.y) * (b.x - a.x) / (b.y - a.y);
                xs.push_back(x);
            }
        }
        if (xs.size() >= 2) {
            std::sort(xs.begin(), xs.end());
            for (size_t i = 0; i + 1 < xs.size(); i += 2) {
                DrawLine(static_cast<int32_t>(xs[i]), y,
                         static_cast<int32_t>(xs[i + 1]), y);
            }
        }
    }
}

void JKDC::Rectangle3D(const JKRect& rect, int32_t depth,
                       uint8_t lightR, uint8_t lightG, uint8_t lightB,
                       uint8_t darkR, uint8_t darkG, uint8_t darkB) {
    if (!renderer_ || rect.IsEmpty() || depth <= 0) return;

    // Highlight: top and left outer edges.
    SetColor(lightR, lightG, lightB, 255);
    DrawLine(rect.x, rect.y, rect.x + rect.w - 1, rect.y);
    DrawLine(rect.x, rect.y, rect.x, rect.y + rect.h - 1);

    // Shadow: bottom and right outer edges.
    SetColor(darkR, darkG, darkB, 255);
    DrawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w - 1, rect.y + rect.h - 1);
    DrawLine(rect.x + rect.w - 1, rect.y, rect.x + rect.w - 1, rect.y + rect.h - 1);

    // Inner inverted edges for depth.
    if (depth > 1) {
        int32_t d = std::min<int32_t>(depth, std::min(rect.w, rect.h) / 2);
        int32_t x1 = rect.x + d;
        int32_t y1 = rect.y + d;
        int32_t x2 = rect.x + rect.w - 1 - d;
        int32_t y2 = rect.y + rect.h - 1 - d;

        SetColor(darkR, darkG, darkB, 255);
        DrawLine(x1, y1, x2, y1);
        DrawLine(x1, y1, x1, y2);

        SetColor(lightR, lightG, lightB, 255);
        DrawLine(x1, y2, x2, y2);
        DrawLine(x2, y1, x2, y2);
    }
}

void JKDC::Box3D(const JKRect& rect, int32_t depth,
                 uint8_t faceR, uint8_t faceG, uint8_t faceB,
                 uint8_t lightR, uint8_t lightG, uint8_t lightB,
                 uint8_t darkR, uint8_t darkG, uint8_t darkB) {
    if (!renderer_ || rect.IsEmpty()) return;

    int32_t d = std::max<int32_t>(0, depth);
    d = std::min<int32_t>(d, std::min(rect.w, rect.h) / 2);

    JKRect face{ rect.x + d, rect.y + d, rect.w - 2 * d, rect.h - 2 * d };
    if (!face.IsEmpty()) {
        SetColor(faceR, faceG, faceB, 255);
        FillRect(face);
    }

    Rectangle3D(rect, depth, lightR, lightG, lightB, darkR, darkG, darkB);
}

} // namespace jk

