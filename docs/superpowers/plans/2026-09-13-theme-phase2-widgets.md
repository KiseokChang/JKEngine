# P2 테마 단계 2(위젯+스위칭 봉합) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 위젯 라이브러리(Win95 실버)의 하드코딩 색 ~46사이트를 JKTheme 위젯 토큰으로 교체하고, 모든 그리기 사이트를 `jk::theme::current()`로 전환한 뒤 프리셋 3종(다크/라이트/클래식)과 theme.json 프리셋 로딩을 붙인다.

**Architecture:** 단계 1의 `include/theme/JKTheme.h`(constexpr kDefault)를 확장 — 위젯 토큰 15종 추가 + `kLight`/`kClassic` 프리셋 + `current()`/`setTheme()` inline 변수 봉합 + `loadPresetFromFile`(theme.json 1키, jkcore 신규 .cpp). 소비처는 리터럴→`current().토큰` 스왑만 (지오메트리 불변).

**Tech Stack:** C++17 / SDL2 / MinGW-Ninja (msys2 ucrt64)

**Spec:** docs/superpowers/specs/2026-09-13-theme-phase2-widgets-design.md

## Global Constraints

- **색만 변경** — 지오메트리/베벨 깊이/드로우 순서/와이어/로그 문자열 불변 (스펙 §1a, 단계 1 D7 계승)
- **모든 소비처는 `current()` 참조** — `kDefault` 직접 참조는 JKTheme.h/JKThemeConfig.cpp 내부만 허용 (스펙 §1b, 게이트 T5에서 grep 검증)
- **IME 조합 배경 알파 64 보존 / imeCaret 값 (255,0,0) 유지** (스펙 D5)
- **범위 밖**: ClientTestWindowApp/EquipApp 콘텐츠 색, 터미널(terminal.json), ImGui 앱, TitleChipColor FNV-1a, 테트리스 퍼플 (스펙 §1c)
- **theme.json은 프리셋 1키만** — `{"preset": "dark"|"light"|"classic"}`; 임의 색 파싱 없음 (스펙 D3). 파일 없음/깨짐 → 조용히 다크 기본값
- 기존→신규 **대응표를 Task 6에서 docs/46으로 기록**
- 프로브 15종(`engine/tools/probes/`) — mcp, e2e, palette, chat, chat_llm, triggers, trust, ratelimit, notify, triggerctl, shot, maximize, desktop_resize, terminal_mouse, terminal_select
- 빌드 전제: `export PATH="/c/msys64/ucrt64/bin:$PATH"` (누락 시 컴파일 없이 exit 0). 빌드 검증은 grep 필터 금지, exit 0 + exe mtime > 소스 mtime (레슨 37). jkagentd.exe 링크 락 시 `taskkill //F //IM jkagentd.exe`
- 실행 브랜치: main 직행 (P1/P2 관례)
- 위치 탐색은 라인번호가 아니라 내용 기준 — 아래 라인번호는 2026-09-13 스캔 시점 참조
- **kClassic 알려진 한계**: 구값에서 스태틱/체크박스 면은 (240,240,240)이지만 widgetFace 단일 토큰 통합으로 kClassic에선 (192,192,192)로 렌더 — 회귀 게이트는 버튼 면(구값 192) 기준. docs/46에 기록

---

### Task 1: JKTheme.h 확장 + current()/프리셋 + theme.json 로더 (jkcore)

**Files:**
- Modify: `engine/include/theme/JKTheme.h`
- Create: `engine/src/theme/JKThemeConfig.cpp`
- Modify: `engine/CMakeLists.txt` (jkcore 소스 목록에 추가 — 기존 패턴 준용)

