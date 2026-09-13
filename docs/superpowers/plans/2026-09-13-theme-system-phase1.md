# P2 테마 단계 1(셸) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Win11 모던 다크 팔레트를 `jk::theme::kDefault` 단일 진실원으로 중앙화하고, 셸 3클러스터(창 크롬/데스크탑 셸/태스크바)의 하드코딩 색을 토큰으로 교체한다.

**Architecture:** 헤더 온리 테마(`include/theme/JKTheme.h`, jkcore) + 기존 드로우 호출의 리터럴→토큰 스왑. 값 변경은 크롬/런처에 집중, 태스크바는 값 불변 토큰화. `JKWindow.cpp`의 2-TU 색 복제(미러) 소멸.

**Tech Stack:** C++17 / SDL2 / MinGW-Ninja (msys2 ucrt64)

**Spec:** docs/superpowers/specs/2026-09-13-theme-system-design.md

## Global Constraints

- **색만 변경** — 지오메트리/레이아웃/와이어 프로토콜/로그 문자열/애셔 불변 (스펙 §2-1, D7)
- **태스크바(ClientTaskbarApp.cpp)는 값 불변 토큰화** — 리터럴→토큰만, 값 동일 (플랜 결정)
- **JKWindow.cpp 클라이언트 영역 배경 `(240,240,240)`은 토큰화하지 않는다** — 위젯 표면이라 단계 2 영역 (스펙 §5-1)
- **터미널/ImGui/위젯 라이브러리(JKControl 10파일)는 건드리지 않는다** (스펙 §0 단계 표)
- 기존→신규 색 **대응표를 Task 6에서 docs/44로 기록** — 커밋별 대응표 대신 최종 표 1장
- 프로브 15종(`engine/tools/probes/`): mcp, e2e, palette, chat, chat_llm, triggers, trust, ratelimit, notify, triggerctl, shot, maximize, desktop_resize, terminal_mouse, terminal_select
- 빌드 전제: `export PATH="/c/msys64/ucrt64/bin:$PATH"` (누락 시 컴파일 없이 exit 0 — cc1plus DLL). 빌드 검증은 grep 필터 금지, exit 0 + exe mtime > 소스 mtime (레슨 37)
- 실행 브랜치: main 직행 (P1 관례 유지)
- 위치 탐색은 라인번호가 아니라 내용 기준 — 아래 라인번호는 스캔 시점 참조

---

### Task 1: JKTheme 헤더 (jkcore)

**Files:**
- Create: `engine/include/theme/JKTheme.h`

**Interfaces:**
- Produces: `#include "theme/JKTheme.h"` → `namespace jk::theme`의 `struct JKTheme` + `inline constexpr JKTheme kDefault`. 모든 색 필드는 `SDL_Color`. 후속 태스크 전부가 소비.

- [ ] **Step 1: 헤더 작성**

