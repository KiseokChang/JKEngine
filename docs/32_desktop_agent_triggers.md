# 32. Desktop Agent 트리거 — 이벤트 버스 + jktriggers (M2b)

상태: **구현 완료** (2026-09-10). 스펙 `docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md`
의 M2b 단계. 이벤트 발행/구독 배관(M1~M2a의 4번째 파이프 위)과 QuickJS 트리거 호스트,
첫 번들 3종, 채팅창 알림 표시.

이전: docs/31 (M2 채팅 창 + LLM 위임). 다음: 스펙 §7 (알림 센터, 스트리밍 등).

## 1. 아키텍처

```
발행자 (아무 에이전트 클라이언트)                    구독자
┌──────────────────────┐   Query    ┌──────────┐  AgentEvent  ┌──────────────┐
│ ClientTerminalApp     │ ────────▶ │ 서버      │ ───────────▶ │ jktriggers.exe│
│   terminal.output     │ publish_  │ (JKWindow │  브로드캐스트 │  (QuickJS)    │
│ 서버 내부 감지         │  event    │  Server)  │              ├──────────────┤
│   app.crashed/        │           │          │              │ jkchat.exe    │
│   window.destroyed    │           │          │              │  [알림] 줄     │
│ jktriggers/jkchat     │           │          │              │ (다음: 알림 센터)│
│   desktop.publish     │           │          │              └──────────────┘
└──────────────────────┘           └──────────┘
```

- **발행**: 모든 에이전트 클라이언트가 `publish_event` 도구를 보낼 수 있다
  (`{"tool":"publish_event","args":{"topic":"<t>","data":<raw JSON>}}`).
  서버는 topic(≤96자, JsonEsc 후에도 ≤96)과 data(≤4096바이트, `GetObjRaw`로
  원시 JSON 추출)를 검증해 `{"topic":...,"data":...,"ts":...}` 봉투로
  모든 `AgentEventSubscriber`에게 브로드캐스트한다.
- **구독**: connect 시 `AgentEventSubscribe(true)`를 선언한 연결만 받는다.
  jktriggers(컨트롤 전용), jkchat, 팔레트가 이미 구독 중.
- **서버 내부 발행**: `PushAgentEvent(topic,id,title,pid)` —
  `window.created/focused/destroyed`, `app.crashed`(M1부터) + `app.crashed`의
  크래시/정상 구분(M2b, 아래 §2). 봉투의 id/title/pid는 **최상위** 필드다.
  publish_event로 발행한 것은 **data 하위**에 들어간다 — 트리거 스크립트는
  이 차이를 알아야 한다 (`e.title` vs `e.text`).

## 2. 서버 변경점

- `publish_event` 도구 (M2b Task 1): `HandleAgentQuery`의 launch_app 앞 브랜치.
  `AgentJson::GetObjRaw` 신규 — JS_JSONStringify 기반 원시 JSON 접근자.
- **app.crashed 감지** (M2b Task 2): `SpawnProcess`가 자식 hProcess를
  `spawnedClients_[pid]`에 보관. 앱 윈도우 클라이언트 연결 끊김 시
  `GetExitCodeProcess` — 코드가 STILL_ACTIVE(259)도 0도 아니면 `app.crashed`,
  아니면 기존대로 `window.destroyed`. 외부에서 직접 시작한 클라이언트는
  테이블에 없으므로 항상 정상 경로.
- **terminal.output** (M2b Task 3): `ClientTerminalApp::PumpPty`가 VT 이스케이프를
  제거한 출력을 250ms/4KiB 코얼레싱해 `publish_event`로 발행 (fire-and-forget,
  응답 링은 계속 드레인).

## 3. jktriggers.exe — 트리거 호스트 (M2b Task 4)

컨트롤 전용 `JKAgentClient` (jkchat 패턴, UI 없음) + 영구 QuickJS 런타임
(quickjs-ng, JKScriptHost 등록 관례 — `JS_NewCFunction` 1함수당 1회).

```
Connect → SubscribeEvents(true) → 루프:
  400ms ping 펌프(밀린 이벤트/응답 플러시) → PollEvents → DispatchEvent
  → FireTimers → Sleep(50)
```

- **스크립트 API**:
  | 함수 | 설명 |
  |---|---|
  | `on(topic, filter, handler)` | topic 정확일치 또는 `"prefix.*"` 글롭. filter는 `{match:/regex/}` — 이벤트 **JSON 문자열**에 RegExp.test. `null`이면 무조건 발화 |
  | `desktop.notify(title, body?)` | `agent.notify` 토픽 방송 → 채팅창 `[알림]` 줄 |
  | `desktop.publish(topic, dataObj)` | `publish_event` 재방송 |
  | `desktop.saveLayout(name)` | 블로킹 쿼리, 응답은 로그로 |
  | `desktop.readFile(relPath)` | exeDir 기준 상대경로, ≤64KiB, 없으면 예외 |
  | `desktop.log(s)` | 호스트 stdout (`[triggers]` 접두, fflush) |
  | `setTimeout/clearTimeout/setInterval/clearInterval` | 호스트측 타이머 테이블, 50ms 틱 |
