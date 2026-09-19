# 58. 앱 도구 허브 (app tool hub) as-built — 2026-09-19

스펙: `docs/superpowers/specs/2026-09-19-app-tool-hub-design.md` / 플랜:
`docs/superpowers/plans/2026-09-19-app-tool-hub.md` / 리드저(룰링·레슨 원장):
`.superpowers/sdd/2026-09-19-app-tool-hub/progress.md`.

커밋 스팬: `07060e9`(플랜 preflight 픽스) → `3443314`(와이어) → `2edab7c`(서버
등록) → `3311995`(카탈로그+중계+게이트) → `30ef72d`(클라 공통) → `f1417c4`
(vplayer) → `8283ed8`(프로브 스테이지 1) → `b9a98ca`(브로커) → `8237f52`
(하이라이트) → `fb0d68a`(스트립+썸네일) → `a1f6247`(jkbridge 문구) → 본 커밋
(skill 래퍼 + docs/58).

## 0. 원천과 결정

- **요구 (2026-09-19, 사용자)**: "말로 할 수 있는 게 많았으면 좋겠어. 앱의
  내부 기능도" + 제안 구조 "MCP hub처럼 앱별 툴리스트가 있고 런치하면
  등록". 사용자와 섹션별 확정(스펙 §0): 첫 앱=vplayer, 경로=B안 동적
  tools/list, 기본 권한=allow, 등록 주체=전 클라(실동작은 창 클라),
  시각화 3단, 콘솔 앱=claude skill 래퍼.
- **실행 중 룰링 4건** (코디네이터, 사용자 확정 포함):
  1. **상관관계가 크기 상한에 선행** (Task 2 리뷰 Important 1) —
     `HandleToolResult`에서 16KiB `result_too_large` 검사가
     `targetConnId == client.Id()` 상관관계보다 앞서 있으면, 순차 채번
     reqId를 추측한 제3 연결이 진짜 요청자의 inflight를 훼손할 수 있다.
     픽스 = `find(reqId)` → 상관관계 먼저 → 크기 상한(이제 target 발신자에게만
     적용). 보안 우선 순서(§3 와이어).
  2. **동일 앱 다중 인스턴스 등록 허용** (Task 3 룰링, 스펙 §4.1 정정
     `2b28988`) — 원문의 "다른 연결의 app 충돌" namespace_conflict 절이 §4.2
     인스턴스 변별(사용자 확정 설계)과 모순이었다: 그 충돌을 봉쇄하면
     vplayer 2개를 띄우는 순간 ambiguous가 영원히 도달 불가. §4.2가 승리.
     남는 봉쇄 = 코어 도구명(kPermMatrix 행과 걸림 — app="app_tool"/
     "list_app_tools" 등록은 이 행들에 걸려 자동 봉쇄, load-bearing) +
     이벤트 예약 접두 + 셸 연결. 비용: 동일 앱 스푸핑 등록은 가능해지지만
     args는 앱 계약 패스스루라 위험 작음.
  3. **프로브 체크 13 = 복제 금지, 편입으로 충족** (Task 10 룰링) —
     하이라이트 픽셀 실측을 probe_app_tools에 복제하지 않고
     `probe_app_tools_highlight.ps1`을 (a) 회귀 스윕 독립 항목 + (b) c15
     중첩 재실행 체크(RESULT: ALL PASS 명시 체크)로 편입. 전용 프로브가
     이미 ×2 ALL PASS — 중복 픽셀 코드는 유지보수 부채.
  4. **jkbridge 승인 문구 픽시 이월** (Task 9 리뷰 M-4 → Task 10) —
     브리지 폰 UI가 app_tool 승인을 기존 폴백 `"[<title> #<id>] 창을
     닫을까요?"`(close_window 재용)로 렌더링하던 기존 결함. docs/56
     "기만적 동의" 선례라 회귀가 아님에도 Task 10 스코프로 픽스(`a1f6247`) —
     `'[<app> 창 #<windowId>] <tool> 실행할까요?'` 브랜치 추가 + close_window
     폴백 문구 불변 유지(프로브가 양쪽 모두 정적 단언).
- **모달 다이얼로그 ≠ 앱** (사용자 지정 설계 노트, §11 후속으로 이월):
  filedlg 음성 내비게이션 스펙을 위한 핵심 구분 — filedlg는 런치-살이 앱이
  아니라 쿼리에 묶인 수명이다. §11 참조.

## 1. 문제 정의

에이전트가 앱을 다루는 현재 수단은 창 단위 조작(list_windows/focus_window/
close_window)과 send_input류 주입뿐 — "vplayer에서 30초로 시크해 줘" 같은
**앱 내부 기능**은 말로 할 수 없다. 주입은 앱별 좌표/UI 구조 지식이 에이전트에
있어야 해 확장 불가. 앱이 런치 시 자기 도구를 선언하면 에이전트가 네이티브로
부르는 허브가 필요하다. 선례 조사(스펙 §0): ①앱 내장 MCP 서버 ②앱 플러그인
소켓+브리지 프로세스 ③외부 자동화 감싸기 — 본 설계는 ②형태이되 앱↔브리지
채널로 새 소켓 대신 기존 창 서버 파이프, 브리지 역할을 기존 jkagentd 브로커가
겸한다.

