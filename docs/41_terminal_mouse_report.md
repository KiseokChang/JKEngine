# 41. 터미널 마우스 보고 + 고급 키 모드 + DECSCUSR

- 날짜: 2026-09-13
- 상태: 구현 완료. 스펙 `docs/superpowers/specs/` (docs/26 단계 3 설계)
- 선행: docs/22 (터미널 기반), docs/26 단계 1 (전각/한글), 단계 2 (선택+클립보드,
  docs/40), docs/40 (선택/클립보드/IME)

docs/26 단계 3 "마우스 보고 + 고급 키 모드 + DECSCUSR"의 as-built 문서. TUI 앱
(htop, vim 등)이 `\x1b[?1000;1006h`를 출력하면 터미널이 마우스 이벤트를 xterm
리포트로 인코딩해 앱 stdin으로 보내고, 화살표 키가 DECSET 1(어플리케이션 커서
키)에 따라 SS3/CSI로 전환되며, 커서 모양이 DECSCUSR로 바뀐다. 작업대는
JKVtParser(모드 추적) + `include/apps/JKTermInput.h`(순수 인코더) +
TerminalView(라우팅) 3층.

## 1. 파서 모드 추적 (JKVtParser)

- **DECSET 1000/1002/1003** → `TermMouseMode{Off, Normal, Button, Any}` 접근자
  `MouseMode()`. 1002/1003은 1000 인코딩을 공유(모션 게이트만 다름). 적용은
  **시퀀스 내 파라미터 순서대로, 마지막이 이긴다**(last-param-wins — 합동
  `\x1b[?1000;1006h`든 `\x1b[?1003h` 후 `\x1b[?1000h`든 이후 값이 덮는다),
  `l`은 Off로.
- **DECSET 1006** → `SgrMouse()` — 인코딩 선택(SGR vs X10) 플래그, 모드 비트와
  직교.
- **DECSET 1** → `AppCursorKeys()` — 화살표 SS3/CSI 전환.
- **합동 DECSET (이번 프로브가 잡은 회귀)**: 앱은 `\x1b[?1000;1006h`처럼 한 CSI에
  여러 모드를 묶어 보내는 것이 일반적인데, private-mode 핸들러가 `p[0]`만
  적용해서 **1006이 조용히 드랍**됐다(뷰가 계속 클래식 X10로 발송). 이제 모든
  파라미터를 순회 적용하고, self-test에 합동 `?1000;1006h`/`?1002;1006h`/
  `?1000;1006l` 체크를 추가. (프로브 4회차까지 X10으로 잡힌 원인 — §6.)
- **DECSET 1049(alt 화면)**는 마우스 모드를 건드리지 않는다 — xterm 표준: 마우스
  보고 on/off는 앱이 자기 출력으로 한다. DECSET 12(깜빡임 토글)와 1004(포커스)는
  미추적.
- **DECSCUSR `CSI Ps SP q`** → 그리드 `CursorShape`: 0/1/2 → Block, 3/4 →
  Underline, 5/6 → Bar. 깜빡임 변형(ps 홀수)은 모양으로 접어 steady 렌더.
  0은 리셋(셸이 실제로 보내는 값). SP 중간바이트 없는 `CSI 3q`는 무시.

## 2. 와이어 포맷 (include/apps/JKTermInput.h)

헤더 온리(SDL/Windows 의존 0) — TerminalView(단일/클라 모드 공유)와 self-test가
하나의 구현을 쓴다(docs/40 §4 선례). 게이팅은 뷰/파서, 문자열 조립만 여기.

### SGR 모드 (DECSET 1006 on): `\x1b[<b;x;yM|m`

| 이벤트 | b (버튼+수식) | 종결자 |
|---|---|---|
| press | 버튼 0/1/2 (좌/중/우) | `M` |
| release | 버튼 0/1/2 | `m` (소문자 — 유일한 release 표기) |
| motion | 버튼+32 (버튼 없는 hover = 3+32) | `M` |
| 휠 | 64 (up) / 65 (down) | `M` |
| 수식 비트 | +4 Shift, +8 Meta(Alt), +16 Ctrl (버튼 바이트에 OR) | |

