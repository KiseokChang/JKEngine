# 터미널 마우스 보고 + 고급 키 모드 + DECSCUSR 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** TUI 앱(htop/vim)이 DECSET 1000/1002/1006으로 마우스를 요청하면 SGR/X10 시퀀스로 pty에 보고 + 앱 커서 키/수식어 인코딩 + DECSCUSR 커서 모양.

**Architecture:** 모드 추적은 파서(JKVtParser), 인코딩은 순수 함수(새 JKTermInput.h — JKTermSelection.h 선례), 라우팅/렌더는 TerminalView. 그리드에는 커서 모양 enum만 추가. 클라 프로토콜 변경 0 — onInput_ 기존 경로 재사용.

**Tech Stack:** C++20, MinGW UCRT64 (`export PATH="/c/msys64/ucrt64/bin:$PATH"`), SDL2, CMake(engine/build)

**Spec:** docs/superpowers/specs/2026-09-13-terminal-mouse-report-design.md

## Global Constraints

- 클라 프로토콜/와이어 변경 금지 — 인코딩 결과는 기존 `onInput_` 경로만 통과.
- 파서는 순수 C++ 유지 (no SDL/Windows 헤더 — JKVtParser.h 선례).
- 인코딩은 순수 인라인 함수 `engine/include/apps/JKTermInput.h` — self-test와
  뷰가 공유 (JKTermSelection.h 선례).
- 기존 동작 보존: 마우스 보고 OFF 상태에서 선택/스크롤/복사는 docs/40 그대로
  (probe_terminal_select가 회귀 게이트).
- self-test: 순수 함수 + 파서 모드를 `jkdesktop.exe test`에 추가, 0 실패.
- 커밋 관례: `feat(terminal)`/`test(terminal)`/`docs(terminal)` + Co-Authored-By.
- 빌드: `cmake --build build --target jkdesktop` — **빌드 출력 tail을 반드시
  확인** (ninja stop + 구 exe 오판 레슨).

---

### Task 1: 파서 모드 추적 + DECSCUSR + 그리드 커서 모양

**Files:**
- Modify: `engine/include/terminal/JKVtParser.h` / `engine/src/terminal/JKVtParser.cpp`
- Modify: `engine/include/terminal/JKTerminalGrid.h` (+ .cpp 필요 시)
- Modify: `engine/src/main.cpp` selftest (파서 모드/DECSCUSR 케이스)

**Interfaces:**
- Produces (Task 2/3 소비): `MouseMode()` → enum {Off, Normal, Button, Any},
  `SgrMouse()` → bool, `AppCursorKeys()` → bool;
  `JKTerminalGrid::SetCursorShape/GetCursorShape` → enum CursorShape
  { Block, Underline, Bar } (기본 Block)

- [ ] **Step 1: 모드 플래그** — HandlePrivateMode에 1/1000/1002/1003/1006 추적
  (2004 선례). enum MouseMode { Off=0, Normal=1000, Button=1002, Any=1003 }.
  h→set, l→reset. 1049/alt는 마우스 모드에 손대지 않음.
- [ ] **Step 2: DECSCUSR** — `CSI Ps SP q`: csiBuf_에 중간 바이트 0x20이 있고
  final이 'q'면 Ps(0-6)를 CursorShape로 매핑(0/1/2→Block, 3/4→Underline,
  5/6→Bar, blink 비트 무시). 파싱은 DispatchCsi 진입 전 csiBuf_ 스캔 한 곳에서
  (새 상태 머신 불필요 — 중간 바이트는 이미 csiBuf_에 적립됨, cpp:158).
- [ ] **Step 3: 그리드** — JKTerminalGrid에 CursorShape enum + 멤버 + setter/
  getter (기본 Block, 리셋/리사이즈가 건드리지 않음).
- [ ] **Step 4: self-test** — DECSET 1000/1002/1006/1 set+reset, alt 종료가
  마우스 모드를 해제하지 않음, DECSCUSR 5종(`\x1b[2 q` Block, `\x1b[3 q`
  Underline, `\x1b[6 q` Bar), 파서 시퀀스 주입은 기존 termselect/terminal
  케이스의 feed() 관례 따름.
- [ ] **Step 5: 빌드 + test 0 + Commit** `feat(terminal): mouse-mode + app-cursor DECSET tracking + DECSCUSR (docs/26 단계 3)`

### Task 2: JKTermInput.h 순수 인코딩 + 키보드 위임

**Files:**
- Create: `engine/include/apps/JKTermInput.h` (header-only)
- Modify: `engine/src/apps/TerminalView.cpp` — HandleKeyDown 화살표/네비 계열을
  EncodeArrow로 위임 (기존 seq 문자열 switch 대체)
- Modify: `engine/src/main.cpp` selftest (인코딩 케이스)

**Interfaces:**
- Consumes: Task 1의 `AppCursorKeys()`
- Produces: `EncodeMouseSgr(btn,x,y,kind,mods)`(kind: Press/Motion→'M',
  Release→'m'), `EncodeMouseX10(btn,x,y)`(누름만, 223 캡), `EncodeArrow(key,
  mods, appCursor)`(key: Up/Down/Left/Right/Home/End/PgUp/PgDn),
  `EncodeWheelAlt(up, n)`(n개 화살표 시퀀스 반환)

- [ ] **Step 1: EncodeMouseSgr** — `\x1b[<b;x;yM/m`: btn 0/1/2 + 4 Shift +
  8 Meta + 16 Ctrl + 32 모션(버튼 눌린 모션) + 64/65 휠. 좌표 1-based.