## 2. 아키텍처 총관

```
claude ──MCP── jkagentd(브로커) ──파이프── JKWindowServer(게이트·레지스트리)
                                              │ AgentToolCall
                                              ▼
                                          앱 (vplayer …)
```

- 등록: 앱 → 서버 (`AgentToolRegister`) — 연결 수명에 묶임
- 발견: 브로커가 tools/list 시점에 서버 `list_app_tools` 질의 → MCP 도구
  `<app>_<tool>` 합성
- 호출: claude → MCP tools/call `vplayer_seek` → 브로커 역매칭 → 서버 도구
  `app_tool` 원 요청 → 3단 게이트(allow/ask 파킹/deny) → `AgentToolCall`
  중계 → 앱 실행 → `AgentToolResult` → reqId 상관관계로 원 요청자 reply
- **args는 원문 패스스루**(jkbridge generic relay 선례) — 서버는 앱 도구
  args를 검증하지 않는다. 앱이 스키마를 선언하고 자기가 검증한다. 이래야 앱
  도구 추가에 서버/브리지 수정이 0.

## 3. 와이어 — MsgType 22/23/24 (`07060e9..3443314`)

`engine/include/ipc/JKWireProtocol.h` + `.cpp`:

| MsgType | 번호 | 방향 | 페이로드 |
|---|---|---|---|
| `AgentToolRegister` | 22 | C→S | `{jsonLen}` + JSON `{app, tools:[{name, description, inputSchema}]}` |
| `AgentToolCall` | 23 | S→C | `{reqId, jsonLen}` + JSON `{app, tool, args}` |
| `AgentToolResult` | 24 | C→S | AgentReplyHeader 레이아웃 재사용 — queryId 자리 = reqId, ok 플래그, jsonLen + JSON |

- 등록 ack: 새 S→C 형식 없이 **AgentReply writer 재사용, queryId=0 고정** —
  등록 성공/거부 사유(`{"ok":false,"error":"bad_name"}` 등)를 본문으로.
  모든 기존 앱의 `PollAgentReply` 소비자(id 스캔, nextQueryId_=1 시작)가
  queryId=0과 충돌하지 않는 것을 Task 4에서 9개 소비자 전수 확인(무해 폐기).
- `WriteAgentToolCall/ReadAgentToolCall` 등 writer/reader 4쌍 — 0x7FFFFFFF
  json 가드 + 절단 검사. 신규 판독기의 잘림 검사는 `<`, 기존 `ReadAgentJson`
  은 `!=` (deferred minor — 후속 무해 입증 시 종결).
- `JKAgentJson`에 `GetArrRaw(key, idx, field, out)` 추가 — inputSchema 원문
  인출(객체는 객체로 통과)용, GetArrStr/GetObjRaw의 형제.

## 4. 서버 — 레지스트리 + 3단 게이트 (`3443314..3311995`)

### 4.1 레지스트리 (`b97ef89`)

- `JKWindowServer.h`: `AppToolDef`(name ≤32/desc ≤512B/inputSchema ≤2KiB
  원문) + `AppToolManifest`(connId/app/windowId/title/tools) +
  `InflightAppTool`(reqId/queryId/requesterConnId/targetConnId/windowId/
  expiresAt — 레슨 33 id 저장) + `appToolManifests_`(connId→매니페스트) +
  `inflightAppTools_`(reqId→중계) + `nextToolReqId_`.
- **연결 수명에 묶임** — `CleanupDisconnectedClients`에서 매니페스트 소멸 +
  inflight 전량 `tool_gone` 회송 + `agent.app_tools_changed` publish
  (events_list 카탈로그 1행 등록 — 신규 토픽 즉시 카탈로그화 레슨 준수).
  별도 언레지스터 메시지 없음.
- 등록 검증: `shell_denied`(셸 특권 연결 봉쇄) → `bad_app`
  (`^[a-z][a-z0-9_]{0,15}$`, ValidAppToolToken 수기 검증) → `too_many_tools`
  (≤32) → `namespace_conflict`(코어 도구명+예약 접두) → 도구별
  `bad_name`/`schema_too_large`. 거부는 등록 ack 본문 사유로.
- 락 규약(레슨 35): 핸들러 4종 전부 clientsMutex_ 보유 경로에서 호출, 재락 0.

### 4.2 list_app_tools + app_tool 중계 (`f8d5003`)

- **`list_app_tools {}`** — 평면 행 카탈로그 `{app, name, description,
  inputSchema(원문, 부재 시 {}), windowId, title, connId}`. 중첩 배열 접근
  없는 리더(레슨 39)라 앱별 중첩이 아니라 평면 행 — 브로커 tools/list 합성과
  agentctl face 양쪽이 그대로 먹는다. kPermMatrix
  `{"list_app_tools","none","allow"}`.
