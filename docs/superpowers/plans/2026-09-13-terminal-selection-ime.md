# 터미널 선택+클립보드+IME 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** TerminalView에 마우스 드래그 선택 → Ctrl+Shift+C/V 클립보드(bracketed paste 게이트 포함) + IME 조합 인라인 오버레이.

**Architecture:** 선택/복사 로직은 순수 함수로 분리해 self-test 가능하게, 뷰는 이벤트 소비+렌더만. 파서 변경은 bracketed paste accessor 추가뿐.

**Tech Stack:** C++20, SDL2, MinGW UCRT64, CMake(engine/build)

**Spec:** docs/superpowers/specs/2026-09-13-terminal-selection-ime-design.md

## Global Constraints

- 그리드/파서/아틀라스 구조 변경 금지 — JKVtParser에 `BracketedPaste()` accessor
  1개 추가만 허용.
- 두 모드(클라 DOCK_FILL 자식 / 단일 메인 윈도우) 모두 동작 — 단일 모드는
  TerminalView::RespondMessage에서 JKWindow 위임 전에 클라 영역 마우스 선처리.
- 클립보드는 SDL_Get/SetClipboardText (JKEdit.cpp:716 선례).
- 셀 페인트 경로만 확장 — 새 그리기 원시 추가 금지(선택 = fg/bg 스왑).
- self-test: 순수 함수 분리 + `jkdesktop.exe test` 0.
- 커밋 관례: `feat(terminal)`/`test(terminal)`/`docs(terminal)` + Co-Authored-By.

---

### Task 1: 선택 + 클립보드 + bracketed paste + self-test

**Files:**
- Modify: `engine/include/apps/TerminalView.h` / `engine/src/apps/TerminalView.cpp`
  (선택 상태, 마우스 처리, HandleKeyDown 복사/붙여넣기, PaintCell 스왑 렌더)
- Modify: `engine/include/terminal/JKVtParser.h` (BracketedPaste() accessor)
- Modify: `engine/src/main.cpp` selftest (순수 함수 테스트) 또는 기존 selftest 위치
  (검색: `AppSelfTest` — grid/parser 테스트가 이미 있는 곳)
- Create: 순수 함수는 새 헤더 `engine/include/apps/JKTermSelection.h`(인라인)에
  넣어 두 모드와 테스트가 공유

**Interfaces:**
- Consumes: JKTermCell.cp/width, JKTermCharWidth, kTermCellW/H, GetScreenClientRect,
  ev.x/y (윈도우 좌표 — 클라 영역 origin은 GetScreenClientRect로 환산),
  onInput_(data,len), parser bracketedPaste_ 플래그
- Produces: `ExtractSelectedText(cells accessor, x0,y0,x1,y1)` (UTF-8, 행 연결),
  `NormalizeSel`, `SanitizeClipboardPaste` (개행 정리 + \x1b 제거 + bracketed
  래핑) — Task 2/3이 소비하는 것은 렌더 상태뿐

- [ ] **Step 1: 순수 함수** — JKTermSelection.h에 NormalizeSel(anchor,end→
  x0,y0,x1,y1 클램프 포함), ExtractSelectedText(행별 cp→UTF-8, 마지막 non-empty
  셀까지만, 행은 \n 연결), SanitizeClipboardPaste(\r\n/\r→\n, \x1b 제거,
  bracketed면 \x1b[200~/201~ 래핑). UTF-8 인코딩 유틸은 JKVtParser의 디코딩 대칭으로
  로컬 구현.
- [ ] **Step 2: 뷰 상태+마우스** — selAnchor_/selEnd_/selActive_ 멤버, RespondMessage에서
  MouseDown(좌버튼, 클라 영역 내)/MouseMove/ MouseUp 처리, 선택 셀 MarkAllDirty,
  단일 모드에서 JKWindow 위임 전 선처리(클라 영역 밖은 위임 유지).
- [ ] **Step 3: 렌더** — OnPaintClient 셀 루프에서 선택 영역이면 reverse 스왑.
- [ ] **Step 4: 복사/붙여넣기** — HandleKeyDown에서 ctrl&&shift C/V (기존 ctrl+문자
  경로보다 우선 — 수정자 확인 순서 주의), SDL_Set/GetClipboardText, 입력 시
  선택 해제.
- [ ] **Step 5: self-test** — AppSelfTest에 정규화/추출/정리 3함수 케이스 추가
  (한글 cp 포함 — AC00 대역 재인코딩 검증).
- [ ] **Step 6: 빌드 + test 0 + Commit** `feat(terminal): text selection + clipboard copy/paste with bracketed paste (docs/26 단계 2)`

### Task 2: IME 조합 인라인 오버레이

**Files:**
- Modify: `engine/include/apps/TerminalView.h` / `engine/src/apps/TerminalView.cpp`
  (preEdit_ 상태, TextEditing 소비, 오버레이 렌더, 클리어 시점)

**Interfaces:**
- Consumes: JKEventType::TextEditing (ev.text = UTF-8 pre-edit), 그리드 커서 위치,
  JKTermCharWidth, PaintGlyph
- Produces: 없음 (뷰 내부)

- [ ] **Step 1: 소비** — RespondMessage에서 TextEditing → preEdit_ 저장 + MarkAllDirty.
- [ ] **Step 2: 렌더** — 커서 셀부터 셀 단위 오버레이 (배경 밝은 톤 + 글리프, 전각
  2셀, 그리드 폭 클램프). UTF-8 디코딩 재사용.
- [ ] **Step 3: 클리어** — Char/KeyDown/붙여넣기/선택 시작 시. (§3 클리어 시점표)
- [ ] **Step 4: 빌드 + test 0 + Commit** `feat(terminal): inline IME pre-edit overlay at cursor`

### Task 3: 프로브 + 회귀 + 문서

**Files:**
- Create: `engine/tools/probes/probe_terminal_select.ps1`
- Create: `docs/40_terminal_selection_ime.md`
- Modify: `docs/26_terminal_feature_roadmap.md` (§2.2/§3 단계 2/5 상태 갱신)

- [ ] **Step 1: 프로브** — `jkdesktop.exe terminal` 단일 모드 기동(SendInput 관례 —
  probe_agent_maximize.ps1의 VIRTUALDESK/FindWindow/표시 선례), SendKeys로
  `echo hello`+Enter → 마우스 드래그 선택 → Ctrl+Shift+C → 클립보드 검증(Get-Clipboard)
  → Ctrl+Shift+V 재실행 → FAIL exit 1.
- [ ] **Step 2: 회귀** — jkdesktop test 0 + maximize/desktop_resize/e2e 프로브
  (터미널 무관 프로브 전부는 불필요 — 이번 변경은 터미널 파일만 손대므로 3종+test로
  충분, docs에 명시).
- [ ] **Step 3: docs/40 + docs/26 갱신 + Commit** `docs(terminal): selection/clipboard/IME — docs/40 + roadmap update`

## Task 의존성

Task 1 → Task 2 (같은 파일, 순차) → Task 3.