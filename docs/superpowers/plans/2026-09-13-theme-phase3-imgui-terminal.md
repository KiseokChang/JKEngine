# P2 테마 단계 3(ImGui+터미널+지정 초기자) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ImGui 8앱에 JKTheme 팔레트 봉합(헤더 온리 래퍼)을 설치하고, 터미널 themeBg/Fg를 값 불변 토큰으로 부분 통합하며, C++20 전환으로 3프리셋을 지정 초기자로 전환한다.

**Architecture:** 헤더 온리 `JKThemeImGui.h`(토큰→ImGuiCol 매핑) + JKTheme 신규 토큰 2종(terminalBg/Fg, kDefault 구값 불변) + JKTerminalConfig 키 부재 플래그 + CMake 전역 C++20. 스타일 훅이 부재했던 8앱에 CreateContext 후 1회 호출 시딩.

**Tech Stack:** C++20(신규 전환) / SDL2 / Dear ImGui v1.92.9b / MinGW-Ninja (msys2 ucrt64)

**Spec:** docs/superpowers/specs/2026-09-13-theme-phase3-imgui-terminal-design.md

## Global Constraints

- **지오메트리/PushStyleVar/드로우 순서 불변** — 색과 초기자 문법만 (스펙 §1)
- **의미색 잔존** — notify 미읽 앰버, vplayer 오버레이 4색+TextColored 3건, snap 투명 WindowBg, browser 에러 레드, 테트리스 퍼플, TitleChipColor (스펙 D3)
- **터미널**: terminal.json 키 지정 시 사용자값 우선, 없을 때만 current() 시딩; VT 16색 팔레트(JKVtParser) 미변경; kDefault terminalBg/Fg는 구값 (0x0C0C0C/0xCCCCCC) 불변 (스펙 D4)
- **모든 소비처는 `current()`** — kDefault 직접 참조는 JKTheme.h/JKThemeConfig.cpp 밖 금지
- 지정 초기자로 전환한 뒤 신규 필드 추가도 지정 초기자로 (T2)
- 프로브 15종(`engine/tools/probes/`) — mcp, e2e, palette, chat, chat_llm, triggers, trust, ratelimit, notify, triggerctl, shot, maximize, desktop_resize, terminal_mouse, terminal_select
- 빌드 전제: `export PATH="/c/msys64/ucrt64/bin:$PATH"` (누락 시 컴파일 없이 exit 0 — 레슨 37). exe 링크 락 시 `taskkill //F //IM jkdesktop.exe` (및 jkagentd.exe)
- 실행 브랜치: main 직행 (관례). 위치 탐색은 내용 기준 — 라인번호는 2026-09-13 스캔 참조
- 기존→신규 **대응표를 Task 6에서 docs/47로 기록**

---

### Task 1: C++20 전역 전환 + 3프리셋 지정 초기자

**Files:**
- Modify: `engine/CMakeLists.txt` (:4 `CMAKE_CXX_STANDARD 17` → `20`)
- Modify: `engine/include/theme/JKTheme.h` (3프리셋 초기화 블록)

**Interfaces:**
- Produces: C++20 빌드 + 지정 초기자 프리셋. 이후 모든 JKTheme 필드 추가는 `.field = {...}` 문법.

- [ ] **Step 1: CMake 전환** — `set(CMAKE_CXX_STANDARD 17)` → `set(CMAKE_CXX_STANDARD 20)` (주석에 근거 한 줄: GNU 16.2 지원 + 지정 초기자 요구).

- [ ] **Step 2: 전체 재빌드로 부작용 실측**

```bash
cd I:/progwork/JKENGINE/engine/build
export PATH="/c/msys64/ucrt64/bin:$PATH"
cmake --build . 2>&1 | tail -5
```

Expected: exit 0. C++17→20 상향이 기존 코드에서 오류를 내면 **기록하고 중단해 보고** (룰링: per-target 전환으로 축소하는 승인 전 컨트롤러 판단 필요).

- [ ] **Step 3: 3프리셋 지정 초기자 전환** — kDefault/kLight/kClassic 초기화를 `.chromeTitleBg = {32, 32, 32, 255},` 형태로 전환(필드 순서 유지, `/*주석*/` 라벨은 필드명이 대체하므로 제거). 값은 1도 불변 — 전환 전후 `git diff`에서 값 토큰 동일성 확인.

- [ ] **Step 4: 기계 검증 + 셀프테스트** — 빌드 exit 0, `./jkdesktop test` → 0 failures. 스크립트로 3프리셋의 지정자 35개씩/필드명 정합 확인.