- [ ] **Step 2: EncodeMouseX10** — `\x1b[M` + 32+b + 32+x + 32+y, x/y 223
  캡, 누름만 (뗌/모션은 빈 문자열 반환 — 표준).
- [ ] **Step 3: EncodeArrow** — mods 없음: appCursor ? `\x1bOA` 계열 :
  `\x1b[A` 계열 (Up/Down/Left/Right + Home/End는 F/H — SS3 `\x1bOH/\x1bOF`,
  CSI `\x1b[H/\x1b[F`). mods 있음: `CSI 1;<m><char>` (m=1+shift1+alt2+ctrl4),
  PgUp/PgDn은 `CSI 5;<m>~`/`CSI 6;<m>~`.
- [ ] **Step 4: EncodeWheelAlt** — up ? `\x1b[A` : `\x1b[B` n개 연결.
- [ ] **Step 5: HandleKeyDown 위임** — 기존 SDLK_UP..SDLK_PAGEDOWN switch를
  EncodeArrow 호출로 교체 (mods는 ev.option; appCursor는 parser 접근자).
  ctrl+문자(0x03) 기존 경로 불변 — **ctrl&&shift C/V 복사/붙여넣기 게이트보다
  뒤**에 위치 유지 (docs/40 키 순서 회귀 금지).
- [ ] **Step 6: self-test** — SGR press/release/motion/wheel/수식어 비트,
  X10(223 캡, 뗌 빈 문자열), 화살표 3형태(CSI/SS3/수식어 Ctrl+Left=`\x1b[1;5D`),
  휠→화살표 n개.
- [ ] **Step 7: 빌드 + test 0 + Commit** `feat(terminal): SGR/X10 mouse + arrow encoding pure fns (JKTermInput.h)`

### Task 3: 뷰 마우스 라우팅 + DECSCUSR 렌더

**Files:**
- Modify: `engine/include/apps/TerminalView.h` / `engine/src/apps/TerminalView.cpp`
  (RespondMessage 마우스 게이트, HandleWheel, PaintCell 커서 모양)

**Interfaces:**
- Consumes: Task 1 접근자들 + grid GetCursorShape, Task 2 인코딩 함수들

- [ ] **Step 1: 마우스 게이트** — RespondMessage 선처리의 마우스 케이스 앞에
  `parser_->MouseMode() != Off && !shift` 분기: MouseDown/Up/Motion →
  EncodeMouseSgr(SGR on) 또는 EncodeMouseX10(누름만, SGR off — 모션/뗌은
  전송 안 함) → onInput_. 좌표 = CellFromPoint 산식 그대로 +1 (1-based),
  그리드 폭/높이 클램프. 모션은 Button 모드에선 버튼 눌린 동안만(버튼 상태
  추적 — MouseDown에서 버튼 비트 기억, Up에서 해제), Any 모드에선 항상.
  휠 → SGR 64/65. **이 분기는 선택 상태를 건드리지 않는다** (ClearSelection
  없음, selActive_ 유지 — 앱이 좌표를 받는 동안 기존 선택은 화면에 유지).
  Shift 눌림(ev.option & KMOD_SHIFT)이면 게이트를 통과해 기존 선택 경로로.
  클라 영역 밖 MouseMove는 기존 위임 로직 유지.
- [ ] **Step 2: 휠 폴백** — MouseMode Off + grid_->InAltScreen() →
  EncodeWheelAlt(up, 3) → onInput_ (스크롤 스킵). Off + 일반 화면 → 기존 로컬
  스크롤 (변경 없음).
- [ ] **Step 3: DECSCUSR 렌더** — PaintCell의 커서 블록(`isCursor && visible
  && blinkOn_`, cpp:201-204)에 shape 분기: Block=기존, Underline=셀 하단 2px,
  Bar=셀 좌측 2px (색상은 기존 커서와 동일).
- [ ] **Step 4: 빌드 + test 0 + Commit** `feat(terminal): mouse event routing to TUI apps + DECSCUSR cursor shapes (docs/26 단계 3)`

### Task 4: 프로브 + 회귀 + 문서

**Files:**
- Create: `engine/tools/probes/probe_terminal_mouse.ps1`
- Create: `docs/41_terminal_mouse_report.md`
- Modify: `docs/26_terminal_feature_roadmap.md` (§2.2/§3 단계 3 상태)

- [ ] **Step 1: 프로브** — `jkdesktop.exe terminal` 단일 모드 기동
  (probe_terminal_select 관례: VIRTUALDESK SendInput, FindWindow, PID 기반
  정리). 임시 스크립트 작성: `[Console]::Out.Write("`e[?1000;1006h")` 후
  stdin 바이트를 16진 텍스트로 화면 출력하는 루프 → 터미널에 실행 명령 타이핑 →
  알려진 셀 SendInput 클릭 → 화면 리드백에 SGR 바이트(`1b 3c 30 ...` 형태)
  도착 assert → 정리 시 `[Console]::Out.Write("`e[?1000l`e[?1006l")` + 프로세스
  정리. ASCII-only + BOM (레슨 50).
- [ ] **Step 2: 회귀** — jkdesktop test 0 + probe_terminal_select(마우스 보고
  OFF 상태 선택 회귀 — 이번 변경의 핵심 게이트) + probe_agent_maximize
  5/5 + probe_desktop_resize 5/5 + probe_agent_e2e 7/7 (터미널 파일 한정
  변경 — 3종+test 충분, docs에 명시).
- [ ] **Step 3: docs/41 + docs/26 갱신 + Commit** `docs(terminal): mouse report — docs/41 + roadmap update`

## Task 의존성

Task 1 → Task 2 (접근자) → Task 3 (모드 게이트+인코딩) → Task 4.