- 좌표는 **1-based 뷰포트 셀**, 스케일링/캡 없음.
- SDL 버튼 1/2/3 → 와이어 0/1/2. 사이드 버튼(4/5)은 미보고.
- SDL `KMOD_*` → 와이어 비트 변환은 뷰가 호출부에서 수행(헤더 SDL-free 유지).

### 클래식 모드 (1006 off): `\x1b[M` + 3바이트

- `(32+Cb)` `(32+x)` `(32+y)` — xterm NORMAL(1000)/BUTTON(1002) 추적의 클래식
  인코딩: **press = Cb 버튼(0/1/2), release = Cb 3**, motion = Cb 버튼+32.
  클래식 프로토콜은 어떤 버튼이 떨어졌는지 구별할 수 없으므로 release는 항상
  Cb=3 하나(xterm 사양). 좌표는 1-based, **1..223 클램프**(32 오프셋이 부호
  바이트 안에 머물도록). press 전용은 **X10 호환 모드(DECSET 9)**의 특성으로,
  우리는 구현하지 않는 모드다.

### 키보드 (spec §4): `EncodeArrow(NavKey, mods, appCursor)`

- mods==0 + appCursor → SS3 `\x1bOA..D`, Home/End `\x1bOH/OF`; mods==0 +
  !appCursor → CSI `\x1b[A..` / `\x1b[H/F`; PgUp/PgDn는 무수식 시 기존
  `\x1b[5~`/`\x1b[6~` 그대로.
- mods!=0 → `\x1b[1;<m><char>` (m = 1 + 1·Shift + 2·Alt + 4·Ctrl — 마우스 비트
  와 다른 CSI `<m>` 계열). PgUp/PgDn는 `\x1b[5;<m>~`/`\x1b[6;<m>~`.

### 휠 → 화살표: `EncodeWheelAlt(up, n)`

마우스 보고 OFF + alt 화면에서 노치당 n개(=3) 화살표 — vim/less 스크롤(Windows
Terminal 관례).

## 3. 라우팅 (TerminalView::HandleMouseEvent / HandleMouseReport)

- **게이트**: MouseDown/Up/Move가 클라 영역 안이고, 선택 드래그 중이 아니며,
  **Shift 미유지**이고, onInput_ + 파서가 있으며 `MouseMode() != Off`면
  `HandleMouseReport`로 보내고 **소비** — 선택 상태는 건드리지 않는다(캡처도
  안 건다). Shift 우회는 Windows Terminal 관례(앱이 마우스를 소유 중에도 복사
  가능), 진행 중인 드래그는 MouseUp까지 로컬 선택 경로를 유지.
- **좌표**: `CellFromPoint` (클램프 포함) + 1 — 리포트는 뷰포트 좌표 그대로,
  스크롤백 리매핑 없음.
- **수식자**: `ev.option`(SDL_Keymod) → Shift=4/Meta=8/Ctrl=16. 이벤트 주입자 두
  곳 모두 스탬프한다 — 단일 모드 `TranslateSDLEvent`(SDL_GetModState,
  JKEvent.cpp), 클라 모드 JKWindowServer InputEventPayload (83a10d2).
- **버튼 비트**: `reportedButtons_`에 현재 눌린 리포트 버튼 비트 유지 — 1002 모드
  모션 게이트(버튼 없으면 무시)와 motion 바이트의 최저 버튼 결정에 사용.
- **모션**: 1000(Normal)은 모션 자체 없음(xterm 표준). 1002는 버튼 홀드 중만,
  1003은 hover(버튼 3) 포함 전부 — SGR에서만 발송. **클래식 모션(Cb 버튼+32)은
  와이어에 존재하나 v1 미보고**(§7 명시 갭 — non-SGR 휠과 같은 사유).