- [ ] **Step 5: 커밋**

```bash
git add engine/CMakeLists.txt engine/include/theme/JKTheme.h
git commit -m "feat(theme): C++20 global + designated initializers for the 3 presets (P2 phase 3)"
```

---

### Task 2: JKThemeImGui.h 래퍼 + 터미널 토큰 2종

**Files:**
- Create: `engine/include/theme/JKThemeImGui.h`
- Modify: `engine/include/theme/JKTheme.h` (terminalBg/terminalFg 2필드 + 3프리셋에 지정 초기자로 추가)

**Interfaces:**
- Produces (T3/T4 소비):
  - `jk::theme::ApplyToImGui(ImGuiStyle& style)` / `jk::theme::ApplyImGuiTheme()`
  - 토큰 `terminalBg`, `terminalFg` (SDL_Color; kDefault/kClassic = {12,12,12}/{204,204,204}, kLight = {250,250,250}/{31,31,31})

- [ ] **Step 1: JKTheme.h에 토큰 2종** — 구조체 끝에 추가(주석: 터미널 기본 전경/배경, terminal.json 키 부재 시에만 소비), 3프리셋에 지정 초기자로 추가:
  - kDefault: `.terminalBg = {12, 12, 12, 255}` (`#0C0C0C` 구값 불변), `.terminalFg = {204, 204, 204, 255}` (`#CCCCCC` 구값)
  - kLight: `.terminalBg = {250, 250, 250, 255}`, `.terminalFg = {31, 31, 31, 255}`
  - kClassic: kDefault와 동일 (구값)

- [ ] **Step 2: JKThemeImGui.h 작성** — JKTheme.h와 동일 스타일의 헤더 온리:

```cpp
// include/theme/JKThemeImGui.h — P2 단계 3: JKTheme→ImGui 팔레트 봉합 (스펙 §1a)
// 헤더 온리 — CMake 변경 없음. 앱은 CreateContext() 직후 1회 호출.
// 지오메트리(라운드/간격)는 ImGui 특성 유지 — 봉합은 팔레트만.
#ifndef JK_THEME_IMGUI_H
#define JK_THEME_IMGUI_H

#include "theme/JKTheme.h"
#include <imgui.h>

namespace jk { namespace theme {

inline ImVec4 ToImVec4(const SDL_Color& c, float alphaScale = 1.0f) {
    return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f,
                  (c.a / 255.0f) * alphaScale);
}

inline void ApplyToImGui(ImGuiStyle& style) {
    const JKTheme& t = current();
    // 표면 — windowClientBg/appClearBg로 수렴 (단계 2 토큰 재사용)
    style.Colors[ImGuiCol_WindowBg]        = ToImVec4(t.windowClientBg);
    style.Colors[ImGuiCol_ChildBg]         = ToImVec4(t.windowClientBg);
    style.Colors[ImGuiCol_PopupBg]         = ToImVec4(t.windowClientBg);
    style.Colors[ImGuiCol_MenuBarBg]       = ToImVec4(t.chromeTitleBg);
    style.Colors[ImGuiCol_TitleBg]         = ToImVec4(t.chromeTitleBg);
    style.Colors[ImGuiCol_TitleBgActive]   = ToImVec4(t.chromeTitleBg);
    style.Colors[ImGuiCol_TitleBgCollapsed]= ToImVec4(t.chromeTitleBg);
    // 텍스트/경계
    style.Colors[ImGuiCol_Text]            = ToImVec4(t.widgetText);
    style.Colors[ImGuiCol_TextDisabled]    = ToImVec4(t.widgetText, 0.5f);
    style.Colors[ImGuiCol_Border]          = ToImVec4(t.chromeBorder);
    style.Colors[ImGuiCol_BorderShadow]    = ToImVec4(t.bevelDark);
    style.Colors[ImGuiCol_Separator]       = ToImVec4(t.bevelLight);
    style.Colors[ImGuiCol_SeparatorHovered]= ToImVec4(t.bevelMid);
    style.Colors[ImGuiCol_SeparatorActive] = ToImVec4(t.focusRing);
    // 입력 필드/버튼 — fieldBg/widgetFace + 상태 변주
    style.Colors[ImGuiCol_FrameBg]         = ToImVec4(t.fieldBg);
    style.Colors[ImGuiCol_FrameBgHovered]  = ToImVec4(t.fieldBg, 1.2f); // 밝기 변주는 아래 참고
    ...
```

  전체 매핑 표는 구현 시 아래 지침으로 완성: **정적 표면은 토큰 1:1, 상태 변주(hover/active)는 같은 토큰의 alphaScale 또는 bevel 토큰 브렌드로** — 새 리터럴 값을 만들지 않는다. Button=widgetFace, ButtonHovered=widgetFace×1.25밝기 헬퍼(`Lighten` inline 헬퍼를 헤더에 추가, ±20% 선형), ButtonActive=bevelMid, Header/Selection=selectionBg, HeaderHovered=selectionBg×1.2, HeaderActive=bevelMid, CheckMark=widgetText, ScrollbarBg=scrollbarTrack, ScrollbarGrab=scrollbarThumb(+Hovered×1.15/Active=focusRing), SliderGrab/Active=selectionBg/focusRing, HeaderText/TextSelectedBg 계열=selectionText/selectionBg, NavHighlight/ModalWindowDimBg는 (0,0,0,알파) 계열 유지.
  - `inline void ApplyImGuiTheme() { ApplyToImGui(ImGui::GetStyle()); }`

