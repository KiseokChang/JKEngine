// jktext_probe — 데스크탑 벡터 폰트(docs/63) 로직 단위 프로브.
// T1: KSSM 코드포인트 → 유니코드 변환 (Task 1).
// T2: 폰트 로드+셀 메트릭. T3: 래스터라이즈 잉크+AA(중간 알파>0=벡터). T4: 폰트 커버리지.
// T5: 캐시 등록 계약.
// T9: 글리프 텍스처 상한+LRU 폐기 (docs/63 §6, 2단계 Task 1).
// T6-T8: JKDC 배선 — 아틀라스 우선/비트맵 폴백/메트릭 불변 (Task 3).
#include <JKHangulUtil.h>
#include <JKTextAtlas.h>
#include <JKResourceCache.h>
#include <JKDC.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// JKTypes.h pulls SDL.h which renames main -> SDL_main (레슨 26: SDL_main 치환은 #undef main).
#undef main

// JKResourceCache 스텁 — 링크만 충족(레일즈: 프로브는 실 캐시를 링크하지 않는다).
static std::vector<std::string> g_created;
static std::vector<std::string> g_evicted;
jk::JKResourceCache::JKResourceCache(jk::JKRenderBackend*) {}
jk::JKResourceCache::~JKResourceCache() = default;
bool jk::JKResourceCache::CreateImageFromRGBA(const std::string& key, int, int,
                                              const std::vector<uint8_t>&) {
    g_created.push_back(key);
    return true;
}
void jk::JKResourceCache::UnloadImage(const std::string& key) {
    for (size_t i = 0; i < g_created.size(); ++i) {
        if (g_created[i] == key) {
            g_created.erase(g_created.begin() + static_cast<ptrdiff_t>(i));
            break;
        }
    }
    g_evicted.push_back(key);
}
bool jk::JKResourceCache::HasImage(const std::string& key) const {
    for (const auto& k : g_created) if (k == key) return true;
    return false;
}
jk::JKRenderBackend::TextureHandle
jk::JKResourceCache::GetImage(const std::string&) const {
    return g_created.empty()
               ? nullptr
               : reinterpret_cast<jk::JKRenderBackend::TextureHandle>(0x1);
}
void jk::JKResourceCache::FlushUploads(jk::JKRenderBackend*) {}

using namespace jk;

// T6-T8: JKDC 배선 — 녹화 백엔드로 BlitTexture/DrawPixel을 관찰한다.
namespace {
class RecordingBackend : public jk::JKRenderBackend {
public:
    void* GetNativeHandle() const override { return nullptr; }
    void SetScale(float, float) override {}
    void GetOutputSize(int& w, int& h) override { w = 640; h = 480; }
    void SetDrawColor(uint8_t, uint8_t, uint8_t, uint8_t) override {}
    void Clear() override {}
    void Present() override {}
    void SetClipRect(const jk::JKRect*) override {}
    void DrawRect(const JKRect&) override {}
    void FillRect(const JKRect&) override {}
    void DrawLine(int32_t, int32_t, int32_t, int32_t) override {}
    void DrawPixel(int32_t, int32_t) override { ++drawPixels; }
    void DrawPoints(const jk::JKPoint*, size_t) override {}
    void DrawPolygon(const std::vector<jk::JKPoint>&) override {}
    jk::JKRenderBackend::TextureHandle CreateTargetTexture(int, int) override {
        return reinterpret_cast<jk::JKRenderBackend::TextureHandle>(0x2);
    }
    void DestroyTexture(jk::JKRenderBackend::TextureHandle) override {}
    void SetRenderTarget(jk::JKRenderBackend::TextureHandle) override {}
    void BlitTexture(jk::JKRenderBackend::TextureHandle texture,
                     const jk::JKRect* src, const jk::JKRect& dst,
                     uint8_t) override {
        blits.push_back({ texture, src ? *src : jk::JKRect{0, 0, 0, 0}, dst });
    }
    struct Blit { void* tex; jk::JKRect src; jk::JKRect dst; };
    std::vector<Blit> blits;
    int drawPixels = 0;
};
} // namespace

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                       \
    do {                                       \
        if (cond) { ++g_pass; }                \
        else { ++g_fail; std::printf("FAIL: %s\n", msg); } \
    } while (0)