- **`app_tool {app, tool, args, windowId?}`** — 중계. args `GetObjRaw`
  원문(>8KiB `args_too_large`). 후보 수집 = 등록된 (app,tool) 조합
  역매칭 + windowId 지정 시 필터(§4.2 룰링으로 다중 인스턴스 전제 성립).
  미지정+복수 = `ambiguous` + `candidates:[{windowId,title}]` — 묵시적
  추측 라우팅 금지. **windowId는 app_tool 호출 인자 레벨**(app/tool 옆)에서
  읽고, 앱 페이로드는 중첩 `"args"` 패스스루 — 프로브 1차 런이 실측으로
  확정한 위치(스펙 §4.2 계약, 서버 결함 아님).
- **ask 파킹 = close_window ask 기계 복제**: 구독자 체크(`approval_unavailable`)
  → `PendingApproval` push(kind="app_tool" + appToolApp/Tool/Args/ConnId
  4필드) → `agent.approval_request` 방송 → replied=false. 버퍼 640→1024
  (title 잘림의 JSON 파열 변형 완화 — 후에 Task 9에서 2048로 재상향).
- **resolve = 파킹-응답형**: allow 시 **재실행형(files_access)이 아니라**
  승인 시점에 앱 생존 재조회(매니페스트+연결 생존, 실패 시 `tool_gone`
  즉답) → inflight 엔트리 생성 + `WriteAgentToolCall` + `deferred` 플래그로
  reply를 HandleToolResult에 넘김. **타이머는 resolve 후 시작**(expiresAt
  설정은 allow 중계 시점 + resolve 경로 — 승인 대기로 10s 오발 방지, 스펙 §9
  시퀀싱). brief의 "resolve 추가 분기 불필요" 전제는 즉응답형에만 성립 —
  kind별 if-else 사슬이라 분기 1개 + deferred 플래그로 최소 확장.
- **중계 만료 스캔**(ProcessPendingMessages, 10s): `tool_timeout` 회송+소거.
- `askCapable`에 `app_tool` 편입 — 파일값 "ask"가 Allow로 열화하지 않게
  (docs/54 §11, files 2종 선례).
- **`AppToolAllowed` 3단**: `app_tool.<app>.<tool>` > `app_tool.<app>` >
  `app_tool`, 파일 부재/키 부재/파서 실패 폴백 = **allow 기본**. 평면 점 키
  리터럴(`{"app_tool.vplayer.seek":"ask"}`)이 동작하는 이유 — AgentJson
  GetStr이 JS_GetPropertyStr 한 단계라 점 포함 프로퍼티 키는 읽힌다. 브로커
  (Task 7)와 동일 순서/규약. deny = `{"ok":false,"error":"denied"}`.
- **self-approve 게이트 자동 적용**: 기존 게이트(requesterId == 승인자 연결
  && kind != close_window → self_approve)가 app_tool에도 그대로 — 자기
  app_tool 파킹은 자기가 승인 못 한다(M1 룰, docs/31 §3 예외 확장 안 함).

### 4.3 실행 중 정정 — 다중 인스턴스 (`3311995`, 스펙 `2b28988`)

§0 룰링 2. `HandleToolRegister`의 타 연결 app 충돌 체크 루프 삭제(코어명
충돌 체크는 유지). `list_app_tools`는 connId 맵 전체 순회라 다중 인스턴스 =
행 N개(브로커는 이름 유니온 dedupe, agentctl face는 전 인스턴스 표기), 연결
정리는 connId 기준이라 무충돌 — 중계/카탈로그/정리 코드는 변경 0(후보 벡터
수집이 이미 대응 구조였음).

## 5. 클라 수신 공통화 (`3311995..f1417c4`)

### 5.1 jkclient 공통 경로 (`30ef72d`)

- `JKClientSurface`: `AgentToolDecl`/`AgentToolCallMsg` 구조체 +
  `SendAgentToolRegister(app, tools)` / `PollToolCall(out)` /
  `SendAgentToolResult(reqId, ok, resultJson)`. ReadLoop에 AgentToolCall
  분기 — `pendingToolCalls_` 바운드 deque(64, drop-newest)에 적재, read
  스레드는 main-thread 작업에 절대 블록하지 않는다.
- `JKClientApplication::OnAgentToolCall(tool, argsJson, resultJson)` 가상
  훅 — 코어 Run() 스윕이 PollToolCall을 배출해 훅을 부르고 결과 전송까지
  처리(**앱은 reqId를 만지지 않는다**). base 구현은
  `{"error":"unsupported_tool"}` false 반환 — 미등록/늦은 해제 앱도 답해서
  inflight가 서버 타임아웃까지 끌려가지 않게 하는 방어.
- 등록 타이밍: `JKClientApplication::Init`은 Connect → 구독 → **OnInit** 순서라
  OnInit 끝에서 등록하면 항상 연결 후 — "연결 전 false(영구 미등록)" 경로
  도달 불가. 재접속 경로도 없어 1회 등록으로 충족(서버 재시작은 매니페스트
  connId 스테일 → `unknown_app_tool` = §9 수명 설계).
- **1프레임 지연 문서화**: AgentToolCall 수신에 QueueInputEvent wake 부재 —
  Run() 프레임 스윕 폴링이라 최대 1프레임 지연(현 구조의 계약, 문서화).

### 5.2 vplayer 등록 6종 + 핸들러 (`f1417c4`)

`ClientVPlayerApp` — 등록 표 6종 + OnInit 끝 등록 + OnAgentToolCall 오버라이드
(~100줄 수준, 새 재생 로직 0):