- [ ] **Step 3: 빌드 + 셀프테스트** — Task 1 Step 2/4와 동일 명령 (exit 0 + 0 failures).

- [ ] **Step 4: 커밋**

```bash
git add engine/include/theme/JKTheme.h engine/include/theme/JKThemeImGui.h
git commit -m "feat(theme): JKThemeImGui palette seam + terminalBg/Fg tokens (P2 phase 3)"
```

---

### Task 3: ImGui 8앱 스왑 (wrapper 호출 + 루트 클리어 토큰화)

**Files:**
- Modify: `engine/src/apps/ClientImGuiDemoApp.cpp` (클리어 :23 `36,36,43`)
- Modify: `engine/src/apps/ClientTaskmgrApp.cpp` (:29 `32,32,38`)
- Modify: `engine/src/apps/ClientPaletteApp.cpp` (:23 `24,24,30`)
- Modify: `engine/src/apps/ClientNotifyApp.cpp` (:23 `24,24,30`)
- Modify: `engine/src/apps/ClientSnapApp.cpp`
- Modify: `engine/src/apps/ClientShotApp.cpp` (:25 `24,24,30`)
- Modify: `engine/src/apps/ClientVPlayerApp.cpp`
- Modify: `engine/src/apps/ClientBrowserApp.cpp`

**Interfaces:**
- Consumes: T2의 `ApplyImGuiTheme()`, `current().appClearBg`

- [ ] **Step 1: 8앱 공통 패턴** — `#include "theme/JKThemeImGui.h"` 추가 후:
  1. `ImGui::CreateContext()` 직후에 `jk::theme::ApplyImGuiTheme();` 1줄 (주석: `// JKTheme 팔레트 봉합 (P2 단계 3)`)
  2. 루트 클리어 리터럴 → `const auto& t = jk::theme::current(); ... dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);` (클리어 리터럴 있는 5앱)
  3. **의미색 잔존 + 주석 확인**: notify 앰버 PushStyleColor, vplayer 4색 Push+TextColored 3건, snap 투명 WindowBg Push, browser 에러 레드 — 코드 불변, 필요시 `// 의도적 잔존 — 의미색 (P2 테마 스왑 제외)` 주석 보강
  4. PushStyleVar(지오메트리) 불변

- [ ] **Step 2: 빌드 + 셀프테스트** — Task 1 Step 2/4 동일. jkapp_*.dll 8종 재링크 mtime 확인.

- [ ] **Step 3: 커밋**

```bash
git add -A engine/src/apps/ClientImGuiDemoApp.cpp engine/src/apps/ClientTaskmgrApp.cpp engine/src/apps/ClientPaletteApp.cpp engine/src/apps/ClientNotifyApp.cpp engine/src/apps/ClientSnapApp.cpp engine/src/apps/ClientShotApp.cpp engine/src/apps/ClientVPlayerApp.cpp engine/src/apps/ClientBrowserApp.cpp
git commit -m "feat(theme): ImGui apps through JKThemeImGui seam — root clears to appClearBg (P2 phase 3)"
```

---

### Task 4: 터미널 부분 통합 (키 부재 시 시딩)