```cpp
// include/theme/JKTheme.h — P2 테마 단계 1 유일 진실원 (스펙 §1)
// 값 근거: Win11 모던 다크 + 액센트 #0078D4. 태스크바 필드는 기존 값 유지
// (값 불변 토큰화 — docs/superpowers/plans/2026-09-13-theme-system-phase1.md).
#ifndef JK_THEME_H
#define JK_THEME_H

#include <SDL.h>

namespace jk { namespace theme {

struct JKTheme {
    // 창 크롬 — JKCompositor(컴포지터)와 JKWindow(단일 프로세스) 공유
    SDL_Color chromeTitleBg;        // 타이틀바 배경
    SDL_Color chromeTitleText;      // 타이틀 텍스트
    SDL_Color chromeBorder;         // 창 테두리/버튼 윤곽
    SDL_Color chromeActiveBorder;   // 활성 창 액센트 경계 (예약 — 현재 미사용, 값만 확정)
    SDL_Color chromeButtonFace;     // 닫기/최대화 버튼 면
    SDL_Color chromeButtonGlyph;    // 버튼 글리프(X/사각형)
    SDL_Color chromeCloseHover;     // 닫기 버튼 호버 (Win11 빨강)
    // 데스크탑/런처 — JKDesktopShell
    SDL_Color desktopBgFallback;    // 배경 애셔 부재 시 클리어색
    SDL_Color launcherCellFace;     // 런처 셀 면 (폴백)
    SDL_Color launcherCellOutline;  // 런처 셀 윤곽
    SDL_Color launcherCellPlaceholder; // 앱별 플레이스홀더 폴백(제너릭)
    // 태스크바 — ClientTaskbarApp (값 = 기존 리터럴 그대로)
    SDL_Color taskbarBg;
    SDL_Color taskbarFaceNormal, taskbarFaceActive, taskbarFaceMinimized;
    SDL_Color taskbarTextNormal, taskbarTextActive, taskbarTextMinimized;
    SDL_Color taskbarBorderActive, taskbarBorderInactive;
};

inline constexpr JKTheme kDefault = {
    /*chromeTitleBg*/        {32, 32, 32, 255},     // #202020 (구: 실버 192,192,192 계열)
    /*chromeTitleText*/      {240, 240, 240, 255},
    /*chromeBorder*/         {68, 68, 68, 255},     // #444444
    /*chromeActiveBorder*/   {0, 120, 212, 255},    // #0078D4
    /*chromeButtonFace*/     {38, 38, 38, 255},
    /*chromeButtonGlyph*/    {240, 240, 240, 255},
    /*chromeCloseHover*/     {196, 43, 28, 255},    // Win11 close red
    /*desktopBgFallback*/    {28, 28, 30, 255},     // #1C1C1E (구: 96,96,96)
    /*launcherCellFace*/     {44, 44, 46, 255},     // (구: 100,100,100)
    /*launcherCellOutline*/  {255, 255, 255, 255},  // 불변
    /*launcherCellPlaceholder*/ {80, 80, 84, 255},  // 제너릭 폴백 (구: 128,128,128)
    /*taskbarBg*/            {24, 26, 32, 255},
    /*taskbarFaceNormal*/    {56, 58, 68, 255},
    /*taskbarFaceActive*/    {92, 98, 122, 255},
    /*taskbarFaceMinimized*/ {38, 40, 46, 255},
    /*taskbarTextNormal*/    {224, 224, 224, 255},
    /*taskbarTextActive*/    {255, 255, 255, 255},
    /*taskbarTextMinimized*/ {120, 120, 120, 255},
    /*taskbarBorderActive*/  {255, 255, 255, 255},
    /*taskbarBorderInactive*/{70, 72, 84, 255},
};

} } // namespace jk::theme

#endif // JK_THEME_H
```

참고: 테트리스 플레이스홀더 `(128,0,128)`(퍼플)은 **개별 앱 식별색이라 토큰 불가** —
제너릭 폴백만 `launcherCellPlaceholder`로. 구현 시 테트리스 케이스는 기존
분기 구조 유지 (셀 face가 플레이스홀더색으로 덮이는 기존 로직 그대로, 값만
`kDefault.launcherCellPlaceholder` 스왑 후 테트리스 특례만 리터럴 잔존 —
Task 6 대응표에 "의도적 잔존"으로 기록).

- [ ] **Step 2: include 도달 확인**

```bash
cd I:/progwork/JKENGINE/engine/build
export PATH="/c/msys64/ucrt64/bin:$PATH"
cmake --build . 2>&1 | tail -3
```
Expected: exit 0 (헤더만 추가 — 빌드 산출물 변화 없음. jkcore의 public
include dir가 `engine/include` 루트이므로 `theme/` 하위는 CMake 변경 불필요.
만약 기존 헤더가 `include/` 루트만 포함이면 CMakeLists jkcore 블록에서
include dir 확인 후 판정 — 코드 변경 없이 확인만).

- [ ] **Step 3: 커밋**

```bash
git add engine/include/theme/JKTheme.h
git commit -m "feat(theme): add JKTheme shared header — Win11 modern default palette (P2 phase 1)"
```

---

### Task 2: 창 크롬 클러스터 (JKCompositor + JKWindow 미러 소멸)

**Files:**
- Modify: `engine/src/server/JKCompositor.cpp` (DrawCloseOverlay ~:313-340, DrawMaximizeButton ~:346-383)
- Modify: `engine/src/JKWindow.cpp` (PaintWindow ~:205-260)

**Interfaces:**
- Consumes: `jk::theme::kDefault` (Task 1)
- Produces: 없음 (드로우 내부만)

- [ ] **Step 1: JKCompositor.cpp 스왑** — 파일 상단에 `#include "theme/JKTheme.h"` 추가. 대응:

| 현 리터럴 | 토큰 | 새 값 |
|---|---|---|
| btn fill `SDL_SetRenderDrawColor(rend, 192,192,192,…)` (두 함수 각각) | `t_.chromeButtonFace` | (38,38,38) |
| outline `(0,0,0)` | `t_.chromeBorder` | (68,68,68) |
| glyph `(255,255,255)` | `t_.chromeButtonGlyph` | (240,240,240) |

패턴(로컬 참조 캡처 후 사용):

```cpp
const auto& t = jk::theme::kDefault;
SDL_SetRenderDrawColor(rend, t.chromeButtonFace.r, t.chromeButtonFace.g, t.chromeButtonFace.b, 255);
```

기존 `JKWindow.cpp:171-229` 미러 주석(JKCompositor.cpp 내)은 값 참조가
사라지므로 **주석도 함께 갱신**: `// colors: jk::theme::kDefault (P2)`.

- [ ] **Step 2: JKWindow.cpp 스왑** — 상단에 include 추가. 대응:

| 현 리터럴 | 토큰 |
|---|---|
| 타이틀바 네이비 `(0,0,128)` | `t_.chromeTitleBg` |
| 타이틀 텍스트 흰색 `(255,255,255)` / 네이비 `(0,0,128)` | `t_.chromeTitleText` / `t_.chromeTitleBg` |
| 닫기 버튼 면 `(192,192,192)` | `t_.chromeButtonFace` |
| 닫기 윤곽 `(0,0,0)` | `t_.chromeBorder` |
| 닫기 X 흰색 `(255,255,255)` | `t_.chromeButtonGlyph` |
| 창 테두리 `(192,192,192)` | `t_.chromeBorder` |
| **클라이언트 배경 `(240,240,240)`** | **스왑 금지 — 리터럴 유지 (Global Constraint)** |

JKDC 경유 호출이므로 `dc.SetColor(t_.chromeTitleBg.r, t_.chromeTitleBg.g, t_.chromeTitleBg.b, 255)` 형태.

- [ ] **Step 3: 빌드 + 셀프테스트**

```bash
cmake --build . 2>&1 | tail -3   # exit 0 + jkdesktop.exe/jkwinserver.exe mtime 확인
./jkdesktop test 2>&1 | tail -1  # 0 failure(s)
```

- [ ] **Step 4: 커밋**

```bash
git add engine/src/server/JKCompositor.cpp engine/src/JKWindow.cpp
git commit -m "feat(theme): window chrome through JKTheme — JKWindow mirror literals retired (P2 phase 1)"
```

---

### Task 3: 데스크탑 셸 (런처 폴백색)

**Files:**
- Modify: `engine/src/desktop/JKDesktopShell.cpp` (Draw ~:191-241)

**Interfaces:**
- Consumes: `jk::theme::kDefault` (Task 1)

- [ ] **Step 1: 스왑** — include 추가 + 대응:

| 현 리터럴 | 토큰 |
|---|---|
| 프레임 클리어 `(96,96,96)` | `t_.desktopBgFallback` |
| 제너릭 폴백 `(100,100,100)` | `t_.launcherCellFace` |
| 마인 플레이스홀더 `(128,128,128)` | `t_.launcherCellPlaceholder` |
| 테트리스 `(128,0,128)` | **리터럴 잔존 (의도적 — 앱 식별색)** |
| 셀 윤곽 흰색 `(255,255,255)` | `t_.launcherCellOutline` |

- [ ] **Step 2: 빌드 + 셀프테스트** — Task 2 Step 3과 동일 명령.

- [ ] **Step 3: 커밋**

```bash
git add engine/src/desktop/JKDesktopShell.cpp
git commit -m "feat(theme): launcher/desktop fallback colors through JKTheme (P2 phase 1)"
```

---

### Task 4: 태스크바 (값 불변 토큰화)

**Files:**
- Modify: `engine/src/apps/ClientTaskbarApp.cpp` (TaskbarButton::OnPaintClient ~:155-197, TaskbarWindow::OnPaintClient ~:201-210)

**Interfaces:**
- Consumes: `jk::theme::kDefault` (Task 1)

- [ ] **Step 1: 스왑** — include 추가 + 대응 (**모든 새 값 = 기존 값 동일**):

