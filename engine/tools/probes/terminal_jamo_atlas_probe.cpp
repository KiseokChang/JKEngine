// terminal_jamo_atlas_probe — 터미널 내부 한글 조합의 preEdit 자모 렌더링
// (docs/61 §22 후속, 유저 보고 "조합과정은 제대로 안보여"). JKGlyphAtlas
// 폴백 범위에 Hangul Compatibility Jamo(U+3130-318F)가 빠져 슬롯 0 →
// 플레이스홀더 바로 그려졌다. RasterizeFallbackPageForTest로 슬롯 존재 +
// 실제 잉크 픽셀을 검증한다(렌더러 불요).
#include <terminal/JKGlyphAtlas.h>
#include <JKResourceCache.h>
#ifdef main
#undef main   // SDL_main 치환 방지(docs/61 §22 레슨 27)
#endif
#include <cstdio>
#include <vector>

using namespace jk;

// 캐시 스텁 — JKGlyphAtlas의 EnsurePrimaryPage 참조를 충족. 렌더러 없이
// RasterizeFallbackPageForTest 경로만 운전한다. UnloadImage는 docs/63 §6 LRU로
// JKTextAtlas가 참조하므로 스텁이 필요(링크 목록 보충 — 레슨 28).
JKResourceCache::~JKResourceCache() = default;
bool JKResourceCache::CreateImageFromRGBA(const std::string&, int, int,
                                          const std::vector<uint8_t>&) {
    return false;
}
void JKResourceCache::UnloadImage(const std::string&) {}

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (cond) { ++g_pass; }                                            \
        else { ++g_fail; std::printf("FAIL: %s\n", msg); }                 \
    } while (0)

static bool HasInk(JKGlyphAtlas& a, uint32_t cp) {
    std::vector<uint8_t> rgba;
    int w = 0, h = 0;
    if (!a.RasterizeFallbackPageForTest(0xCCCCCC, false, cp, &rgba, &w, &h)) {
        return false;   // 슬롯 자체가 없다
    }
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        if (rgba[i] || rgba[i + 1] || rgba[i + 2]) return true;   // 잉크 있음
    }
    return false;
}

int main() {
    JKGlyphAtlas atlas;
    // 주 폰트(consola) + 폴백(malgun) — ClientTerminalApp와 동일 구성.
    if (!atlas.Init("C:\\Windows\\Fonts\\consola.ttf", 8, 16)) {
        std::printf("SKIP: consola init failed\n");
        return 77;
    }
    atlas.InitFallback("C:\\Windows\\Fonts\\malgun.ttf");

    // T1: 자모 슬롯 존재 + 잉크 — 조합 중 표시의 정체(U+3131 ㄱ, U+3141 ㅁ,
    // U+3163 ㅣ, 쌍자모 U+3132 ㄲ, 받침용 U+3139 ㄹ).
    CHECK(HasInk(atlas, 0x3131), "T1 giok U+3131 ink");
    CHECK(HasInk(atlas, 0x3141), "T1 mieum U+3141 ink");
    CHECK(HasInk(atlas, 0x3163), "T1 i U+3163 ink");
    CHECK(HasInk(atlas, 0x3132), "T1 ssang-giok U+3132 ink");
    CHECK(HasInk(atlas, 0x3139), "T1 riul U+3139 ink");

    // T2: 완성형 음절(기존 동작 회귀) — 가/학.
    CHECK(HasInk(atlas, 0xAC00), "T2 ga U+AC00 ink");
    CHECK(HasInk(atlas, 0xD559), "T2 hank U+D559 ink");

    // T3: 범위 외는 여전히 슬롯 없음(플레이스홀더 경로 유지).
    CHECK(!atlas.RasterizeFallbackPageForTest(0xCCCCCC, false, 0x0300,
                                              nullptr, nullptr, nullptr),
          "T3 outside range still no slot");

    // T4: 조합형 자모(U+1100 블록)도 기존 범위 유지.
    CHECK(HasInk(atlas, 0x1100), "T4 jamo block U+1100 ink");

    std::printf("%d/%d checks passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}