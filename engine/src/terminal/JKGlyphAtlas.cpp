#define STB_TRUETYPE_IMPLEMENTATION

#include <terminal/JKGlyphAtlas.h>
#include <terminal/JKTerminalGrid.h>
#include <JKResourceCache.h>
#include <stb_truetype.h>
#include <algorithm>
#include <cstdio>

namespace jk {

namespace {

// ASCII printable + box drawing / block elements.
constexpr uint32_t kRange1First = 0x20, kRange1Last = 0x7E;
constexpr uint32_t kRange2First = 0x2500, kRange2Last = 0x259F;

// Fallback-font coverage: CJK/wide blocks the primary (Consolas) font lacks.
// Pages are chunked so a texture is only built for a chunk actually touched.
struct FallbackRange { uint32_t first; uint32_t last; };
constexpr int kFbChunk = 512;          // glyphs per chunk page
constexpr int kFbCols = 32;            // slots per row in the page texture
constexpr FallbackRange kFbRanges[] = {
    { 0x1100, 0x11FF },   // Hangul Jamo
    { 0x2E80, 0xD7A3 },   // CJK radicals .. Hangul syllables
    { 0xF900, 0xFAFF },   // CJK compatibility ideographs
    { 0xFF00, 0xFF6F },   // Fullwidth / halfwidth forms
    { 0xFFE0, 0xFFE6 },   // Fullwidth signs
};
constexpr int kFbRangeCount =
    static_cast<int>(sizeof(kFbRanges) / sizeof(kFbRanges[0]));

} // namespace

JKGlyphAtlas::JKGlyphAtlas() = default;
JKGlyphAtlas::~JKGlyphAtlas() = default;

int JKGlyphAtlas::SlotOf(uint32_t cp) {
    if (cp >= kRange1First && cp <= kRange1Last) {
        return static_cast<int>(cp - kRange1First);
    }
    if (cp >= kRange2First && cp <= kRange2Last) {
        return static_cast<int>(kRange1Last - kRange1First + 1) +
               static_cast<int>(cp - kRange2First);
    }
    return -1;
}

int JKGlyphAtlas::Slots() const {
    return static_cast<int>(kRange1Last - kRange1First + 1) +
           static_cast<int>(kRange2Last - kRange2First + 1);
}

bool JKGlyphAtlas::Init(const std::string& fontPath, int cellW, int cellH) {
    cellW_ = std::max(4, cellW);
    cellH_ = std::max(8, cellH);
    info_.reset();

    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, fontPath.c_str(), "rb");
#else
    f = std::fopen(fontPath.c_str(), "rb");
#endif
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) { std::fclose(f); return false; }
    fontData_.resize(static_cast<size_t>(size));
    const size_t read = std::fread(fontData_.data(), 1, fontData_.size(), f);
    std::fclose(f);
    if (read != fontData_.size()) return false;

    info_ = std::make_unique<stbtt_fontinfo>();
    if (!stbtt_InitFont(info_.get(), fontData_.data(), 0)) {
        info_.reset();
        return false;
    }

    // Scale so the 'M' advance matches the cell width — for monospace fonts
    // this makes every glyph exactly one cell wide. Fall back to fitting
    // ascent+descent into the cell height.
    int advW = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(info_.get(), 'M', &advW, &lsb);
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(info_.get(), &ascent, &descent, &lineGap);
    if (advW > 0) {
        scale_ = static_cast<float>(cellW_) / static_cast<float>(advW);
    } else if (ascent - descent > 0) {
        scale_ = static_cast<float>(cellH_) / static_cast<float>(ascent - descent);
    } else {
        info_.reset();
        return false;
    }
    // The em box must not exceed the cell; shrink if it does (rare).
    const float emPx = static_cast<float>(ascent - descent) * scale_;
    if (emPx > static_cast<float>(cellH_) && ascent - descent > 0) {
        scale_ *= static_cast<float>(cellH_) / emPx;
    }
    baselineRow_ = static_cast<int>(
        (static_cast<float>(cellH_) - emPx) * 0.5f +
        static_cast<float>(ascent) * scale_);
    pages_.clear();
    return true;
}

bool JKGlyphAtlas::InitFallback(const std::string& fontPath) {
    fbInfo_.reset();
    fbScale_ = 0.0f;
    fbBaselineRow_ = 0;

    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, fontPath.c_str(), "rb");
#else
    f = std::fopen(fontPath.c_str(), "rb");
