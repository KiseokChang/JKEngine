// jktext_probe — 데스크탑 벡터 폰트(docs/63) 로직 단위 프로브.
// T1: KSSM 코드포인트 → 유니코드 변환 (Task 1).
// T2: 폰트 로드+셀 메트릭. T3: 래스터라이즈 잉크+AA(중간 알파>0=벡터). T4: 폰트 커버리지.
// T5: 캐시 등록 계약.
// T9: 글리프 텍스처 상한+LRU 폐기 (docs/63 §6, 2단계 Task 1) + T9(c) LRU≠FIFO.
// T10: 보조 폰트 체인 — 미커버 cp 승계+캐시 키 접두어 분리 (2단계 Task 2).
// T6-T8: JKDC 배선 — 아틀라스 우선/비트맵 폴백/메트릭 불변 (Task 3).
// T11: 셀 메트릭 진실원 — ComputeCellMetrics 산출식+클램프, 기본 1.0 (Task 3).
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

    // T9(c): LRU가 FIFO가 아님 — 터치 이벤트가 폐기 순서를 바꾼다(1단계 리뷰
    // MINOR 보충). 채움(12색×94=1128) 후 폐기 104건 = (fg0 전체 94건 +
    // (fg1, 0x21..0x2A)) → 최연장 생존 키 = (fg1, 0x2B). 그 키를 터치(MRU 이동)
    // → 신규 등록 3건. LRU면 터치된 키가 생존하고 터치 시점의 다음
    // front((fg1, 0x2C))가 폐기된다. FIFO였다면 터치가 무시되어 (fg1, 0x2B)
    // 자체가 폐기됐을 것이다.
    {
        JKTextAtlas a4;
        if (!a4.Init(fontPath, 8, 16, 16)) {
            std::printf("SKIP: vector font init failed (%s)\n", fontPath.c_str());
            return 77;
        }
        static const uint32_t kFg2[12] = {
            0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00,
            0x00FFFF, 0xFF00FF, 0x808080, 0x336699, 0xCCCCCC, 0x556677
        };
        for (size_t f = 0; f < 12; ++f) {
            for (uint32_t cp = 0x21; cp <= 0x7E; ++cp) {
                if (!a4.EnsureGlyph(&cache, kFg2[f], cp)) break;
            }
        }
        const uint32_t kTouchedFg = kFg2[1];
        const uint32_t kTouchedCp = 0x2B;   // 채움 후 live front(폐기 104건)
        CHECK(a4.GlyphSrc(kTouchedFg, kTouchedCp).w != 0,
              "T9(c) 사전조건 — 터치 대상이 최연장 생존 키");
        const size_t evBase = g_evicted.size();
        CHECK(a4.EnsureGlyph(&cache, kTouchedFg, kTouchedCp),
              "T9(c) 터치 히트 true");
        // 터치 후 신규 등록 3건 — 기존 fg 집합에 없던 색+ASCII(커버 확정).
        for (uint32_t cp = 0x61; cp <= 0x63; ++cp) {
            CHECK(a4.EnsureGlyph(&cache, 0x123456, cp), "T9(c) 신규 등록 true");
        }
        CHECK(a4.GlyphSrc(kTouchedFg, kTouchedCp).w != 0,
              "T9(c) 터치 키 생존 — LRU가 FIFO가 아님");
        bool frontEvicted = false, touchedEvicted = false;
        for (size_t i = evBase; i < g_evicted.size(); ++i) {
            if (g_evicted[i] == a4.PageKey(kFg2[1], 0x2C)) frontEvicted = true;
            if (g_evicted[i] == a4.PageKey(kTouchedFg, kTouchedCp)) touchedEvicted = true;
        }
        CHECK(frontEvicted, "T9(c) 터치 후 최연장(front) 폐기 — 터치가 순서를 바꿈");
        CHECK(!touchedEvicted, "T9(c) 터치 키 미폐기");
    }

    // T10: 보조 폰트 체인 (docs/63 §6 2단계 Task 2) — primary=consola.ttf
    // (한글 글리프 없음 — 하드코딩 가정 금지, FindGlyphIndex 경유 선체크),
    // fallback=맑은 고딕(기본 경로). 체인: 1차 EnsureGlyph 실패 → fallback 페이지
    // 등록(키 접두어 desktextf_ — 1차 키와 충돌 봉쇄).
    {
        JKTextAtlas ac;
        if (!ac.Init("C:/Windows/Fonts/consola.ttf", 8, 16, 16)) {
            std::printf("SKIP: consola.ttf unavailable — T10 not exercised\n");
        } else {
            JKResourceCache cache2(nullptr);
            // 선체크: consola가 '가'를 커버한다면 체인 전제가 깨진다 — 스킵.
            {
                std::vector<uint8_t> rgba;
                int w = 0, h = 0;
                if (ac.RasterizeGlyphForTest(0x000000, 0xAC00, &rgba, &w, &h)) {
                    std::printf("SKIP: primary covers Hangul — chain premise "
                                "violated, T10 not exercised\n");
                } else {
                    CHECK(!ac.EnsureGlyph(&cache2, 0xCCCCCC, 0xAC00),
                          "T10 선체크 — consola '가' 미커버(Rasterize false)");
                    // 체인 미설정(InitFallback 전): fallback 요청도 false.
                    CHECK(!ac.EnsureGlyph(&cache2, 0xCCCCCC, 0xAC00, true),
                          "T10 체인 미설정 — fallback 요청 false");
                    CHECK(!ac.InitFallback("C:\\nonexistent_font_dir\\x.ttf"),
                          "T10 InitFallback 파일 부재 -> false(체인 없음)");
                    CHECK(ac.InitFallback(fontPath),
                          "T10 InitFallback(맑은 고딕) true");
                    CHECK(ac.IsLoaded() && ac.IsFallbackLoaded(),
                          "T10 두 면 로드");
                    CHECK(!ac.EnsureGlyph(&cache2, 0xCCCCCC, 0xAC00),
                          "T10(a) 1차 '가' 실패(consola 미커버)");
                    CHECK(ac.EnsureGlyph(&cache2, 0xCCCCCC, 0xAC00, true),
                          "T10(b) fallback '가' 등록 true");
                    CHECK(ac.GlyphSrc(0xCCCCCC, 0xAC00).w == 0,
                          "T10 1차 등록 아님 — face 키 분리");
                    CHECK(ac.GlyphSrc(0xCCCCCC, 0xAC00, true).w == 16,
                          "T10(c) fallback GlyphSrc 16px");
                    CHECK(ac.PageKey(0xCCCCCC, 0xAC00, true) ==
                              std::string("desktextf_00cccccc_00ac00"),
                          "T10(c) fallback 키 접두어 desktextf_");
                    CHECK(ac.PageKey(0xCCCCCC, 'A') ==
                              std::string("desktext_00cccccc_000041"),
                          "T10 1차 키 접두어 불변");
                    CHECK(ac.EnsureGlyph(&cache2, 0xCCCCCC, 'A'),
                          "T10(d) ASCII는 1차 성공(체인 미개입)");
                    CHECK(ac.GlyphSrc(0xCCCCCC, 'A').w == 8,
                          "T10(d) ASCII 1차 등록(8px)");
                }
            }
        }
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
    // T11: 셀 메트릭 진실원 (docs/63 §6 Task 3, text.font_scale 옵트인).
    // (a) 순수 산출 함수 단정 — 설정 개입 없음. (b) 기본 경유 — 이 프로브 exe
    // 옆 tools/probes/state\settings.json이 없어 font_scale 미설정 = 1.0.
    {
        using jk::text::ComputeCellMetrics;
        const jk::text::CellMetrics c10 = ComputeCellMetrics(1.0f);
        CHECK(c10.engW == 8 && c10.hanW == 16 && c10.cellH == 16,
              "T11(a) ComputeCellMetrics(1.0) == {8,16,16}");
        const jk::text::CellMetrics c125 = ComputeCellMetrics(1.25f);
        CHECK(c125.engW == 10 && c125.hanW == 20 && c125.cellH == 20,
              "T11(a) ComputeCellMetrics(1.25) == {10,20,20}");
        const jk::text::CellMetrics c30 = ComputeCellMetrics(3.0f);
        CHECK(c30.engW == 24 && c30.hanW == 48 && c30.cellH == 48,
              "T11(a) ComputeCellMetrics(3.0) == {24,48,48}");
        const jk::text::CellMetrics c05 = ComputeCellMetrics(0.5f);
        CHECK(c05.engW == 8 && c05.hanW == 16 && c05.cellH == 16,
              "T11(a) ComputeCellMetrics(0.5) == {8,16,16} (하한 클램프)");
        const jk::text::CellMetrics cHi = ComputeCellMetrics(99.0f);
        CHECK(cHi.engW == 24 && cHi.hanW == 48 && cHi.cellH == 48,
              "T11(a) ComputeCellMetrics(99) == 상한 클램프 {24,48,48}");
        const jk::text::CellMetrics& live = jk::text::GetCellMetrics();
        CHECK(live.engW == 8 && live.hanW == 16 && live.cellH == 16,
              "T11(b) GetCellMetrics() 미설정 기본 {8,16,16}");
        // (c) fractional 불변식(docs/65 O4): hanW는 engW 유도(2×) — 독자
        // 반올림이던 옛 산출식은 1.2에서 hanW=19 vs 2×engW=20으로 어긋나
        // JKEdit 쌍 매핑이 셀당 최대 1px 표류했다. 정수·비정수 전역 단정.
        for (float fs : { 1.05f, 1.2f, 1.6f, 1.7f, 2.1f, 2.5f, 2.9f }) {
            const jk::text::CellMetrics c = ComputeCellMetrics(fs);
            if (c.hanW != 2 * c.engW) {
                std::printf("INFO: scale %.2f engW=%d hanW=%d\n", fs, c.engW,
                            c.hanW);
            }
            CHECK(c.hanW == 2 * c.engW,
                  "T11(c) ComputeCellMetrics hanW == 2*engW (fractional)");
        }
    }

    // T12: 기호 행 왕복(docs/65 O5) — KS X 1001 A1-A2 기호(■□●◆)는 조합형과
    // 바이트 동일이라 Utf8ToKssm이 등가 매핑한다. 옛 코드는 A1-A2 행 미매핑으로
    // 위젯 텍스트가 ?로 렌더됐다. 이모지는 CP949 인코딩 불가 — UTF-16
    // 서러게이트 2유닛이 각각 '?'로 치환돼 "??"(2바이트)가 된다(문서화 한계).
    {
        const std::string kSym = Utf8ToKssm("■□●◆");
        CHECK(kSym.size() == 8, "T12 symbols -> 4 kssm pairs");
        CHECK(KssmToUtf8(kSym.c_str()) == "■□●◆", "T12 kssm -> utf8 round trip");
        CHECK(KssmCodepointToUnicode(static_cast<uint8_t>(kSym[0]),
                                     static_cast<uint8_t>(kSym[1])) == 0x25A0,
              "T12 ■ -> U+25A0 (atlas vector path)");
        const char symPair[3] = { static_cast<char>(kSym[0]),
                                  static_cast<char>(kSym[1]), 0 };
        CHECK(KssmCharLenAt(symPair, 2, 0) == 2,
              "T12 symbol pair is one valid cell-pair (caret boundary)");
        const std::string mixed = Utf8ToKssm("가■A");
        CHECK(KssmToUtf8(mixed.c_str()) == "가■A", "T12 hangul+symbol+ascii mixed");
        CHECK(Utf8ToKssm("\xF0\x9F\x98\x80") == "??",
              "T12 emoji (CP949-unencodable surrogate pair) -> '??' "
              "(documented limit)");
    }

    std::printf("PASS %d FAIL %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}