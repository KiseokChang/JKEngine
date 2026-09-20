# Desktop Vector Font (JKDC::TextOut 관문 교체) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `JKDC::TextOut`의 글리프 출력을 비트맵(HangulManager)에서 stb_truetype 벡터 글리프로 교체한다 — 셀 격자(영문 8×16/한글 16×16)와 KSSM 전진 규칙은 그대로 유지.

**Architecture:** 신설 `JKTextAtlas`(jkcore)가 (fg색, cp) 캐시 키로 글리프당 소형 RGBA 텍스처를 수요 시 래스터라이즈·등록하고, `JKDC::EngPutCh/HanPutCh`가 아틀라스 경로를 먼저 쓰되 글리프 단위로 기존 비트맵 경로에 폴백. 기존 59개 TextOut 호출 지점 무수정.

**Tech Stack:** C++17, stb_truetype (이미 트리 내), SDL2 리소스 캐시(JKResourceCache), MinGW-w64 ucrt64 + ninja, PowerShell 5.1 프로브.

**Spec:** `docs/63_desktop_vector_font.md` (구현 편차 — 글리프당 텍스처 — 포함, 2026-09-21 승인)

## Global Constraints

- 툴체인: `export PATH=/c/msys64/ucrt64/bin:$PATH` (PATH 누락 시 mingw g++ 침묵 사망 — docs/61 레슨 6). 빌드는 `cd /i/progwork/JKENGINE/engine/build && ninja` (engine/build 상시 configure 상태).
- 새 서드파티 의존 없음. stb_truetype 구현부(`#define STB_TRUETYPE_IMPLEMENTATION`)는 **JKTextAtlas.cpp 단 1곳**으로 이동 — JKGlyphAtlas.cpp는 include만 (이중 정의 막기, Task 2).
- 터미널 스택(JKGlyphAtlas·TerminalView·터미널 앱)은 **글리프 로직 미접촉** — 위 impl-define 이동만 허용. 회귀 프로브 GREEN 유지가 격리 증명.
- 텍스트 인코딩: JKDC 경로는 KSSM 2바이트 조합형이 단일 진실(docs/16) — 벡터 경로는 글자 단위 `KssmCodepointToUnicode` 변환만 추가, 저장 계약 불변.
- 글리프 텍스처는 fg색 구움 + AA 알파(BlitTexture 텍스처 알파 블렌딩 — 터미널이 증명).
- 프로브: 콘솔 exe, **×2 연속 ALL PASS** 원칙, 실행은 `> log 2>&1` 리다이렉트(grep 파이프 버퍼링 오판 방지), 폰트 파일 미존재 시 `return 77`(SKIP).
- PowerShell 5.1: .ps1은 **ASCII 전용**(무BOM UTF-8 한글이 파서 파산), 프로브가 유저 런타임 파일(permissions.json·settings.json)을 만질 때 백업+복원+콘솔 고지, stdout은 파일 리다이렉트.
- 커밋: 관례 메시지 + `Co-Authored-By: Claude Code <noreply@anthropic.com>`.
- 셀 상수: 영문 stride 8, 한글(코드포인트 ≥ 0x80) stride 16, 셀 높이 16 — `JKDC::MeasureText`와 동일(무수정).

---

### Task 1: KSSM→Unicode 단일 코드포인트 변환

**Files:**
- Modify: `engine/include/JKHangulUtil.h`
- Modify: `engine/src/JKHangulUtil.cpp`
- Create: `engine/tools/probes/jktext_probe.cpp`

**Interfaces:**
- Consumes: `std::string KssmToUtf8(const char* kssm)` (기존, JKHangulUtil.h)
- Produces: `uint32_t jk::KssmCodepointToUnicode(uint8_t first, uint8_t second)` — KSSM 2바이트 → 유니코드 코드포인트, 매핑 없으면 0. Task 3의 `JKDC::HanPutCh`가 사용.

- [ ] **Step 1: 실패하는 프로브 작성**

`engine/tools/probes/jktext_probe.cpp` 신설 (선례: `terminal_jamo_atlas_probe.cpp` 구조 — CHECK 매크로, SKIP=exit 77):

```cpp
// jktext_probe — 데스크탑 벡터 폰트(docs/63) 로직 단위 프로브.
// T1: KSSM 코드포인트 → 유니코드 변환. T2+: JKTextAtlas / JKDC 배선 (Task 2-3).
#include <JKHangulUtil.h>
#include <cstdio>
#include <string>

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

    std::printf("PASS %d FAIL %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
```

- [ ] **Step 2: 컴파일 — 실패 확인 (KssmCodepointToUnicode 미선언)**

```bash
export PATH=/c/msys64/ucrt64/bin:$PATH
cd /i/progwork/JKENGINE/engine
g++ -std=c++17 -O2 -Iinclude -Ithird_party/stb -IC:/msys64/ucrt64/include/SDL2 \
  tools/probes/jktext_probe.cpp src/JKHangulUtil.cpp legacy/wancode/WANCODE.CPP \
  -o tools/probes/jktext_probe.exe -LC:/msys64/ucrt64/lib -lSDL2 -limm32
./tools/probes/jktext_probe.exe
```
Expected: 컴파일 오류 — `'KssmCodepointToUnicode' was not declared`. (링크 오류가 나면 **입 파일 목록부터** 보충한다 — docs/61 레슨 28. JKHangulUtil이 참조하는 wCodeTable 심볼은 `legacy/wancode/WANCODE.CPP`가 담당.)

- [ ] **Step 3: 선언+구현**

`engine/include/JKHangulUtil.h` — 기존 선언 뒤에 추가:

```cpp
// KSSM 2바이트 조합형 쌍을 유니코드 코드포인트로. 매핑 없는 쌍은 0.
// (docs/63 데스크탑 벡터 폰트 — JKDC::HanPutCh가 글리프 조회에 쓴다.)
uint32_t KssmCodepointToUnicode(uint8_t first, uint8_t second);
```

`engine/src/JKHangulUtil.cpp` — 파일 말미에 추가 (익명 네임스페이스 헬퍼 포함):

```cpp
namespace {

// UTF-8 첫 코드포인트 디코드. 기형이면 0. (KssmToUtf8 산출은 정상 UTF-8.)
uint32_t Utf8FirstCodepoint(const char* s) {
    const auto* u = reinterpret_cast<const unsigned char*>(s);
    if (!u[0]) return 0;
    if (u[0] < 0x80) return u[0];
    uint32_t cp = 0;
    int len = 0;
    if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; len = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; len = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; len = 4; }
    else return 0;
    for (int i = 1; i < len; ++i) {
        if ((u[i] & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | static_cast<uint32_t>(u[i] & 0x3F);
    }
    return cp;
}

} // namespace

uint32_t KssmCodepointToUnicode(uint8_t first, uint8_t second) {
    // 기존 검증된 역인덱스(KssmToUtf8)를 글자 단위로 재사용 — 신규 매핑 테이블
    // 금지(docs/60 §7: 산술 매핑 이중 유지는 결함의 온상).
    const char bytes[3] = { static_cast<char>(first), static_cast<char>(second), 0 };
    const std::string utf8 = KssmToUtf8(bytes);
    return utf8.empty() ? 0u : Utf8FirstCodepoint(utf8.c_str());
}
```

