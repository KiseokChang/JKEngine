# docs/33 — 데스크톱 에이전트: 알림 센터 (jkapp_notify)

날짜: 2026-09-11
이전: docs/32 (M2b 트리거) · 스펙: docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md §7
계획: docs/superpowers/plans/2026-09-11-agent-notify-center.md

## 1. 요약

스펙 §7 알림 센터를 단일 ImGui 윈도우 클라이언트 `jkapp_notify.dll`로 구현했다.
단일 알림 히스토리를 소유하고, 창 내부 토스트 스트립 + 창 제목 안읽음 배지로
새 알림을 표시하며, `open_notify` 에이전트 도구와 팔레트 `/notify`로 토글한다.

MVP 한계(스펙 합의): 정규 클라이언트는 topmost가 될 수 없으므로 화면 구석
토스트는 셸 통합 트랙(4단계)으로 미룬다 — 대신 창 내부 토스트 + 작업표시줄
버튼 텍스트(제목 배지)로 동일 정보를 제공한다.

## 2. 구성 요소

### 2.1 WindowTitle C→S (MsgType 21)

클라이언트가 자기 창 제목을 갱신하는 새 프로토콜 메시지.

- `JKClientSurface::SendWindowTitle(utf8)` — 96바이트 상한, 연결 상태 확인.
- 서버 `ProcessClientMessage`의 `WindowTitle` 분기: `client.SetTitle()` +
  `PushWindowListUnsafe()` — 작업표시줄 버튼 텍스트가 즉시 따라온다.
  (docs/28에서 예고했던 SetTitle C→S 확장)

### 2.2 jkapp_notify (ImGui 클라, 팔레트/taskmgr 템플릿)

- 420×560 chromeless "Notifications" 창, 16ms 타이머 프레임 게이트.
- 자기 창 연결로 `SendAgentEventSubscribe(true)` — M2a의 관대한 푸시 덕에
  정규 클라도 이벤트를 구독할 수 있다.
- `DrainAgentEvents` 폴링 → 구독 토픽 필터 → 히스토리 적재.

### 2.3 상태 파일 (`<exeDir>\state\`)

- `notify.json` — 구독 설정. `{"topics":[{"topic":"agent.notify"},...]}`.
  배열의 객체 형태(기존 `GetArrStr` 접근자 재사용). 없으면 기본 `agent.notify`.
- `notify_history.json` — `{"entries":[{topic,title,body,ts,read},...]}`.
  최신이 마지막, 200개 상한, 추가 시 즉시 저장. ts는 epoch 초.

### 2.4 UI

- 헤더: "안읽음 N" + 모두 읽음/지우기 버튼.
- 히스토리: 최신순, 안읽음 항목 앰버 하이라이트. `[HH:MM] 제목 — 본문` 형식,
  agent.notify가 아닌 토픽은 토픽 접두어 표시.
- 토스트 스트립: 최신 항목 5초 표시, 마지막 2초 페이드 (ImGui Alpha).
- 한글: Malgun Gothic + `GetGlyphRangesKorean()` (실패 시 기본 폰트로 열화).

### 2.5 배지 + 토글

- `UpdateBadge()`: 안읽음>0이면 제목을 `Notifications (N)`으로 갱신하고
  `SendWindowTitle`로 전송. 마지막 전송 제목과 같으면 스킵(파이프 스팸 방지).
- 앱 재시작 시 `LoadHistory()` 후 `UpdateBadge()` — 안읽음 수가 파일에서 복원된다.
- 서버 `open_notify` 도구 / 팔레트 `/notify` → `ToggleClientByTitleUnsafe`.

## 3. 토글 코어 일반화

`TogglePalette`를 `ToggleClientByTitleUnsafe(title, app)` 코어로 분리했다.

- **락 규약**: `HandleAgentQuery`는 `clientsMutex_`를 이미 잡은 상태로
  호출된다(주석 명시). 코어에서 락을 잡으면 비재귀 뮤텍스라 즉사 데드락 —
  첫 실측에서 agentctl이 정확히 그렇게 멈췄다. 코어는 락 없이 동작하고
  (`SpawnClient`도 락 없음 — launch_app 선례), Alt+Space 경로인
  `TogglePalette`만 래퍼에서 lock→core→unlock→`PushWindowList()` 순서를 쓴다.
- **배지-토글 키 충돌**: 배지가 제목을 `Notifications (2)`로 바꾸면 정확
  매치 토글은 키를 잃고 두 번째 창을 스폰한다(프로브가 잡아낸 설계 결함).
  `TitleMatchesToggleKey`가 bare 제목 또는 `key (…)` 배지 형식을 모두 받는다.

## 4. 검증

`tools/probes/probe_agent_notify.ps1` — 6 체크, 전부 `list_windows` 제목과
state 파일 판정 (ImGui 앱은 WM_GETTEXT로 읽을 수 없음 — docs/32 레슨):

1. open-spawn: `open_notify` → list_windows에 "Notifications" 창.
2. publish: publish_event 2회 ok.
3. badge-2: 제목이 `Notifications (2)`로 갱신.
4. history-file: 히스토리 2개, read==0 2개.
5. toggle-focus: 두 번째 open_notify → 창 1개 유지(포커스만).
6. restart-restore: 클라 kill → 재오픈 → 배지 (2) 복원.

전체 회귀: selftest 0 실패, mcp/e2e/palette/chat/chat_llm/triggers/notify 전부 PASS.

## 5. 구현 레슨

1. **AgentQuery 핫 패스는 clientsMutex_를 이미 잡고 들어온다** — 그래서
   `PushWindowListUnsafe`/`FocusClient(caller-holds)` 같은 Unsafe 헬퍼가
   존재. 핫 패스에서 부르는 새 헬퍼는 반드시 락 없이 설계하고, 락이 필요한
   공개 경로는 래퍼로 분리.
2. **자기 제목을 바꾸는 앱을 제목으로 토글하면 안 된다** — 배지가 키를
   오염시킨다. 근본 해결은 연결에 앱 이름을 실는 프로토콜 확장(C→S hello에
   app 필드)이고, 이번 MVP는 `key (…)` 접두 매치로 타협.
3. **빌드 출력을 grep으로 필터하지 말 것** — 링크 실패(Permission denied,
   살아있는 exe 잠금)가 필터에 가려져 스테일 바이너리로 프로브를 돌렸다.
   빌드 후 exe mtime이 최신 소스보다 뒤인지 확인하는 습관.
4. **LoadHistory 이후 배지 갱신** — 복원된 안읽음 수는 OnInit에서
   UpdateBadge()를 명시 호출해야 제목에 반영된다(이벤트 도착까지 지연 안 됨).

## 6. 파일

- `include/ipc/JKWireProtocol.h` — `WindowTitle = 21`.
- `include/client/JKClientSurface.h`, `src/client/JKClientSurface.cpp` — SendWindowTitle.
- `include/server/JKClientConnection.h` — SetTitle.
- `src/server/JKWindowServer.cpp` — WindowTitle 분기, open_notify,
  TogglePalette→ToggleClientByTitleUnsafe 분리.
- `include/apps/ClientNotifyApp.h`, `src/apps/ClientNotifyApp.cpp` — 앱 본체.
- `src/apps/JKAppModule_notify.cpp` — 모듈 메타.
- `src/apps/ClientPaletteApp.cpp` — /notify.
- `tools/probes/gen_notify_icon.ps1` + `assets/icons/launcher_notify@{1x,2x}.png`.
- `CMakeLists.txt` — jkapp_notify 타깃 + notify.jkx 패키징 + 아이콘.
- `tools/probes/probe_agent_notify.ps1`.