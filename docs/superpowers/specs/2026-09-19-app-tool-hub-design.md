# 앱 도구 허브 (app tool hub) 설계 — 2026-09-19

as-built 문서는 docs/58 예정. 본 스펙은 설계 원본.

## 0. 원천과 결정

- **사용자 요구 (2026-09-19)**: "말로 할 수 있는 게 많았으면 좋겠어. 앱의
  내부 기능도" + 제안 구조 "MCP hub처럼 앱별 툴리스트가 있고 런치하면
  등록". 로드맵 메모리 갱신 6의 합의 스케치를 이 세션에서 사용자와
  섹션별로 확정했다.
- **확정 결정 목록 (2026-09-19, 전부 사용자 선택)**:
  1. 첫 실동작 앱 = **vplayer**.
  2. LLM 도구 목록 경로 = **B안: 동적 tools/list** (브로커가 tools/list
     시점에 서버에 질의해 등록 앱 도구를 개별 MCP 도구로 합성). 세션
     시작 후 뜬 앱 도구는 그 세션 LLM에 안 보인다는 점을 수용한 한계로
     문서화 (claude 세션이 매턴 짧아 실질 영향 작음).
  3. 앱 도구 기본 권한 = **allow**. 조정은 permissions.json 3단 키
     (도구별 > 앱별 > 전역) 해석.
  4. 등록 주체 = **전 클라** (연결형 클라면 누구나 AgentToolRegister
     가능). 단 v1에서 실제 등록을 구현하는 것은 창 클라(vplayer) —
     콘솔 앱은 등록 경로를 태우지 않는다(§7).
  5. 승인 대상 시각화 = **3단 전부** docs/58에 담는다 (§5).
  6. 콘솔 앱 노출 전략 = **claude skill(문서형) 래퍼** — 사용자 명시
     "내 skill은 claude skill을 말하는 거에요" (§7).
- **선례 조사 (설계 근거)**: GUI 앱 MCP화 외부 선례는 3형태 —
  ①앱이 자체 MCP 서버 내장(Figma 데스크톱, JetBrains IDE 2025.1+,
  Home Assistant) ②앱 플러그인 소켓 + 별도 브리지 프로세스가 MCP로
  번역(Blender MCP, Unity MCP, Ableton MCP) ③외부 자동화 프로토콜을
  MCP로 감싸 앱 구동(chrome-devtools-mcp/CDP). 본 설계는 **②형태**이되,
  앱↔브리지 채널로 새 소켓 대신 기존 창 서버 파이프를 쓰고 브리지 역할을
  기존 jkagentd 브로커가 겸한다. ①형태(앱마다 자체 stdio MCP 서버)는
  claude 세션이 매턴 스폰되므로 세션당 사이드카가 뜨고 죽고, 앱 도달에
  또 파이프 브리지가 필요해 레이어만 늘어나므로 배제.

## 1. 문제 정의

에이전트가 앱을 다루는 현재 수단은 창 단위 조작(list_windows,
focus_window, close_window)과 입력 주입뿐 — "vplayer에서 30초로 시크해
줘" 같은 **앱 내부 기능**은 말로 할 수 없다. send_input류 주입은 앱별
좌표/UI 구조 지식을 에이전트가 갖고 있어야 해서 확장 불가. 앱이 런치 시
자기 도구를 선언하면 에이전트가 앱 기능을 네이티브로 부를 수 있는
허브가 필요하다.

## 2. 아키텍처 총관

```
claude ──MCP── jkagentd(브로커) ──파이프── JKWindowServer(게이트·레지스트리)
                                              │ AgentToolCall
                                              ▼
                                          앱 (vplayer …)
```

- 등록: 앱 → 서버 (`AgentToolRegister`)
- 발견: 브로커가 tools/list 시점에 서버 `list_app_tools` 질의 → MCP
  도구 합성
- 호출: claude → MCP tools/call `vplayer_seek` → 브로커가 서버 도구
  `app_tool` 원 요청 전송 → 서버 게이트 → `AgentToolCall` 중계 → 앱 실행
  → `AgentToolResult` → reqId 상관관계로 원 요청자 reply
