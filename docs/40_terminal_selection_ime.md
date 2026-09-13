# 40. 터미널 텍스트 선택 + 클립보드 + IME 조합 표시

- 날짜: 2026-09-13
- 상태: 구현 완료. 스펙 `docs/superpowers/specs/2026-09-13-terminal-selection-ime-design.md`
- 선행: docs/22 (터미널 기반), docs/26 단계 1 (전각/한글 렌더), 단계 5
  (terminal.json, JKVtParser bracketed paste 2004 플래그 추적)

docs/26 단계 2(선택+클립보드)와 단계 5 IME 조합 인라인의 as-built 문서.
작업대는 TerminalView 하나로 클라 모드(ClientTerminalApp — 뷰 = DOCK_FILL 자식,
마우스는 HitTest→RespondMessage)와 단일 모드(TerminalApp — 뷰 = 메인 윈도우,
클라 영역 마우스는 JKWindow::RespondMessage가 드롭하므로 오버라이드에서 JKWindow
위임 전에 선처리) 양쪽을 커버한다.

## 1. 선택 모델/렌더

- **상태**: `selAnchor_` + `selEnd_` (JKPoint, 그리드 셀 좌표, **-1 = 선택 없음**),
  `selDragging_` (드래그 중 — 캡처 유지용). 박스 선택은 행 단위 rect가 전부인
  모델(ConPTY 재생 타깃 그리드, docs/22 §5.2 — 리플로우 없음을 문서로 명시).
- **셀 환산**: `CellFromPoint` — ev.x/y에서 `GetScreenClientRect()` 원점을 빼고
  kTermCellW/H로 나눠 클램프. 뷰포트 행은 `ViewportRowToLive(r, offset, rows)`로
  라이브 그리드 행으로 매핑(§4).
- **라이프사이클**: 클라 영역 좌클릭 MouseDown → 기존 선택 해제 + anchor=end=셀 +
  `SetCapture`(창 밖 드래그 유지 — JKEdit.cpp:308 선례); MouseMove → end 갱신 +
  `MarkAllDirty()`; MouseUp → 드래그 플래그 해제, **anchor==end면 해제**(클릭은
  선택 아님, 스펙 §1). 영역 밖 이벤트는 드래그 중일 때만 소비(좌표는 엣지 셀로
  클램프 — 끌고 나간 극단 유지).
- **마우스 선처리**: 단일 모드에서 뷰가 메인 윈도우라 JKWindow::RespondMessage가
  클라 영역 마우스를 드롭(`target != this` 가드, JKWindow.cpp:478)하므로
  `TerminalView::RespondMessage`가 **JKWindow 위임 전에 HandleMouseEvent 실행**.
  클라 모드에서도 같은 창/표면 좌표계라 한 핸들러가 양쪽을 커버.
- **렌더**: PaintCell에서 선택 셀은 **fg/bg 스왑**만 (reverse와 동일 시각) — 새
  그리기 원시 없음. 선택된 빈 셀은 배경이 themeFg(0xCCCCCC)가 되어 하이라이트로
  보인다. 단, 셸이 ANSI 색을 준 셀(PSReadLine 에러 행의 red bg 등)은 그 색이
  우선 — 시각 진단 시 기본 색 셀만 샘플할 것.

## 2. 복사/붙여넣기 + bracketed paste

- **복사 (Ctrl+Shift+C)**: `CopySelection` — NormalizeSel → `ExtractSelectedText`
  (라이브 그리드 접근자) → `SDL_SetClipboardText` (JKEdit.cpp:716 선례). 선택
  없으면 무동작. 복사 후에도 선택 유지.
