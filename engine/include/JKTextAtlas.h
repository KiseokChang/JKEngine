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
#include <algorithm>
#include <cmath>
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

// sfnt face가 CFF(PostScript) 인상체인지 (docs/70 §8.4 판정 2 봉합).
// stb_truetype의 CFF/CID 지원이 NotoSansCJK 계열에서 어설션 사망으로
// 이어지므로(WSL 실측 — client abort rc=134, "offsize >= 1 && offsize <= 4"),
// face-0 개방 전 걸러낸다: faceOffset의 sfnt 표 디렉터리를 걷어 'CFF '/'CFF2'
// 표 존재를 확인. 단독 ttf·TrueType-flavor .ttc는 false → 정상 개방.
// 디렉터리를 못 걷는 손상 데이터는 true(보수적 거부)로 둔다.
bool SfntFaceHasCff(const uint8_t* data, size_t size, int faceOffset);

// 셀 메트릭 진실원 (docs/63 §6 text.font_scale, 옵트인 셀 확대). scale 1.0 =
// 비트맵 8x16/16x16 격자와 동일 {8, 16, 16}; 미설정 소자 기본은
// DefaultFontScale()(Win 1.0 — 기존 픽셀동일 계약 승계, posix 1.5 — 스펙
// 2026-10-09-phone-text-scale 사용자 확정).
struct CellMetrics {
    int engW;   // ASCII 셀 폭 (scale 1.0 = 8)
    int hanW;   // KSSM 2바이트 쌍 셀 폭 (scale 1.0 = 16)
    int cellH;  // 셀 높이 (scale 1.0 = 16)
};

// 순수 산출 함수 (프로브 단정용 — 설정 파싱 개입 없음). 산출식:
// engW=max(4, round(8*s)), hanW=2*engW(유도 — docs/65 O4, 소수 scale에서
// 독자 반올림 hanW≠2×engW가 JKEdit 쌍 매핑 1px 표류), cellH=max(8, round(16*s)).
// s는 허용 범위 [1.0, 3.0]으로 클램프한다(파싱 단계의 범위 검사가 정문 게이트 —
// 여기는 방어선 클램프로, 0.5 같은 하한 미달 입력도 {8,16,16}로 수렴).
// 헤더 inline으로 옮긴 이유 (T2, 2026-10-09-phone-text-scale): posix selftest
// 쌍둥이(tools/posix_selftest)가 g++ 직링크다 — STB 구현 정의+JKResourceCache
// 링크 체인을 안 얹어도 이 순수 부품을 어댑터 축에서 단정하기 위해. 산출식은
// JKTextAtlas.cpp에서 이동한 원문 그대로.
inline CellMetrics ComputeCellMetrics(float s) {
    if (s < 1.0f) s = 1.0f;
    if (s > 3.0f) s = 3.0f;
    // hanW는 engW 유도(2×) — 독자 반올림(lround(16s))하면 소수 scale에서
    // hanW ≠ 2×engW가 돼 JKEdit의 "쌍=2셀×engW" 매핑이 셀당 최대 1px
    // 표류했다(docs/65 O4). 셀 모델의 진실은 "KSSM 쌍 = eng 셀 2개"다.
    // (주의: CellMetrics 필드 순서는 {engW, hanW, cellH} — hanW 슬롯에
    // 높이를 넣지 않도록 초기자 순서를 지킨다.)
    const int engW = std::max(4, static_cast<int>(std::lround(8.0f * s)));
    return CellMetrics{
        engW,
        2 * engW,
        std::max(8, static_cast<int>(std::lround(16.0f * s))),
    };
}

// 설정 미도달(settings.json 키 부재/파싱 실패/범위 밖) 시의 소자 기본 배율 —
// 컴파일타임 플랫폼 상수 (스펙 2026-10-09-phone-text-scale 사용자 확정 2026-
// 10-09): Windows 1.0(docs/78 픽셀동일 계약 승계 — Windows 무변), 그 밖 축
// (posix/WSL/폰)은 1.5. GetCellMetrics의 미설정 분기가 소비하고, selftest
// 쌍둥이(2t 계열)가 이 상수로 플랫폼 기대값을 직접 단정한다.
inline float DefaultFontScale() {
#if defined(_WIN32)
    return 1.0f;
#else
    return 1.5f;
#endif
}

// 비트맵 폴백 확대 매핑 순수 헬퍼 (스펙 결정 — 폴백 글리프 = 목표 셀 크기
// nearest 좌표 확대, 텍스처 신설 불요 · DC 픽셀 드로잉 경로 유지): 목표 셀
// 좌표 dstIdx ∈ [0, dstSpan)이 소스 비트맵의 어느 행/열 샘플을 복사할지
// 잠가준다 — src = min(dstIdx*srcSpan/dstSpan, srcSpan-1). 성질:
// srcSpan==dstSpan이면 **항등**(dstIdx 그대로 — 확대 배율 1.0의 Windows
// 픽셀동일 단정 산치), dstSpan이 커지면 소스 전체가 목표 셀을 nearest
// 격자로 채운다. 병적 입력(dstSpan<1, 음수 등)은 방어적으로 첫 샘플 0.
inline int StretchNearestIndex(int dstIdx, int srcSpan, int dstSpan) {
    if (dstIdx <= 0 || dstSpan <= 0 || srcSpan <= 0) return 0;
    const int src = dstIdx * srcSpan / dstSpan;
    return src >= srcSpan ? srcSpan - 1 : src;
}

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

