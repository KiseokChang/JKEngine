# jkdesktop Desktop Agent Platform 설계

- 날짜: 2026-09-09
- 상태: 스펙 (구현 계획 대기)
- 범위: 1단계 = Desktop Agent API + 브로커(jkagentd) + MCP 서버. 2단계 이후는 로드맵.
- 시나리오 카탈로그(브레인스토밍 전체 지도): `2026-09-09-desktop-agent-scenarios.md`

## 1. 배경과 동기

jkdesktop은 자체 윈도우 서버(SDL2) 위에 앱(DLL/.jkx 패키지), 태스크바, ImGui, OSR
브라우저, ConPTY 터미널, vplayer, QuickJS 스크립팅 브리지를 갖춘 데스크탑
플랫폼이다. 여기에 AI 에이전트를 붙이면 Windows 데스크탑 자동화와 결정적으로 다른
강점이 생긴다:

- 에이전트가 OCR/스크린샷 추측 없이 **구조화된 윈도우 상태**(창 목록, 포커스,
  z-order, 앱 이벤트)를 네이티브로 얻는다.
- 자동화 API 표면을 처음부터 설계할 수 있다 — 자동화가 해킹이 아니라 시민권자다.
- 특정 창의 프레임버퍼를 직접 캡처할 수 있다.

브레인스토밍 결과, 에이전트의 모든 얼굴(사용자 커맨드 바, 외부 MCP 에이전트,
개발/테스트 자동화)이 **동일한 능력 집합**을 요구한다: 윈도우·앱·이벤트·로그를
구조적으로 조회/조작하는 API. 이 문서는 그 공통 기반인 **Desktop Agent API**와
그를 실어 나르는 브로커를 정의한다.

## 2. 아키텍처

```
[커맨드바/패널창]  [MCP 서버]  [probes/자동화 스크립트]   ← 세 얼굴 (프론트)
        └───────── Desktop Agent API ─────────┘       ← 하나의 API
                        │
              [브로커 jkagentd]                        ← 이벤트 버스·권한 관문·receipt
                        │
              [윈도우 서버 (jkserver)]                  ← 이벤트 소스 + API 실행
```

핵심 원칙:

- **하나의 API, 여러 얼굴.** 커맨드 바의 "창 두 개 나란히"와 MCP의 `set_layout`은
  같은 함수를 호출한다. 에이전트 기능을 따로 구현하지 않고, 플랫폼이 자동화를
  일등 시민으로 갖게 된다.
- **브로커는 사이드카다.** jkagentd가 죽어도 데스크탑은 정상 동작한다. 이벤트
  버스, 권한 검사, receipt 기록은 전부 브로커에 둔다.
- **3-lib 분할과 정렬.** Agent API는 jkserver 프로토콜의 일부로 설계한다. 분할이
  완성되면 MCP 서버는 얇은 클라이언트가 되고, 에이전트가 서버 설계의 증명서가 된다.

## 3. Desktop Agent API (1단계)

MCP tool 형태로 노출하는 첫 세트:

| Tool | 내용 |
|---|---|
| `list_windows` | 앱/제목/geometry/포커스 — 구조화 정보, 스크린샷 추측 없음 |
| `launch_app` | 앱 이름 또는 .jkx 경로로 실행 |
| `close_window` | 창 종료 (권한: control, 인라인 승인 대상) |
| `focus_window` | 포커스 전환 |
| `save_layout` / `restore_layout` | 창 배치 스냅샷 저장/복원 — 데스크탑 undo의 기반 |
| `read_log` | jkdesktop/term 로그 tail·필터 |
| `terminal_exec` | ConPTY 세션 생성 + 출력 스트림 읽기 (권한: execute) |

전송: stdio MCP ↔ jkagentd ↔ 윈도우 서버 IPC. MCP 서버는 자체 UI/상태를 갖지 않는
얇은 어댑터다.

## 4. 이벤트 버스 (1단계에 최소 포함)

**소스**: 윈도우 서버 코어(창/앱/입력 이벤트를 이미 알고 있음) → 브로커가 구독·배포.

**토픽 분류학**:

- 윈도우: `created / destroyed / focused / unfocused / moved / resized / minimized / restored`
- 앱: `started / exited / crashed / hung`
- 입력: `user_idle(sec)` / `user_active`
- 터미널: `output` (정규식 매칭 필터 지원)
- 미디어: `player.paused / track_changed`

**페이로드 예시**:

```json
{"topic":"window.destroyed", "app":"vplayer", "win_id":42, "ts":...}
{"topic":"terminal.output", "session":3, "text":"...error C2065...", "match":"error"}
{"topic":"app.crashed", "app":"minesweeper", "exit":139}
```