**Interfaces:**
- Produces (이후 태스크 전부 소비):
  - `jk::theme::current()` → `const JKTheme&`
  - `jk::theme::setTheme(const JKTheme*)`
  - `jk::theme::kLight`, `jk::theme::kClassic` (constexpr)
  - `jk::theme::loadPresetFromFile(const std::string& path)` → `bool`
  - `jk::theme::DefaultThemePath()` → `std::string` (exe 옆 theme.json)
  - 위젯 토큰 15종: `widgetFace, widgetText, fieldBg, selectionBg, selectionText, bevelLight, bevelDark, bevelMid, scrollbarTrack, scrollbarThumb, focusRing, imeCompositionBg, imeCaret, windowClientBg, appClearBg`

- [ ] **Step 1: JKTheme.h — 필드 추가 (기존 19필드 뒤에 append; 위치 의존 초기화 방지)**

`struct JKTheme` 마지막(taskbarBorderInactive 뒤)에 추가:

```cpp
    // 위젯 라이브러리 — JKControl 계열 (단계 2)
    SDL_Color widgetFace;       // 버튼/스크롤/메뉴/콤보버튼/스태틱/체크박스 면
    SDL_Color widgetText;       // 위젯 텍스트/글리프/캐럿
    SDL_Color fieldBg;          // 입력 필드/리스트/콤보 배경
    SDL_Color selectionBg;      // 선택/하이라이트 배경 (액센트)
    SDL_Color selectionText;    // 선택/하이라이트 텍스트
    SDL_Color bevelLight;       // 3D 가장자리 (밝음)
    SDL_Color bevelDark;        // 3D 가장자리 (어두움)
    SDL_Color bevelMid;         // 썸 그림자/중간 톤
    SDL_Color scrollbarTrack;   // 스크롤바 트랙
    SDL_Color scrollbarThumb;   // 스크롤바 썸 면
    SDL_Color focusRing;        // 포커스 링
    SDL_Color imeCompositionBg; // IME 조합 배경 (알파 64 보존 필수)
    SDL_Color imeCaret;         // IME 조합 캐럿/에러 (값 유지 토큰)
    // 표면 — 클라이언트 영역/앱 클리어 (단계 2)
    SDL_Color windowClientBg;   // JKWindow 클라 영역 배경
    SDL_Color appClearBg;       // JKClientApplication 표면 클리어
```

`kDefault` 초기화 뒤에 동일 순서로 append:

```cpp
    /*widgetFace*/       {43, 43, 43, 255},
    /*widgetText*/       {240, 240, 240, 255},
    /*fieldBg*/          {26, 26, 26, 255},
    /*selectionBg*/      {0, 120, 212, 255},
    /*selectionText*/    {255, 255, 255, 255},
    /*bevelLight*/       {70, 70, 70, 255},
    /*bevelDark*/        {16, 16, 16, 255},
    /*bevelMid*/         {40, 40, 40, 255},
    /*scrollbarTrack*/   {56, 56, 56, 255},
    /*scrollbarThumb*/   {86, 86, 86, 255},
    /*focusRing*/        {0, 120, 212, 255},
    /*imeCompositionBg*/ {0, 120, 212, 64},   // 알파 64 보존
    /*imeCaret*/         {255, 0, 0, 255},    // 값 유지
    /*windowClientBg*/   {32, 32, 32, 255},
    /*appClearBg*/       {32, 32, 32, 255},
```

- [ ] **Step 2: JKTheme.h — current() 봉합 + 프리셋 3종**

`kDefault` 뒤에 추가:

```cpp
// --- 프리셋 (단계 2 스펙 §1b) ---
// kLight: Win11 라이트. kClassic: 단계 1 이전 값 전부 (픽셀 회귀 도구).
// taskbar는 단계 1에서 값 불변 토큰화였으므로 kClassic의 taskbar = kDefault와 동일.
inline constexpr JKTheme kLight = { /* 34필드 — 아래 값표 그대로 */ };
inline constexpr JKTheme kClassic = { /* 34필드 — 아래 값표 그대로 */ };

// --- 스위칭 봉합 ---
// 기동 시 1회 setTheme가 정석 — 렌더 중 스왑은 P3(핫스왑)에서 재검토.
inline const JKTheme* activeTheme_ = &kDefault;
inline JKTheme const& current() { return *activeTheme_; }
inline void setTheme(const JKTheme* t) { if (t) activeTheme_ = t; }

// theme.json {"preset": "dark"|"light"|"classic"} 1키 로더. 파일 없음/깨짐
// → false, 상태 불변 (기본값=다크가 곧 정답).
bool loadPresetFromFile(const std::string& path);
std::string DefaultThemePath();  // exe 옆 theme.json (terminal.json 패턴 준용)
```