- args는 **원문 패스스루**(jkbridge generic relay 선례) — 서버는 앱 도구
  args를 검증하지 않는다. 앱이 스키마를 선언하고 자기가 검증한다. 이래야
  앱 도구 추가에 서버 수정이 0.

## 3. IPC — 신규 메시지 3종

`JKWireProtocol.h` MsgType 연번 (현재 최대 21=WindowTitle):

| MsgType | 번호 | 방향 | 페이로드 |
|---|---|---|---|
| `AgentToolRegister` | 22 | C→S | `{app, tools:[{name, description, inputSchema}]}` |
| `AgentToolCall` | 23 | S→C | `{reqId, app, tool, args}` |
| `AgentToolResult` | 24 | C→S | `{reqId, ok, result\|error}` |

- `inputSchema`는 MCP inputSchema 객체(JSON) — 브로커가 tools/list에
  그대로 실으므로 앱이 MCP 호환 형식으로 선언하는 것이 계약.
- `AgentToolRegister` 재전송 = upsert (도구 표 교체).
- `AgentToolRegister`에 대한 서버 응답: 새 reply 형식 대신
  AgentQuery/Reply가 아닌 전용 경로가 필요하면 복잡해지므로 — 등록은
  **AgentToolResult 역방향 재사용이 아니라 등록 확인을 응답 페이로드로
  하는 AgentReply와 동일한 fixed-header reply 프레임**을 쓴다. 구현 시
  `AgentReply` writer를 재사용해 `{"ok":true}`/`{"ok":false,"error":...}`
  를 돌려주고, reqId는 등록용 자체 채번. (와이어에 새 S→C 형식을
  늘리지 않는다.) 앱은 자기가 보낸 적 없는 reqId의 reply를 무시한다
  (등록 ack는 정보성 — 앱이 등록 성공에 블록하지 않고, 거부 사유는
  서버 로그+카탈로그 부재로도 판별 가능).
- 페이로드는 AgentQuery 계열과 같은 "fixed header + JSON" 관용구
  (JKWireProtocol §AgentQueryHeader 선례).

## 4. 서버 측 — 레지스트리 + 게이트

### 4.1 레지스트리

```
struct AppToolManifest {
    uint32_t connId;          // 소유 연결 — 수명 기준
    std::string app;          // 네임스페이스 (연결이 선언)
    uint32_t windowId;        // 창 클라만 (0 = 제어 연결)
    std::string title;        // 카탈로그 표기용 창 제목
    struct Tool { std::string name, description, inputSchema; };
    std::vector<Tool> tools;
};
std::map<uint32_t, AppToolManifest> appToolManifests_;  // connId → 매니페스트
```

- **연결 수명에 묶임** — CleanupDisconnectedClients에서 매니페스트 소멸 +
  `agent.app_tools_changed` publish. 별도 언레지스터 메시지 없음.
- 등록 검증(거부 시 사유 응답):
  - `bad_app` — app 네임스페이스: `^[a-z][a-z0-9_]{0,15}$`
  - `bad_name` — 도구명: `^[a-z][a-z0-9_]{0,31}$`
  - `too_many_tools` — 연결당 도구 ≤32
  - `schema_too_large` — inputSchema ≤2KiB (desc ≤512B, args ≤8KiB,
    result ≤16KiB는 호출 경로 상한)
  - `namespace_conflict` — app이 코어 도구명, 이벤트 예약 접두
    (`window.`, `agent.` 등 events_list 카탈로그의 서버 토픽 접두
    선례), 또는 **기존 등록된 다른 연결의 app**와 충돌. 예외: 같은
    connId의 재등록은 언제나 허용(upsert).
- 셸 특권 연결(taskbar 등 IsShell)의 등록은 봉쇄 — 셸 도구 표면 오염 방지.

### 4.2 인스턴스 변별

