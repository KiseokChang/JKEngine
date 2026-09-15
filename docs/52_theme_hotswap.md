# 52 — P3 테마 핫스왑 as-built (theme_set 도구 + mtime 폴링)

- 날짜: 2026-09-16
- 플랜: docs/superpowers/plans/2026-09-16-batch-followups.md Part D
- 선행: docs/45(프리셋 로더) + docs/46(토큰화) + docs/47(ImGui 봉합)

## 설계 결정

### 트리거 2중 — 서버 도구 + mtime 폴링 (와이어 이벤트 배제)

- **서버 도구 `theme_set {preset}`**: 서버 프로세스 즉시 `setTheme` +
  theme.json 기록. 서버 크롬(셸 배경/런처 셀)은 페인트 시점 `current()`
  소비자라 다음 프레임부터 즉시 추종.
- **theme.json mtime 폴링 (500ms)**: 클라 전체 커버. 후보였던 와이어 이벤트
  `theme.changed`는 **agent 구독자만 커버**해 비구독 네이티브 앱(게임,
  vplayer 등)이 빠진다 — 폴링이 전포괄이므로 이벤트는 YAGNI로 뺐다. 선례:
  `JK_SCRIPT_WATCH` 500ms OnIdle 폴링.
- 클라 폴링 첫 틱은 시드(mtime 기록) + 로드 1회 — 기동 로딩과 멱등 중복
  (같은 프리셋 재로드는 같은 포인터 대입이라 무해). stderr에
  `[theme] preset '<p>' from <path>` 1행이 로더 흔적.
- `WriteThemePresetFile`은 쓰기 직후 mtime을 등록해 **자기 쓰기에 대한
  셀프 폴링 오탐**을 막는다.

### 소비 시점 3류형 → 재적용 대응표

| 류형 | 소비 지점 | 대응 |
|---|---|---|
| (a) 페인트 시점 `current()` | JKDC 기본 인자, 위젯 페인트 내 지역 참조, 셸 SDL 그리기 | 무처리 — setTheme만으로 즉시 추종 |
| (b) ctor/멤버 캡처 | `JKControl.h` textR_/backR_ 6멤버 + 위젯 ctor SetBackColor/SetTextColor | `JKControl::ApplyTheme()` 가상 + children_ 재귀 재캡처 + InvalidateRect |
| (c) 스냅샷 | ImGuiStyle 1회 `ApplyImGuiTheme` 9앱, 터미널 `view->SetTheme` 1회 | 베이스 `OnThemeChanged()` 가상 훅 → 앱별 오버라이드 |

- **fieldBg 함정**: JKEdit/JKListBox/JKComboBox의 ctor는 back에 widgetFace가
  아니라 **fieldBg**를 캡처한다 — 베이스 ApplyTheme가 widgetFace로 덮으면
  틀리므로 이 3개는 오버라이드로 자기 캡처만 재포착 후 children_ 재귀.
  JKButton/JKScrollBar/JKCheckBox는 widgetFace 캡처라 베이스 동작이 정확.
- 폴링 틱 위치: `JKClientApplication::Run` + `JKApplication::Run` (서버 모드).
  순서 = PollPresetFile → mainWindow_->ApplyTheme() → OnThemeChanged().
  기본 클래스는 imgui에 링크되지 않으므로(jkclient→jkcore만) ImGui 재적용은
  앱 오버라이드(`jk::theme::ApplyImGuiTheme()`)에서 — 9앱에 2행씩.
- 터미널: terminal.json 명시 키(themeBgSet/themeFgSet)가 이기는 P2 단계 3
  규칙 준용 — 시딩 경로만 핫스왑 추종. cfg가 OnInit 지역이라 stash 멤버
  4개(themeBgSet_/themeFgSet_/themeBg_/themeFg_)로 전달.

### 권한

`theme_set` 기본 **Allow** (외관 변경 — close_window/trust_request/
run_console_app의 ask 티어와 달리 파괴적이지 않다). jkagentd 기본 allow
(permissions.json 기본 테이블에 자동 포함 — 차단 키 아님).

## As-built

- `jk::theme::PollPresetFile()` / `WriteThemePresetFile(preset)` —
  JKThemeConfig.cpp (WIN32_FILE_ATTRIBUTE_DATA mtime, 정적 시드 -1).
- `JKControl::ApplyTheme()` 가상 — JKControl.cpp 기본(widgetText/widgetFace
  재캡처 + 재귀) + fieldBg 3종 오버라이드.
- `OnThemeChanged()` 가상 — JKClientApplication / JKApplication 보호 훅.
  9 ImGui 앱 = ApplyImGuiTheme 재적용. ClientTerminalApp/TerminalApp =
  시딩 규칙 재적용.
- 서버 도구 `theme_set` — JKWindowServer.cpp HandleAgentQuery 체인
  (run_console_app 옆). bad preset → `{"ok":false,"error":"bad_preset"}`.
- 배선: jkagentd 4곳(list/known/permissions 기본 allow/args-rebuild,
  docs/51 레슨), 팔레트 `/theme dark|light|classic` + /help 1행, jkchat
  `/theme` 동일.
- JKDesktopShell은 별도 ApplyTheme 불필요 — 페인트 시점 current() 소비자.

## 검증

- probe_theme_swap.ps1: 도구 응답 / theme.json 내용 / 클라 폴링 재로드
  stderr 라인(스폰 후 + 라이브 스왑 후) / bad_preset / dark 복원 teardown.
- 눈확인(사용자 항목): 서버 크롬 즉시, 클라 ≤500ms, 터미널/작업표시줄/런처
  색 추종, fieldBg 3종(입력창/리스트)이 위젯면 색으로 덮이지 않는지.