- **행 추출 규칙**: 행별 **마지막 non-empty 셀까지만**, 빈 행은 건너뛰지 않고
  `\n` 연결(Windows Terminal 관례). **내부 빈 셀은 공백으로 복사** — 그리드는
  지워진/안 쓰인 셀을 cp==0로 저장하는데, ConPTY가 커서 위치 재지정 업데이트로
  프롬프트 행 중간에 구멍을 남기면 cp==0가 그대로 raw NUL이 되고 **std::string
  속 NUL이 SDL_SetClipboardText를 그 바이트에서 끊는다**(실측: 119바이트 선택이
  클립보드에선 첫 37바이트로 도착 — 프로브에서 간헐 실패의 원인, §5). wide-cell
  follower(width==0, cp==0)는 여전히 무출력 — wide 글리프가 행의 마지막 non-empty.
- **붙여넣기 (Ctrl+Shift+V)**: `SDL_GetClipboardText` → `SanitizeClipboardPaste`
  (\r\n/\r→\n, ESC 바이트 제거) → **bracketed paste 게이트**: JKVtParser가 이미
  추적 중인 DECSET 2004(`parser_->BracketedPaste()`)가 on이면
  `\x1b[200~` … `\x1b[201~`로 감싸 onInput_. 붙여넣으면 선택 해제 + preEdit 클리어
  + `scrollOffset_=0`(라이브 뷰 복귀).
- **키 순서 (HandleKeyDown)**: 클립보드 코드(ctrl+shift+C/V)를 ctrl+문자 경로보다
  **먼저** 검사 — ctrl+C **without shift는 여전히 0x03(SIGINT)**. 어떤 키든 선택을
  해제하되 **클립보드 코드와 bare modifier는 예외** — chord의 C/V보다 먼저 도착하는
  Ctrl/Shift KeyDown에서 선택을 지우면 CopySelection이 읽을 게 없어지므로 배제가
  필수다. alt는 ESC 프리픽스, ctrl+문자는 0x1F 마스크(기존 docs/22 §6.1 매핑).

## 3. IME 오버레이

- **이벤트**: `JKEventType::TextEditing` — 파이프라인 기존 경로(서버 JKWindowServer
  → 클라 JKClientSurface, 단일 모드 main.cpp)를 RespondMessage에서 소비해
  `preEdit_` 저장 + `MarkAllDirty()`.
- **렌더**: 커서 셀부터 preEdit_를 `DecodeUtf8` 코드포인트 배열로 셀 단위 오버레이
  — 배경을 themeFg 저휘도 톤으로 칠하고 글리프를 그 위에(JKEdit.cpp:417 조합 표시
  선례). 전각은 2셀(`JKTermCharWidth`), 그리드 폭 초과는 잘라냄.
- **클리어 시점**: Char(커밋 교체)·아무 KeyDown·붙여넣기·선택 시작. 순서는 SDL이
  KeyDown 먼저 TextEditing 나중 — 조합이 같은 프레임에 다시 그려진다. 커밋 직전
  1프레임 겹침은 ConPTY 재출력으로 해소(스펙 §3 허용).
- 참고: 단일 모드 SDL 창은 IME 컨텍스트가 붙어 있다(서버만 DetachIme,
  JKWindowServer.cpp:189 — 서버는 원시 키 전달자라 조합이 필요 없다). 사용자 IME
  조합은 TextEditing/Char로 이 뷰에 도착한다.

## 4. 순수 함수 (include/apps/JKTermSelection.h)

헤더 온리(SDL/Windows 의존 0) — TerminalView(양쪽 모드 공유)와 exe self-test가
하나의 구현을 쓴다. 뷰 코드는 좌표/이벤트만 담당하고 텍스트 로직은 전부 여기.

- **`NormalizeSel(ax, ay, bx, by, cols, rows)`** — anchor/end를 clamp + min/max로
  (x0,y0)-(x1,y1) 포함 rect로 정규화. 음수/범위 밖 드래그(창 밖으로 나간 드래그)는
  가장자리 셀로 스냅. cols/rows <= 0이면 빈 rect.
- **`ViewportRowToLive(r, off, rows)`** — 스크롤백 상태에서 뷰포트 행 → 라이브 그리드
  행 (라이브 최상단 = history - offset). r < off(스냅샷 위 행)는 라이브 0으로
  클램프 — **v1은 라이브 행만 선택**. CellFromPoint와 self-test가 같은 산식을 쓰는
  단일 진실원.
