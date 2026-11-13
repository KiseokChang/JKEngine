// src/text/JKTextParts.cpp — 벡터 텍스트 지연 부품 번들(docs/106 §3).
#include <text/JKTextParts.h>

#include <JKDC.h>
#include <JKHangulManager.h>
#include <JKResourceCache.h>
#include <JKSDLRenderBackend.h>
#include <JKTextAtlas.h>

#include <cstdio>
#include <string>

namespace jk {
namespace text {

FontParts::FontParts() = default;
FontParts::~FontParts() = default;

bool FontParts::Ensure(SDL_Renderer* renderer) {
    if (!renderer) return false;
    if (!font) font = std::make_unique<HangulManager>();
    if (!backend) backend = std::make_unique<JKSDLRenderBackend>(renderer);
    if (!cache) cache = std::make_unique<JKResourceCache>(backend.get());
    if (!atlas) {
        atlas = std::make_unique<JKTextAtlas>();
        const CellMetrics m = GetCellMetrics();
        const std::string fp = ResolveDesktopFontPath();
        if (fp.empty()) {
            std::fprintf(stderr, "[text] no vector font configured — bitmap "
                                 "glyphs stay\n");
        } else if (!atlas->Init(fp, m.engW, m.cellH, m.hanW)) {
            std::fprintf(stderr, "[text] vector font init failed (%s) — bitmap "
                                 "glyphs stay\n", fp.c_str());
        } else {
            const std::string fb = ResolveDesktopFallbackPath();
            if (!fb.empty() && !atlas->InitFallback(fb)) {
                std::fprintf(stderr, "[text] fallback font init failed (%s)\n",
                             fb.c_str());
            }
        }
    }
    return atlas && atlas->IsLoaded();
}

void FontParts::AttachTo(JKDC& dc) const {
    if (font) dc.SetHangulManager(font.get());
    if (atlas && cache && atlas->IsLoaded()) {
        dc.SetTextAtlas(atlas.get(), cache.get());
    }
}

bool FontParts::Ready() const { return atlas && atlas->IsLoaded(); }

void FontParts::Reset() {
    if (cache && backend) {
        cache->UnloadAllImages();
        cache->FlushUploads(backend.get());
    }
    atlas.reset();
    cache.reset();
    backend.reset();
    font.reset();
}

}  // namespace text
}  // namespace jk