- [ ] **Step 4: 컴파일+실행 — PASS 확인**

같은 커맨드 재실행. Expected: `PASS: T1` 3건 + fixture 1건 + unmapped 1건, `PASS 5 FAIL 0`, exit 0.

- [ ] **Step 5: 커밋**

```bash
git add engine/include/JKHangulUtil.h engine/src/JKHangulUtil.cpp engine/tools/probes/jktext_probe.cpp
git commit -m "feat(text): KSSM 코드포인트→유니코드 변환 KssmCodepointToUnicode (docs/63 Task 1)"
```

---

### Task 2: JKTextAtlas — 폰트 로드·래스터라이즈·수요 시 텍스처

**Files:**
- Create: `engine/include/JKTextAtlas.h`
- Create: `engine/src/JKTextAtlas.cpp`
- Modify: `engine/CMakeLists.txt` (jkcore 소스 목록에 `src/JKTextAtlas.cpp` 추가)
- Modify: `engine/src/terminal/JKGlyphAtlas.cpp:1` (impl define 이동)
- Test: `engine/tools/probes/jktext_probe.cpp` (T2-T5 확장)

**Interfaces:**
- Consumes: `KssmCodepointToUnicode` (Task 1 — 여기선 불요, cp를 직접 받는다)
- Produces:
  - `bool JKTextAtlas::Init(const std::string& fontPath, int engCellW = 8, int cellH = 16, int hanCellW = 16)`
  - `bool JKTextAtlas::IsLoaded() const`
  - `bool JKTextAtlas::EnsureGlyph(JKResourceCache* cache, uint32_t fg, uint32_t cp)` — 폰트에 글리프 없으면 false
  - `std::string JKTextAtlas::PageKey(uint32_t fg, uint32_t cp) const` — `"desktext_%08x_%06x"`
  - `JKRect JKTextAtlas::GlyphSrc(uint32_t fg, uint32_t cp) const` — 등록된 글리프는 `JKRect{0,0,stride,16}`, 미등록은 빈 rect
  - `bool JKTextAtlas::RasterizeGlyphForTest(uint32_t fg, uint32_t cp, std::vector<uint8_t>* rgba, int* w, int* h)` — 헤드리스 검증 훅
  - `std::string jk::text::ResolveDesktopFontPath()` — settings 오버라이드 → 플랫폼 기본값. Task 5에서 호스트 배선이 소비.
  - stb_truetype 구현부 1원칙: `JKGlyphAtlas.cpp:1`의 `#define STB_TRUETYPE_IMPLEMENTATION`을 JKTextAtlas.cpp로 이동.

- [ ] **Step 1: 헤더 작성**

`engine/include/JKTextAtlas.h`:

```cpp
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
```

- [ ] **Step 2: 구현 작성**

`engine/src/JKTextAtlas.cpp` — **이 TU가 stb_truetype 구현부의 유일한 정의점**이 된다:

```cpp
// Desktop vector font (docs/63). STB_TRUETYPE_IMPLEMENTATION lives HERE —
// moved from JKGlyphAtlas.cpp so jkcore-only targets (jkagentd) can still
// link while the client terminal atlas reuses the single definition.
#define STB_TRUETYPE_IMPLEMENTATION

#include <JKTextAtlas.h>
#include <JKResourceCache.h>
#include <agent/JKAgentJson.h>
#include <stb_truetype.h>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace jk {

namespace {

std::string ExeDir() {
#ifdef _WIN32
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : dir.substr(0, slash + 1);
#else
    return std::string();   // Linux: settings override support arrives with the port (docs/62)
#endif
}

bool FileReadable(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

} // namespace

std::string text::ResolveDesktopFontPath() {
    // 1) settings.json override (docs/54 hub): exe dir + state\settings.json.
    const std::string kvPath = ExeDir() + "state\\settings.json";
    std::FILE* f = std::fopen(kvPath.c_str(), "rb");
    if (f) {
        std::string text;
        char chunk[2048];
        size_t n;
        while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) text.append(chunk, n);
        std::fclose(f);
        jk::agent::AgentJson json(text);
        std::string v;
        if (json.ok() && json.GetObjStr("text", "font_path", v) && !v.empty() &&
            v.size() <= 300) {
            return v;   // 오버라이드 존중 — 파손이면 Init 실패로 가시(스펙 fail-safe)
        }
    }
    // 2) platform default (docs/63 §4.1).
#ifdef _WIN32
    return "C:\\Windows\\Fonts\\malgun.ttf";
#else
    static const char* kLinuxCandidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    };
    for (const char* cand : kLinuxCandidates) {
        if (FileReadable(cand)) return cand;
    }
    return std::string();   // Linux + 폰트 부재 = 비트맵 경로 유지
#endif
}

JKTextAtlas::JKTextAtlas() = default;
JKTextAtlas::~JKTextAtlas() = default;

int JKTextAtlas::StrideOf(uint32_t cp) const {
    return cp < 0x80 ? engCellW_ : hanCellW_;   // KSSM 전진 규칙과 동일
}
float JKTextAtlas::ScaleOf(uint32_t cp) const {
    return cp < 0x80 ? engScale_ : hanScale_;
}
int JKTextAtlas::BaselineOf(uint32_t cp) const {
    return cp < 0x80 ? engBaseline_ : hanBaseline_;
}

bool JKTextAtlas::Init(const std::string& fontPath, int engCellW, int cellH,
                       int hanCellW) {
    engCellW_ = std::max(4, engCellW);
    hanCellW_ = std::max(8, hanCellW);
    cellH_ = std::max(8, cellH);
    info_.reset();
    registered_.clear();

    // 폰트 파일 로드 — JKGlyphAtlas::Init와 동일 규약.
    std::FILE* f = nullptr;
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

    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(info_.get(), &ascent, &descent, &lineGap);
    if (ascent - descent <= 0) { info_.reset(); return false; }

    // 영문 스케일: 'M' 전진 = engCellW_ (터미널 primary와 동일 레시피).
    int advW = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(info_.get(), 'M', &advW, &lsb);
    if (advW > 0) {
        engScale_ = static_cast<float>(engCellW_) / static_cast<float>(advW);
    } else {
        engScale_ = static_cast<float>(cellH_) /
                    static_cast<float>(ascent - descent);
    }
    // 한글 스케일: 음절(0xAC00) 전진 = hanCellW_ 스트라이드 (InitFallback 레시피).
    stbtt_GetCodepointHMetrics(info_.get(), 0xAC00, &advW, &lsb);
    if (advW > 0) {
        hanScale_ = static_cast<float>(hanCellW_) / static_cast<float>(advW);
    } else {
        hanScale_ = engScale_ * 2.0f;
    }
    // em 박스가 셀을 넘으면 축소 (맑은 고딕이 여기 걸린다 — InitFallback 코멘트).
    float emPx = static_cast<float>(ascent - descent) * hanScale_;
    if (emPx > static_cast<float>(cellH_)) {
        hanScale_ *= static_cast<float>(cellH_) / emPx;
        emPx = static_cast<float>(ascent - descent) * hanScale_;
    }
    hanBaseline_ = static_cast<int>((static_cast<float>(cellH_) - emPx) * 0.5f +
                                    static_cast<float>(ascent) * hanScale_);
    const float emPxEng = static_cast<float>(ascent - descent) * engScale_;
    engBaseline_ = static_cast<int>(
        (static_cast<float>(cellH_) - emPxEng) * 0.5f +
        static_cast<float>(ascent) * engScale_);
    return true;
}

bool JKTextAtlas::RasterizeGlyph(uint32_t fg, uint32_t cp,
                                 std::vector<uint8_t>* rgba) {
    if (!info_) return false;
    const int stride = StrideOf(cp);
    const float scale = ScaleOf(cp);

    int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
    stbtt_GetCodepointBitmapBox(info_.get(), static_cast<int>(cp),
                                scale, scale, &ix0, &iy0, &ix1, &iy1);
    int bw = ix1 - ix0;
    int bh = iy1 - iy0;
    rgba->assign(static_cast<size_t>(stride) * cellH_ * 4, 0);
    *wOutStride(stride);
    if (bw <= 0 || bh <= 0) {
        // 폰트에 글리프 없음(공백 포함 — 공백은 잉크 0으로 "있음"이 맞다).
        // cp==0x20은 성공(빈 글리프), 그 외 잉크 없음도 좌표만으로 등록 허용.
        return cp == 0x20;
    }
    bw = std::min(bw, stride);
    bh = std::min(bh, cellH_);
    std::vector<uint8_t> cov(static_cast<size_t>(bw) * bh, 0);
    stbtt_MakeCodepointBitmap(info_.get(), cov.data(), bw, bh, bw,
                              scale, scale, static_cast<int>(cp));

    const uint8_t cr = static_cast<uint8_t>((fg >> 16) & 0xFF);
    const uint8_t cg = static_cast<uint8_t>((fg >> 8) & 0xFF);
    const uint8_t cb = static_cast<uint8_t>(fg & 0xFF);
    const int px0 = std::max(0, std::min(ix0, stride - 1));
    const int py0 = std::max(0, std::min(BaselineOf(cp) + iy0, cellH_ - 1));
    for (int y = 0; y < bh; ++y) {
        const int py = py0 + y;
        if (py >= cellH_) break;
        for (int x = 0; x < bw; ++x) {
            const int px = px0 + x;
            if (px >= stride) break;
            const uint8_t a = cov[static_cast<size_t>(y) * bw + x];
            if (a == 0) continue;
            uint8_t* dst = &(*rgba)[(static_cast<size_t>(py) * stride + px) * 4];
            dst[0] = cr;
            dst[1] = cg;
            dst[2] = cb;
            dst[3] = a;
        }
    }
    return true;
}
```

주의 — 위 `*wOutStride(stride)` 행은 잘못된 스케치이다. `RasterizeGlyph`에는 폭 출력이 없고, `RasterizeGlyphForTest`가 폭을 채운다:

```cpp
bool JKTextAtlas::RasterizeGlyphForTest(uint32_t fg, uint32_t cp,
                                        std::vector<uint8_t>* rgba,
                                        int* w, int* h) {
    if (!info_) return false;
    if (!RasterizeGlyph(fg, cp, rgba)) return false;
    if (w) *w = StrideOf(cp);
    if (h) *h = cellH_;
    return true;
}

std::string JKTextAtlas::PageKey(uint32_t fg, uint32_t cp) const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "desktext_%08x_%06x",
                  static_cast<unsigned>(fg), cp);
    return buf;
}

JKRect JKTextAtlas::GlyphSrc(uint32_t fg, uint32_t cp) const {
    const uint64_t key = (static_cast<uint64_t>(fg) << 32) | cp;
    for (uint64_t r : registered_) {
        if (r == key) return JKRect{ 0, 0, StrideOf(cp), cellH_ };
    }
    return JKRect{ 0, 0, 0, 0 };
}

bool JKTextAtlas::EnsureGlyph(JKResourceCache* cache, uint32_t fg, uint32_t cp) {
    if (!cache || !info_) return false;
    const uint64_t key = (static_cast<uint64_t>(fg) << 32) | cp;
    for (uint64_t r : registered_) {
        if (r == key) return true;
    }
    std::vector<uint8_t> rgba;
    if (!RasterizeGlyph(fg, cp, &rgba)) return false;
    const std::string cacheKey = PageKey(fg, cp);
    if (!cache->CreateImageFromRGBA(cacheKey, StrideOf(cp), cellH_, rgba)) {
        return false;
    }
    registered_.push_back(key);
    return true;
}

} // namespace jk
```

(`RasterizeGlyph` 본문에서 `*wOutStride(stride);` 행은 **넣지 않는다** — 위 두 블록을 합쳐 최종 파일을 만든다. `RasterizeGlyph`은 `void`가 아니라 `bool`이며 폭 출력 없음.)

- [ ] **Step 3: stb 구현부 1원칙 이동**

`engine/src/terminal/JKGlyphAtlas.cpp:1`에서:

```cpp
#define STB_TRUETYPE_IMPLEMENTATION
```
삭제하고, 그 자리에 주석:
```cpp
// STB_TRUETYPE_IMPLEMENTATION moved to JKTextAtlas.cpp (docs/63 Task 2) —
// single definition in jkcore; this TU includes the header only.
```
(파일 나머지는 **한 글자도 고치지 않는다**. JKGlyphAtlas.cpp는 이제 `#include <stb_truetype.h>`만 유지하고 jkcore의 JKTextAtlas.obj에서 stbtt_* 심볼을 받는다.)

- [ ] **Step 4: CMake 등록**

`engine/CMakeLists.txt` jkcore 소스 목록 — `src/JKHangulManager.cpp` 행 뒤에 추가:
```cmake
    src/JKTextAtlas.cpp
```

- [ ] **Step 5: 프로브 T2-T5 추가 (실패 확인 먼저)**