| 도구 | 인자 | 동작 |
|---|---|---|
| `open` | `{path}` | 비동기 오픈(BeginOpen 워커 스폰) — 즉시 accepted, 진행은 get_status |
| `play_pause` | `{}` | 토글 |
| `seek` | `{seconds int}` | 절대 시각 시크 |
| `set_volume` | `{percent int 0..100}` | 코어 SetVolume 0..1 변환 |
| `set_av_delay` | `{seconds int -1..1}` | 코어 클램프 ±1 |
| `get_status` | `{}` | `{opened, opening, openFailed, paused, ended, pos, dur, volume, error}` |

- 전 도구 프레임 스레드 + UI가 쓰는 동일 PlayerCore 세터 경로 — 새 락 0.
- **brief 결함 1건 픽스**: `!player_ → no_player` 가드가 `open` 분기 위에
  있으면 새 인스턴스가 open을 못 받는다(이것이 유일한 player_ 생성 경로) —
  open 분기를 가드 위로 이동. 실측 확인(open → opened:true).
- 정수 인자 검증은 앱이: 범위 밖 → `{"error":"bad_args","need":...}` false
  (패스스루 계약). AgentJson GetInt의 실수 강제 완화(0.5→0)는 클램프라
  오동작 없음 — 프로브는 정수 인자만 사용(강제 회피, deferred minor).
- 미등록 도구명은 서버 역매칭이 `unknown_app_tool`로 먼저 답하므로 앱의
  `unknown_tool` 분기는 도달 불가 방어선(유지).

## 6a. 브로커 동적 합성 (`8283ed8..b9a98ca`)

`engine/tools/jkagentd/main.cpp`:

- **`ComposeToolsListJson`**: `kToolsListJson` → `kCoreToolsListJson`(정적부 +
  코어 2종 `list_app_tools`/`app_tool`) + tools/list 시점에
  `list_app_tools` QueryRaw → 동적 행 접합. MCP 도구명 `<app>_<tool>`,
  description 앞 `[<app>]` 접두, inputSchema 원문, `std::set` 이름 유니온
  (다중 인스턴스 = 동명 1개 — 인스턴스 변별은 서버). tail splice는
  `core.rfind("]}")` + npos 가드. **서버 질의 실패/사망 = 정적부만 반환** —
  브로커 사망이 아니라 앱 도구만 잠깐 안 보이는 수준의 안전 폴백.
- **충돌 가드 3종(스킵 + stderr 진단 1행)**: ①동적명이 코어 도구명 충돌
  (app="files", tool="list" → `files_list` — 서버 namespace_conflict는 app
  자체만 비교하므로 복합명은 못 잡는다) ②malformed inputSchema ③**복합명
  충돌** = 서로 다른 (app,tool) 쌍이 같은 합성명(`coll_a`+`b` vs
  `coll`+`a_b`) — 임의 선택 금지, tools/list에서 제외.
- **`ResolveAppTool`**: 미지명 tools/call마다 실시간 역매칭(`app+"_"+tool ==
  mcpName` 전 행 검사) — 접두 추측 금지(이름에 `_` 포함 가능). 유니크일 때만
  게이트·중계 진행. 복합명 충돌 = `ambiguous_tool` + candidates (서버 §4.2
  거울). 서버 부재 = `unknown_tool` 즉답(행블록 없음).
- **브로커 게이트 `BrokerAppToolAllowed`**: 서버와 동일 3단·동일 폴백
  (키 부재 = allow 명시 — "브로커 키 누락=deny" 레슨 재발 방지), 정확한
  allow/ask/deny만 인정(그 외 값은 폴백 — 픽스 라운드에서 서버와 바이트 동일화).
  deny만 선차단, ask는 서버 파킹 파이프라인으로 통과(브로커에는 승인 UI가
  없다). permissions.json 핫리드(호출마다).
- **에러 매핑 셰이프 재작성 없음** — 중계 응답은 원문을 content[0].text에
  싣는다. receipts도 동적 경로 기록(기존 관례).
- 폰 브리지(jkbridge)는 generic relay라 **중계 경로 무수정** — 확장 프리미스
  실증 (probe_jkbridge에 `bridge-app-tool-strip` 체크로 회귀 가드). 승인
  경로만 후속 픽스(아래 §6b).
- **레슨(docs/51 재발)**: 새 서버 도구 추가 시 브로커 4곳 — tools/list, IsKnownTool,
  LoadPermissions(kNames), args-rebuild 분기. 이번에도 코어 2종 직접 호출
  분기가 필요했다.

### 6b. 최종리뷰 픽스 — jkbridge 별도 승인 연결 (최종리뷰 Important 2)

- **결함**: 폰의 approve가 tools/call 중계와 **같은 세션 파이프 연결**로 나갔다
  → ask 게이트 앱 도구의 파킹 requester가 곧 jkbridge 연결이고, 폰 Allow 탭의
  approve가 서버 self-approve 게이트(`requesterId == client.Id()`, kind !=
  close_window → `self_approve`, docs/31 §3)에 막혔다. "폰에서 앱을 말로
  다룬다"의 표면 기능이 fail-closed로 죽어 있었다.