**Files:**
- Modify: `engine/include/apps/JKTerminalConfig.h` (`bool themeBgSet=false, themeFgSet=false` 추가)
- Modify: `engine/src/apps/JKTerminalConfig.cpp` (`getColor("themeBg")` 성공 시 플래그 세팅 — :152-153 부근)
- Modify: `engine/src/apps/ClientTerminalApp.cpp` (:50 시딩 분기)
- Modify: `engine/src/apps/TerminalApp.cpp` (:48 시딩)

**Interfaces:**
- Consumes: T2의 `terminalBg`/`terminalFg` 토큰

- [ ] **Step 1: JKTerminalConfig 플래그** — public 멤버 2개 추가 + Load()에서 getColor 성공 후 플래그 세팅 (getColor 시맨틱 자체는 불변).

- [ ] **Step 2: 시딩 2곳 수정** — 공통 패턴:

```cpp
#include "theme/JKTheme.h"
// ...
if (cfg.themeBgSet || cfg.themeFgSet)
    view->SetTheme(cfg.themeBg, cfg.themeFg);          // 사용자 지정 우선
else
    view->SetTheme(jk::theme::current().terminalBg,    // 테마 시딩 (P2 단계 3)
                   jk::theme::current().terminalFg);
```

주의: 키가 하나만 지정된 절반 사례(예: themeBg만 있음)는 **지정된 키만 사용자값, 나머지는 토큰**으로 처리 — SetTheme를 두 인자 각각 조건부로 구성.

- [ ] **Step 3: 빌드 + 셀프테스트** — 동일 명령.

- [ ] **Step 4: 커밋**

```bash
git add engine/include/apps/JKTerminalConfig.h engine/src/apps/JKTerminalConfig.cpp engine/src/apps/ClientTerminalApp.cpp engine/src/apps/TerminalApp.cpp
git commit -m "feat(theme): terminal themeBg/Fg seeded from current() when terminal.json lacks keys (P2 phase 3)"
```

---

### Task 5: 최종 게이트 (검증 전용 — 구현 금지)

**Files:** 없음 (보고서만: `.superpowers/sdd/2026-09-13-theme-phase3-imgui-terminal/task-5-report.md`)

- [ ] **Step 1: 풀 빌드 + mtime 게이트** — exe/dll 8종+jkapp_taskbar/jkdesktop/jkwinserver 최신성.
- [ ] **Step 2:** `jkdesktop test` → 0 failures.
- [ ] **Step 3: 프로브 15종** — 전부 exit 0.
- [ ] **Step 4: kDefault 누수 grep** — JKTheme.h/JKThemeConfig.cpp 밖 `jk::theme::kDefault` 0건.
- [ ] **Step 5: 지정 초기자 기계 검증** — 3프리셋 지정자 수 35×3, 필드명=구조체 필드 1:1, 값이 단계 2 게이트 값과 동일(스크립트).
- [ ] **Step 6: ImGui 팔레트 스크린샷** — palette 또는 taskmgr 앱을 다크(기본)로 구동 → 스크린샷 → `theme-phase3-imgui-dark.png`; theme.json `{"preset":"classic"}` → 재구동 → `theme-phase3-imgui-classic.png` (눈검증: 팔레트 전환 추종) → 삭제.
- [ ] **Step 7: 터미널 3상 실측** — (a) terminal.json 부재: 터미널 배경 픽셀 = (12,12,12) (구값 유지 실증); (b) terminal.json에 themeBg 임의색 지정: 사용자값 렌더 확인 후 삭제; (c) theme.json classic: 시딩이 kClassic 토큰(=구값)을 따르는지.

---

### Task 6: 문서 산출물

**Files:**
- Create: `docs/47_theme_phase3_imgui_terminal.md` (EOF 개행 필수)
- Modify: `docs/superpowers/specs/2026-09-13-theme-phase3-imgui-terminal-design.md` (장부에 실행 직감 추가)

- [ ] **Step 1: docs/47 작성** — 개요(사용자 통합 질문→P4 봉인 링크)/커밋 표/대응표(ImGui 매핑 표+루트 클리어 구값→appClearBg, 터미널 시딩 표, 의미색 잔존 목록)/검증(게이트 실측)/실행 직감/후속(P3 핫스왑, P4 통합 질문).
- [ ] **Step 2: 스펙 장부에 실행 직감 행 추가.**
- [ ] **Step 3: 커밋**

```bash
git add docs/47_theme_phase3_imgui_terminal.md docs/superpowers/specs/2026-09-13-theme-phase3-imgui-terminal-design.md
git commit -m "docs: record P2 theme phase 3 execution — docs/47 + spec ledger"
```