#endif
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) { std::fclose(f); return false; }
    fbFontData_.resize(static_cast<size_t>(size));
    const size_t read = std::fread(fbFontData_.data(), 1, fbFontData_.size(), f);
    std::fclose(f);
    if (read != fbFontData_.size()) { fbFontData_.clear(); return false; }

    fbInfo_ = std::make_unique<stbtt_fontinfo>();
    if (!stbtt_InitFont(fbInfo_.get(), fbFontData_.data(), 0)) {
        fbInfo_.reset();
        return false;
    }

    // Scale so a Hangul syllable's advance fills the 2-cell stride — wide
    // glyphs then visually match the cell grid. Fall back to fitting the em
    // box into the cell height.
    int advW = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(fbInfo_.get(), 0xAC00, &advW, &lsb);
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(fbInfo_.get(), &ascent, &descent, &lineGap);
    const float stride = static_cast<float>(cellW_) * 2.0f;
    if (advW > 0) {
        fbScale_ = stride / static_cast<float>(advW);
    } else if (ascent - descent > 0) {
        fbScale_ = static_cast<float>(cellH_) /
                   static_cast<float>(ascent - descent);
    } else {
        fbInfo_.reset();
        return false;
    }
    // The em box must not exceed the cell; shrink if it does (Malgun Gothic
    // hits this: its em is taller than two narrow cells).
    float emPx = static_cast<float>(ascent - descent) * fbScale_;
    if (emPx > static_cast<float>(cellH_) && ascent - descent > 0) {
        fbScale_ *= static_cast<float>(cellH_) / emPx;
        emPx = static_cast<float>(ascent - descent) * fbScale_;
    }
    fbBaselineRow_ = static_cast<int>(
        (static_cast<float>(cellH_) - emPx) * 0.5f +
        static_cast<float>(ascent) * fbScale_);
    return true;
}

int JKGlyphAtlas::FallbackChunkOf(uint32_t cp) {
    // Global chunk index across all fallback ranges (chunks never span
    // ranges) — the same indexing EnsureFallbackPage resolves back to a
    // codepoint window.
    int chunk = 0;
    for (int i = 0; i < kFbRangeCount; ++i) {
        if (cp >= kFbRanges[i].first && cp <= kFbRanges[i].last) {
            return chunk + static_cast<int>((cp - kFbRanges[i].first) / kFbChunk);
        }
        const uint32_t span = kFbRanges[i].last - kFbRanges[i].first + 1;
        chunk += static_cast<int>((span + kFbChunk - 1) / kFbChunk);
    }
    return -1;
}

int JKGlyphAtlas::FallbackSlotOf(uint32_t cp) {
    // Offset within the containing RANGE; % kFbChunk yields the slot inside
    // the chunk page.
    for (int i = 0; i < kFbRangeCount; ++i) {
        if (cp >= kFbRanges[i].first && cp <= kFbRanges[i].last) {
            return static_cast<int>(cp - kFbRanges[i].first);
        }
    }
    return -1;
}

std::string JKGlyphAtlas::PageKey(uint32_t fg, bool bold, uint32_t cp) const {
    char buf[48];
    if (SlotOf(cp) >= 0) {
        std::snprintf(buf, sizeof(buf), "termglyph_%08x_%c",
                      static_cast<unsigned>(fg), bold ? 'b' : 'n');
    } else {
        std::snprintf(buf, sizeof(buf), "termglyphfb_%d_%08x_%c",
                      FallbackChunkOf(cp), static_cast<unsigned>(fg),
                      bold ? 'b' : 'n');
    }
    return buf;
}

bool JKGlyphAtlas::EnsurePage(JKResourceCache* cache, uint32_t fg, bool bold,
                              uint32_t cp) {
    if (!cache) return false;
    if (SlotOf(cp) >= 0) return EnsurePrimaryPage(cache, fg, bold);
    if (fbInfo_ && FallbackChunkOf(cp) >= 0) {
        return EnsureFallbackPage(cache, fg, bold, FallbackChunkOf(cp));
    }
    return false;
}

