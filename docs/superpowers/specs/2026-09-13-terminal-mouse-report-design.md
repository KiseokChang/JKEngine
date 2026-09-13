# 터미널 마우스 보고 + 고급 키 모드 + DECSCUSR

- 날짜: 2026-09-13
- 상태: 설계 확정 (사용자 승인 — "포함해줘...쭉쭉 진행", DECSCUSR 포함)
- 선행: docs/22 (터미널 기반), docs/26 단계 1 (한글 렌더 ✅), 단계 2
  (선택+클립보드 ✅ docs/40, `JKTermSelection.h` + TerminalView 선처리 구조),
  단계 5 (terminal.json + bracketed paste ✅)
- 후속 문서: 구현 완료 시 `docs/41_terminal_mouse_report.md` + docs/26 §3 단계 3 상태 갱신

## 0. 배경 / 표준

마우스 보고는 xterm(1990년대) 이래의 표준 프로토콜이며 Windows Terminal/iTerm2/
kitty/alacritty 전부 구현. htop/vim/mc 등 TUI 앱은 앱 쪽이 이미 표준을 지원하므로
우리 쪽 구현만으로 상호운용이 성립한다. 이 스펙은 표준 이식이지 새 개념이 아니다.

**ConPTY 참고**: conhost는 앱의 원 스트림을 정규화해 전달하지만, 앱이 출력한
DECSET 1000/1002/1003/1006은 터미널로 **통과**시킨다 (Windows Terminal이 같은
경로로 마우스 모드를 인지). 우리 파서는 이미 2004(bracketed paste)를 같은
방식으로 추적 중. ConPTY는 터미널→pty 방향의 SGR 마우스 시퀀스를
INPUT_RECORD(MOUSE_EVENT)로 디코딩해 TUI 앱에 전달한다 (Windows Terminal과
동일 경로 — WSL htop 포함).

## 1. 파서 — DECSET/DECSCUSR 추적 (JKVtParser)

- **DECSET 추적 추가** (`HandlePrivateMode`):
  - `1` → `appCursor_` (어플리케이션 커서 키)
  - `1000` → mouseMode_ = Normal
  - `1002` → mouseMode_ = Button (버튼 눌린 동안 모션 포함)
  - `1003` → mouseMode_ = Any (모든 모션) — 1002와 인코딩 동일, 게이트만 다름
  - `1006` → `sgrMouse_` (SGR 인코딩)
  - 리셋 규칙: DECSET 해제(h→l) 시 각 플래그 해제. 1049/alt 종료가 마우스 모드를
    건드리지 않음 (xterm 표준 — 앱이 스스로 끈다)
  - `12`(blink) 등 나머지는 기존 default 무시 경로 유지
- **DECSCUSR** (`CSI Ps SP q` — 중간 바이트 0x20, final 'q'):
  - `Ps` 0/1/2 → Block, 3/4 → Underline, 5/6 → Bar (blink 비트 무시 — v1은
    항상 steady 렌더)
  - 저장: grid의 커서 속성으로 `JKTerminalGrid::SetCursorShape/GetCursorShape`
    (enum CursorShape { Block, Underline, Bar }, 기본 Block)
  - 파서에는 새 헬퍼 하나(중간 바이트 있는 CSI 경로) — 기존 DispatchCsi 시그니처
    유지, 중간 바이트 발견 시 별도 상태/분기
- **접근자**: `MouseMode()`(enum Off/Normal/Button/Any), `SgrMouse()`,
  `AppCursorKeys()` — TerminalView가 인코딩 게이트에 사용

## 2. 인코딩 — 순수 함수 (새 헤더 JKTermInput.h)

Header-only (`JKTermSelection.h` 선례), no SDL/Windows 의존 → self-test와 뷰가 공유.

- **`EncodeMouseSgr(btn, x, y, kind, mods)`** — `\x1b[<b;x;yM/m`
  - `btn` 0/1/2 (좌/중/우), +4 Shift, +8 Meta, +16 Ctrl
  - 모션: +32 (버튼 눌린 모션). 휠: 64(상)/65(하)
  - `kind`: Press/Motion → `M`, Release → `m`. 좌표는 **1-based 뷰포트** (xterm
    표준), x/y = col+1/row+1 (255 cap — SGR은 캡 불필요하지만 표준상 유지 안 함,
    그냥 그대로)
- **`EncodeMouseX10(btn, x, y)`** — 비-SGR(1006 꺼짐) 폴백: `\x1b[M` + 32+b +
  32+x + 32+y (1-based, **223 cap**, 누름만 — 뗌/모션은 X10에서 미전송, 표준)
- **`EncodeArrow(key, mods, appCursor)`** — 화살표/Home/End/PgUp/PgDn:
  - mods 없음 + appCursor → `\x1bOA` 계열 (SS3)
  - mods 없음 + !appCursor → `\x1b[A` 계열 (CSI, 기존 동작)
  - mods 있음 → `CSI 1;<m><char>` (m = 1 + shift1 + alt2 + ctrl4, char =
    A/B/C/D/H/F — Home/End도 같은 계열; PgUp/PgDn은 mods 있으면 기존
    `\x1b[5;<m>~` 형태)