- (app, tool) 레지스트리 조회는 **모든 연결의 매니페스트를 걷어서**
  후보 집합을 만든다. 후보 복수 시:
  - `app_tool`에 `windowId` 지정 → 그 연결 직행
  - 미지정 + 후보 1개 → 라우팅
  - 미지정 + 후보 복수 → **에러 `ambiguous`** + 후보
    `[{windowId, title}]` 반환 (묵시적 추측 라우팅 금지 — 조용히 엉뚱한
    창을 건드리는 것보다 에러 한 번). LLM은 한 턴에 자기교정.
- 브로커 tools/list에서 중복 이름은 **이름당 1개만** 노출(유니크 이름
  유니온). `vplayer7_seek` 같은 창id 기반 MCP명은 창 id가 재부팅마다
  바뀌므로 배제.
- `app_tool` 응답에 `windowId` 에코 — 감사 기록이 어느 인스턴스가
  실행했는지 추적 가능.

### 4.3 게이트 — app_tool 행 + 3단 키 해석

- `kPermMatrix`에 `{"app_tool", "server", "allow"}` 행 추가.
- 해석 순서: `app_tool.<app>.<tool>` > `app_tool.<app>` > `app_tool`
  (도구별 > 앱별 > 전역). 파일에 키가 없으면 다음 단계로 폴백, 전역까지
  없으면 **기본 allow**.
- `askCapable`에 `app_tool` 편입 — 파일값 `"ask"`면 기존 승인 파이프라인
  재사용(파킹, kind=`app_tool`). 승인 스트립 문구는
  "[<app> 창 #<id>] <tool> 실행할까요?" (§5 2단과 결합).
- deny = 거부. 브로커 쪽도 동일 3단 해석으로 MCP 경로 선차단 — 브로커
  기본값은 **allow 명시**(브로커 키 누락=deny 레슨 재발 방지).
- permission_set이 이 도구를 건드리는 경로: permission_set은 코어 도구
  키 1개를 바꾸는 도구다. `app_tool.vplayer` 같은 3단 키 편집은
  permissions.json 파일 편집이 승인 행위(기존 계약 유지) — permission_set
  확장은 하지 않는다(YAGNI, 후속).

### 4.4 서버 도구 2종

- **`list_app_tools {}`** → `{"ok":true,"apps":[{"app","windowId",
  "title","connId","tools":[{"name","description","inputSchema"}]}]}`
  — 카탈로그 조회. 브로커 tools/list 합성과 agentctl face 양쪽이 먹는다.
  kPermMatrix `{"list_app_tools","none","allow"}`.
- **`app_tool {app, tool, args, windowId?}`** → 중계. args는 원문
  패스스루(스키마 검증 안 함). 응답 `{"ok":true,"windowId":..,
  "result":<앱이 반환한 것>}` / 에러는 §8 표면.

## 5. 승인 대상 시각화 — 3단

ask 파킹은 텍스트로만 무엇에 승인하는지 전달하므로, 대상을 눈으로
보여주는 레이어. 서버가 컴포지터라 **앱별 코드 0줄**.

1. **대상 창 하이라이트 (서버 크롬 드로잉 — 핵심)**: 파킹 동안 서버가
   대상 창 레이어 위에 호박색 링 + 상단 배너 "에이전트 승인 대기:
   <app>.<tool>"을 그린다. `DrawCloseOverlay`와 같은 서버 측 오버레이
   드로잉 경로. resolve(허용/거부/타임아웃) 시 즉시 해제.
   **승인 파이프라인 자체의 기능**으로 만들어 대상 windowId를 갖는 기존
   ask 도구(close_window, run_console_app)에도 적용 — 모든 ask 흐름의
   최상위 확인 레이어. 셸/캡처 오버레이는 close 게이트와 동일 면제.
   대상이 windowId 0(제어 연결)이면 하이라이트 생략.
2. **승인 스트립 정보 강화**: `agent.approval_request` 이벤트에
   `target: {app, tool, windowId, title}` 확장(app_tool만). 기존 도구는
   기존 문구 유지(필드 추가만 — 하위호환).
