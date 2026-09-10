# docs/34 — 데스크톱 에이전트: 트리거 활성/비활성 UI

날짜: 2026-09-11
이전: docs/32 (M2b 트리거), docs/33 (알림 센터)
계획: docs/superpowers/plans/2026-09-11-agent-triggerctl.md

## 1. 요약

트리거(jktriggers 번들)를 에이전트 도구와 팔레트 슬래시 커맨드로 on/off한다.
브레인스토밍 결정: 독립 매니저 앱이 아니라 **팔레트 + 도구** (기존 패턴 재사용,
알림 센터와 같은 빠른 경로). 상태의 단일 진실원은 `state/triggers.json`.

## 2. 구성 요소

### 2.1 상태 파일 (`<exeDir>\state\`)

- `triggers.json` — 서버가 쓰는 활성 플래그 (trigger_toggle).
  `{"triggers":[{"name":"trig_build","enabled":0},...]}`. 없는 항목 = 활성(1).
- `triggers_loaded.json` — jktriggers가 시작 시 쓰는 매니페스트.
  **컨테이너×토픽 평면 행** `{"triggers":[{"name":"trig_build","topic":"terminal.output"},...]}`
  — AgentJson 중첩 배열 접근이 없어 평면으로 뽑기로 확정 (AgentJson은
  `GetArrStr` 단일 레벨 접근자만 제공).

### 2.2 jktriggers

- `TriggerReg.source` = 컨테이너명 (`.jkx` 확장자 제거) — JsOn에서
  `g_currentSource`를 태깅해 등록된다. 컨테이너가 enable/disable의 단위.
- `ReloadTriggerFlags()` — `state/triggers.json`을 읽어 C 레벨 플래그 적용
  (스크립트는 건드리지 않음). 시작 시 1회 + `triggers.reload` 이벤트마다.
- `DispatchEvent` 게이트: `if (!t.enabled) continue;`
- `WriteLoadedManifest()` — 시작 시 매니페스트 기록.

### 2.3 서버 도구 (권한 게이트 밖 — launch_app 등급)

- **`trigger_toggle {"name":<str>,"on":<0|1>}`** — `state/triggers.json`
  읽기-수정-쓰기 + 성공 시 `triggers.reload` publish (publish_event와 같은
  `{"topic","data","ts"}` 인벨롭).
- **`trigger_list`** — loaded 매니페스트 + flags 병합.
  `{"ok":true,"triggers":[{"name","topics":[...],"enabled"},...]}`.

### 2.4 팔레트

- `/triggers` — 목록 조회 (trigger_list reply를 로그로 표시)
- `/trigger <name> on|off` — 토글 (args로 `on:1|0` 전송)
- `/help` 갱신

## 3. 검증

`tools/probes/probe_agent_triggerctl.ps1` — 5 체크:

1. loaded-manifest: 기동 3초 내 triggers_loaded.json에 3종 존재.
2. list: trigger_list ok + 3개 enabled:1.
3. off-blocks: notify 앱 구독자 기동 → trig_build off → publish
   terminal.output 에러 → 히스토리 엔트리 증가 없음.
4. on-resumes: on → publish → 히스토리 +1 (실제 트리거가 agent.notify 발화).
5. state-file: triggers.json에 `{"name":"trig_build","enabled":1}`.

전부 파일/agentctl 판정 (레슨 34). 전체 회귀: selftest 0, jkagentd
selftest 0, mcp/e2e/palette/chat/chat_llm/triggers/notify/triggerctl 전부 PASS.

## 4. 레슨

1. **`std::map::operator[]` 기본값 0 함정** — trigger_list가 flags 파일이
   없을 때 enabled를 0으로 표시했다. "없으면 활성" 정책이라 매니페스트 적재
   시 `entry.second = 1` 명시. merge 스타일 코드에서 기본값 정책을
   자료구조 선택이 결정하지 않게 주의.
2. **중첩 배열 접근이 없으면 데이터 형식을 평면으로 내린다** — topics 배열을
   컨테이너 안에 넣으려다 AgentJson API 확인에서 막힘; 매니페스트를
   컨테이너×토픽 평면 행으로 바꾸면 기존 접근자 그대로 동작.
3. 트리거 활성화 상태는 **이벤트로 밀고 파일로 읽는다** — 서버가 파일을
   쓰고 reload 이벤트만 발행; jktriggers는 파일이 유일한 진실원.
   서버가 jktriggers 프로세스를 몰라도 되는 느슨한 결합.

## 5. 파일

- `tools/jktriggers/main.cpp` — source/enabled, ReloadTriggerFlags,
  WriteLoadedManifest, reload 이벤트, DispatchEvent 게이트.
- `src/server/JKWindowServer.cpp` — trigger_toggle/trigger_list.
- `src/apps/ClientPaletteApp.cpp` — /triggers, /trigger.
- `tools/probes/probe_agent_triggerctl.ps1`.