**kLight 값표** (34필드 순서 = 구조체 순서):

```cpp
inline constexpr JKTheme kLight = {
    /*chromeTitleBg*/        {243, 243, 243, 255},
    /*chromeTitleText*/      {0, 0, 0, 255},
    /*chromeBorder*/         {204, 204, 204, 255},
    /*chromeActiveBorder*/   {0, 120, 212, 255},
    /*chromeButtonFace*/     {229, 229, 229, 255},
    /*chromeButtonGlyph*/    {0, 0, 0, 255},
    /*chromeCloseHover*/     {196, 43, 28, 255},
    /*desktopBgFallback*/    {243, 243, 243, 255},
    /*launcherCellFace*/     {249, 249, 249, 255},
    /*launcherCellOutline*/  {200, 200, 200, 255},
    /*launcherCellPlaceholder*/ {204, 204, 208, 255},
    /*taskbarBg*/            {243, 243, 243, 255},
    /*taskbarFaceNormal*/    {229, 229, 229, 255},
    /*taskbarFaceActive*/    {204, 204, 204, 255},
    /*taskbarFaceMinimized*/ {236, 236, 236, 255},
    /*taskbarTextNormal*/    {0, 0, 0, 255},
    /*taskbarTextActive*/    {0, 0, 0, 255},
    /*taskbarTextMinimized*/ {96, 96, 96, 255},
    /*taskbarBorderActive*/  {0, 0, 0, 255},
    /*taskbarBorderInactive*/{200, 200, 200, 255},
    /*widgetFace*/           {240, 240, 240, 255},
    /*widgetText*/           {0, 0, 0, 255},
    /*fieldBg*/              {255, 255, 255, 255},
    /*selectionBg*/          {0, 120, 212, 255},
    /*selectionText*/        {255, 255, 255, 255},
    /*bevelLight*/           {255, 255, 255, 255},
    /*bevelDark*/            {160, 160, 160, 255},
    /*bevelMid*/             {160, 160, 160, 255},
    /*scrollbarTrack*/       {229, 229, 229, 255},
    /*scrollbarThumb*/       {205, 205, 205, 255},
    /*focusRing*/            {0, 120, 212, 255},
    /*imeCompositionBg*/     {0, 120, 212, 64},
    /*imeCaret*/             {255, 0, 0, 255},
    /*windowClientBg*/       {249, 249, 249, 255},
    /*appClearBg*/           {240, 240, 240, 255},
};
```

**kClassic 값표** (구값 — docs/45 §3 대응표의 "기존 리터럴" 열):