- **릴리즈**: SGR은 `\e[<b;x;ym`, 클래식은 Cb=3 (`\e[M` + 35) — 어떤 버튼이든
  동일(xterm 사양). non-SGR에서도 릴리즈를 보내므로 1000 without 1006 앱이
  클릭에 갇히지 않는다.

## 4. 휠 (TerminalView::HandleWheel)

- **보고 ON**: 로컬 스크롤백 스크롤 없이 앱에 귀속. SGR 모드 + 마지막 마우스
  위치가 클라 안이면 **버튼 64(up)/65(down) press 리포트**를 노치당 1회(상한 3,
  EncodeWheelAlt 상한과 동일) — 버튼 이벤트와 동일한 와이어 수식 비트
  (Shift=4/Meta=8/Ctrl=16, `MouseMod` enum)를 포함한다. 휠 이벤트는 좌표가
  없으므로 직전 MouseMove가 기록한 `lastMouse_` 셀로 보고하며, 타이틀바 위 휠은
  클램프 (1,1) 리포트를 만들지 않게 게이트. **non-SGR 휠은 미보고** — 실제
  xterm은 클래식 인코딩(버튼 64/65)으로도 보내므로 v1 격차로 명시(§7).
- **보고 OFF + alt 화면**: 휠 → 화살표 3개/노치(vim 스크롤).
- **보고 OFF + 메인 화면**: 기존 로컬 스크롤백 3행/노치.
- 참고: 83a10d2까지 단일 모드 SDL_MOUSEWHEEL 이벤트가 파이프라인에서 드랍됐다
  (TerminalApp/PcxApp 휠 핸들러가 안 불림) — TranslateSDLEvent에 케이스 추가로
  해결됐고 이 휠 리포트 경로가 그 위에 서 있다.

## 5. 렌더: 커서 모양

- Block: 기존 외곽 2중 rect. Underline: 셀 하단 2px 바. Bar: 좌측 2px 세로바.
  색은 Block과 동일. 깜빡임은 모든 모양에 적용(주기 530ms 기존 유지, DECSET 12
  미추적이라 블링크 설정 자체는 불가).

## 6. 프로브 + ConPTY 실측

- **프로브 `engine/tools/probes/probe_terminal_mouse.ps1`** — 사용법:
  `powershell -ExecutionPolicy Bypass -File engine\tools\probes\probe_terminal_mouse.ps1`
  (exit 0 = PASS). 단일 모드 기동(probe_terminal_select 관례: FindWindow,
  TOPMOST 핀, VIRTUALDESK SendInput, PID 정리) 후 터미널 셸 안에서 리더 스크립트를
  전면 실행시킨다: 콘솔 입력 모드를 raw + `ENABLE_VIRTUAL_TERMINAL_INPUT` +
  `ENABLE_MOUSE_INPUT` + **QuickEdit OFF**로 set, `\x1b[?1000;1006h` 출력, stdin을
  1바이트씩 읽어 `$env:TEMP\jkterm_mouse_dump.txt`에 hex 2자로 적체(READY 마커
  먼저). 데드라인 없음 — 타임아웃 제어자는 프로브(기동 엔진을 PID로 kill). detach
  리더는 PSReadLine과 입력 큐를 다투므로 배제.
- **검증 (하드 게이트 3종)**: 알려진 셀 (30,10) SendInput 클릭 → 덤프에 SGR press
  `1b5b3c303b..4d` 도착 + **좌표 디코드 일치**(x=31, y=11 == 셀+1, 인코딩 자체를
  증명), SGR 휠 `1b5b3c36343b` 존재(휠은 SGR 모드에서만 발송 — 1006 도달 증명,
  X10 관측은 게이트 불충족 진단 전용), hover 이동 후 **SGR 모션 부재**
  (`\e[<32;`..`\e[<62;` 범위 부재 — 모션 바이트는 btn+mods+32 = 32..62,
  1000은 모션을 보고하지 않음). 보조로 release `m`(6d).