- **픽스**: BridgeSession에 승인 전용 **제2 제어 연결**(`approve_` +
  approveMtx_ — Hello+subscribe=0 선언, 이벤트 구독 없음, 펌프 미사용;
  jkchat 크로스 승인 선례). approve 프레임만 이 연결로 **블로킹 Query** —
  승인 도구는 승인자에 대해서는 파킹 없이 즉답(지연 응답은 원 요청자 쪽만)이라
  블로킹이 안전. 연결 실패는 `{"type":"error","text":"approval connection
  failed"}` 즉답 — fail-closed 동일. 세션 연결의 SendQuery+labels 경로는
  중계 원본 그대로(레슨: pump 재접속 agentMtx_ 규약 건드리지 않음).
- **회귀 고정**: probe_jkbridge §6b — 가짜 앱(probeapp.echo) 등록 → ask 게이트
  → 폰 tools/call(세션 연결이 파킹 소유) → 폰 approve → `approved:true` ack +
  앱 AgentToolCall 수신 + 파킹 결과가 라벨 reply("at")로 폰 도착, 4체크
  e2e. 픽스 전이라면 approve가 `self_approve`로 즉답해 이 체크는 반드시
  실패한다(체크가 픽스를 변별).

## 7. 승인 시각화 3단 (`b9a98ca..8237f52`, `8237f52..fb0d68a`)

### 7.1 1단 — 대상 창 하이라이트 (`57ce670`)

- **훅 배치(placement rationale)**: DrawCloseOverlay는 JKWindowServer가 아니라
  `JKCompositor::Composite` 안(레이어 루프, layersMutex_ 보유)에 있다. 하이라이트를
  같은 컴포지트 패스에 넣으려면 **`SetOverlayHook`**(컴포지터에 `OverlayHook =
  function<void(float outputScale)>` 추가)가 유일한 동일패스 진입점이다:
  ①Composite() **리턴 후** 그리기는 present가 이미 끝난 뒤라 백버퍼에 그린
  것이 다음 프레임 clear에 지워져 영구 소실 — 배치 불가 ②레이어 루프 **내부**
  호출은 훅이 FindLayerById로 layersMutex_ 재획득 → std::mutex 교착 — 기각.
  그래서 호출점 = 레이어 루프 스코프 밖 + present 전(이번 프레임에 바로 화면).
  `Composite(false)`(캡처 리드백)에서도 그린다 — "보이는 것이 곧 캡처다".
  의존성 역전: 컴포지터는 pendingApprovals_를 모른다.
- **스레드 안전**: pendingApprovals_ 접근 19곳 전부 서버 루프 스레드
  (ProcessPendingMessages→ProcessClientMessage→HandleAgentQuery) — 훅은 같은
  스레드의 컴포지트 패스에서 읽으므로 새 락 0. 락프리 규약은 헤더 멤버 주석에
  명문화.
- **DrawApprovalHighlights**: `targetId != 0`만 수집(같은 창 복수 파킹 =
  1회 드로잉), 면제 = `IsShell() || Title() == kCaptureOverlayTitle`(close
  게이트와 동일). **전체화면 레이어는 일부러 면제 안 함** — 전체화면 앱에
  대한 승인이야말로 눈으로 대상을 확인해야 할 때고, 배너는 크롬이 아니라
  승인 알림이라 "전체화면 레이어는 앱이 상단 스트립을 소유" 규칙에 걸리지
  않는다(면제 시 60s간 파킹 요청 완전 비가시 — 리뷰 승인). 호박색
  (230,140,40) 3px 링 + 상단 밴드 + "에이전트 승인 대기: <app>.<tool>".
- **텍스트 경로**: 크롬 타이틀과 동일 글리프 경로 재사용 — 단 서버는 텍스트를
  그린 적이 없으므로 JKDC+HangulManager를 서버에 붙임(jkserver는 jkcore에 이미
  링크 — 새 링크 의존 0). ~1kHz 컴포짓 루프에서 per-frame 글리프 불가 →
  SDL_TEXTUREACCESS_TARGET 텍스처 캐시, **그 프레임에 쓰인 키만 남기는
  프레임별 프룬** — 키 소멸이 곧 텍스처 해제, 별도 타이머 없음.
- **기능 일반화**: 승인 파이프라인 자체의 기능이라 app_tool뿐 아니라
  targetId를 갖는 모든 ask(close_window 등)에 적용 — 모든 ask 흐름의
  최상위 확인 레이어. close_window는 name이 없어 대상 창 제목으로 표기
  (라이브 스모크: "…: Test Window").
- 수용한 순간 비용: 배너가 파킹 중 대상 창의 서버 그린 close X를 시각적으로
  가림(히트존 생존, 최대 60s) — 승인 대기 중 닫기 유도가 아니므로 수용.

### 7.2 2단 — 스트립 정보 강화 (`fb0d68a`)

- `agent.approval_request` 페이로드에 app_tool 분기 전용
  `target:{app,tool,windowId,title}` + `thumb` 필드. **`target_id`는 유지** —
  Task 8 하이라이트 소비자(브리지:452 폴백) 하위호환. 다른 kind의 페이로드는
  무수정. 버퍼 1024→2048(잘림 위험 완화 — 2048B 이벤트 버퍼 자체는 기존부터의
  상한, deferred minor).