```cpp
inline constexpr JKTheme kClassic = {
    /*chromeTitleBg*/        {0, 0, 128, 255},      // 구 네이비
    /*chromeTitleText*/      {255, 255, 255, 255},
    /*chromeBorder*/         {192, 192, 192, 255},  // 구 실버
    /*chromeActiveBorder*/   {0, 0, 128, 255},      // 단계 1 전에는 미존재 — 구 테두리색으로 근사
    /*chromeButtonFace*/     {192, 192, 192, 255},
    /*chromeButtonGlyph*/    {255, 255, 255, 255},
    /*chromeCloseHover*/     {196, 43, 28, 255},    // 호버 페인트 미구현이라 무영향
    /*desktopBgFallback*/    {96, 96, 96, 255},
    /*launcherCellFace*/     {100, 100, 100, 255},
    /*launcherCellOutline*/  {255, 255, 255, 255},
    /*launcherCellPlaceholder*/ {128, 128, 128, 255},
    /*taskbarBg*/            {24, 26, 32, 255},     // 이하 taskbar 10필드 = kDefault와 동일 (값 불변 토큰화)
    /*taskbarFaceNormal*/    {56, 58, 68, 255},
    /*taskbarFaceActive*/    {92, 98, 122, 255},
    /*taskbarFaceMinimized*/ {38, 40, 46, 255},
    /*taskbarTextNormal*/    {224, 224, 224, 255},
    /*taskbarTextActive*/    {255, 255, 255, 255},
    /*taskbarTextMinimized*/ {120, 120, 120, 255},
    /*taskbarBorderActive*/  {255, 255, 255, 255},
    /*taskbarBorderInactive*/{70, 72, 84, 255},
    /*widgetFace*/           {192, 192, 192, 255},  // 구 실버 면 (스태틱 240과의 차이는 Global Constraints 마지막 항목)
    /*widgetText*/           {0, 0, 0, 255},
    /*fieldBg*/              {255, 255, 255, 255},
    /*selectionBg*/          {0, 0, 128, 255},      // 구 네이비 선택
    /*selectionText*/        {255, 255, 255, 255},
    /*bevelLight*/           {255, 255, 255, 255},
    /*bevelDark*/            {0, 0, 0, 255},
    /*bevelMid*/             {128, 128, 128, 255},
    /*scrollbarTrack*/       {220, 220, 220, 255},
    /*scrollbarThumb*/       {255, 255, 255, 255},
    /*focusRing*/            {0, 0, 255, 255},
    /*imeCompositionBg*/     {0, 0, 255, 64},       // 구 파랑 조합 — 알파 64
    /*imeCaret*/             {255, 0, 0, 255},
    /*windowClientBg*/       {240, 240, 240, 255},
    /*appClearBg*/           {192, 192, 192, 255},
};
```

- [ ] **Step 3: JKThemeConfig.cpp 작성** — quickjs 없이 1키만 해석 (스펙 D3):

```cpp
// src/theme/JKThemeConfig.cpp — theme.json 프리셋 1키 로더 (P2 단계 2 스펙 §1b)
// terminal.json과 달리 quickjs를 띄우지 않는다: 키가 하나뿐이므로 문자열
// 스캔이면 충분. 파일 없음/깨짐 → false, 활성 테마 불변 (기본값=다크).
#include "theme/JKTheme.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace jk { namespace theme {

bool loadPresetFromFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::string buf;
    char chunk[4096];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) buf.append(chunk, n);
    std::fclose(f);

    const size_t key = buf.find("\"preset\"");
    if (key == std::string::npos) return false;
    const size_t colon = buf.find(':', key + 8);
    if (colon == std::string::npos) return false;
    const size_t q1 = buf.find('"', colon + 1);
    if (q1 == std::string::npos) return false;
    const size_t q2 = buf.find('"', q1 + 1);
    if (q2 == std::string::npos) return false;
    const std::string preset = buf.substr(q1 + 1, q2 - q1 - 1);

    if (preset == "light")       setTheme(&kLight);
    else if (preset == "classic") setTheme(&kClassic);
    else if (preset == "dark")    setTheme(&kDefault);
    else return false;
    std::printf("[theme] preset '%s' from %s\n", preset.c_str(), path.c_str());
    std::fflush(stdout);
    return true;
}

std::string DefaultThemePath() {
    std::string dir;
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        dir = exePath;
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir.resize(slash + 1);
    }
#endif
    return dir + "theme.json";
}

} } // namespace jk::theme
```

CMakeLists.txt의 jkcore 소스 목록에 `src/theme/JKThemeConfig.cpp` 추가 (기존 항목 옆, 알파벳순 관례 준용).

- [ ] **Step 4: 빌드 + 셀프테스트**

```bash
cd I:/progwork/JKENGINE/engine/build
export PATH="/c/msys64/ucrt64/bin:$PATH"
cmake --build . 2>&1 | tail -3
./jkdesktop test 2>&1 | tail -1
```