- **ConPTY 실측 (Win11 26200 conhost)**:
  - 클라(리더)가 stdout에 쓴 DECSET은 conhost가 터미널로 전달한다 — 파서가
    1000/1006을 모두 봤다.
  - **raw+VT-input 리더는 우리 SGR 바이트를 그대로 받는다** — MOUSE_EVENT
    INPUT_RECORD로의 변환 없음(microsoft/terminal PR #4856 계열 입력
    패스스루). 클래식 콘솔 앱(ENABLE_MOUSE_INPUT만)은 MOUSE_EVENT 레코드를
    받는 PR #9970 계약과 공존하는 두 경로.
  - **QuickEdit이 켜 있으면 conhost가 마우스를 아예 억제**한다(PR #9970:
    `SetConsoleMode(ENABLE_MOUSE_INPUT)` + QuickEdit OFF일 때만 conhost가
    `?1003;1006h`를 터미널로 전송). 리더는 QuickEdit를 끈다 — 실제 TUI 앱이
    하는 것과 동일.
- **프로브 이터레이션**: 1차 — 덤프에 클래식 X10(`\e[M` + 32+btn + 32+x + 32+y,
  좌표는 정확)만 기록. 원인 2개 겹침: (a) 리더의 QuickEdit ON → conhost 마우스
  억제, (b) **엔진 회귀** — 파서가 합동 DECSET의 첫 파라미터만 적용(§1, 커밋
  923847a로 수정). 4차(QuickEdit OFF)에도 X10 → 5차에서 DECSET을
  `\e[?1000h\e[?1006h`로 분리하자 SGR press/release가 그대로 도착해 파서 버그
  특정 → 수정 후 6차에서 합동 `\e[?1000;1006h`로 전 SGR 통과.

## 7. 제한

- **DECSCUSR 깜빡임 변형 무시** — ps 3/5(blink)도 steady 모양으로 렌더. DECSET
  12(깜빡임 토글) 미추적. RIS(ESC c)는 DECSCUSR를 Block으로 리셋 — Resize/alt
  스왑은 보존.
- **클래식 모션 미보고 (v1 갭)** — 클래식 인코딩은 Cb=버튼+32 모션 형태가
  와이어에 존재하지만(1002 버튼 홀드 모션, 1003 hover 포함) 우리는 SGR에서만
  모션을 보낸다. non-SGR 휠 미보고와 같은 사유 — 스펙 §3 letter가 SGR 전용으로
  명시한 것을 따른다. 클래식 press/release는 보고한다(release = Cb 3).
- **non-SGR 휠 미보고** — 실제 xterm은 non-SGR normal 추적에서도 휠을 64/65
  클래식 인코딩으로 보내지만 v1은 SGR에서만(스펙 §3 letter 명시 갭).
- **리포트 경로에 SetCapture 없음** — 마우스가 창 밖에서 업되면 MouseUp 이벤트가
  뷰에 도달하지 않아 `reportedButtons_`에 스테일 비트가 남는다(1002 모션 게이트가
  계속 통과). 다음 클릭의 down/up으로 정상화. 선택 경로는 기존 캡처 유지.
- 사이드 버튼(4/5) 미보고, 포커스 리포팅(1004) 미추적. DECSET 9(X10 호환,
  press 전용) 미구현 — 1000/1002/1003은 클래식 Cb 인코딩을 쓴다.

## 8. 테스트