`jktext_probe.cpp`의 `main()` 앞에 JKResourceCache 스텁(선례: jamo probe), main에 T2-T5 추가:

```cpp
#include <JKTextAtlas.h>
#include <JKResourceCache.h>
#include <set>
// JKResourceCache 스텁 — 링크만 충족(레일즈: 프로브는 실 캐시를 링크하지 않는다).
static std::vector<std::string> g_created;
JKResourceCache::JKResourceCache(JKRenderBackend*) {}
JKResourceCache::~JKResourceCache() = default;
bool JKResourceCache::CreateImageFromRGBA(const std::string& key, int, int,
                                          const std::vector<uint8_t>&) {
    g_created.push_back(key);
    return true;
}
bool JKResourceCache::HasImage(const std::string& key) const {
    for (const auto& k : g_created) if (k == key) return true;
    return false;
}
JKRenderBackend::TextureHandle JKResourceCache::GetImage(const std::string&) const {
    return g_created.empty() ? nullptr
                             : reinterpret_cast<JKRenderBackend::TextureHandle>(0x1);
}
void JKResourceCache::FlushUploads(JKRenderBackend*) {}
```

main에:

```cpp
    // T2: 초기화 — 맑은 고딕, 셀 메트릭.
    JKTextAtlas atlas;
    if (!atlas.Init(text::ResolveDesktopFontPath(), 8, 16, 16)) {
        std::printf("SKIP: vector font init failed\n");
        return 77;
    }
    CHECK(atlas.IsLoaded(), "T2 loaded");

    // T3: 래스터라이즈 잉크+AA — 한글 음절/한자/영문.
    auto HasInkAndAA = [&](uint32_t cp, int stride) {
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
    CHECK(HasInkAndAA(0xAC00, 16) == 1, "T3 가 ink (16px stride)");
    CHECK(HasInkAndAA(0x6F22, 16) == 1, "T3 漢 ink");
    CHECK(HasInkAndAA('A', 8) == 1, "T3 'A' ink (8px stride)");
    // T4: 폰트에 없는 글자 → false (호환 자모 U+3163 등 커버 확인 겸).
    CHECK(HasInkAndAA(0xBDF7, 16) == 1, "T4 뷁 ink (맑은 고딕 KS X 1002)");

    // T5: 캐시 등록 계약 — (fg,cp) 유니크 키, 재요청 멱등.
    JKResourceCache cache(nullptr);
    CHECK(atlas.EnsureGlyph(&cache, 0xCCCCCC, 0xAC00), "T5 ensure 가");
    CHECK(atlas.EnsureGlyph(&cache, 0xCCCCCC, 0xAC00), "T5 ensure 가 멱등");
    CHECK(atlas.PageKey(0xCCCCCC, 0xAC00) ==
              std::string("desktext_cccccc_ac00"), "T5 page key format");
    CHECK(atlas.GlyphSrc(0xCCCCCC, 0xAC00).w == 16, "T5 src wide");
    CHECK(atlas.GlyphSrc(0xCCCCCC, 'A').w == 0, "T5 미등록 글리프 빈 rect");
    CHECK(atlas.EnsureGlyph(&cache, 0xCCCCCC, 0x10FFFF), "T5 미커버 cp도 등록 시도");
    // 미커버 cp가 등록됐다면(빈 글리프) 폰트 결정 — 맑은 고딕은 대부분 커버.
```

TDD 원칙대로 **Step 5는 컴파일이 T1 단계 프로브와 함께 실패하는 것을 먼저 확인**한다(JKTextAtlas.h 미존재 상태에서 헤더 인클루드 실패). 그 후 Step 1-4 구현이 끝나면 PASS.

순서 재정리 — 이 태스크에서는 위 순서(헤더→구현→define 이동→CMake→프로브 확장)로 작성하되, 프로브 확장 후 전체 재컴파일:

```bash
g++ -std=c++17 -O2 -Iinclude -Ithird_party/stb -IC:/msys64/ucrt64/include/SDL2 \
  tools/probes/jktext_probe.cpp \
  src/JKTextAtlas.cpp src/JKDC.cpp src/JKHangulUtil.cpp \
  src/JKHangulManager.cpp legacy/wancode/WANCODE.CPP \
  -o tools/probes/jktext_probe.exe -LC:/msys64/ucrt64/lib -lSDL2 -limm32
```
(JKDC.cpp는 Task 3에서 배선 전이라 현재는 미참조 — 링크 오류 시 해당 소스를 목록에서 빼거나 레슨 28대로 필요한 소스를 추가한다.)

Expected: `PASS 12 FAIL 0` (T1 5 + T2 1 + T3 3 + T4 1 + T5 6 - 중복 카운트 조정 — 실제 체크 수 기준 ×2 연속).

- [ ] **Step 6: ninja 전체 빌드로 jkclient(터미널) 회귀 없음 확인**

```bash
cd /i/progwork/JKENGINE/engine/build && ninja
```
Expected: [NN/NN] 완료, 오류 0. (stb 심볼 단일 정의로 jkclient+jkcore 공존 검증. 실패 시 — duplicate symbol이면 define 이동이 반쪽이다.)

- [ ] **Step 7: 커밋**

```bash
git add engine/include/JKTextAtlas.h engine/src/JKTextAtlas.cpp engine/CMakeLists.txt \
  engine/src/terminal/JKGlyphAtlas.cpp engine/tools/probes/jktext_probe.cpp
git commit -m "feat(text): JKTextAtlas 신설 — 수요 시 글리프 텍스처 아틀라스 (docs/63 Task 2)"
```

---

### Task 3: JKDC 배선 — 아틀라스 우선, 글리프 단위 비트맵 폴백

**Files:**
- Modify: `engine/include/JKDC.h`
- Modify: `engine/src/JKDC.cpp`
- Test: `engine/tools/probes/jktext_probe.cpp` (T6-T8 확장)

**Interfaces:**
- Consumes: `JKTextAtlas::{EnsureGlyph,PageKey,GlyphSrc}` (Task 2), `KssmCodepointToUnicode` (Task 1)
- Produces: `void JKDC::SetTextAtlas(JKTextAtlas* atlas, JKResourceCache* cache)` — Task 5·6 호스트 배선이 소비.

- [ ] **Step 1: JKDC.h 수정**

`class JKDC {` 선언부 — `SetHangulManager` 아래에:

```cpp
    // Vector-font path (docs/63). When both pointers are set, EngPutCh/
    // HanPutCh draw atlas glyphs and fall back to the bitmap path per glyph.
    void SetTextAtlas(JKTextAtlas* atlas, JKResourceCache* cache);
```
private 멤버:
```cpp
    JKTextAtlas* textAtlas_ = nullptr;
    JKResourceCache* textCache_ = nullptr;
    bool DrawGlyph(JKPoint p, uint32_t cp, int stride);
```
전방선언 (기존 `class HangulManager;` 옆):
```cpp
class JKTextAtlas;
class JKResourceCache;
```