Expected: exit 0 + 0 failure(s).

- [ ] **Step 5: 커밋**

```bash
git add engine/include/theme/JKTheme.h engine/src/theme/JKThemeConfig.cpp engine/CMakeLists.txt
git commit -m "feat(theme): widget tokens + kLight/kClassic presets + current() seam + theme.json loader (P2 phase 2)"
```

---

### Task 2: 3D/표면 클러스터 스왑 + 단계 1 소비처 current() 전환

**Files:**
- Modify: `engine/include/JKDC.h` (~:50-57 기본 인자)
- Modify: `engine/src/JKButton.cpp` (~:13,14,21,24,33,39)
- Modify: `engine/src/JKScrollBar.cpp` (~:13,14,69,79,90)
- Modify: `engine/src/JKMessageBox.cpp` (~:26,27)
- Modify: `engine/src/JKWindow.cpp` (클라 배경 ~:270 + 단계 1 크롬 소비처)
- Modify: `engine/src/client/JKClientApplication.cpp` (~:491)
- Modify: `engine/src/apps/AppLauncherItem.cpp` (~:24-25)
- Modify: `engine/src/server/JKCompositor.cpp` + `engine/src/desktop/JKDesktopShell.cpp` + `engine/src/apps/ClientTaskbarApp.cpp` (kDefault→current() 전환만)

**Interfaces:**
- Consumes: Task 1의 `current()` + 위젯 토큰 15종
- Produces: 없음 (드로우 내부만). 단, `JKDC.h` 기본 인자가 토큰을 참조하게 되므로 이후 모든 Box3D/Rectangle3D 호출이 자동 테마 추종

- [ ] **Step 1: JKDC.h 기본 인자 토큰화** — include 추가 후:

```cpp
    void Rectangle3D(const JKRect& rect, int32_t depth,
                       uint8_t lightR = jk::theme::current().bevelLight.r,
                       uint8_t lightG = jk::theme::current().bevelLight.g,
                       uint8_t lightB = jk::theme::current().bevelLight.b,
                       uint8_t darkR = jk::theme::current().bevelDark.r,
                       uint8_t darkG = jk::theme::current().bevelDark.g,
                       uint8_t darkB = jk::theme::current().bevelDark.b);
    void Box3D(const JKRect& rect, int32_t depth,
               uint8_t faceR = jk::theme::current().widgetFace.r,
               uint8_t faceG = jk::theme::current().widgetFace.g,
               uint8_t faceB = jk::theme::current().widgetFace.b,
               /* light/dark 동일 패턴 */);
```

기본 인자는 호출 시점 평가이므로 current()를 그대로 쓸 수 있다 (라이브러리 표면의 색도
프리셋을 따르게 되는 지점 — 스펙 §0 직감). 주석도 갱신: `// 3D-styled rectangles
(colors default to jk::theme::current() widgets — P2 단계 2).`

- [ ] **Step 2: 파일별 스왑** — 각 파일 상단에 `#include "theme/JKTheme.h"` 추가, 지역 참조 캡처 `const auto& t = jk::theme::current();` 후 리터럴→토큰. **kDefault 직접 참조 금지 (Global Constraints).**

| 파일 (스캔 라인) | 현 리터럴 | 토큰 |
|---|---|---|
| JKButton.cpp (:13,14 면/텍스트, :21,24,33,39 베벨) | 면 192 / 텍스트 0 / light 255 / dark 0 | widgetFace / widgetText / bevelLight / bevelDark |
| JKScrollBar.cpp (:13,14 트랙/썸, :69,79 베벨+그림자, :90 썸면) | 트랙 220 / 썸 255 / 그림자 128 / light 255 / dark 0 | scrollbarTrack / scrollbarThumb / bevelMid / bevelLight / bevelDark |
| JKMessageBox.cpp (:26,27 면/텍스트) | 192 / 0 | widgetFace / widgetText |
| JKWindow.cpp (:270 클라 배경) | 240,240,240 | windowClientBg (단계 1 스왑 금지 해제) |
| JKClientApplication.cpp (:491 클리어) | 192,192,192 | appClearBg |
| AppLauncherItem.cpp (:24-25 재지정 192/0) | 192 / 0 | widgetFace / widgetText |