bool JKGlyphAtlas::EnsurePrimaryPage(JKResourceCache* cache, uint32_t fg,
                                     bool bold) {
    if (!info_ || !cache) return false;

    uint64_t key = (static_cast<uint64_t>(fg) << 1) | (bold ? 1u : 0u);
    auto it = pages_.find(key);
    if (it != pages_.end()) return true;

    const int slots = Slots();
    const int pageW = slots * cellW_;
    std::vector<uint8_t> rgba(static_cast<size_t>(pageW) * cellH_ * 4, 0);

    const uint8_t cr = static_cast<uint8_t>((fg >> 16) & 0xFF);
    const uint8_t cg = static_cast<uint8_t>((fg >> 8) & 0xFF);
    const uint8_t cb = static_cast<uint8_t>(fg & 0xFF);

    // stbtt Make*/Get*Bitmap produce a tight box bitmap (row 0 = glyph top,
    // baseline at -iy0); compute the box and stamp it at the baseline.
    std::vector<uint8_t> cov;
    for (int slot = 0; slot < slots; ++slot) {
        const uint32_t cp = (slot < static_cast<int>(kRange1Last - kRange1First + 1))
            ? kRange1First + static_cast<uint32_t>(slot)
            : kRange2First + static_cast<uint32_t>(slot - (kRange1Last - kRange1First + 1));

        int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
        stbtt_GetCodepointBitmapBox(info_.get(), static_cast<int>(cp),
                                    scale_, scale_, &ix0, &iy0, &ix1, &iy1);
        int bw = ix1 - ix0;
        int bh = iy1 - iy0;
        if (bw > 0 && bh > 0) {
            bw = std::min(bw, cellW_);
            bh = std::min(bh, cellH_);
            cov.assign(static_cast<size_t>(bw) * bh, 0);
            stbtt_MakeCodepointBitmap(info_.get(), cov.data(), bw, bh, bw,
                                      scale_, scale_, static_cast<int>(cp));
            if (bold) {
                // Synthetic bold: dilate coverage one pixel to the right.
                for (int y = 0; y < bh; ++y) {
                    uint8_t* row = cov.data() + static_cast<size_t>(y) * bw;
                    for (int x = bw - 1; x > 0; --x) {
                        row[x] = static_cast<uint8_t>(std::max(row[x], row[x - 1]));
                    }
                }
            }
            // Stamp into the page: baseline at baselineRow_, left bearing
            // from the box, clipped to the cell.
            const int px0 = slot * cellW_ + std::max(0, std::min(ix0, cellW_ - 1));
            const int py0 = std::max(0, std::min(baselineRow_ + iy0, cellH_ - 1));
            for (int y = 0; y < bh; ++y) {
                const int py = py0 + y;
                if (py >= cellH_) break;
                for (int x = 0; x < bw; ++x) {
                    const int px = px0 + x;
                    if (px >= (slot + 1) * cellW_) break;
                    const uint8_t a = cov[static_cast<size_t>(y) * bw + x];
                    if (a == 0) continue;
                    uint8_t* dst = &rgba[(static_cast<size_t>(py) * pageW + px) * 4];
                    dst[0] = cr;
                    dst[1] = cg;
                    dst[2] = cb;
                    dst[3] = a;
                }
            }
        }
    }

    if (!cache->CreateImageFromRGBA(PageKey(fg, bold, 'W'), pageW, cellH_, rgba)) {
        return false;
    }
    pages_[key] = true;
    return true;
}

