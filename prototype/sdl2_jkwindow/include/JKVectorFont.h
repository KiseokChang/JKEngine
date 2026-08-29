#ifndef JKVECTORFONT_H
#define JKVECTORFONT_H

#include <JKDC.h>
#include <JKTypes.h>

#include <cstdint>
#include <memory>
#include <string>

namespace jk {

// Lightweight vector font engine for the original JKENGINE .VFT outline fonts.
// Parses FONTINFO/MAPENTRY tables and glyph opcodes (MOVETO/LINETO/CURVETO/ENDCHAR),
// applies a CTM (scale/rotate/slant), and rasterizes outlines via scanline
// edge-parity fill using existing JKDC primitives.
class JKVectorFont {
public:
    enum CodeArea {
        English = 0,
        Hangul  = 1,
        Hanja   = 2,
        Special = 3
    };

    explicit JKVectorFont(const std::string& fontDir);
    ~JKVectorFont();

    // Load a .VFT file into a code area / font id slot.
    bool LoadFont(const std::string& fileName, CodeArea area, int fontId);
    void SetFont(CodeArea area, int fontId);

    // Current transformation matrix (scale is also set by SetSize).
    void ResetCTM();
    void SetSize(int x, int y);
    void Rotate(int angle);
    void Slant(int angle);

    // Draw a byte string (EUC-KR / KSSM encoded) at p using the current text color.
    void DrawString(JKDC& dc, JKPoint p, const std::string& str);
    void DrawString(JKDC& dc, int x, int y, const std::string& str) {
        DrawString(dc, JKPoint{ x, y }, str);
    }

    // Metrics.
    int GetCharWidth(uint16_t ch);
    JKPoint MeasureString(const std::string& str);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // JKVECTORFONT_H