JKWindow.cpp의 단계 1 크롬 소비처(`kDefault.` 참조)도 `current().`로 전환.
JKDC 경유 색상 인자 형태: `dc.SetColor(t.widgetFace.r, t.widgetFace.g, t.widgetFace.b, 255)`.

- [ ] **Step 3: 단계 1 소비처 current() 전환** — `JKCompositor.cpp`, `JKDesktopShell.cpp`, `ClientTaskbarApp.cpp`의 `jk::theme::kDefault` → `jk::theme::current()` (값 변화 없음, 참조만 교체).

- [ ] **Step 4: 빌드 + 셀프테스트** — Task 1 Step 4 동일 명령. jkapp_taskbar.dll/jkwinserver.exe mtime 확인.

- [ ] **Step 5: 커밋**

```bash
git add -A engine/include/JKDC.h engine/src/JKButton.cpp engine/src/JKScrollBar.cpp engine/src/JKMessageBox.cpp engine/src/JKWindow.cpp engine/src/client/JKClientApplication.cpp engine/src/apps/AppLauncherItem.cpp engine/src/server/JKCompositor.cpp engine/src/desktop/JKDesktopShell.cpp engine/src/apps/ClientTaskbarApp.cpp
git commit -m "feat(theme): widget 3D/surface cluster + phase-1 consumers through current() (P2 phase 2)"
```

---

### Task 3: 필드/텍스트 클러스터 스왑 (에디트/리스트/콤보/체크/스태틱/메뉴/컨트롤)

**Files:**
- Modify: `engine/src/JKEdit.cpp` (~:19,20 필드, :111,117 배경, :140,149 선택, :166,175,181 텍스트/캐럿, :203,214,219 IME)
- Modify: `engine/src/JKListBox.cpp` (~:25,26,111,116,124,126,128)
- Modify: `engine/src/JKComboBox.cpp` (~:12,13,81,86,96,97)
- Modify: `engine/src/JKCheckBox.cpp` (~:10,11,17,22,26)
- Modify: `engine/src/JKStatic.cpp` (~:14,19)
- Modify: `engine/src/JKMenu.cpp` (~:13,14,85,93,95,147,149,156,158,160)
- Modify: `engine/src/JKControl.cpp` (~:374 포커스 링)
- Modify: `engine/include/JKControl.h` (~:169-174 멤버 기본값 backR_/textR_ 등)

**Interfaces:**
- Consumes: Task 1의 `current()` + 위젯 토큰

- [ ] **Step 1: JKControl.h 멤버 기본값** — backR_/textR_ 등 멤버 기본값은 `jk::theme::current().widgetFace.r` 형태의 표현식으로 교체 (include 추가. 멤버 초기화 시점 = 생성 시점이므로 기동 로딩 후 생성되는 위젯은 프리셋을 따름).

- [ ] **Step 2: 파일별 스왑** — include + `const auto& t = jk::theme::current();` 캡처 후:

| 파일 (스캔 라인) | 현 리터럴 | 토큰 |
|---|---|---|
| JKEdit.cpp (:19,20 필드 배경/텍스트) | 255 / 0 | fieldBg / widgetText |
| 〃 (:111 필드 재페인트, :117 읽기전용 240) | 255 / 240 | fieldBg / widgetFace |
| 〃 (:140,149 선택 배경/텍스트) | 0,0,128 / 255,255,255 | selectionBg / selectionText |
| 〃 (:166,175,181 캐럿 0) | 0 | widgetText |
| 〃 (:203,214,219 IME 조합 배경 (0,0,255,**64**) / 캐럿 (255,0,0)) | 파랑/빨강 | **imeCompositionBg (알파 64 그대로)** / **imeCaret (값 유지)** |
| JKListBox.cpp (:25,26 배경/텍스트) | 255 / 0 | fieldBg / widgetText |
| 〃 (:111,116 선택 0,0,128/흰색) | 네이비 | selectionBg / selectionText |
| 〃 (:124,126,128 베벨 light/dark/기타) | 255 / 0 | bevelLight / bevelDark |
| JKComboBox.cpp (:12,13 필드/텍스트) | 255 / 0 | fieldBg / widgetText |
| 〃 (:81 베벨) | 255/0 | bevelLight / bevelDark |
| 〃 (:86,96,97 버튼 면/화살표) | 192 / 0 | widgetFace / widgetText |
| JKCheckBox.cpp (:10,11 면/텍스트) | 240 / 0 | widgetFace / widgetText |
| 〃 (:17,22,26 체크마크) | 0 | widgetText |
| JKStatic.cpp (:14,19 텍스트/면) | 0 / 240 | widgetText / widgetFace |
| JKMenu.cpp (:13,14,85,93,95 메뉴 면/텍스트/하이라이트) | 192 / 0 / 0,0,128 | widgetFace / widgetText / selectionBg |
| 〃 (:147,149,156,158 팝업) | 동일 클러스터 | 동일 토큰 |
| 〃 (**:160 팝업 텍스트 리터럴 우회**) | 0 (직접 SetRenderDrawColor) | **widgetText로 교정** (스펙 §1c-4 — 유일한 페인트 내 우회) |
| JKControl.cpp (:374 포커스 링 0,0,255) | 파랑 | focusRing |

주의: 위 라인/값은 스캔 참조 — **구현자는 실제 코드를 읽고 역할(면/텍스트/선택/베벨/캐럿)에 맞는 토큰을 매핑**한다. 표와 실제 역할이 어긋나면 역할 우선하고 보고서에 기록. 선택 텍스트(흰색)는 selectionText, 그 외 일반 텍스트는 widgetText로 구분.

- [ ] **Step 3: 빌드 + 셀프테스트** — Task 1 Step 4 동일.

- [ ] **Step 4: 커밋**

```bash
git add -A engine/src/JKEdit.cpp engine/src/JKListBox.cpp engine/src/JKComboBox.cpp engine/src/JKCheckBox.cpp engine/src/JKStatic.cpp engine/src/JKMenu.cpp engine/src/JKControl.cpp engine/include/JKControl.h
git commit -m "feat(theme): field/text widget cluster through current() — incl. IME alpha + menu popup literal fix (P2 phase 2)"
```

---

### Task 4: theme.json 기동 로딩 와이어 (프로세스 3점)

**Files:**
- Modify: `engine/src/main.cpp` (jkdesktop main — `int main(` :2559 근처, 창 생성 전)
- Modify: `engine/src/jkwinserver_main.cpp` (`int main(` :15 근처)
- Modify: `engine/src/client/JKClientApplication.cpp` (`Init(` :81 머리)

**Interfaces:**
- Consumes: Task 1의 `loadPresetFromFile` + `DefaultThemePath`

- [ ] **Step 1: 3곳에 동일 2줄** (include 후):

```cpp
jk::theme::loadPresetFromFile(jk::theme::DefaultThemePath());
```

위치: 각 프로세스의 최초 렌더 이전 (서버는 main 초반, 클라는 Init 머리 —
terminal.json "next to the exe" 패턴 준용). 이중 로딩(서버+클라)은 무해 —
같은 파일을 같은 결과로 읽는다.

- [ ] **Step 2: 빌드 + 셀프테스트** — Task 1 Step 4 동일.

- [ ] **Step 3: 커밋**

```bash
git add engine/src/main.cpp engine/src/jkwinserver_main.cpp engine/src/client/JKClientApplication.cpp
git commit -m "feat(theme): theme.json preset loading at process startup — server/client/init (P2 phase 2)"
```

---