- **`AppendUtf8` / `DecodeUtf8`** — 표준 1-4바이트 UTF-8 인/디코더 쌍(BCP 없음,
  BOM/서러게이트 처리 없음). DecodeUtf8은 잘못된 리드/과소 길이/서러게이트를
  **바이트당 U+FFFD 1개**로 디코딩해 호출자의 셀 페이싱이 입력 길이와 동기화되고,
  U+10FFFF 초과(F4 90 80 80 등)도 거절한다. IME 오버레이의 preEdit_ 분해와
  self-test가 공유.
- **`ExtractSelectedText(cellAt, sel)`** — 템플릿 접근자 `cellAt(col,row) → const
  JKTermCell&`로 뷰의 라이브 그리드와 self-test의 더미 벡터를 하나의 구현으로.
  §2의 행 규칙(마지막 non-empty까지만, 빈 행 포함, 내부 구멍은 공백) 구현.
- **`SanitizeClipboardPaste(raw, bracketed)`** — 개행 정리 + ESC 제거 +
  bracketed 래핑 게이트.

## 5. 테스트

- **self-test (`jkdesktop.exe test`, 25체크, 0 failure(s))** — normalize 순서/클램프/
  빈 그리드, viewport 매핑 3종, 행 잘림, AC00 재인코딩(3바이트 UTF-8), 행 `\n` 결합,
  follower-only 빈 행, 빈 선택, **내부 구멍 → 공백**(NUL 잘림 회귀 — 아래 프로브 교훈),
  paste \r\n/\r/ESC 정리, bracketed 래핑, 미브래킷, DecodeUtf8 ascii/한글/왕복/
  FFFD 4종/>U+10FFFF+resync.
- **프로브 `engine/tools/probes/probe_terminal_select.ps1` (5체크, exit 1 on FAIL)**
  — 단일 모드 `jkdesktop.exe terminal` 기동(FindWindow("SDL_app","Terminal") —
  서버 관례와 달리 뷰가 자체 SDL 창, 제목은 TerminalApp::Init("Terminal",800,500)):
  (1) 스폰+창 표시(TOPMOST 핀), (2) 포커스 확인 후 `cls`+`echo hello` 타이핑 →
  **화면 리드백**(전체 드래그+복사가 곧 검증기 — split-line index == 그리드 행)으로
  독립 "hello" 행 탐색, (3) 그 행 드래그 선택 → Ctrl+Shift+C → `Get-Clipboard` ==
  "hello" (**하드 검증**) + 보너스 스크린샷, (4) `echo ` 타이핑 후 Ctrl+Shift+V →
  커맨드 라인 `echo hello` 2회, (4b) Enter로 재실행 → 독립 "hello" 2회 + 보너스
  스크린샷, (5) PID/커맨드라인 정리.