- jkchat 스트립: target 존재 → `"[<app> 창 #<id>] <tool> 실행할까요?"`
  (docs/56 레슨 — close_window "닫을까요" 재용 금지, 대상 정직 표기) / 구버전
  서버(target 부재) → `"[앱 도구] <name> 실행할까요?"` 하위호환.
- jkbridge도 동일 문구(`a1f6247`) — 폰과 데스크탑이 같은 정직 문구.

### 7.3 3단 — 조작 전 썸네일 (`fb0d68a`)

- `CaptureLayerToPng(connId, path)` — capture_window 본문의 readback→WritePng
  경로 추출(동작 불변, `window_not_found` vs `write_failed` 문구 보존).
  **창 클라의 windowId == connId**(HandleToolRegister가
  `IsControlOnly() ? 0 : client.Id()`로 등록, 컴포지터 레이어 id = 연결 id)
  라서 헬퍼 시그니처(connId) 그대로 올바른 창을 잡는다.
- 파킹 시점 캡처(서버 루프 스레드 — capture_window와 동일 비용 규약, 스레드
  추가 없음) → `state\screenshots\approval_<ts>_<req>.png`(StateDir 절대경로
  — capture_window reply와 같은 규약, cwd 무관). 가려져도 shm 표면 직독이라
  항상 마지막 커밋 프레임이 나온다. 캡처 실패 = `thumb` 필드 생략만, 승인은
  계속(비치명 — windowId 0 제어 연결 매니페스트가 자연 실패 경로).
- 승인 거부/만료 후에도 파일 잔존 — shot_*와 같은 state/screenshots 수명
  규약. UI 소비(알림 센터 확장 카드)는 후속 스펙.

## 8. 오류 표면 표 (as-built)

| 표면 | 사유 | 발신자 |
|---|---|---|
| `shell_denied` | 셸 특권 연결의 등록 | 서버 등록 ack |
| `bad_app` / `bad_name` | 네임스페이스/도구명 토큰 검증 | 〃 |
| `too_many_tools` | 연결당 도구 >32 | 〃 |
| `schema_too_large` | inputSchema >2KiB | 〃 |
| `namespace_conflict` | 코어 도구명/예약 접두 충돌 | 〃 |
| `args_too_large` | args >8KiB | 서버 app_tool |
| `unknown_app_tool` | 미등록 조합/연결 사망(정리 대기) | 〃 |
| `ambiguous` + `candidates:[{windowId,title}]` | 미지정+복수 후보 | 〃 |
| `denied` | 3단 게이트 deny | 〃 |
| `approval_unavailable` | ask인데 구독자 없음 | 〃 |
| `self_approve` | 자기 파킹 자기 승인(close_window만 예외) | 승인 도구 |
| `denied_by_user` | ask 파킹 거부 | 〃 |
| `tool_timeout` | 중계 후 10s 무응답 | 서버 만료 스캔 |
| `tool_gone` | 중계 대기 중/승인 시점에 앱 종료 | 연결 정리·resolve |
| `result_too_large` | 앱 결과 >16KiB | 서버 HandleToolResult |
| `bad_args` | 앱 측 인자 검증(패스스루 계약) | 앱 |
| `unknown_tool` | 브로커가 모르는 동적명(서버 부재 포함) | 브로커 |
| `ambiguous_tool` + `candidates:[{app,tool}...]` | 복합명 충돌(브로커 합성명) | 브로커 |

**응답 형태 as-built 주의**: 앱이 보고한 실패(ok==0)도 본문 리터럴은
`{"ok":true,...,"error":<앱 json>}` — ok 플래그는 와이어 헤더에만 실린다.
소비자는 `error` 멤버 존재로 분기한다(프로브 c3 실측 확정).

## 9. 프로브와 회귀 결과 (`c3e4d50`..`41f1cfc`)

- **probe_app_tools.ps1** (스테이지 1→2→3 누적, 59체크) ×2 연속 ALL PASS —
  카탈로그 6행/seek e2e(pos 실측)/오류 표면/**2인스턴스 ambiguous+candidates**/
  자동 정리(agent.app_tools_changed 구독 먼저 수신)/등록 거부 3종/deny/ask
  파킹+크로스 승인+파킹 reply/tool_timeout(9.2s, 하한 3s 체크)/MCP 동적
  합성+호출+receipt/서버 부재 정적 폴백/복합명 ambiguous_tool/스테이지 3
  (c15 하이라이트 중첩+c16 target·thumb PNG 매직+c17 jkchat 실UI WM_GETTEXT
  정확 일치).
- **probe_app_tools_highlight.ps1** ×2 ALL PASS — 픽셀 실측(밴드 호박 비율
  0.978, 링 열 스캔 1.0, 다크 글리프 >20, 해소 후 0). BGRA/RGBA 채널 오타 +
  3px 스트로크 vs CopyFromScreen 1px 원점 오차 2건 프로브 자체 결함 실측 픽스.
