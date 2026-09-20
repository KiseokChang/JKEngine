// jktext_probe — 데스크탑 벡터 폰트(docs/63) 로직 단위 프로브.
// T1: KSSM 코드포인트 → 유니코드 변환 (Task 1).
// T2: 폰트 로드+셀 메트릭. T3: 래스터라이즈 잉크. T4: 폰트 커버리지.
// T5: 캐시 등록 계약. (JKDC 배선은 Task 3.)
#include <JKHangulUtil.h>
#include <JKTextAtlas.h>
#include <JKResourceCache.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// JKTypes.h pulls SDL.h which renames main -> SDL_main (레슨 26: SDL_main 치환은 #undef main).
#undef main

// JKResourceCache 스텁 — 링크만 충족(레일즈: 프로브는 실 캐시를 링크하지 않는다).
static std::vector<std::string> g_created;
jk::JKResourceCache::JKResourceCache(jk::JKRenderBackend*) {}
jk::JKResourceCache::~JKResourceCache() = default;
bool jk::JKResourceCache::CreateImageFromRGBA(const std::string& key, int, int,
                                              const std::vector<uint8_t>&) {
    g_created.push_back(key);
    return true;
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

    // T3: 래스터라이즈 잉크+AA — 한글 음절/한자/영문.
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
        return (ink > 0) ? 1 : 0;
    };
    CHECK(HasInk(0xAC00, 16) == 1, "T3 가 ink (16px stride)");
    CHECK(HasInk(0x6F22, 16) == 1, "T3 漢 ink");
    CHECK(HasInk('A', 8) == 1, "T3 'A' ink (8px stride)");

    // T4: 폰트에 없는 글자 → false. 뷁(KS X 1002)은 커버 여부를 폰트가 결정.
    const int bbaelk = HasInk(0xBDF7, 16);
    if (bbaelk == 0) {
        std::printf("INFO: 뷁 U+BDF7 not covered — no-glyph false path\n");
    } else {
        CHECK(bbaelk == 1, "T4 뷁 ink (맑은 고딕 KS X 1002)");
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

    std::printf("PASS %d FAIL %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}