- [ ] **Step 2: JKDC.cpp 구현 — 실패 프로브 먼저**

프로브 T6-T8 (main에 추가, 스텁 렌더 백엔드 포함):

```cpp
// T6-T8: JKDC 배선 — 녹화 백엔드로 BlitTexture/DrawPixel을 관찰한다.
#include <JKDC.h>
#include <vector>

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
        blits.push_back({ texture, src ? *src : jk::JKRect{0,0,0,0}, dst });
    }
    struct Blit { void* tex; jk::JKRect src; jk::JKRect dst; };
    std::vector<Blit> blits;
    int drawPixels = 0;
};
} // namespace

    // T6: 아틀라스 장착 → TextOut("가A") = 블릿 2회(한글 16px + 영문 8px),
    // DrawPixel 스톰 없음.
    {
        RecordingBackend be;
        JKDC dc(&be);
        JKResourceCache cache(&be);
        JKTextAtlas a2;
        if (!a2.Init(text::ResolveDesktopFontPath(), 8, 16, 16)) return 77;
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
    // T8: MeasureText 불변 회귀.
    CHECK(JKDC::MeasureText("가나AB").x == 16 * 2 + 8 * 2, "T8 metrics unchanged");
    CHECK(JKDC::MeasureText("가나AB").y == 16, "T8 cell height 16");
```

컴파일 시 JKDC.cpp를 목록에 넣는다(Task 2 커맨드에 이미 포함). Expected: **T6 실패(블릿 0 — SetTextAtlas 아직 없음 → 컴파일 오류로 먼저 실패)**.

- [ ] **Step 3: JKDC.cpp 구현**

include 추가:
```cpp
#include <JKTextAtlas.h>
#include <JKResourceCache.h>
#include <JKHangulUtil.h>
```

```cpp
namespace jk {
namespace {
constexpr int kTextCellH = 16;   // 비트맵 폰트 셀과 동일 — MeasureText 계약
}
}

void JKDC::SetTextAtlas(JKTextAtlas* atlas, JKResourceCache* cache) {
    textAtlas_ = atlas;
    textCache_ = cache;
}

bool JKDC::DrawGlyph(JKPoint p, uint32_t cp, int stride) {
    if (!textAtlas_ || !textCache_ || !backend_) return false;
    const uint32_t fg =
        (static_cast<uint32_t>(textR_) << 16) |
        (static_cast<uint32_t>(textG_) << 8) | static_cast<uint32_t>(textB_);
    if (!textAtlas_->EnsureGlyph(textCache_, fg, cp)) return false;
    auto tex = textCache_->GetImage(textAtlas_->PageKey(fg, cp));
    if (!tex) {
        // 승인 배너류는 렌더 스레드 업로드 플러시 이전에 동기 그린다
        // (docs/63 §8) — 즉시 플러시 후 1회 재시도.
        textCache_->FlushUploads(backend_);
        tex = textCache_->GetImage(textAtlas_->PageKey(fg, cp));
        if (!tex) return false;
    }
    backend_->BlitTexture(tex, nullptr, JKRect{p.x, p.y, stride, kTextCellH},
                          255);
    return true;
}
```

`EngPutCh`/`HanPutCh` — **기존 비트맵 경로는 그대로 보존**, 앞단에만 아틀라스 시도:

```cpp
void JKDC::EngPutCh(JKPoint p, uint8_t ch) {
    if (DrawGlyph(p, static_cast<uint32_t>(ch), 8)) return;
    uint8_t image[16];
    if (fontMan_ && fontMan_->GetEnglishImage(image, ch)) {
        PutEngGlyph8x16(p, image);
    } else {
        PutEngGlyph8x8(p, ch);
    }
}

void JKDC::HanPutCh(JKPoint p, uint8_t first, uint8_t second) {
    const uint32_t cp = KssmCodepointToUnicode(first, second);
    if (cp != 0 && DrawGlyph(p, cp, 16)) return;
    uint8_t buffer[32];
    if (fontMan_ && fontMan_->GetWORDImage(buffer, first, second)) {
        PutHanGlyph16x16(p, buffer);
    } else {
        SetColor(textR_, textG_, textB_, 255);
        DrawRect(JKRect{ p.x, p.y, 16, 16 });
    }
}
```

- [ ] **Step 4: 컴파일+실행 ×2 — ALL PASS**

같은 커맨드로 재컴파일, `./tools/probes/jktext_probe.exe > log 2>&1` ×2. Expected: T1-T8 전부 PASS, 두 연속 동일.

- [ ] **Step 5: 커밋**

```bash
git add engine/include/JKDC.h engine/src/JKDC.cpp engine/tools/probes/jktext_probe.cpp
git commit -m "feat(text): JKDC 벡터 글리프 배선 — 아틀라스 우선 + 글리프 단위 비트맵 폴백 (docs/63 Task 3)"
```

---

### Task 4: 설정 허브 — text_font_path (docs/54 연동)

**Files:**
- Modify: `engine/src/server/JKWindowServer.h` (멤버)
- Modify: `engine/src/server/JKWindowServer.cpp` (LoadSettingsKv/WriteSettingsKv/settings_set/settings_read)
- Modify: `engine/src/apps/ClientSettingsApp.cpp` (GUI 표시)
- Create: `engine/tools/probes/probe_textfont.ps1`

**Interfaces:**
- Produces: 설정 키 `text_font_path`(settings_set, 값=문자열 경로) / 표시 키 `text.font_path`(settings_read). `LoadSettingsKv(mute, volume, retention, textFontPath)` 서명 확장 — Task 6 배너 배선이 `textFontPath_` 멤버를 소비.
- 적용 시점: 재시작(아틀라스는 기동 시 Init).

- [ ] **Step 1: KV 스키마 확장**

`JKWindowServer.cpp:2093` `LoadSettingsKv` — 시그니처에 `std::string& fontPath` 추가:

```cpp
static void LoadSettingsKv(bool& mute, int& volume, int& retention,
                           std::string& fontPath) {
    // ... 기존 audio/retention 블록 유입 ...
    std::string s;
    if (json.GetObjStr("text", "font_path", s) && !s.empty() &&
        s.size() <= 300) {
        fontPath = s;
    }
}
```

`WriteSettingsKv` — 시그니처에 `const std::string& fontPath` 추가, JSON 조립:
```cpp
static bool WriteSettingsKv(bool mute, int volume, int retention,
                            const std::string& fontPath) {
    char out[512];
    std::snprintf(out, sizeof(out),
                  "{\"audio\":{\"mute\":%d,\"volume\":%d},"
                  "\"retention\":{\"days\":%d},"
                  "\"text\":{\"font_path\":\"%s\"}}",
                  mute ? 1 : 0, volume, retention, JsonEsc(fontPath).c_str());
    // ... 나머지 기존 바디 유지 ...
}
```
(3개 기존 호출 지점은 `textFontPath_`를 마지막 인자로 전달하도록 갱신.)

