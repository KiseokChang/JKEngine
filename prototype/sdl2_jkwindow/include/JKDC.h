#ifndef JKDC_H
#define JKDC_H

#include <JKTypes.h>
#include <SDL.h>
#include <cstdint>
#include <cstddef>
#include <vector>

namespace jk {

class HangulManager;

class JKDC {
public:
    explicit JKDC(SDL_Renderer* renderer);

    void SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void SetTextColor(uint8_t r, uint8_t g, uint8_t b);
    void SetBackColor(uint8_t r, uint8_t g, uint8_t b);
    void Clear();
    void Present();

    // Clipping helpers (coordinates are in the current SDL renderer coordinate space).
    void PushClipRect(const JKRect& rect);
    void PopClipRect();

    void DrawRect(const JKRect& rect);
    void FillRect(const JKRect& rect);
    void DrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
    void DrawPixel(int32_t x, int32_t y);

    // Primitive shapes (use the current draw color set by SetColor()).
    void Circle(JKPoint center, int32_t radius);
    void Ellipse(JKPoint center, int32_t rx, int32_t ry);
    void Arc(JKPoint center, double startAngle, double endAngle, int32_t radius);
    void Pieslice(JKPoint center, double startAngle, double endAngle, int32_t radius);
    void DrawPolygon(const std::vector<JKPoint>& points);
    void FillPolygon(const std::vector<JKPoint>& points);
    void SolidBar(const JKRect& rect);

    // 3D-styled rectangles (colors are explicit, independent of SetColor).
    void Rectangle3D(const JKRect& rect, int32_t depth,
                       uint8_t lightR = 255, uint8_t lightG = 255, uint8_t lightB = 255,
                       uint8_t darkR = 0,   uint8_t darkG = 0,   uint8_t darkB = 0);
    void Box3D(const JKRect& rect, int32_t depth,
               uint8_t faceR = 192,  uint8_t faceG = 192,  uint8_t faceB = 192,
               uint8_t lightR = 255, uint8_t lightG = 255, uint8_t lightB = 255,
               uint8_t darkR = 0,    uint8_t darkG = 0,    uint8_t darkB = 0);

    // Bitmap-font text output. Strings are interpreted as byte sequences:
    //   - bytes < 0x80        -> 8-pixel wide ASCII glyph
    //   - 0x80 byte pairs     -> 16-pixel wide Hangul glyph (EUC-KR style)
    void TextOut(JKPoint p, const char* str);
    void TextOut(JKPoint p, size_t n, const char* str);
    void TextOutX(const JKRect& rect, const char* str, uint8_t adjflag = 0, bool wrapping = false);

    void EngPutCh(JKPoint p, uint8_t ch);
    void HanPutCh(JKPoint p, uint8_t first, uint8_t second);
    void PutCh(JKPoint p, uint16_t code);

    void SetHangulManager(HangulManager* hm) { fontMan_ = hm; }
    HangulManager* GetHangulManager() const { return fontMan_; }

    SDL_Renderer* GetRenderer() const { return renderer_; }

private:
    SDL_Renderer* renderer_ = nullptr;
    HangulManager* fontMan_ = nullptr;

    uint8_t textR_ = 0;
    uint8_t textG_ = 0;
    uint8_t textB_ = 0;
    uint8_t backR_ = 255;
    uint8_t backG_ = 255;
    uint8_t backB_ = 255;

    struct SavedClip {
        bool enabled = false;
        SDL_Rect rect = { 0, 0, 0, 0 };
    };
    std::vector<SavedClip> clipStack_;

    void TextOutInRect(const JKRect& rect, JKPoint p, size_t n, const char* str);
    void PutEngGlyph8x8(JKPoint p, uint8_t ch);
    void PutEngGlyph8x16(JKPoint p, const uint8_t* image);
    void PutHanGlyph16x16(JKPoint p, const uint8_t* buffer);
};

} // namespace jk

#endif // JKDC_H