- **probe_jkbridge 24체크** PASS(신규 `bridge-app-tool-strip` — 서빙 JS 정적
  단언: 브라우저 렌더링은 와이어로 못 보므로 서빙 바이트가 최대 증거).
- **전체 회귀 스윕 16런 전부 exit 0**(sweep10_*.log): probe_app_tools ×2,
  highlight, agent_chat, agent_trust, jkbridge, vpt4/vpt9/vpt5_e2e/vpt11/
  vpt5_avsync, settings, notes, files, selftest 0, jkdesktop test 0 failures.
  승인 파이프라인 일반화(하이라이트)라 agent_chat/agent_trust 회귀 필수였다.
- 첫 공식런 c4 실패 1건 = 제2 vplayer 스폰-등록 레이스(무변경 코드, 인프라
  플래키 — 이후 2연속+3런 c14 전부 PASS).

### 9.1 permissions.json 사고와 복구 (사용자 상태 안전)

- **사고**: dev 런 1·2가 프레임 읽기 정지로 timeout 강제 종료되면서
  finally(복원)가 실행되지 않았고, c4가 쓴 `{"close_window":"allow"}`가
  남았다. 다음 dev 런이 그 잔여물을 "사용자 상태"로 백업(고정 파일명)해
  **사용자의 원래 permissions.json 백업이 소실** — 강제 종료가 백업 사슬을
  오염시킨 사고.
- **복구**: 당일 세션 트랜스크립트의 `agent_permissions` 전면 목록(유일한
  권위 소스)으로 원본 키 집합을 특정해 6키 재구성 → 이후 permission_set
  포함 7키 확정. 프로브 종료 시점 바이트 동일 보존 실측(164B, UTF-8 무 BOM).
- **재발 방지(프로브에 반영)**: ①백업 파일명 **PID별 유니크** — 강제 종료된
  이전 런의 잔여물이 다음 런의 백업을 덮지 않음 ②**stale-residue 가드** —
  TEMP의 `perm_pre_apptools_*` 잔여물 검사, 존재하면 `FAIL: setup-stale-residue`
  + **exit 1, 프로브 미실행**(네거티브 테스트 실측) ③기준선 명시 — 기동
  백업 직후 Clear-Perms로 c1-c3가 유저 라이브 파일이 아니라 부재 기준선에서
  실행됨을 코멘트와 동작 일치.
- **상시 위협(픽스 안 함, 문서화)**: `probe_agent_chat.ps1`이 종료 시
  permissions.json을 **무조건 Remove-Item** — 파일 부재 상태로 두는 결함.
  부재 파일 = close_window 기본 deny(M1 룰)로 채팅 닫기 전면 실패하는
  2026-09-19 실사고와 동일 부류. 다른 프로브의 기존 결함이라 이 플랜에서
  고치지 않았고 **후속 소각 과제**로 추적(§11). 프로브 관례 자체
  (as-found 복원)는 유지 — 아무 프로브도 사용자 상태를 조작해 재구성해선
  안 된다.

## 10. 레슨

1. **"유일한 진입점"을 먼저 실측하라** — 하이라이트 배치에서 브리프가 가리킨
   "DrawCloseOverlay 옆"은 JKWindowServer에 없었다(컴포지터 소재). present
   후 그리기가 영구 소실되는 이유까지 실측으로 확정하고 훅 설계로 귀결.
   락 재획득 교착(layersMutex_)은 호출점 주석에 고정.
2. **보안 검사는 상관관계 뒤가 아니라 앞-이-아니다** — 크기 상한 같은
   "무해해 보이는" 검사도 신원 확인보다 먼저 있으면 제3자 주입 경로가 된다
   (Task 2 Important 1).
3. **스펙 절 간 모순은 도달 불가능성 테스트로 발견된다** — "ambiguous 프로브가
   통과하려면 등록 2인스턴스가 필요한데 등록이 막힌다"는 concern 4가 §4.1
   원문 결함을 잡았다. 프로브 체크가 스펙 문구를 역검증한 사례.
4. **사용자 상태 파일 백업은 강제 종료에도 안전해야 한다** — PID 유니크
   백업명 + 잔여물 fail-fast 가드 + 기준선 명시 3종 세트. 그리고 "무조건
   Remove-Item"하는 구세대 프로브가 살아 있는 한 사용자 permissions.json은
   언제든 사라질 수 있다(§9.1).
5. **한국어 코드포인트를 수기 계산하지 마라** — c17 1차 실패의 원인은
   jkchat이 아니라 프로브의 수기 코드포인트 오류(창 0xCC28→실제 U+CC3D, 앱
   0xC575→U+C571). WM_GETTEXT 코드포인트 덤프 진단으로 jkchat 렌더링 정확
   확인 후 교정. **python `ord()` 실측 필수.**
6. **원시 파이프 클라 함정(PS5.1)**: ReadAsync 버림 읽기(타임아웃으로 버려진
   pending read가 파이프에 잔존 → 다음 ReadAsync 무기한 블록) — PeekNamedPipe
   P/Invoke 폴링 후 가용할 때만 동기 Read. acceptor는 Hello 뒤 CreateSurface
   또는 AgentEventSubscribe만 받는다(AgentToolRegister를 2번째로 보내면
   "expected CreateSurface"로 파기). AgentEvent/AgentToolRegister 헤더는
   4바이트 — 12바이트 고정 skip은 정규식 우연 매칭으로 버틴 것.