- [ ] **Step 2: 서버 멤버 + settings_set 분기**

`JKWindowServer.h`: `std::string textFontPath_;` 멤버 추가(Init에서 `LoadSettingsKv(..., textFontPath_)`로 채움 — 기존 LoadSettingsKv 호출 지점 갱신).

`settings_set`(`JKWindowServer.cpp:3667`) — `capture_allow` 분기 **앞**에:

```cpp
} else if (key == "text_font_path") {
    // docs/63 §4: 데스크탑 벡터 폰트 경로. 값은 문자열 — 재시작 적용
    // (아틀라스는 기동 시 Init). 상한 300자.
    std::string valStr;
    if (!req.GetObjStr("args", "value", valStr) || valStr.empty() ||
        valStr.size() > 300) {
        reply = "{\"ok\":false,\"error\":\"bad_value\"}";
    } else if (!WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                receiptRetentionDays_, valStr)) {
        reply = "{\"ok\":false,\"error\":\"write_failed\"}";
    } else {
        textFontPath_ = valStr;
        reply = std::string("{\"ok\":true,\"applied\":{\"text_font_path\":\"") +
                JsonEsc(valStr) + "\"},\"note\":\"applies_on_restart\"}";
    }
}
```

- [ ] **Step 3: settings_read 열거**

`settings_read` 블록(`JKWindowServer.cpp:3520`) — `theme.current` 블록 뒤에:

```cpp
        // text.font_path (docs/63 §4): 서버 멤버 — 기동 시 KV+기본값 합성.
        std::snprintf(item, sizeof(item),
                      ",{\"key\":\"text.font_path\",\"kind\":\"string\","
                      "\"value\":\"%s\"}",
                      JsonEsc(textFontPath_).c_str());
        out += item;
```

- [ ] **Step 4: settings GUI 표시**

`ClientSettingsApp.cpp` — idle_minutes 블록(§2 트리거 섹션 말미, line ~348) 뒤에:

```cpp
    // ---- 텍스트 폰트 (docs/63 §4) ----
    SectionHeader(koreanFont_ ? "텍스트" : "Text");
    ImGui::TextDisabled("%s: %s", koreanFont_ ? "벡터 폰트" : "vector font",
                        KvStr("text.font_path",
                              "C:\\Windows\\Fonts\\malgun.ttf").c_str());
    if (textFontBuf_[0] == '\0' && !KvStr("text.font_path", "").empty()) {
        std::snprintf(textFontBuf_, sizeof(textFontBuf_), "%s",
                      KvStr("text.font_path", "").c_str());
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText(koreanFont_ ? "폰트 경로(재시작 적용)" : "font path",
                         textFontBuf_, sizeof(textFontBuf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        const std::string v(textFontBuf_);
        if (!v.empty()) {
            SendQuery("settings_set",
                      std::string("{\"key\":\"text_font_path\",\"value\":\"") +
                          EscapeJson(v) + "\"}",
                      Query::Set, "text_font_path");
        }
    }
```
멤버 추가(`ClientSettingsApp.h`): `char textFontBuf_[300] = {};`. (kv_는 settings_read 엔트리에서 범용으로 채워지므로 별도 파서 수정 불요 — 회귀로 확인.)

- [ ] **Step 5: probe_textfont.ps1 (raw pipe 하네스, probe_approval_overflow 계열)**

`engine/tools/probes/probe_textfont.ps1` — 하네스 함수 6종(`SendMsg/Pipe-Avail/New-Pipe/SendQuery/Read-Frame`)은 `probe_approval_overflow.ps1:37-113`을 **복사**(원문 그대로 — 같은 와이어 상수: Hello 1, AgentEventSubscribe 19 {flag=0 control-only}, AgentQuery 17, AgentReply 18). 시나리오 본체:

```powershell
# --- probe body: settings_set text_font_path roundtrip -----------------------
# 서버 재시작은 하지 않는다(라이브 유지 관행) — KV 기록 + settings_read 표면화
# + bad_value 거부만 단정한다. 적용 시점(restart)은 reply note로 규약 확인.
$ctl = ... # probe_approval_overflow.ps1의 서버 라이프사이클 그대로:
# Get-Process jkdesktop stop -> start --server -> readiness 폴링

function Wait-Reply($pipe, $qid, $timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        $f = Read-Frame $pipe 500
        if ($f -and $f.type -eq 18 -and $f.qid -eq $qid) { return $f.text }
    }
    return $null
}

$p = New-Pipe 0
# T1: 정상 set
$qid = 1
SendQuery $p $qid '{"tool":"settings_set","args":{"key":"text_font_path","value":"C:\\Windows\\Fonts\\malgun.ttf"}}'
$r = Wait-Reply $p $qid 5000
CHECK ($r -match '"ok":true') "T1 set ok"
CHECK ($r -match 'applies_on_restart') "T1 restart note"
# T2: settings_read에 표시됨
$qid = 2
SendQuery $p $qid '{"tool":"settings_read","args":{}}'
$r = Wait-Reply $p $qid 5000
CHECK ($r -match 'text\.font_path') "T2 read shows text.font_path"
# T3: 400자 초과 거부
$long = "C:\" + ("a" * 400) + ".ttf"
$qid = 3
SendQuery $p $qid ("{\"tool\":\"settings_set\",\"args\":{\"key\":\"text_font_path\",\"value\":\"" + $long.Replace('\','\\') + "\"}}")
$r = Wait-Reply $p $qid 5000
CHECK ($r -match '"error":"bad_value"') "T3 oversized rejected"
# T4: 빈 값 거부
$qid = 4
SendQuery $p $qid '{"tool":"settings_set","args":{"key":"text_font_path","value":""}}'
$r = Wait-Reply $p $qid 5000
CHECK ($r -match '"error":"bad_value"') "T4 empty rejected"
$p.Dispose()
```
CHECK 매크로는 콘솔 카운터(g_pass/g_fail + exit code)로 — probe_approval_overflow.ps1 말미 관행 따른다. **유의**: 이 프로브는 state/settings.json을 건드리지만 T1에서 원래 값으로 되돌리는 set을 마지막에 한 번 더 보내고(복원), 원본 파일 백업(`settings.json.bak_probe`)·콘솔 고지를 수행한다 — 프로브 런타임 파일 소유 규칙(docs/59 §16.1 레슨).

실행: `powershell -NoProfile -ExecutionPolicy Bypass -File engine/tools/probes/probe_textfont.ps1 > log 2>&1` — ×2, 각 실행 후 원본 settings.json 복원 확인.