- **프로브 SendInput 교훈 (docs/39 이후 신규)**:
  - **합성 VK 입력에 스캔 코드 필수** — SDL 2.32는 lParam 스캔 코드를
    WindowsScanCodeToSDLScanCode로 매핑하고 SDL_SCANCODE_UNKNOWN이면 키다운을
    드롭한다(wParam 폴백 테이블은 화살표+VK_CONTROL+VK_V뿐 — VK_SHIFT/문자 제외).
    `MapVirtualKey(vk, MAPVK_VK_TO_VSC)`를 모든 VK 이벤트에.
  - **Enter는 VK_RETURN 경로만** — SDL_SendKeyboardText가 `< ' '` 텍스트를
    드롭하므로 KEYEVENTF_UNICODE '\r'는 조용히 사라진다. (덕분에 chord의 컨트롤
    WM_CHAR 0x03/0x16이 pty로 새는 경로도 없다.)
  - **ctrl+shift+C는 이 리그의 RegisterHotKey 소유자가 시스템 전역에서 삼킨다** —
    앱 계측 증명: ctrl/shift 키다운은 도착, C 키다운은 포커스 윈도우 도달 전에
    입력 스트림에서 제거(ctrl+shift+V는 통과). 프로브는 **modifier만 실
    SendInput으로 내리고 chord 문자는 WM_KEYDOWN을 터미널 창에 직접 PostMessage**
    (핫키 필터 우회 — 실제 키보드 리포트와 동일한 스캔 코드 lParam 필수). 게시된
    modifier는 신뢰 없음(SDL이 실제 키보드 상태로 재동기화) — 문자 키만 게시.
  - **간헐 "클립보드가 첫 37바이트만"** — 앱 계측으로 CopySelection rect/len과
    SDL_SetClipboardText rc=0을 확인하고 raw CF_UNICODETEXT GlobalSize를 비교해
    데이터 자체가 잘린 것을 특정: 선택 텍스트 속 **내부 cp==0 셀이 NUL 바이트로
    직렬화**되어 SDL_SetClipboardText가 그 지점에서 끊었다. ConPTY가 프롬프트 행에
    구멍을 남기는지가 매번 달라 간헐로 보였다. 앱 버그 — §2의 내부 공백 규칙으로
    수정, self-test 추가. (Get-Clipboard/클립보드 감시자/핫키 루트 원인 아님.)
  - 셸이 ANSI 색을 준 셀은 선택 하이라이트가 안 보인다 — 픽셀 밝기 진단은 기본색
    셀에서만 (§1).
- **회귀**: `jkdesktop.exe test` 0 failure(s) + probe_agent_maximize 5/5,
  probe_desktop_resize 5/5, probe_agent_e2e 7/7 — 전부 exit 0. **13종 전수가
  아닌 이유**: 이번 변경은 터미널 파일만 손댄다(JKTermSelection.h, TerminalView,
  main.cpp self-test + 프로브/문서) — 서버/에이전트/챗/shot 등은 터미널 소스와
  무관하므로 3종 프로브 + self-test로 충분(docs/39 전수 선례는 서버 크롬 변경).

## 6. 제한

- **스크롤백 선택**: v1은 라이브 화면만 — 스크롤 중 드래그는 라이브 행으로
  클램프된다(스냅샷 위 행은 라이브 0, 스펙 §5).
- **포커스 상실 선택 클리어 미구현** — WM_KILLFOCUS 대응 이벤트가 파이프라인에
  없다. 창 전환 시 선택이 남는다(재클릭/키 입력으로 해제).
- **ev.text 64바이트 상한** — TranslateSDLEvent의 strncpy(JKEvent.cpp:51): 서버/
  단일 모드 공통. IME 문장 커밋이 한 번에 64바이트를 넘으면 잘려 들어온다(커밋은
  보통 짧게 쪼개진다).
- **워드 더블클릭/쉬프트+클릭 확장 후속** (스펙 §1) — 박스 선택만.
- **멀티라인 붙여넣기는 `
`** (스펙 §2) — 키보드 Enter 경로는 ``을 보내므로
  cmd.exe 같은 셸에서는 붙여넣은 줄이 자동 실행되지 않을 수 있다. 비 bracketed
  대상에 `
→` 변환은 후속.
- 블록 선택은 행 단위 rect(리플로우 모델 없음, §1).
- 클라 모드 클립보드는 OS 클립보드 직행 — 서버를 경유하지 않는다(스펙 §5).

- 커밋: 84e031a (선택+클립보드+bracketed paste 코어), 3d5a1ea (리뷰 픽스 — 스크롤
  매핑/캡처 누수/paste 선택 해제), c84a8bf (ViewportRowToLive 순수 함수 추출),
  3410a3b (IME pre-edit 오버레이), 2cca7f8 (DecodeUtf8 >U+10FFFF), 본 프로브 +
  내부 구멍 공백 규칙 수정, 본 문서 + docs/26 갱신.