// 크롬 타이틀 밴드 높이 순수 산식 (T3 — 스펙 2026-10-09-phone-text-scale 결정
// 1): 현행 상수 24 = "비트맵 글리프 셀 16px + 위 4px + 아래 4px 여백"의 합
// (원문 실측 — JKWindow.cpp OnRectChanged kTitle=24·server/JKCompositor.h
// kChromeTitleBar=24, 타이틀 텍스트는 TextOutX ADJ_YCENTER로 cellH를 세로
// 중앙정렬했다). 그래서 산식 = cellH + 8(4+4 여백), 최소 현행값 24 보장.
// **s=1.0에서 cellH=16 → 16+8=24 — 현행 상수와 정확히 등호**(Windows 창
// 타이틀 픽셀동일 단정의 산치), posix 기본 1.5(cellH=24)에서는 32로 커져
// 1.5 글리프가 클립되지 않는다. 호출부는 JKWindow.cpp(밴드 그리기 측 —
// 클라 표면 안의 크롬)와 JKWindowServer.cpp(크롬 히트테스트 존·승인 배너
// 밴드 두께 — "MUST stay in sync" 계약) 둘 다다.
// cellH 하단 방어선: ComputeCellMetrics가 s∈[1,3]로 클램프하므로 cellH는
// [16,48] — max()는 바인딩하지 않는 방어선(비정상 셀에서도 현행값 이하로
// 밴드가 수축하지 않는다).
inline int ComputeChromeTitleBarHeight(int cellH) {
    constexpr int kLegacyTitleBar = 24;  // kTitle/kChromeTitleBar의 원문 현행값
    constexpr int kTitlePad = 8;         // 글리프 셀 위 4px + 아래 4px (16+8=24)
    return std::max(kLegacyTitleBar, cellH + kTitlePad);
}

// 적용 진실원 — 프로세스 메트릭을 소비한다(GetCellMetrics = 기동 시 settings
// 직독, 프로세스당 1회 고정 → 재시작 적용 계약 그대로).
inline int ChromeTitleBarHeight() {
    return ComputeChromeTitleBarHeight(GetCellMetrics().cellH);
}

// 앱 본문 상단 오프셋 순수 산식 (T3 fix r1 — 리뷰 Important I-2): ImGui 클라
// 앱 9곳(AgentMgr/Browser/Chat/Files/Library/Notes/Settings/Shot/VPlayer)의
// 고정 topY=30은 "밴드 24 + 본문 여백 6"(원문 주석 레슨 8 "서버 크롬이 상단
// 24pt를 먹는다 — y>=30부터")의 분해 — 그래서 밴드 산식 위에 여백 6px를
// 별도 상수로 얹는다(둘을 합쳐 재분해하지 않는다 — s=1.0 항등 보존: 밴드
// 산식이 24 등호이므로 오프셋도 24+6=30 등호, Windows 무변). posix 기본
// 1.5(밴드 32)에서는 38로 커져 본문이 밴드와 겹치지 않는다(I-2 상단
// 사각지대 해소). 순수 함수 — selftest 쌍둥이가 어댑터 축에서 단정.
inline int ComputeAppContentTopOffset(int cellH) {
    constexpr int kContentTopMargin = 6;  // 밴드 아래 여백 (topY 30 − 밴드 24)
    return ComputeChromeTitleBarHeight(cellH) + kContentTopMargin;
}

// 적용 진실원 — GetCellMetrics 소비(밴드 산식과 같은 재시작 적용 계약).
inline int AppContentTopOffset() {
    return ComputeAppContentTopOffset(GetCellMetrics().cellH);
}

} // namespace text

class JKTextAtlas {
public:
    // Glyph-texture ceiling (docs/63 §6): each glyph is one small per-cell
    // texture (~stride x cellH, a few hundred bytes to ~1KB) — at 1024 the
    // per-host cost is ~1MB AT THE DEFAULT CELL SIZE (scale 1.0). text.font_scale
    // scales cellH too, so the bound is scale-dependent: at scale 3.0 a wide
    // glyph is 48x48x4 ≈ 9.2KB → up to ~10MB per host. Still acceptable; do not
    // state ~1MB as scale-invariant. Beyond the cap the oldest
    // (least-recently-used) (fg, cp, face) texture is unloaded from the cache
    // and lazily re-registered on demand, so the live set tracks what the
    // frame draws.
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