int main() {
    // T1: KSSM→cp. 2바이트 쌍은 Utf8ToKssm 라운드트립으로 구한다.
    const std::string kHan = Utf8ToKssm("한");   // 완성형 음절
    const std::string kGa  = Utf8ToKssm("가");
    const std::string kHanja = Utf8ToKssm("漢"); // 한자 영역
    CHECK(kHan.size() == 2 && kGa.size() == 2 && kHanja.size() == 2,
          "T1 fixture: kssm pairs are 2 bytes");
    CHECK(KssmCodepointToUnicode(static_cast<uint8_t>(kHan[0]),
                                 static_cast<uint8_t>(kHan[1])) == 0xD55C,
          "T1 한 -> U+D55C");
    CHECK(KssmCodepointToUnicode(static_cast<uint8_t>(kGa[0]),
                                 static_cast<uint8_t>(kGa[1])) == 0xAC00,
          "T1 가 -> U+AC00");
    CHECK(KssmCodepointToUnicode(static_cast<uint8_t>(kHanja[0]),
                                 static_cast<uint8_t>(kHanja[1])) == 0x6F22,
          "T1 漢 -> U+6F22");
    // 매핑 없는 쌍(예약 영역) — 0 반환으로 폴백 신호.
    CHECK(KssmCodepointToUnicode(0xFF, 0xFE) == 0, "T1 unmapped pair -> 0");

    // T2: 초기화 — 맑은 고딕, 셀 메트릭.
    JKTextAtlas atlas;
    const std::string fontPath = text::ResolveDesktopFontPath();
    if (fontPath.empty()) {
        std::printf("SKIP: no desktop vector font on this platform\n");
        return 77;
    }
    if (!atlas.Init(fontPath, 8, 16, 16)) {
        std::printf("SKIP: vector font init failed (%s)\n", fontPath.c_str());
        return 77;
    }
    CHECK(atlas.IsLoaded(), "T2 loaded");

    // T3: 래스터라이즈 잉크+AA — 한글 음절/한자/영문. 반환값: 0=잉크 없음/미커버,
    // 1=잉크 있으나 중간 알파 없음(비트맵형 계단), 2=잉크+AA(벡터 래스터라이즈).
    auto HasInk = [&](uint32_t cp, int stride) {
        std::vector<uint8_t> rgba;
        int w = 0, h = 0;
        if (!atlas.RasterizeGlyphForTest(0x000000, cp, &rgba, &w, &h)) return 0;
        if (w != stride || h != 16) return -1;
        int ink = 0, aa = 0;
        for (size_t i = 3; i < rgba.size(); i += 4) {
            if (rgba[i] > 0) ++ink;
            if (rgba[i] > 8 && rgba[i] < 247) ++aa;   // 중간값 = 안티에일리어싱
        }
        if (ink == 0) return 0;
        return (aa > 0) ? 2 : 1;
    };
    CHECK(HasInk(0xAC00, 16) == 2, "T3 가 ink+AA (16px stride)");
    CHECK(HasInk(0x6F22, 16) == 2, "T3 漢 ink+AA");
    CHECK(HasInk('A', 8) == 2, "T3 'A' ink+AA (8px stride)");

    // T4: 폰트에 없는 글자 → false. 뷁(KS X 1002)은 커버 여부를 폰트가 결정.
    const int bbaelk = HasInk(0xBDF7, 16);
    if (bbaelk == 0) {
        std::printf("INFO: 뷁 U+BDF7 not covered — no-glyph false path\n");
    } else {
        CHECK(bbaelk >= 1, "T4 뷁 ink (맑은 고딕 KS X 1002)");
    }

    // T5: 캐시 등록 계약 — (fg,cp) 유니크 키, 재요청 멱등.
    JKResourceCache cache(nullptr);
    CHECK(atlas.EnsureGlyph(&cache, 0xCCCCCC, 0xAC00), "T5 ensure 가");
    CHECK(atlas.EnsureGlyph(&cache, 0xCCCCCC, 0xAC00), "T5 ensure 가 멱등");
    CHECK(atlas.PageKey(0xCCCCCC, 0xAC00) ==
              std::string("desktext_00cccccc_00ac00"), "T5 page key format");
    CHECK(atlas.GlyphSrc(0xCCCCCC, 0xAC00).w == 16, "T5 src wide");
    CHECK(atlas.GlyphSrc(0xCCCCCC, 'A').w == 0, "T5 미등록 글리프 빈 rect");
    CHECK(g_created.size() == 1, "T5 재요청시 캐시 등록 1회만");
    // 폰트에 없는 cp는 false — 콜러 폴백 계약(헤더 주석).
    CHECK(!atlas.EnsureGlyph(&cache, 0xCCCCCC, 0x10FFFF),
          "T5 미커버 cp -> false (폴백 계약)");

    // T9: 글리프 텍스처 상한+LRU (docs/63 §6) — fg 12색 x ASCII 0x21..0x7E(94개)
    // = 1128 등록 > kMaxGlyphTextures(1024). 폐기는 등록 순서 기준 LRU(front).
    {
        JKTextAtlas a3;
        if (!a3.Init(fontPath, 8, 16, 16)) {
            std::printf("SKIP: vector font init failed (%s)\n", fontPath.c_str());
            return 77;
        }
        static const uint32_t kFg[12] = {
            0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00,
            0x00FFFF, 0xFF00FF, 0x808080, 0x336699, 0xCCCCCC, 0x556677
        };
        const size_t kFill = sizeof(kFg) / sizeof(kFg[0]);
        bool allOk = true;
        for (size_t f = 0; f < kFill && allOk; ++f) {
            for (uint32_t cp = 0x21; cp <= 0x7E; ++cp) {
                if (!a3.EnsureGlyph(&cache, kFg[f], cp)) { allOk = false; break; }
            }
        }
        CHECK(allOk, "T9 배치 EnsureGlyph 전부 성공");
        const size_t cap = JKTextAtlas::kMaxGlyphTextures;
        size_t live = 0;
        for (size_t f = 0; f < kFill; ++f) {
            for (uint32_t cp = 0x21; cp <= 0x7E; ++cp) {
                if (a3.GlyphSrc(kFg[f], cp).w != 0) ++live;
            }
        }
        CHECK(live == cap, "T9(a) 상한 — 등록 글리프 수 == kMaxGlyphTextures(1024)");

        // (b) LRU 순서: 가장 먼저 등록한 (kFg[0], 0x21)이 폐기되어 GlyphSrc 비었고,
        // 재 EnsureGlyph는 true(재등록) — 폐기 기록은 캐시 스텁에 남는다.
        const std::string oldestKey = a3.PageKey(kFg[0], 0x21);
        CHECK(a3.GlyphSrc(kFg[0], 0x21).w == 0, "T9(b) 최연장 글리프 폐기 — GlyphSrc 빈 rect");
        bool evictedRecorded = false;
        for (const auto& k : g_evicted) {
            if (k == oldestKey) { evictedRecorded = true; break; }
        }
        CHECK(evictedRecorded, "T9(b) 최연장 키 UnloadImage 기록");
        CHECK(a3.EnsureGlyph(&cache, kFg[0], 0x21), "T9(b) 폐기 후 재등록 true");
        CHECK(a3.GlyphSrc(kFg[0], 0x21).w == 8 && cache.HasImage(oldestKey),
              "T9(b) 재등록 후 GlyphSrc 복원+캐시 재등록");
        size_t live2 = 0;
        for (size_t f = 0; f < kFill; ++f) {
            for (uint32_t cp = 0x21; cp <= 0x7E; ++cp) {
                if (a3.GlyphSrc(kFg[f], cp).w != 0) ++live2;
            }
        }
        CHECK(live2 == cap, "T9(b) 재등록 후에도 상한 유지(재폐기 1건)");
    }

    // T6: 아틀라스 장착 → TextOut("가A") = 블릿 2회(한글 16px + 영문 8px),
    // DrawPixel 스톰 없음.
    {
        RecordingBackend be;
        JKDC dc(&be);
        JKResourceCache cache(&be);
        JKTextAtlas a2;
        if (!a2.Init(text::ResolveDesktopFontPath(), 8, 16, 16)) {
            std::printf("SKIP: vector font init failed (%s)\n",
                        fontPath.c_str());
            return 77;
        }
        dc.SetTextAtlas(&a2, &cache);
        dc.SetTextColor(0x33, 0x66, 0x99);
        dc.TextOut(JKPoint{10, 10}, Utf8ToKssm("가A").c_str());
        CHECK(be.blits.size() == 2, "T6 two glyph blits");
        if (be.blits.size() == 2) {
            CHECK(be.blits[0].dst.w == 16 && be.blits[0].dst.h == 16,
                  "T6 wide glyph cell 16x16");
            CHECK(be.blits[1].dst.w == 8 && be.blits[1].dst.h == 16,
                  "T6 ascii glyph cell 8x16");
        }
        CHECK(be.drawPixels < 10, "T6 no bitmap pixel storm");
        CHECK(a2.GlyphSrc(0x336699, 0xAC00).w == 16, "T6 fg baked key");
    }
    // T7: 아틀라스 미장착 → 기존 비트맵 경로 (DrawPixel 스톰).
    {
        RecordingBackend be;
        JKDC dc(&be);
        dc.SetTextColor(0, 0, 0);
        dc.TextOut(JKPoint{10, 10}, "A");
        CHECK(be.blits.empty(), "T7 no atlas -> no blits");
        CHECK(be.drawPixels > 0, "T7 bitmap path still active");
    }
    // T8: MeasureText 불변 회귀 — KSSM 완성형 쌍 16px, ASCII 8px, 높이 16.
    {
        const std::string mixed = Utf8ToKssm("가나AB");
        CHECK(JKDC::MeasureText(mixed.c_str()).x == 16 * 2 + 8 * 2,
              "T8 metrics unchanged");
        CHECK(JKDC::MeasureText(mixed.c_str()).y == 16, "T8 cell height 16");
    }

    std::printf("PASS %d FAIL %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}