**구독 모델**: 토픽 필터 + payload 정규식 등록 → 비동기 큐 배달. 구독자별 rate
limit(초과 시 drop + 통계), 미배달분은 dead-letter. 1단계에서는 구독자가
MCP `subscribe` tool과 자체 큐만 있어도 충분하다.

**자동화 스크립트(2단계)**: QuickJS로 트리거 스크립트 작성, .jkx 매니페스트에
트리거 선언:

```js
on("terminal.output", {match: /error C\d+|fatal/i}, async (e) => {
  await desktop.notify("빌드 실패 감지", {open_log: e.session});
});
on("user_idle", {sec: 1800}, () => desktop.layout.save("away"));
```

## 5. 권한/프라이버시 모델

| 등급 | 내용 | 기본값 |
|---|---|---|
| observe | 창 목록·이벤트 구독 | 앱별 옵트인 |
| control | 포커스·이동·닫기·레이아웃 | 닫기는 인라인 승인 |
| execute | 앱 실행·terminal_exec | 매번 또는 세션 승인 |
| capture | 창 픽셀 캡처 (observe와 별도) | 재미/분석 기능만 요청 |
| files / network | 파일 접근·모델 API 호출 | 설치 시 선언 + 첫 실행 승인 |

- **매니페스트 확장** (.jkx 2단계): `permissions: [observe, control:close?confirm, execute:apps]`
  형태로 등급+조건을 한 줄 선언. 1단계에서는 MCP 클라이언트(외부 에이전트)에
  세션 단위 승인을 적용한다.
- **강제 지점**: 모든 API 호출은 브로커 관문 통과. 캐퍼빌리티 토큰 검증. 승인
  필요 동작은 커맨드 바 인라인 프롬프트로 위임(2단계; 1단계는 콘솔/알림 승인).
- **receipt**: `{ts, agent, call, args요약, result, 승인자(user/auto)}` append-only
  로그. 모든 조작 동작이 기록된다. 타임라인 쿼리의 백엔드.
- **타임라인 프라이버시**: receipt 보존 기본 7일, 앱별 observe 옵트아웃("비밀 창"),
  설정에서 전체 끄기.

## 6. 로드맵 (2단계 이후)

1. **커맨드 바** — 태스크바 통합 팔레트(`Alt+Space`), ImGui 렌더. 두 모드 입력
   (슬래시 = 결정적, 자연어 = 파서→에이전트). 실행 전 레이아웃 미리보기 오버레이 +
   undo 칩. 인라인 승인 UX. 에이전트 패널 창(스트리밍 텍스트 + 미니 위젯,
   앱과 동일 ImGui 렌더 경로).
2. **agent = .jkx 앱** — `agent` 패키지 타입, 설치 시 권한 표시, 트리거 스크립트.
3. **멀티 에이전트** — 에이전트 간 통신도 이벤트 버스로(`agent.request/help` 토픽 +
   능력 레지스트리). 리서처는 브라우저 창, 코더는 터미널에서 각자 동작.
4. **시나리오 기능** — 마인스위퍼 코치(capture로 보드 캡처), vplayer 씬 검색
   (자막/프레임 인덱싱 → jog dial seek 연결), 아침 복원(레이아웃 + receipt/타임라인).
5. **QuickJS AI tool** — 스크립트 앱에 `agent.summarize(...)` 등 노출.

## 7. 리스크와 방어

| 리스크 | 방어 |
|---|---|
| 클라우드 왕복 지연이 데스크탑 UX 즉각성을 깸 | 결정적 커맨드(실행/레이아웃)는 로컬 파싱, 무거운 추론만 에이전트 위임 |
| 환각으로 잘못된 조작 | undo(레이아웃 스냅샷) + 승인 게이트 + receipt — 3중 방어 |
| 이벤트 버스 스팸/폭주 | 구독자별 rate limit + dead-letter |
| 브로커 크래시 | 사이드카 격리 — 데스크탑 본체와 독립 수명 |

## 8. 테스트 전략

- Desktop Agent API의 각 tool은 기존 probes 체계(`engine/tools/probes/*.ps1`,
  로그 기반 검증 플레이북 doc 15)에 편승한다: 빌드 → MCP로 launch → probe →
  `read_log` 판정 → 리포트. probes가 MCP tool로 승격되는 형태.
- 이벤트 버스는 합성 이벤트 주입으로 배달·rate limit·dead-letter를 단위 테스트.
- 권한 관문은 캐퍼빌리티 토큰 없는 호출의 거부를 테이블 기반 케이스로 검증.

## 9. 비목표 (Non-goals, 1단계)

- 커맨드 바 UI 구현 (2단계)
- 자연어 파서/모델 라우팅 — 1단계는 외부 에이전트(Claude Code 등)가 그 역할을 한다
- 타임라인 질의 UI — receipt만 안정적으로 쌓는 것까지가 1단계
- 멀티 에이전트 협업 프로토콜 — 이벤트 버스 위에서 2단계에 설계