| 현 리터럴 | 토큰 |
|---|---|
| 바 배경 `(24,26,32)` | `t_.taskbarBg` |
| 활성 면 `(92,98,122)` / 텍스트 흰색 | `taskbarFaceActive` / `taskbarTextActive` |
| 최소화 면 `(38,40,46)` / 텍스트 `(120,120,120)` | `taskbarFaceMinimized` / `taskbarTextMinimized` |
| 일반 면 `(56,58,68)` / 텍스트 `(224,224,224)` | `taskbarFaceNormal` / `taskbarTextNormal` |
| 활성 경계 흰색 / 비활성 `(70,72,84)` | `taskbarBorderActive` / `taskbarBorderInactive` |

`TitleChipColor`(FNV-1a, ~:23-32)는 **건드리지 않는다** (스펙 §2-5 개인화).
스캔에서 최소화 면 `(38,40,46)`과 비활성 경계 `(70,72,84)` — 대응표로 확정.

- [ ] **Step 2: 빌드 + 셀프테스트** — Task 2 Step 3과 동일 (jkapp_taskbar 재링크 확인:
  `ls -la jkapp_taskbar.dll` mtime).

- [ ] **Step 3: 커밋**

```bash
git add engine/src/apps/ClientTaskbarApp.cpp
git commit -m "feat(theme): taskbar colors through JKTheme — token-only, values unchanged (P2 phase 1)"
```

---

### Task 5: 최종 게이트 (검증 전용 — 구현 금지)

**Files:** 없음 (보고서만: `.superpowers/sdd/2026-09-13-theme-system-phase1/task-5-report.md`)

- [ ] **Step 1: 풀 빌드 + mtime 게이트** — `cmake --build .` exit 0, 두 exe + jkapp_taskbar.dll이 engine/src+include 전체보다 최신.
- [ ] **Step 2:** `jkdesktop test` → 0 failures.
- [ ] **Step 3: 프로브 15종 순차 실행** — 전부 exit 0. (색 변경이 파서/와이어를 건드리지 않음의 실측)
- [ ] **Step 4: 스크린샷 스모크** — probe_shot.ps1 실행 → 캡처본 확인; 서버 실행 상태에서
  태스크바/크롬/런처가 다크 톤인지 스크린샷 1장을 `.superpowers/sdd/.../theme-phase1-after.png`로 저장
  (사용자 눈검증 자료 — 스펙 §4). **단일 프로세스 크롬 확인 추가**: `./jkdesktop minesweeper`를
  잠시 구동해 단일 프로세스 타이틀바/테두리(구 네이비→#202020)도 캡처에 포함 — 미러 소멸이
  양쪽에서 같은 값을 보장하는지 확인 (스펙 §4).
- [ ] **Step 5: 리터럴 누수 스캔** — 아래 grep에서 잔존 확인(허용: 테트리스 퍼플 + 클라이언트 배경 240):

```bash
cd I:/progwork/JKENGINE/engine
grep -n "192, *192, *192\|0, *0, *128" src/server/JKCompositor.cpp src/JKWindow.cpp src/desktop/JKDesktopShell.cpp src/apps/ClientTaskbarApp.cpp
# Expected: JKWindow.cpp 클라이언트 배경(240,240,240) 주변 0,0,128 참조 외 없음 — 발견 시 Task 2-4로 되돌아가 스왑 누락 처리
```

---

### Task 6: 문서 산출물

**Files:**
- Create: `docs/44_theme_system_phase1.md`
- Modify: `docs/superpowers/specs/2026-09-13-theme-system-design.md` (§6 장부에 실행 직감 추가)

- [ ] **Step 1: docs/44 작성** — 개요/커밋 표/**기존→신규 대응표** (스캔 사이트 단위, 의도적 잔존 2건 명시: 테트리스 퍼플, 클라이언트 배경 240)/검증 결과/스크린샷 경로/후속(단계 2 위젯, 단계 3 ImGui·터미널).
- [ ] **Step 2: 스펙 §6 실행 직감 행 추가** (예: 값 불변 토큰화의 효용 — 리뷰가 값 무관해져 토큰 정확성만 검사).
- [ ] **Step 3: 커밋**

```bash
git add docs/44_theme_system_phase1.md docs/superpowers/specs/2026-09-13-theme-system-design.md
git commit -m "docs: record P2 theme phase 1 execution — docs/44 + spec ledger"
```