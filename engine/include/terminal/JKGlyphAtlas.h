#ifndef JKGLYPHATLAS_H
#define JKGLYPHATLAS_H

// Terminal glyph atlas (docs/22 §5.1): stb_truetype-rasterized monospace cells
// uploaded through JKResourceCache as one texture page per (fg color, bold)
// pair. The terminal view blits src-rects out of the page; the render backend
// has no per-blit tint, so each page bakes the fg color into the pixels.
//
// Ranges: ASCII 0x20-0x7E plus box drawing/blocks 0x2500-0x259F (ConPTY
// repaints use both), from the primary font. Wide/CJK codepoints
// (JKTermCharWidth > 1 and friends) come from an OPTIONAL secondary font
// (InitFallback, e.g. Malgun Gothic) served in lazily-rasterized chunk pages
// (docs/26 단계 1). Codepoints with no slot get a placeholder rect.

// stb_truetype.h is included only in JKGlyphAtlas.cpp (its implementation
// section is not include-guarded); here stbtt_fontinfo stays incomplete and
// the constructor/destructor are defined out-of-line so client TUs holding
// unique_ptr<JKGlyphAtlas> never need the full type.
struct stbtt_fontinfo;

#include <JKTypes.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace jk {

class JKResourceCache;

class JKGlyphAtlas {
public:
    // Out-of-line: keeps stbtt_fontinfo incomplete for client TUs.
    JKGlyphAtlas();
    ~JKGlyphAtlas();

    // Loads the font file into memory and computes cell metrics. cellW/cellH
    // are logical pixels (8x16 for the terminal). Returns false when the font
    // cannot be read (view falls back to placeholder rectangles).
    bool Init(const std::string& fontPath, int cellW, int cellH);

    // Optional secondary font for CJK/wide glyphs (e.g. malgun.ttf). Failure
    // only degrades those glyphs to placeholders; the primary font stays.
    bool InitFallback(const std::string& fontPath);

    bool IsLoaded() const { return info_ != nullptr; }
    bool HasFallback() const { return fbInfo_ != nullptr; }

    // Lazily rasterizes + registers the page covering cp for (fg, bold) into
    // the cache. Returns false only when neither font has a slot for cp.
    bool EnsurePage(JKResourceCache* cache, uint32_t fg, bool bold, uint32_t cp);

    // Cache key of the page covering cp (valid after a successful EnsurePage).
    std::string PageKey(uint32_t fg, bool bold, uint32_t cp) const;

    // Source rect within the page texture for cp; empty when cp has no slot.
    JKRect GlyphSrc(uint32_t cp) const;

    // Diagnostic hook (self-test): rasterizes the fallback chunk page
    // covering cp into tightly packed RGBA, no resource cache involved.
    // False when cp has no fallback slot.
    bool RasterizeFallbackPageForTest(uint32_t fg, bool bold, uint32_t cp,
                                      std::vector<uint8_t>* rgba, int* w,
                                      int* h);

    int CellW() const { return cellW_; }
    int CellH() const { return cellH_; }

private:
    static int  SlotOf(uint32_t cp);
    int         Slots() const;
    // Fallback chunk page identity: -1 when cp has no fallback slot.
    static int  FallbackChunkOf(uint32_t cp);
    static int  FallbackSlotOf(uint32_t cp);

    // Page builders (one cache texture per pages_ key). Main-thread only.
    bool EnsurePrimaryPage(JKResourceCache* cache, uint32_t fg, bool bold);
    bool EnsureFallbackPage(JKResourceCache* cache, uint32_t fg, bool bold,
                            int chunk);
    // Rasterizes one fallback chunk page into RGBA (no cache). Outputs the
    // page dims and the chunk's first codepoint (for the cache key).
    bool RasterizeFallbackPage(uint32_t fg, bool bold, int chunk,
                               std::vector<uint8_t>* rgba, int* pageW,
                               int* pageH, uint32_t* chunkFirstOut);

    std::vector<uint8_t> fontData_;
    std::unique_ptr<stbtt_fontinfo> info_;
    float scale_ = 0.0f;
    int   baselineRow_ = 0;     // baseline within a cell (from cell top)
    int   cellW_ = 8;
    int   cellH_ = 16;

    std::vector<uint8_t> fbFontData_;
    std::unique_ptr<stbtt_fontinfo> fbInfo_;
    float fbScale_ = 0.0f;
    int   fbBaselineRow_ = 0;

    std::unordered_map<uint64_t, bool> pages_;
};

} // namespace jk

#endif // JKGLYPHATLAS_H