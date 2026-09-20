#ifndef JKTEXTATLAS_H
#define JKTEXTATLAS_H

// Desktop vector-font atlas (docs/63): stb_truetype-rasterized glyphs for the
// JKDC::TextOut path. Keeps the bitmap font's cell grid (English 8x16 /
// Hangul-wide 16x16) and its 8/16px advance, so all existing layout code is
// untouched. Each glyph becomes ONE small texture keyed (fg color, cp) in the
// JKResourceCache, with the fg color baked into the pixels (same as the
// terminal JKGlyphAtlas — the render backend has no per-blit tint).
// One font file, two scales: 'M' advance fills the 8px English stride, a
// Hangul syllable's advance fills the 16px stride (JKGlyphAtlas::InitFallback
// recipe).

struct stbtt_fontinfo;

#include <JKTypes.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace jk {

class JKResourceCache;

namespace text {

// Font path resolution (docs/63 §4.1): the settings.json `text.font_path`
// override wins as-is (a broken override fails visibly at Init — the spec's
// fail-safe is the bitmap fallback, not a silent default swap); with no
// override the platform default applies (Windows malgun.ttf, Linux Noto CJK
// candidates). Empty return = caller stays on the bitmap path.
std::string ResolveDesktopFontPath();

} // namespace text

class JKTextAtlas {
public:
    JKTextAtlas();
    ~JKTextAtlas();

    bool Init(const std::string& fontPath, int engCellW = 8, int cellH = 16,
              int hanCellW = 16);
    bool IsLoaded() const { return info_ != nullptr; }

    // Lazily rasterizes cp baked with `fg` and registers it in `cache`.
    // False when the font has no glyph for cp (caller falls back per glyph).
    bool EnsureGlyph(JKResourceCache* cache, uint32_t fg, uint32_t cp);

    // Cache key of the (fg, cp) glyph texture — valid after EnsureGlyph.
    std::string PageKey(uint32_t fg, uint32_t cp) const;

    // Source rect for (fg, cp); empty when not registered.
    JKRect GlyphSrc(uint32_t fg, uint32_t cp) const;

    // Test hook: rasterizes the single glyph into stride x cellH RGBA,
    // no resource cache involved. False when the font has no glyph.
    bool RasterizeGlyphForTest(uint32_t fg, uint32_t cp,
                               std::vector<uint8_t>* rgba, int* w, int* h);

private:
    int  StrideOf(uint32_t cp) const;
    float ScaleOf(uint32_t cp) const;
    int  BaselineOf(uint32_t cp) const;
    // Rasterizes cp into rgba (stride x cellH, baked fg, baseline aligned).
    bool RasterizeGlyph(uint32_t fg, uint32_t cp, std::vector<uint8_t>* rgba);

    std::vector<uint8_t> fontData_;
    std::unique_ptr<stbtt_fontinfo> info_;
    float engScale_ = 0.0f;
    float hanScale_ = 0.0f;
    int   engBaseline_ = 0;
    int   hanBaseline_ = 0;
    int   engCellW_ = 8;
    int   hanCellW_ = 16;
    int   cellH_ = 16;

    // Registered (fg, cp) set — mirrors what the cache holds.
    std::vector<uint64_t> registered_;   // (fg << 32) | cp
};

} // namespace jk

#endif // JKTEXTATLAS_H