7. **app_tool의 windowId는 호출 인자 레벨**(app/tool 옆), 앱 args는 중첩
   `"args"` 패스스루 — 첫 dev 런이 실측으로 확정한 위치.
8. **복합명 충돌은 서버가 못 잡는다** — namespace_conflict는 app 자체만
   비교. 브로커 합성명 계층의 충돌(files_list / coll_a_b)은 브로커가
   2-패스로 자기 검출해야 한다(1-패스는 최초 행을 이미 방출한 뒤 충돌을
   알게 되는 회귀 — 픽스 라운드 내 첫 런 c14가 포착).
9. **기능 일반화의 대가는 회귀 대상 확대** — 하이라이트를 승인 파이프라인
   기능으로 만들자 close_window 등 기존 ask 전체가 새 드로잉 경로를 탄다.
   agent_chat/agent_trust 회귀 필수 판정이 맞았다.

## 11. 후속 / 범위 밖

- **filedlg 음성 내비게이션 (다음 스펙, 사용자 확정 2026-09-19)** — 폰에서
  파일 대화상자를 다룰 수 없다는 갭(file_open의 파킹+스폰+result 회수는
  있으나 다이얼로그 내부 조작 도구가 전혀 없다). 설계 방향: filedlg가 이번
  허브(AgentToolRegister)로 자기 도구 등록 — `navigate{key: up|down|pgup|
  pgdn|parent|select}` / `list{}` / `choose{}`. jkbridge 제네릭 릴레이 덕에
  **브리지 무수정으로 폰 노출**. 한계 수용: 폰엔 다이얼로그가 비가시라 list
  결과를 에이전트가 읽어주는 형태.
  **모달 다이얼로그 ≠ 앱 설계 노트(사용자 2026-09-19 지정)** — filedlg는
  런치-살이 앱이 아니라 **쿼리에 묶인 수명**(파킹에서 태어나 선택/취소/600s
  만료로 사망 → 등록/해제 반복)이다: ①choose는 앱 동작이 아니라 **파킹된
  file_open의 해소자** — docs/48 file_open_result의 requesterConnId 에코
  상관관계로만 해소 ②동시성 1-in-flight라 허브의 ambiguous 로직이 무의미 —
  유일 위험은 **스테일 다이얼로그**: 도구 호출 시점에 슬롯 소유자 재검증
  (docs/48 MAJOR 교차결함 동류), 죽으면 tool_gone ③카탈로그에서 모달 표기/
  네임스페이스 분리는 스펙 단계의 결정 포인트.
- **썸네일 소비 UI**(알림 센터 확장 카드) — 후속 스펙.
- **permission_set의 3단 키 편집 확장** — 파일 편집이 승인 행위인 기존
  계약 유지.
- **probe_agent_chat의 무조건 permissions.json Remove-Item** — 상시 위협
  후속 소각(§9.1). as-found 복원 관례로 정렬 권장. TEMP에 2026-09-18자
  구세대 perm 백업 잔여 3개(perm_pre_files/notes/set.json, 내용 {})도
  잔여-수용 사고 클래스의 퇴비 — 정리 검토.
- **콘솔 앱의 도구 등록 채택** — 프로토콜은 전 클라에게 열어둠. 현재 콘솔
  앱 노출은 claude skill(문서형) 래퍼(§0 결정 6): 샘플
  `engine/templates/console-app/SKILL.md`(sampletodo 사용 계약 —
  run_console_app ask 게이트/terminal.output·read_events/terminal_exec/
  jkctl 3종). **배치**: chat LLM worker cwd는 `state\chat.json`의
  `directory` 키(JKLlmEngine::LoadChatConfig)가 결정 — 그 디렉터리의
  `.claude\skills\console-app-sampletodo\SKILL.md` 또는 유저 레벨
  `~\.claude\skills\`. 엔진 코드 0줄.
- **vplayer 이외 앱**(mine/browser 등) 도구 세트 — 각 앱 착수 시 별도.
- **deferred minors 원장** — inputSchema 정합성 미검증(불량 스키마 1개가
  tools/list 스킵으로 방어되나 서버 등록은 원문 임베드)/매니페스트 내 중복
  도구명 허용/resolve 시점 도구 존재·deny 재검사 부재(파킹-응답형 계약
  문서화)/windowId 0 명시 지정=미지정 취급 경계/denied 응답 windowId 에코
  누락/AgentJson GetInt 강제 완화/등록 ack 무음(파이프 오류 시 로그 1줄
  경화 후보)/pendingToolCalls_ 64 도달 시 drop-newest/훅의 tool 에코
  무이스케이프(서버 ValidAppToolToken이 현재 방어)/targets.empty() 조기
  리턴 시 캐시 프룬 스킵(1텍스처 자가치유)/다중 파킹 동일창 1이름 표기/
  2048B 이벤트 버퍼 상한/clip 폴백 부재/tool_gone 음성검증 부재 등 —
  전량 `.superpowers/sdd/2026-09-19-app-tool-hub/progress.md` 원장 참조.