- **`EncodeWheelAlt(key, n)`** — 마우스 보고 OFF + alt 화면에서 휠 1노치 →
  위/아래 화살표 시퀀스 n개 (`n=3` 고정 — Windows Terminal 기본 노치당 3행)
- 기존 `TerminalView::HandleKeyDown`의 화살표 switch는 이 함수로 위임 (중복 제거)

## 3. 뷰 — 마우스 라우팅 (TerminalView)

RespondMessage 선처리(단일 모드 JKWindow 위임 전 / 클라 모드 HitTest 도달) 안에서:

- **게이트**: `parser_->MouseMode() != Off`면 마우스 다운/업/무브/휠을 로컬 처리
  대신 인코딩 → `onInput_`로 pty 전송. **예외: Shift 눌림** — Shift+좌클릭/드래그는
  로컬 선택 경로로 강제 (Windows Terminal 관례 — 마우스를 앱이 먹고 있어도
  복사가 가능해야 한다)
- 좌표: 기존 `CellFromPoint`와 동일 산식(뷰포트, `ViewportRowToLive` 불필요 —
  보고 좌표는 뷰포트 그대로 1-based). 클램프는 그리드 폭/높이
- 휠: 마우스 보고 ON → SGR 64/65로 전송 (스크롤백 스크롤 안 함 — 앱이 요청한
  영역이므로). OFF + alt 화면 → `EncodeWheelAlt` (vim 스크롤). OFF + 일반 화면 →
  기존 로컬 스크롤 (변경 없음)
- **선택 상호작용**: 마우스 보고 ON 구간의 이벤트는 선택 상태를 건드리지 않는다
  (ClearSelection도 안 함 — 앱이 좌표를 받고, 기존 선택은 화면에 유지). Shift
  우회로 진입한 드래그는 기존 선택 로직 그대로 (기존 선택 있으면 클릭 시 해제)
- 클라 모드: 입력 경로 동일 — `onInput_`이 IPC를 타고 서버 → pty (기존 키 입력
  경로 재사용, 신규 와이어 없음)

## 4. 키보드 — 앱 커서 키 + 수식어

- `HandleKeyDown`의 화살표/네비 계열을 `EncodeArrow`로 교체 — DECSET 1 활성
  시 vim 삽입 모드에서 `\x1bOA` 등 SS3 전송, Ctrl+Left/Right는 `CSI 1;5D/C`로
  vim 단어 이동
- ctrl+문자 기존 매핑(0x03 등) 불변. Shift 조합은 `CSI 1;2<char>`

## 5. DECSCUSR 렌더 (TerminalView)

- 커서 블록 그리는 지점에서 `grid_->GetCursorShape()`에 따라: Block = 기존,
  Underline = 셀 하단 2px, Bar = 셀 좌측 2px. 색상/반전 로직은 기존 커서와 동일
- blink 미지원 (steady만) — docs에 제한 명시

## 6. 테스트

- **self-test**: `JKTermInput.h` 순수 함수 — SGR press/release/motion/wheel/
  수식어 비트, X10 폴백(223 캡, 뗌 미전송), 화살표 3형태(CSI/SS3/수식어),
  휠→화살표 n개. 파서 — DECSET 1000/1002/1006/1 set/reset, DECSCUSR 5종
  (q 시퀀스 → shape enum), alt 종료가 마우스 모드를 해제하지 않음
- **프로브 `probe_terminal_mouse.ps1`**: `jkdesktop.exe terminal` 단일 모드 기동
  → 임시 .ps1 스크립트 작성(`[Console]::Out.Write("`e[?1000;1006h")` 후 stdin
  바이트를 16진 텍스트로 화면 출력하는 루프) → 터미널에 실행 명령 타이핑 →
  SendInput으로 알려진 셀 클릭 → 화면 리드백에 SGR 바이트(`1b 3c ...` 형태)
  도착 assert → 종료 시 마우스 모드 해제 시퀀스 출력 후 정리(PID 기반
  Stop-Process, probe_terminal_select 관례). htop/vim은 사용자 실측 항목.
- **회귀**: jkdesktop test 0 + probe_terminal_select(선택이 Shift 없이 그대로
  동작 — 마우스 보고 OFF 상태) + probe_agent_maximize/desktop_resize/e2e.
  이번 변경은 터미널 파일 한정이라 3종+test 충분 (docs에 명시)

## 7. 제한 (v1)

- blink 미구현 — DECSCUSR 모양만 (항상 steady)
- X10 폴백은 누름만 (표준) — 모션/뗌은 SGR 모드에서만
- 휠→화살표는 노치당 3행 고정 (설정화 후속)
- 마우스 보고 중 무브 이벤트는 Button 모드에선 버튼 눌린 동안만 (표준)
- 서버/클라 모드 공통: 포커스 없는 뷰로의 마우스 이벤트는 기존 라우팅 따름
- WSL htop/vim 실측은 사용자 항목 (프로브는 ConPTY 레벨에서 검증)