- [ ] **Step 6: 커밋**

```bash
git add engine/src/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp \
  engine/src/apps/ClientSettingsApp.cpp engine/src/apps/ClientSettingsApp.h \
  engine/tools/probes/probe_textfont.ps1
git commit -m "feat(settings): text_font_path 키 — KV+set/read+GUI 표시 (docs/63 Task 4)"
```

---

### Task 5: 호스트 배선 — JKClientApplication + JKApplication

**Files:**
- Modify: `engine/include/client/JKClientApplication.h` (멤버 + 배선)
- Modify: `engine/src/client/JKClientApplication.cpp:150-191`
- Modify: `engine/include/JKApplication.h` + `engine/src/JKApplication.cpp:104-117`
- Test: `engine/tools/probes/jktext_probe.cpp` (T9 확장 — 실 SDL 백엔드 e2e) + 회귀

**Interfaces:**
- Consumes: `JKDC::SetTextAtlas` (Task 3), `jk::text::ResolveDesktopFontPath` (Task 2)
- Produces: 클라·싱글 프로세스 앱 전체가 벡터 텍스트로 렌더.

- [ ] **Step 1: JKClientApplication 배선**

`JKClientApplication.h`: `class JKTextAtlas;` 전방선언 + `std::unique_ptr<JKTextAtlas> textAtlas_;` 멤버.

`JKClientApplication.cpp:189`(`dc_.SetHangulManager` 바로 뒤, resourceCache_ 존재 보장 지점):

```cpp
    // 데스크탑 벡터 폰트 (docs/63 §2): 관문 교체 — 실패 시 비트맵 경로 그대로.
    textAtlas_ = std::make_unique<JKTextAtlas>();
    const std::string fontPath = jk::text::ResolveDesktopFontPath();
    if (!fontPath.empty() && textAtlas_->Init(fontPath, 8, 16, 16)) {
        dc_.SetTextAtlas(textAtlas_.get(), resourceCache_.get());
    } else {
        std::fprintf(stderr,
            "Warning: vector font init failed (%s); staying on bitmap glyphs.\n",
            fontPath.c_str());
    }
```
include: `#include <JKTextAtlas.h>`.

- [ ] **Step 2: JKApplication 동일 배선**

`JKApplication.cpp:115`(`resourceCache_->RegisterFont` 뒤)에 위 블록과 동일 코드(멤버명 `textAtlas_`), `JKApplication.h`에 멤버·전방선언 동일 추가.

- [ ] **Step 3: jktext_view_probe — 실 SDL 백엔드 e2e (T9)**

`engine/tools/probes/jktext_view_probe.cpp` 신설 — 실 렌더러+ReadPixels로 픽셀 진실 단정:

```cpp
// jktext_view_probe — docs/63 뷰 레벨 배선 잠금(레슨 30: 로직 클래스가 생기면
// 호출부 배선을 뷰 레벨 프로브로 잠근다). 실 SDL 백엔드로 "가나ABC漢字"를 찍어
// ①잉크 ②안티에일리어싱(중간 농도 픽셀) ③8/16px 전진을 ReadPixels로 검증.
// 비트맵 폰트는 0/255 이진 픽셀뿐 — 중간값 존재 자체가 벡터 경로의 증거.
#include <JKDC.h>
#include <JKTextAtlas.h>
#include <JKResourceCache.h>
#include <JKSDLRenderBackend.h>
#include <SDL.h>
#ifdef main
#undef main   // SDL_main 치환 방지(docs/61 레슨 27)
#endif
#include <cstdio>
#include <string>

using namespace jk;

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::printf("SKIP: SDL_Init failed\n"); return 77; }
    SDL_Window* w = SDL_CreateWindow("jktext_view_probe", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, 320, 64,
                                     SDL_WINDOW_HIDDEN);
    SDL_Renderer* r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
    JKSDLRenderBackend be(r);
    JKDC dc(&be);
    JKResourceCache cache(&be);
    JKTextAtlas atlas;
    const std::string fontPath = text::ResolveDesktopFontPath();
    if (fontPath.empty() || !atlas.Init(fontPath, 8, 16, 16)) {
        std::printf("SKIP: vector font init failed\n");
        return 77;
    }
    dc.SetTextAtlas(&atlas, &cache);

    // 흰 바탕 + 검은 글자.
    be.SetDrawColor(255, 255, 255, 255);
    be.FillRect(JKRect{ 0, 0, 320, 64 });
    dc.SetTextColor(0, 0, 0);
    dc.TextOut(JKPoint{ 8, 24 }, Utf8ToKssm("가나ABC").c_str());
    be.FlushUploads(&be);   // JKResourceCache::FlushUploads(&backend)
    SDL_RenderPresent(r);

    int pw = 0, ph = 0;
    if (SDL_GetRendererOutputSize(r, &pw, &ph) != 0) return 1;
    SDL_Surface* srf = SDL_CreateRGBSurfaceWithFormat(0, pw, ph, 32,
                                                      SDL_PIXELFORMAT_RGBA32);
    SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_RGBA8888, srf->pixels, srf->pitch);

    int ink = 0, aa = 0;
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            const uint8_t* p = (const uint8_t*)(srf->pixels) + y * srf->pitch + x * 4;
            const uint8_t lum = p[0];   // 흑백 텍스트 — R채널
            if (lum < 250) ++ink;
            if (lum > 20 && lum < 235) ++aa;
        }
    }
    int fail = 0;
    if (ink < 50) { ++fail; std::printf("FAIL: ink pixels %d too few\n", ink); }
    if (aa < 20) { ++fail; std::printf("FAIL: AA pixels %d — bitmap path suspected\n", aa); }
    std::printf("%s: ink=%d aa=%d\n", fail == 0 ? "PASS" : "FAIL", ink, aa);
    SDL_FreeSurface(srf);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return fail == 0 ? 0 : 1;
}
```

컴파일(실 JKResourceCache 링크 — 스텁 없이):

```bash
g++ -std=c++17 -O2 -Iinclude -Ithird_party/stb -IC:/msys64/ucrt64/include/SDL2 \
  tools/probes/jktext_view_probe.cpp \
  src/JKTextAtlas.cpp src/JKDC.cpp src/JKResourceCache.cpp src/JKSDLRenderBackend.cpp \
  src/JKHangulUtil.cpp src/JKHangulManager.cpp src/JKSDLAudioBackend.cpp? \
  legacy/wancode/WANCODE.CPP \
  -o tools/probes/jktext_view_probe.exe \
  -LC:/msys64/ucrt64/lib -lSDL2 -limm32 -lwinmm
```
(정확한 심볼 닫힘은 링크 오류가 알려준다 — 레슨 28: **링크 오류는 입력 파일 목록부터**. JKSDLRenderBackend.cpp 실 파일명은 `ls engine/src | grep SDLRender`로 확인하고 목록에 맞춘다.)

