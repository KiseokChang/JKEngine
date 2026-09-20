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
// recipe). docs/63 §6 2단계: a second (fallback) face can be loaded for
// codepoints the primary face does not cover — cache keys are face-separated.

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

// 보조 폰트 체인 경로 (docs/63 §6 2단계): settings.json `text.font_fallback`
// 직독(ResolveDesktopFontPath의 settings 직독 선례). **기본값 없음** — 빈
// 문자열 반환 = 체인 미설정(호스트는 InitFallback을 시도하지 않고 1차 폰트만
// 쓴다). 상한 300자는 font_path와 같은 캡.
std::string ResolveDesktopFallbackPath();

// 셀 메트릭 진실원 (docs/63 §6 text.font_scale, 옵트인 셀 확대). 기본
// scale 1.0 = 비트맵 셀과 동일 {8, 16, 16} — 기존 레이아웃·프로브 픽셀동일.
struct CellMetrics {
    int engW;   // ASCII 셀 폭 (기본 8)
    int hanW;   // KSSM 2바이트 쌍 셀 폭 (기본 16)
    int cellH;  // 셀 높이 (기본 16)
};

// 순수 산출 함수 (프로브 단정용 — 설정 파싱 개입 없음). 산출식:
// engW=max(4, round(8*s)), hanW=max(8, round(16*s)), cellH=max(8, round(16*s)).
// s는 허용 범위 [1.0, 3.0]으로 클램프한다(파싱 단계의 범위 검사가 정문 게이트 —
// 여기는 방어선 클램프로, 0.5 같은 하한 미달 입력도 {8,16,16}로 수렴).
CellMetrics ComputeCellMetrics(float s);

// 설정 진실원: settings.json `text.font_scale`(문자열 float)을 **직독**해
// 산출한다(ResolveDesktopFontPath의 settings 직독 선례). 함수 로컬 static —
// C++11 스레드 안전 초기화로 프로세스당 1회 산출. MeasureText가 static이라
// 모든 경로(MeasureText/TextOut/위젯)가 이 경유다. **재시작 적용** — 실행 중
// settings_set 반영 없음(프로세스 수명 = 메트릭 수명).
// (이름 규약 주의: 같은 스코프에 struct CellMetrics와 CellMetrics() 함수가
// 공존하면 함수 이름이 클래스 이름을 가린다(C++ 기본 탐색 규칙 — 검증:
// `struct Foo{}; const Foo& Foo();` 후 `Foo x;` 파산) — 그래서 접근자는
// GetCellMetrics로 접두어를 붙였다. brief의 `text::CellMetrics()`(구조체와
// 동명) 이름은 이 규칙과 충돌해 채택 불가.)
const CellMetrics& GetCellMetrics();

} // namespace text

class JKTextAtlas {
public:
    // Glyph-texture ceiling (docs/63 §6): each glyph is one small per-cell
    // texture (~stride x cellH, a few hundred bytes to ~1KB) — at 1024 the
    // per-host cost is ~1MB. Beyond the cap the oldest (least-recently-used)
    // (fg, cp, face) texture is unloaded from the cache and lazily
    // re-registered on demand, so the live set tracks what the frame draws.
    static constexpr size_t kMaxGlyphTextures = 1024;

    JKTextAtlas();
    ~JKTextAtlas();

    bool Init(const std::string& fontPath, int engCellW = 8, int cellH = 16,
              int hanCellW = 16);
    bool IsLoaded() const { return primary_.info != nullptr; }

    // 보조 폰트 체인 (docs/63 §6 2단계): Init 성공 후 1회 — 동일 셀 메트릭으로
    // fallback_ 면을 로드한다. 실패(파일 부재/파손)는 false = 체인 없이 계속
    // (호스트가 경고 1줄을 남기고 1차 폰트만 사용). 재 Init은 체인을 해제한다
    // (호스트가 InitFallback을 다시 시도한다).
    bool InitFallback(const std::string& fontPath);
    bool IsFallbackLoaded() const { return fallback_.info != nullptr; }

    // Lazily rasterizes cp baked with `fg` and registers it in `cache`.
    // False when the font has no glyph for cp (caller falls back per glyph).
    // useFallbackPage=true는 1차 폰트 미커버 cp의 보조 폰트 페이지 — 캐시 키가
    // 접두어(desktextf_)로 분리돼 1차 등록과 충돌하지 않는다. 기본 false =
    // 기존 1차 경로(기존 호출부 무수정).
    bool EnsureGlyph(JKResourceCache* cache, uint32_t fg, uint32_t cp,
                     bool useFallbackPage = false);

    // Cache key of the (fg, cp, face) glyph texture — valid after EnsureGlyph.
    std::string PageKey(uint32_t fg, uint32_t cp,
                        bool useFallbackPage = false) const;

    // Source rect for (fg, cp, face); empty when not registered.
    JKRect GlyphSrc(uint32_t fg, uint32_t cp,
                    bool useFallbackPage = false) const;

    // Test hook: rasterizes the single glyph into stride x cellH RGBA,
    // no resource cache involved. False when the font has no glyph.
    bool RasterizeGlyphForTest(uint32_t fg, uint32_t cp,
                               std::vector<uint8_t>* rgba, int* w, int* h,
                               bool useFallbackPage = false);

private:
    // 한 폰트 파일 = 한 면. primary_/fallback_ 두 면이 같은 셀 격자(공유
    // engCellW_/hanCellW_/cellH_)를 쓰고 스케일/베이스라인만 면별로 갖는다.
    struct Face {
        std::vector<uint8_t> data;
        std::unique_ptr<stbtt_fontinfo> info;
        float engScale = 0.0f;
        float hanScale = 0.0f;
        int   engBaseline = 0;
        int   hanBaseline = 0;
    };

    int  StrideOf(uint32_t cp) const;   // 셀 폭은 면과 무관(공유 격자)
    // Rasterizes cp into rgba (stride x cellH, baked fg, baseline aligned).
    // useFallbackPage=true면 fallback_ 면으로 래스터라이즈한다(1차가 미커버일
    // 때만 호출된다 — 커버리지 검사도 선택된 면 기준).
    bool RasterizeGlyph(uint32_t fg, uint32_t cp, std::vector<uint8_t>* rgba,
                        bool useFallbackPage);
    // 폰트 파일 로드+셀 메트릭 산출(Init 본체에서 추출 — primary/fallback 공용).
    // 성공 시 face를 채우고 true, 실패 시 face는 비어있는 상태로 false.
    static bool LoadFace(const std::string& fontPath, Face* face,
                         int engCellW, int cellH, int hanCellW);

    // Unloads the least-recently-used registered glyph texture from `cache`
    // and drops it from registered_ (keeps size <= kMaxGlyphTextures).
    void EvictOldest(JKResourceCache* cache);

    Face  primary_;
    Face  fallback_;
    int   engCellW_ = 8;
    int   hanCellW_ = 16;
    int   cellH_ = 16;

    // Registered glyph keys, LRU-ordered: front = least recently used,
    // back = most recently used. Mirrors what the cache holds.
    // 키 인코딩 (docs/63 §6 2단계 — 보조 폰트 체인): (fg << 34) | (cp << 1) | fb
    // — fg는 24비트 RGB라 34 시프트 후 58비트에 들어온다. fb 비트가 보조 폰트
    // 페이지 구분(같은 (fg,cp)가 두 면에 공존 가능). EvictOldest가 이 인코딩을
    // 복원하는 유일한 지점이다.
    std::vector<uint64_t> registered_;
};

} // namespace jk

#endif // JKTEXTATLAS_H