bool JKGlyphAtlas::RasterizeFallbackPage(uint32_t fg, bool bold, int chunk,
                                         std::vector<uint8_t>* rgba,
                                         int* pageW, int* pageH,
                                         uint32_t* chunkFirstOut) {
    if (!fbInfo_ || chunk < 0) return false;

    // Find the chunk's cp window: chunks never span ranges.
    uint32_t chunkFirst = 0;
    int chunkSlots = 0;
    {
        int remaining = chunk;
        for (int i = 0; i < kFbRangeCount; ++i) {
            const uint32_t span = kFbRanges[i].last - kFbRanges[i].first + 1;
            const int chunks = static_cast<int>((span + kFbChunk - 1) / kFbChunk);
            if (remaining < chunks) {
                chunkFirst = kFbRanges[i].first + static_cast<uint32_t>(remaining) * kFbChunk;
                chunkSlots = static_cast<int>(std::min<uint32_t>(
                    kFbChunk, kFbRanges[i].last - chunkFirst + 1));
                break;
            }
            remaining -= chunks;
        }
        if (chunkSlots <= 0) return false;
    }

    // Every slot gets the wide stride so narrow/wide glyphs can mix in a page.
    const int stride = cellW_ * 2;
    const int pw = kFbCols * stride;
    const int ph = ((kFbChunk + kFbCols - 1) / kFbCols) * cellH_;
    rgba->assign(static_cast<size_t>(pw) * ph * 4, 0);

    const uint8_t cr = static_cast<uint8_t>((fg >> 16) & 0xFF);
    const uint8_t cg = static_cast<uint8_t>((fg >> 8) & 0xFF);
    const uint8_t cb = static_cast<uint8_t>(fg & 0xFF);

    std::vector<uint8_t> cov;
    for (int slot = 0; slot < chunkSlots; ++slot) {
        const uint32_t cp = chunkFirst + static_cast<uint32_t>(slot);
        const int gw = (JKTermCharWidth(cp) == 2) ? cellW_ * 2 : cellW_;

        int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
        stbtt_GetCodepointBitmapBox(fbInfo_.get(), static_cast<int>(cp),
                                    fbScale_, fbScale_, &ix0, &iy0, &ix1, &iy1);
        int bw = ix1 - ix0;
        int bh = iy1 - iy0;
        if (bw <= 0 || bh <= 0) continue;
        bw = std::min(bw, gw);
        bh = std::min(bh, cellH_);
        cov.assign(static_cast<size_t>(bw) * bh, 0);
        stbtt_MakeCodepointBitmap(fbInfo_.get(), cov.data(), bw, bh, bw,
                                  fbScale_, fbScale_, static_cast<int>(cp));
        if (bold) {
            for (int y = 0; y < bh; ++y) {
                uint8_t* dilateRow = cov.data() + static_cast<size_t>(y) * bw;
                for (int x = bw - 1; x > 0; --x) {
                    dilateRow[x] = static_cast<uint8_t>(
                        std::max(dilateRow[x], dilateRow[x - 1]));
                }
            }
        }
        const int col = slot % kFbCols;
        const int row = slot / kFbCols;
        const int px0 = col * stride + std::max(0, std::min(ix0, gw - 1));
        // Baseline within the slot's OWN row band — glyph rows land in
        // [row*cellH_, (row+1)*cellH_).
        const int py0 = row * cellH_ +
                        std::max(0, std::min(fbBaselineRow_ + iy0, cellH_ - 1));
        for (int y = 0; y < bh; ++y) {
            const int py = py0 + y;
            if (py >= (row + 1) * cellH_) break;
            for (int x = 0; x < bw; ++x) {
                const int px = px0 + x;
                if (px >= col * stride + gw) break;
                const uint8_t a = cov[static_cast<size_t>(y) * bw + x];
                if (a == 0) continue;
                uint8_t* dst = &(*rgba)[(static_cast<size_t>(py) * pw + px) * 4];
                dst[0] = cr;
                dst[1] = cg;
                dst[2] = cb;
                dst[3] = a;
            }
        }
    }

    *pageW = pw;
    *pageH = ph;
    if (chunkFirstOut) *chunkFirstOut = chunkFirst;
    return true;
}

bool JKGlyphAtlas::EnsureFallbackPage(JKResourceCache* cache, uint32_t fg,
                                      bool bold, int chunk) {
    if (!cache || chunk < 0) return false;

    // Key: chunk below, fg/bold above — no collision with primary keys whose
    // chunk bits are zero.
    const uint64_t key = (static_cast<uint64_t>(fg) << 21) |
                         (static_cast<uint64_t>(bold ? 1u : 0u) << 20) |
                         static_cast<uint64_t>(chunk);
    auto it = pages_.find(key);
    if (it != pages_.end()) return true;

    std::vector<uint8_t> rgba;
    int pageW = 0, pageH = 0;
    uint32_t chunkFirst = 0;
    if (!RasterizeFallbackPage(fg, bold, chunk, &rgba, &pageW, &pageH,
                               &chunkFirst)) {
        return false;
    }
    if (!cache->CreateImageFromRGBA(PageKey(fg, bold, chunkFirst), pageW, pageH,
                                    rgba)) {
        return false;
    }
    pages_[key] = true;
    return true;
}

bool JKGlyphAtlas::RasterizeFallbackPageForTest(uint32_t fg, bool bold,
                                                uint32_t cp,
                                                std::vector<uint8_t>* rgba,
                                                int* w, int* h) {
    const int chunk = FallbackChunkOf(cp);
    if (chunk < 0) return false;
    return RasterizeFallbackPage(fg, bold, chunk, rgba, w, h, nullptr);
}

JKRect JKGlyphAtlas::GlyphSrc(uint32_t cp) const {
    const int slot = SlotOf(cp);
    if (slot >= 0) {
        return JKRect{ slot * cellW_, 0, cellW_, cellH_ };
    }
    if (fbInfo_) {
        const int chunk = FallbackChunkOf(cp);
        if (chunk >= 0 && FallbackSlotOf(cp) >= 0) {
            const int s = FallbackSlotOf(cp) % kFbChunk;   // slot within the chunk page
            const int stride = cellW_ * 2;
            const int col = s % kFbCols;
            const int row = s / kFbCols;
            const int gw = (JKTermCharWidth(cp) == 2) ? cellW_ * 2 : cellW_;
            return JKRect{ col * stride, row * cellH_, gw, cellH_ };
        }
    }
    return JKRect{ 0, 0, 0, 0 };
}

} // namespace jk