3. **대상 창 썸네일**: 파킹 시점에 capture_window 경로로 대상 창 현재
   프레임을 `state\screenshots\approval_<ts>_<req>.png`로 캡처해 이벤트
   data에 경로 첨부 — "조작 전" 상태 기록. 캡처 실패는 비치명(필드
   생략). UI 소비(알림 센터 확장 카드 등)는 후속 스펙.

## 6. 브로커(jkagentd) — 동적 tools/list + 라우팅

- **tools/list 동적 합성**: `kToolsListJson`을 "코어 정적부 + 앱 도구
  동적부"로 분해. 정적부는 현 문자열 유지, 동적부는 tools/list 요청
  시점에 서버 `list_app_tools` 질의로 합성.
  - MCP 도구명: `<app>_<tool>` (MCP 도구명은 점을 안 받는 관례 →
    언더스코어; 카탈로그/권한 표기는 `vplayer.seek` 정식 네임스페이스).
  - description 앞에 `[<app>]` 접두 — LLM이 소속을 안다.
  - inputSchema는 앱 선언 그대로. 중복 이름은 이름당 1개.
  - 서버 질의 실패/타임아웃 시 안전 폴백: **정적부만 반환** — 브로커
    사망이 아니라 앱 도구만 잠깐 안 보이는 수준.
- **tools/call 라우팅**: 동적 이름 수신 → 접두 추측이 아니라 **등록된
  (app, tool) 조합 역매칭**으로 파싱(app/도구명에 `_` 포함 가능) →
  서버 도구 `app_tool {app, tool, args}` 원 요청 전송. 이후 게이트/
  파킹/중계는 서버가 처리(브로커는 ask를 몰라도 됨 — 파킹을 원 요청자
  블록으로 소비하는 기존 선례).
- 브로커 게이트: 3단 키 해석으로 MCP 경로 선차단(§4.3).
- 알려지지 않은 동적 이름(앱이 방금 종료 등) → 기존 unknown-tool 에러.
- **폰 브리지(jkbridge)**: generic relay(args GetRaw 패스스루)라 서버
  쪽 도구 추가에 무수정 — 구현 시 체크리스트로 브리지 도구 목록 UI가
  있다면 `list_app_tools` 기반 갱신인지 확인만.
- **receipts**: app_tool 중계도 브로커 경로면 기록(기존 관례 유지).
  직결 face(agentctl)는 서버 이벤트/로그로만.

## 7. 콘솔 앱 전략 = claude skill 래퍼

- 콘솔 앱의 에이전트 노출은 **도구 등록이 아니라 claude skill(문서형)
  래퍼** — skill 문서가 "이 앱은 run_console_app으로 스폰하고,
  terminal_exec로 X를 하고, terminal.output로 결과를 읽는다"는 사용
  계약을 담는다. 엔진 코드 0줄.