- **self-test (`jkdesktop.exe test`, 253체크, 0 failure(s))** — 247(244 + 합동
  DECSET 3) + 최종 리뷰 6체크: 클래식 press Cb=버튼 / **클래식 release Cb=3** /
  클래식 모션 미보고(빈 문자열), 휠 수식 비트(shift+up b=68, ctrl+down b=81),
  RIS가 DECSCUSR를 Block으로 리셋(Resize/alt 스왑은 보존). 커버리지: DECSET
  1000/1002/1003 set/reset/다운그레이드, 1006 직교성, 합동 파라미터, DECSET 1,
  alt 스왑 무영향, DECSCUSR 0-6 매핑 + 0 리셋 + alt/리사이즈/RIS, `EncodeMouseSgr`
  press/release/motion/휠/수식 비트, `EncodeMouseX10` press/release/223 클램프/
  모션 갭, `EncodeArrow` CSI/SS3/수식형, `EncodeWheelAlt`.
- **프로브 회귀 (exit 0 전부)**:
  - `probe_terminal_mouse.ps1` — PASS (press 게이트 + release + SGR 휠).
  - `probe_terminal_select.ps1` — PASS 5체크. **이번 변경의 핵심 게이트**:
    마우스 보고 OFF 상태에서 선택/복사가 종전대로 동작함을 증명.
  - `probe_agent_maximize.ps1` 5/5, `probe_desktop_resize.ps1` 5/5,
    `probe_agent_e2e.ps1` 7/7 — 전부 PASS.
- **회귀 스코프 근거**: 변경은 터미널 뷰/파서 파일(JKVtParser, JKTermInput.h,
  TerminalView, main.cpp self-test)과 JKEvent/JKWindowServer의 마우스 `ev.option`
  스탬핑(옵션 필드 사용 — 키보드 경로 불변)뿐이다. 터미널 소비자(선택 프로브) +
  이벤트 옵션 필드를 같이 쓰는 에이전트 채널 3종(maximize/resize/e2e)이 모든
  손댄 소비자를 커버 — 13종 전수 불필요(docs/40과 동일 판단).

- 커밋: 923847a (합동 DECSET 파서 픽스 + self-test 3체크), 147afa5
  (probe_terminal_mouse), acb1fb9 (프로브 SGR 하드 게이트 — 리뷰 MAJOR-1),
  07e2da8 (docs/26 수식 비트 교정), 최종 리뷰 커밋들 (클래식 release/휠 수식/
  RIS 리셋 + 프로브 모션 게이트), 본 문서 + docs/26 갱신.
## 9. 사용자 버그 보고 검증 (2026-09-13, 문서 이후)

**보고**: WSL htop에서 마우스 무반응("글자 선택 하이라이트"가 대신 그려짐).
**결론: 코드 결함 아님 — 실행 중이던 프로세스가 docs/41 이전 구식 바이너리를
메모리에 올리고 있었던 것.** 재시작만으로 해소.

증거 체인 (tmp/diag_v6*.ps1, 계측은 임시로 넣었다가 제거 — 커밋 본 문서):

1. **구식 프로세스 재현**: 구 서버가 띄운 클라이언트 터미널에서 htop 드래그 →
   회색 로컬 선택 밴드(마우스 모드 OFF의 특징적 증상), htop 무반응. hex-dump
   리더의 `\e[?1000;1006h`조차 모드를 못 켬.
2. **현재 빌드 클라이언트 모드 전 구간 PASS** (계측: JKTERM_MOUSE_DBG=1 →
   파서 DECSET 수신 로그 + 뷰 게이트 상태 로그): 리더 DECSET 수신 → 뷰 게이트
   `mode=1 sgr=1` 전환 → 클릭/드래그/휠이 SGR로 인코딩되어 ConPTY stdin 도달
   (`ESC[<0;21;11M` 등 hex 덤프로 확인). 드래그 중 선택 밴드 없음(모드 ON).
3. **사용자 시나리오 재현**: 새 클라이언트에서 `wsl htop` → 클릭 시 htop의
   시안색 선택 바가 PID 1 → 13으로 이동. 클라이언트 모드에서도 end-to-end 정상.

교훈: 기능 커밋 후 사용자 환경의 **장기 실행 프로세스 재시작**을 확인할 것 —
DLL이 메모리에 로드된 채로 있으면 최신 빌드 검증이 안 된다.