### Task 5: 최종 게이트 (검증 전용 — 구현 금지)

**Files:** 없음 (보고서만: `.superpowers/sdd/2026-09-13-theme-phase2-widgets/task-5-report.md`)

- [ ] **Step 1: 풀 빌드 + mtime 게이트** — `cmake --build .` exit 0; jkdesktop.exe/jkwinserver.exe/jkapp_taskbar.dll(+재링크된 앱 dll)이 engine/src+include 전체보다 최신.
- [ ] **Step 2:** `jkdesktop test` → 0 failures.
- [ ] **Step 3: 프로브 15종 순차 실행** — 전부 exit 0.
- [ ] **Step 4: kDefault 누수 grep** — `kDefault` 직접 참조가 JKTheme.h/JKThemeConfig.cpp 밖에 없는지:

```bash
cd I:/progwork/JKENGINE/engine
grep -rn "jk::theme::kDefault" src/ include/ --include=*.cpp --include=*.h | grep -v "theme/JKTheme.h" | grep -v "JKThemeConfig.cpp"
# Expected: main.cpp의 단위테스트 참조(있다면)만 허용 — 그 외 발견 시 Task 2/3으로 되돌아가 전환 누락 처리
```

- [ ] **Step 5: kDefault 다크 스크린샷 + 픽셀 측정** — `probe_shot.ps1` 계열로 서버 모드 + 단일 프로세스 minesweeper 캡처 → **위젯 버튼 면 픽셀 = #2B2B2B (43,43,43) 측정** (PowerShell System.Drawing GetPixel, docs/45 §4 패턴). 저장: `.superpowers/sdd/2026-09-13-theme-phase2-widgets/theme-phase2-dark.png`.
- [ ] **Step 6: kClassic 픽셀 회귀** — jkdesktop.exe 옆에 `theme.json` 작성:

```json
{"preset": "classic"}
```

단일 프로세스 minesweeper 구동 → 스크린샷 → **버튼 면 픽셀 = (192,192,192) 측정 + 타이틀바 네이비 (0,0,128) 확인**. 확인 후 theme.json **삭제**(원복). 저장: `theme-phase2-classic-regression.png`. (구값으로의 복원이 프리셋 시스템의 정합성을 닫는다 — 스펙 §2)
- [ ] **Step 7: kLight 스모크** — theme.json을 `{"preset": "light"}`로 1회 교체 구동 → 스크린샷(눈검증: 라이트 표면/검은 텍스트) → 삭제. 저장: `theme-phase2-light.png`.
- [ ] **Step 8: JKMenu 팝업 스모크** — 팝업 열림 경로 구동(기존 프로브/수동 경로 활용) → 팝업이 다크면/밝은 텍스트로 그려지는지 확인.

---

### Task 6: 문서 산출물

**Files:**
- Create: `docs/46_theme_phase2_widgets.md` (EOF 개행 필수)
- Modify: `docs/superpowers/specs/2026-09-13-theme-phase2-widgets-design.md` (장부에 실행 직감 추가)

- [ ] **Step 1: docs/46 작성** — 개요/커밋 표/**기존→신규 대응표**(인벤토리 46사이트 단위, IME 알파/imeCaret 값 유지 명시, JKMenu :160 교정, kClassic 스태틱 240→192 한계)/검증 결과(kClassic·다크 픽셀 측정값)/스크린샷 경로/후속(P3 핫스왑, 단계 3 ImGui·터미널).
- [ ] **Step 2: 스펙 장부에 실행 직감 행 추가** (예: JKDC 기본 인자 토큰화 — 라이브러리 표면의 색이 프리셋을 따르게 된 지점; kClassic 회귀 게이트의 실측 결과).
- [ ] **Step 3: 커밋**

```bash
git add docs/46_theme_phase2_widgets.md docs/superpowers/specs/2026-09-13-theme-phase2-widgets-design.md
git commit -m "docs: record P2 theme phase 2 execution — docs/46 + spec ledger"
```