- **호이스팅**: DispatchEvent가 `data.text`를 이벤트 최상위로 올린다 (`e.text`) —
  terminal.output 피드 편의. 비동기 핸들러는 `JS_ExecutePendingJob`으로 정산.
- **로딩**: `<exeDir>\state\triggers\*.js`(개발) + `<exeDir>\apps\triggers\*.jkx`
  (패키지, 아래 §4). 로드 실패는 로그하고 계속.

## 4. .jkx 트리거 컨테이너 + 패커 (M2b Task 5)

- 소스: `engine/tools/triggers/<name>/{manifest.txt, *.js}`
- 패킹: `jktriggers --pack <srcDir> <outDir>` — CMake `triggers` ALL 타깃이
  `build/apps/triggers/<name>.jkx`로 패킹. 소스가 비어도 성공(즉, 폴더 없음=noop).
- 로더는 매니페스트를 **로컬에서만** 파싱한다 (`name=`/`trigger=` 쉼표 목록).
  서버의 `ScanJkxApps`는 `apps/*.jkx`만 보므로 `apps/triggers/` 컨테이너는
  런처 그리드에 나오지 않는다.

## 5. 첫 번들 3종 (M2b Task 6)

| 컨테이너 | 트리거 | 동작 |
|---|---|---|
| `trig_build` | `terminal.output` × `/error C\d+\|fatal error\|error:/i` | 60초 디바운스 → `[알림] 빌드 실패 감지 — <스니펫 200자>` |
| `trig_idle` | `window.focused/created/destroyed` 활동 + 10초 간격 | 임계값(기본 30분, `state/idle_minutes` 오버라이드 — 0도 유효) 초과 시 1회 `saveLayout("auto_idle")` + `[알림] 자동 저장` |
| `trig_crash` | `app.crashed` | `[알림] 앱 비정상 종료 — <title> (pid N)` |

교훈: `parseInt(x) || 30`는 0을 기본값으로 덮는다 — "0이 유효한 설정값"이면
`isNaN` 검사로 분기할 것 (trig_idle이 실측에서 이 버그로 발화 실패했다).

## 6. 채팅창 알림 (M2b Task 7)

`jkchat HandleEvent`에 `agent.notify` 브랜치 — 봉투 `data.title/data.body` →
`[알림] <title> — <body>` (무제면 `(무제)`). 알림 센터 앱(스펙 §7)은 이
이벤트 버스를 나중에 구독하는 두 번째 소비자가 될 뿐이다.

## 7. 테스트

- `probe_agent_triggers.ps1` (7 체크): publish → 빌드 실패 알림(채팅 트랜스크립트
  크로스프로세스 판독) → taskkill 크래시 → `앱 비정상 종료` → 정상 close로
  비정상 줄 증가 없음(graceful 회귀) → idle_minutes=0 → `자동 저장` +
  `save_layout ok`.
- 회귀 전부 녹색: `jkdesktop test` 0, `jkagentd --selftest` 0,
  mcp 5/5, e2e 7/7, palette 4/4, chat 7/7, chat_llm 2/2, triggers 7/7.

### 프로브가 잡아낸 버그
- `DispatchEvent` 필터: `RegExp.test`에 이벤트 **문자열**이 아니라 파싱된
  **객체**를 넘겨 `[object Object]`만 검사 → 필터가 항상 미스. 필터 없는
  트리거만 테스트되어 Task 4/5에서 미검출 — 필터 경로는 반드시 실측할 것.

### agentctl 공백 제한
`agentctl`은 네이티브 argv — PS의 `\"` 이스케이프가 따옴표 상태를 뒤집어
**JSON 안에 공백이 있으면 인수가 쪼개져** `bad_request`. 파이프로 통신하는
jktriggers/jkchat/터미널은 영향 없음. 프로브 페이로드는 공백을 피함
(`/error:/i`는 `error:C2084`도 잡는다).

## 8. 제한과 다음 단계

- **rate limit/dead-letter 없음**: 핸들러 예외는 로그만 하고 트리거는 계속.
  무한 루프 트리거(publish→on 셀프 루프)는 호스트가 막지 않는다 — 번들 작성
  규율 + 이후 rate limiter.
- **async 이벤트 루프 아님**: DispatchEvent는 동기 호출 + pending job 정산.
  긴 핸들러가 펌프를 막는다 (saveLayout의 블로킹 Query도 동일).
- **스트리밍 아님**: terminal.output은 250ms/4KiB 코얼레싱된 청크 — 행 경계
  보장 없음. LLM 토큰 스트리밍은 스펙 §7 후속.
- **알림 센터 앱**: ~~`agent.notify` 소비자 추가~~ — **구현됨 (2026-09-11,
  docs/33)**: jkapp_notify (ImGui 클라, 히스토리/토스트/제목 배지).
- **M2b 남은 것**: 트리거 활성/비활성 UI, `state/triggers` 재적재(현재 시작 시
  1회), 스크립트 서명/신뢰 모델.