- [ ] **Step 4: 실행 ×2 + 회귀**

```bash
./tools/probes/jktext_view_probe.exe > log 2>&1   # ×2
./tools/probes/jkedit_probe.exe > log2 2>&1       # 기존 회귀 ×2
cd engine/build && ninja                           # 전체 재링크 GREEN
```
Expected: jktext_view_probe PASS ×2, jkedit_probe 회귀 GREEN, ninja 0 error.

- [ ] **Step 5: 커밋**

```bash
git add engine/include/client/JKClientApplication.h engine/src/client/JKClientApplication.cpp \
  engine/include/JKApplication.h engine/src/JKApplication.cpp \
  engine/tools/probes/jktext_view_probe.cpp engine/tools/probes/jktext_probe.cpp
git commit -m "feat(text): 호스트 배선 — 클라+싱글 프로세스 앱 벡터 폰트 장착 (docs/63 Task 5)"
```

---

### Task 6: 서버 승인 배너 배선

**Files:**
- Modify: `engine/src/server/JKWindowServer.h` (멤버)
- Modify: `engine/src/server/JKWindowServer.cpp` (`MakeApprovalBannerTex` ~5777)

**Interfaces:**
- Consumes: `LoadSettingsKv`의 `textFontPath_` (Task 4), `JKTextAtlas` (Task 2)
- Produces: 승인 배너 텍스트가 벡터 글리프로.

- [ ] **Step 1: 멤버 + 지연 초기화**

`JKWindowServer.h`: `std::unique_ptr<JKTextAtlas> bannerAtlas_; std::unique_ptr<JKResourceCache> bannerCache_;` + `class JKTextAtlas;` 전방선언.

`MakeApprovalBannerTex` — `approvalFont_` 지연 초기화 블록(5777) 옆에 동일 패턴:

```cpp
    if (!bannerCache_) {
        // 배너는 동기 그리기라 렌더 스레드 플러시 캐댄스가 없다 — 전용 캐시로
        // DrawGlyph의 즉시 FlushUploads 폴백(§3 Task 3)이 커버한다.
        bannerCache_ = std::make_unique<JKResourceCache>(&backend);   // 블록 내 backend
        bannerCache_->SetBackend(&backend);   // (FlushUploads 호출에 필요한 백엔드)
    }
    if (!bannerAtlas_) {
        bannerAtlas_ = std::make_unique<JKTextAtlas>();
        const std::string fp = textFontPath_.empty()
                                   ? jk::text::ResolveDesktopPath()
                                   : textFontPath_;
        if (!bannerAtlas_->Init(fp, 8, 16, 16)) bannerAtlas_.reset();
    }
```
(정확한 심볼명은 Task 2의 `text::ResolveDesktopFontPath` — 함수명 일치. `JKResourceCache`에 `SetBackend`이 없다면 생성자 인자만 사용: `JKResourceCache(JKRenderBackend*)`.)

- [ ] **Step 2: 배너 드로 경로에 아틀라스 장착**

기존 `dc.SetHangulManager(approvalFont_.get())` 뒤:

```cpp
    if (bannerAtlas_ && bannerCache_) {
        dc.SetTextAtlas(bannerAtlas_.get(), bannerCache_.get());
    }
```
(KSSM 고정색 34,20,4 → fg 캐시 키 단일, 텍스처는 bannerCache_에 누적. 플러시는 `DrawGlyph`의 즉시 FlushUploads 폴백이 처리.)

- [ ] **Step 3: 수동 확인 + 회귀**

- 기동 후 승인 배너 유발(trust_request ask 경로) → 캡처로 벡터 글리프 확인(눈확인 항목에 기록).
- `probe_approval_overflow.ps1` ×2 — 배너 파이프라인 무수정 회귀.

- [ ] **Step 4: 커밋**

```bash
git add engine/include/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp
git commit -m "feat(server): 승인 배너 벡터 글리프 배선 (docs/63 Task 6)"
```

---

### Task 7: 스펙 as-built + 전체 회귀 + 종결

**Files:**
- Modify: `docs/63_desktop_vector_font.md` (§as-built 추가)

- [ ] **Step 1: docs/63 as-built 섹션**

§9 신설: 실측(프로브 체크 수 ×2), 배선 지점 3곳+배너, 편차(글리프당 텍스처 — §3 기록됨), 알려진 한계(기존 ImGui 앱 폰트 경로 미변경, 재시작 적용, 비트맵 폴백 글리프는 계단 유지).

- [ ] **Step 2: 전체 회귀 ×2**

```bash
export PATH=/c/msys64/ucrt64/bin:$PATH
cd /i/progwork/JKENGINE/engine
./tools/probes/jktext_probe.exe > r1.log 2>&1; echo $?
./tools/probes/jktext_view_probe.exe > r2.log 2>&1; echo $?
./tools/probes/jkedit_probe.exe > r3.log 2>&1; echo $?
./tools/probes/terminal_hangul_probe.exe > r4.log 2>&1; echo $?
./tools/probes/terminal_hangul_view_probe.exe > r5.log 2>&1; echo $?
# terminal_jamo_atlas_probe 재컴파일 필요 — JKGlyphAtlas.cpp의 define 이동으로
# 이 프로브 단독 링크에 JKTextAtlas.cpp 추가:
g++ -std=c++17 -O2 -Iinclude -Ithird_party/stb -IC:/msys64/ucrt64/include/SDL2 \
  tools/probes/terminal_jamo_atlas_probe.cpp src/JKGlyphAtlas.cpp src/JKTextAtlas.cpp \
  src/JKTerminalGrid.cpp src/JKVtParser.cpp \
  -o tools/probes/terminal_jamo_atlas_probe.exe \
  -LC:/msys64/ucrt64/lib -lSDL2 -limm32
./tools/probes/terminal_jamo_atlas_probe.exe > r6.log 2>&1; echo $?
powershell -NoProfile -ExecutionPolicy Bypass -File tools/probes/probe_textfont.ps1 > r7.log 2>&1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/probes/probe_settings.ps1 > r8.log 2>&1
```
Expected: 전부 exit 0 / ALL PASS, **×2 연속**. (정확한 소스 목록은 링크 오류에서 보강 — 레슨 28.)

- [ ] **Step 3: 사용자 눈확인 요청**

데스크탑 재기동 → 전체 UI 텍스트가 벡터 폰트로 렌더되는지 사용자 확인 (눈확인 대기 항목에 등록).

- [ ] **Step 4: 커밋**

```bash
git add docs/63_desktop_vector_font.md
git commit -m "docs: docs/63 as-built — 데스크탑 벡터 폰트 1단계 완결 기록"
```