- 샘플로 콘솔 앱 1종의 skill 래퍼 예제를 만든다. 배치 위치: chat LLM
  worker cwd가 `state\chat.json` `directory`에 고정되므로 그 경로의
  `.claude\skills\` 또는 유저 레벨 `~\.claude\skills\` — 구현 시
  chat.json 기본 directory 기준으로 확정하고 docs/58에 기록.
- 프로토콜 자체는 전 클라에게 열려 있으므로 콘솔 앱이 미래에 등록 경로를
  채택하는 것은 막지 않는다.

## 8. vplayer 도구 세트 + 클라 수신 공통화

### 8.1 vplayer 등록 도구 6종 (전부 PlayerCore 기존 경유 — 새 재생 로직 0)

| 도구 | 인자 | 동작 |
|---|---|---|
| `open` | `{path}` | 비동기 오픈(T1 OpenStage) — 즉시 accepted 응답, 진행은 get_status로 추적 |
| `play_pause` | `{}` | 토글 |
| `seek` | `{seconds}` | 절대 시각 시크(기존 2단 시크) |
| `set_volume` | `{percent 0..100}` | 볼륨 |
| `set_av_delay` | `{seconds -10..10}` | A/V 오프셋 |
| `get_status` | `{}` | `{file, pos, duration, playing, fps, underruns, av_delay, volume}` — 상태행 데이터 재사용 |

- `advance`(상대 시크)는 제외 — get_status로 pos 읽고 절대 시크 계산으로
  충분 (YAGNI).
- 인자 위반(범위 밖 percent 등)은 앱이 검증해 error 반환 — 서버는
  검증하지 않는다(§2 패스스루 계약).

### 8.2 클라 수신 공통화 (jkclient)

- `JKClientSurface`: `AgentToolCall` 메시지 큐 적재 + `AgentToolResult`
  전송 헬퍼 — **jkclient 공통 1회 구현**.
- 앱이 할 일: ①초기화 시 `AgentToolRegister` 전송 ②프레임 루프에서
  `PollToolCall()` 폴링(기존 DrainAgentEvents와 같은 프레임 펌프 결합
  관용구) ③핸들러에서 PlayerCore 세터 호출 ④결과 반환.
- vplayer 앱 측 코드 = 등록 표 + 폴링+핸들러 (~100줄 수준).
- 스레드 안전: 도구 핸들러는 **UI/프레임 스레드**에서 실행 — 기존
  UI→PlayerCore 세터 경로(SetJog, 슬라이더)와 동일하므로 새 락 설계 없음.

## 9. 오류 처리

- 등록 거부(§4.1 표): `bad_app` / `bad_name` / `too_many_tools` /
  `schema_too_large` / `namespace_conflict`
- 호출 경로: `unknown_app_tool` / `ambiguous`(후보 목록) /
  `tool_timeout`(10s) / `tool_gone`(중계 대기 중 앱 종료) / 게이트
  `denied`·`capture_ask` 선례 준수
- **시퀀싱**: 게이트 판정 → allow: 즉시 중계+타이머 시작 / ask: 파킹
  (타이머는 resolve 후 시작 — 승인 대기로 도구 타임아웃 오발 방지) /
  deny: 에러 reply만
- 썸네일 캡처 실패 = 비치명(이벤트 필드 생략, 승인은 계속)
- 대상이 제어 연결(windowId 0)이면 하이라이트 생략, 스트립에는 표기

## 10. 테스트

- 신규 `probe_app_tools.ps1` (×2 연속 ALL PASS):
  1. vplayer 스폰 → `list_app_tools`에 6도구 노출
  2. 브로커 `tools/list`에 `vplayer_seek` 동적 합성 확인
  3. `app_tool seek` e2e — `get_status`로 pos 실측 변화
  4. 2인스턴스 ambiguity 에러 + 후보 목록
  5. 창 닫기 → 카탈로그 소멸(자동 정리) + `agent.app_tools_changed`
  6. 게이트 deny(거부) / ask(파킹+승인 스트립 문구+하이라이트+썸네일
     경로 첨부) — permissions.json 편집으로 구동
  7. 등록 검증 거부(bad_name 등)
  8. 무응답 스텁 클라(도구 등록만 하고 result를 안 보내는 테스트 클라)
     → tool_timeout
  9. 대상 창 하이라이트 픽셀 실측(기존 캡처 프로브 관용구) 1체크
- 회귀: 기존 전 프로브 + `jkagentd --selftest` + vplayer
  vpt4/vpt9/vpt5/vpt11 (도구 호출이 재생에 영향 없음)
- 승인 파이프라인 기능 일반화(하이라이트) 때문에 probe_agent_chat /
  probe_agent_trust 회귀 필수 — 기존 ask 문구 필드 추가 하위호환 검증

## 11. 범위 밖 / 후속

- 썸네일 소비 UI(알림 센터 확장 카드) — 후속 스펙
- permission_set의 3단 키 편집 확장 — 후속 (파일 편집이 승인 행위인
  기존 계약 유지)
- 콘솔 앱의 도구 등록 채택 — 프로토콜은 열어둠, P4 SDK 템플릿 미포함
- MCP 도구명의 세션 후 런치 앱 미노출 — 수용한 한계(§0 결정 2)
- 승인 스트립의 썸네일 렌더링 — §5 3단 UI 소비는 후속
- vplayer 이외 앱(mine/browser 등) 도구 세트 — 각 앱 착수 시 별도