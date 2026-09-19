# filedlg 음성 내비게이션 설계 (file dialog voice navigation) — 2026-09-19

상위: docs/58 §11 후속 / 사용자 확정(2026-09-19 "넵"). 앱 도구 허브
(docs/58, 스펙 `2026-09-19-app-tool-hub-design.md`) 위에 얹는 두 번째 소비자.

## 0. 원천과 결정

- **요구**: "열기 대화상자에서 파일 리스트 위/아래, pgdn/pgup, 상위 폴더,
  선택이 말로 되어야 리모트 구동" — 폰(jkbridge)에서 file_open이 띄운
  다이얼로그를 에이전트가 대신 조작한다. 사용자 지정 캐치: **"앱과 모달
  다이얼로그는 차이가 있다. 그 부분이 캐치됐으면 좋겠다"** — 모달성을
  프로토콜에 구조적으로 반영(§4-5), 부록 코멘트가 아님.
- **결정 1 — 도구 주체는 filedlg 앱 자체**: 허브의 AgentToolRegister 경로
  그대로. 브리지/브로커 제네릭 릴레이 덕에 폰 노출에 jkbridge 수정 0.
- **결정 2 — 등록 시점은 params 수락 직후**(PumpReplies 성공 경로).
  이유: 도구 가시성 == 슬롯 소유 불변식을 처음부터 성립시킨다(§5 가드가
  등록 직후 공백창에서 tool_gone 오판하지 않게). 수동 `--client filedlg`
  실행(슬롯 없음)은 params가 안 오므로 도구 미등록 — 슬롯 없는 choose는
  무의미하므로 정합. 트레이드오프: 디버그 수동 기동에서 음성 도구 부재.
- **결정 3 — 모달 플래그를 와이어에 추가**(선택 필드, 기본 false):
  AgentToolRegister JSON `"modal":true` → `AppToolManifest.modal` →
  `list_app_tools` 행에 `"modal":true`. 스폰-살이 앱(vplayer)과 쿼리-수명
  모달(filedlg)의 구분을 카탈로그 소비자가 기계적으로 알게 한다.
- **결정 4 — 스테일 가드는 서버가 슬롯 소유로 판정**(docs/48 MAJOR-1
  교차결함 동류, 앱 자기 판단 아님): `PendingFileDialog.dialogConnId`(신규,
  file_dialog_params 처리 시점 기록) != 매니페스트 connId → 중계 거부
  `tool_gone`. 고아 다이얼로그(슬롯 만료/회수 후 생존)의 도구를 전면 봉쇄.
- **결정 5 — file_open 비동기 모드**(폰 생존성의 핵심): 현재 file_open은
  파킹 응답이 해소 때까지 안 오므로 브로커 QueryRaw가 최대 600s 블록 —
  그 사이 에이전트가 filedlg 도구를 부를 수 없어 음성 흐름이 성립하지
  않는다(설계 노트가 놓친 갭, 본 스펙에서 해소). `args.wait:"event"`(기본
  "reply" = 현행) → 즉시 `{"ok":true,"parked":true}` 회답 + 해소 시점에
  이벤트 `file.open_result {ok, path|error}` 방송. 브로커는 file_open에
  한해 wait:"event"를 주입한다(§7). vplayer 등 창 클라 직호출은 기본
  "reply"라 현행 계약 불변.
- **결정 6 — 예약 접두 "file." 추가**: publish_event의
  kReservedTopicPrefixes에 편입 — file.open_result 스푸핑 봉쇄(docs/54
  NIT-3 window.* 선례). 서버 발행 토픽과 사용자 발행 토픽의 네임스페이스
  분리 원칙 준수.
- **결정 7 — choose와 navigate.select의 역할 분리**: choose는 이름 지정
  즉해소(에이전트 효율: list → choose 2콜 완결), select는 키보드 Enter
  동등(현재 선택지/파일박스 작동). dir에 대한 choose/select는 하강이
  정답(레거시 OnOk 계약) — 결과로 `descended:true`를 돌려 다이얼로그가
  살아있음을 명시.

## 1. 문제 정의

file_open 파이프라인(docs/48): 서버가 쿼리 파킹 → filedlg:<json> 스폰 →
다이얼로그가 file_dialog_params 1회 질의 → 조작은 사람만 → file_open_result
1회 발신. 에이전트가 할 수 있는 건 시작뿐 — 다이얼로그 내부(리스트 열람/
선택 이동/상위 폴더/최종 선택)에 도구가 전혀 없다. 폰에선 다이얼로그가
비가시라 list 결과를 에이전트가 읽어주는 형태가 된다(한계 수용).

## 2. 아키텍처 총관

```
폰/에이전트 ──file_open(wait:event)──▶ 서버: 파킹+즉시 ack
                                        └─ filedlg:<json> 스폰
filedlg 기동 ──params 질의──▶ 서버: 파라미터+requesterConnId 회답
                                 + dialogConnId 기록(슬롯 소유 확정)
filedlg ──AgentToolRegister("filedlg", modal:true, 3종)──▶ 서버 레지스트리
폰/에이전트 ──filedlg_navigate/list/choose──▶ 브로커 ──app_tool 중계──▶
  서버: [모달 가드] 슬롯 소유 검증 ─▶ filedlg ─▶ AgentToolResult
filedlg choose(파일) ──file_open_result──▶ 서버: 파킹 해소
  + file.open_result 이벤트 방송 ─▶ 폰 에이전트가 read_events로 수취
```

## 3. filedlg 도구 3종 (클라 구현)

등록: `SendAgentToolRegister("filedlg", tools)` — 앱명 `filedlg`
(`^[a-z][a-z0-9_]{0,15}$` 통과). 스키마는 MCP inputSchema 원문.

### 3.1 `navigate {key?, index?}`

- `key`: `up|down|pgup|pgdn|parent|select` — `index`(0-based, entries_ 직접
  지정)와 배타 사용 권장. 둘 다 없으면 bad_args, 둘 다 있으면 **index가
  이긴다**(명시 인자 우선, 모호 추측 금지).
- up/down: selectedIdx_ ±1 클램프(0..size-1). pgup/pgdn: 페이지 이동 —
  페이지 크기 = 엔트리 자식 높이 / GetFrameHeightWithSpacing(최소 1)로
  프레임 시점 실측. parent: NavigateUp(). select: OnOk() 동등 —
  파일이면 해소(Finish), 폴더면 하강, 대상 없으면 error.
- 이동 시 레거시 OnSelect 계약 유지: SetFileName 미러(fileBuf 갱신).
- 선택 변경 후 **스크롤 인뷰**: 클리퍼 루프에서 i==selectedIdx_ 항목에
  SetScrollHereY(폴더 더블클릭 유사 위치) — `scrollToSelection_` 플래그로
  다음 프레임 1회.
- 응답: `{"ok":true,"dir":..., "count":N, "index":i, "selected":{"name":...,
  "isDir":bool}, "file":fileBuf_, ...}` — select가 파일을 열었으면
  `{"ok":true,"resolved":true,"path":...}`(choose와 동일 해소 응답),
  폴더 하강이면 `descended:true`.

### 3.2 `list {offset?, limit?}`

- offset 0-based, limit 기본 50 최대 200(초과 요청은 클램프 — 캡 프로브
  함정 방지). 응답: `{"ok":true,"dir":..., "total":N, "offset":o,
  "entries":[{"name":...,"isDir":bool}...], "selected":i|-1,
  "file":fileBuf_, "error":error_|부재}`. entries_ 전체가 아닌 페이지만 —
  수천 엔트리 디렉터리에서 응답 폭주 봉쇄.
- error_ 비어있지 않으면(접근 거부 등) `error` 필드로 전달 — 에이전트가
  상황을 말로 전달할 수 있게.

### 3.3 `choose {name?}`

- name 있으면 해당 엔트리를 fileBuf에 세팅 후 OnOk()와 동일 경로: 폴더면
  하강(`{"ok":true,"descended":true,"dir":...}`), 파일이면 해소.
  name이 entries_에 없으면 `{"ok":true,"error":"no_such_entry"}`(상태
  불변). name 없으면 현재 fileBuf/선택지로 OnOk() — 대상 없으면
  `{"ok":true,"error":"nothing_selected"}`.
- 해소 시 `{"ok":true,"resolved":true,"path":...}` — **직후 Finish 경로**:
  코어가 훅 반환 직후 SendAgentToolResult를 먼저 보내고(JKClientApplication
  Run 스윕 :286→:287, `!running_` 체크 :292 이전) 종료하므로 훅 안에서
  RequestQuit해도 결과 전송 보장. 이 순서는 Task에서 실측 확인.

### 3.4 공통

- 앱 실패도 `{"ok":true,"error":...}` (docs/58 레슨 f — 플래그는 와이어
  헤더). 인자 검증은 앱이(패스스루 계약). 프레임 스레드 처리 — 새 락 0,
  블로킹 I/O 금지(RefreshList는 기존 UI가 쓰던 동일 경로).
- 등록 표:

| 도구 | 인자 | 스키마 스니펫 |
|---|---|---|
| `navigate` | key enum + index int | `{"type":"object","properties":{"key":{"type":"string","enum":["up","down","pgup","pgdn","parent","select"]},"index":{"type":"integer"}}}` |
| `list` | offset/limit int | `{"type":"object","properties":{"offset":{"type":"integer"},"limit":{"type":"integer"}}}` |
| `choose` | name string(선택) | `{"type":"object","properties":{"name":{"type":"string"}}}` |

## 4. 와이어/레지스트리 — modal 플래그

- MsgType 22 AgentToolRegister JSON: 선택 필드 `"modal":true` 추가.
  `HandleToolRegister`가 파싱 → `AppToolManifest.modal`(bool, 기본 false).
  검증 체인 변경 없음(모달은 권한이 아니라 수명/가드 표기).
- `list_app_tools` 행: `"modal":true` 부가(false는 생략 아니고 명시 —
  소비자 파싱 단순화). 브로커 합성(tools/list)은 modal을 소비하지 않음
  (MCP명 규칙 불변) — 스펙 상 명시적 비목표.

## 5. 서버 — 슬롯 소유자 재검증 (모달 가드)

- `PendingFileDialog`에 `uint32_t dialogConnId = 0` 추가.
  `file_dialog_params` 성공 경로에서 `client.Id()` 기록 — 슬롯이 "어떤
  다이얼로그 연결에게 파라미터를 줬는가"의 진실원. 기존 requesterConnId는
  **요청자**(file_open 호출자) 상관관계로 역할 구분 유지.
- app_tool 중계 경로에서 대상 매니페스트 확정 직후:
  `manifest.modal && pendingFileDialog_.dialogConnId != manifest.connId`
  → 중계하지 않고 `tool_gone` 즉답. 대상: filedlg 도구 전종 — 도구는
  "다이얼로그가 슬롯을 소유한 동안만" 존재한다는 모달 계약의 집행.
  - 커버 케이스: ①수동 기동(파킹 슬롯 없음 — params 실패로 애초 미등록이나
    등록 후 슬롯 소멸 경로까지 이중 방어) ②슬롯 만료(600s)/해소 후 살아있는
    고아 다이얼로그 ③새 file_open이 슬롯을 재사용한 뒤 옛 다이얼로그의 도구
    호출 — 옛 다이얼로그는 params 선착순에 밀려 dialogConnId 기록이
    없/불일치 → 봉쇄.
- 정리 경로 불변: 다이얼로그 종료(해소/취소/크롬 X) → 연결 소멸 → 매니페스트
  자동 정리 + inflight tool_gone + agent.app_tools_changed (docs/58 §4.1).
- 락 규약: 가드 판정은 HandleAgentQuery clientsMutex_ 보유 경로 — 신규 락 0.

## 6. 서버 — file_open wait 모드 + file.open_result 이벤트

- `file_open` args에 `wait` 선택 문자열: `"reply"`(기본, 현행 — 파킹 응답이
  해소 때 도착) | `"event"`(즉시 `{"ok":true,"parked":true}` 회답, 해소는
  이벤트로). 다른 값은 bad_args. `PendingFileDialog.waitAsync`(bool)로
  모드 기록.
- 해소(file_open_result) 경로: waitAsync면 요청자 AgentReply 대신
  `publish_event`로 `file.open_result` 방송 — 페이로드
  `{"ok":true,"path":...}` / 취소 `{"ok":false}`. 비waitAsync 경로 현행
  불변(AgentReply). 양쪽 모두 슬롯 소진/상관 검증 로직 공유.
- 만료 스캔(600s): waitAsync 슬롯 만료 시 `file.open_result
  {"ok":false,"error":"expired"}` 방송 추가. reply 모드 현행 불변.
- 이벤트 카탈로그: `events_list`에 `file.open_result` 즉시 등록(레슨 —
  신규 토픽 즉시 카탈로그화).
- kReservedTopicPrefixes에 `"file."` 추가 — publish_event로
  file.open_result 스푸핑 봉쇄(결정 6).
- 방송 규약: 서버 발행이라 연결 예산(rate limiter) 비적용 — docs/38 서버
  토픽 선례 확인 후 준수.

## 7. 브로커 (jkagentd)

- MCP file_open 호출에 한해 args에 `"wait":"event"` 주입(기존 args-rebuild
  분기 계열 — window_fullscreen on 주입 선례). tools/list의 file_open
  description 갱신: "returns parked immediately; the resolution arrives as
  the file.open_result desktop event (poll read_events)".
- 동적 합성(filedlg_navigate/list/choose)은 기존 제네릭 경로 — 수정 0.
- 폰(jkbridge) 제네릭 릴레이는 브리지 수정 0으로 filedlg 도구 노출.

## 8. 오류 표 (as-built)

| 상황 | 응답 |
|---|---|
| navigate key/index 모두 없음 | `{"ok":true,"error":"bad_args"}` |
| navigate index 범위 밖 | `{"ok":true,"error":"bad_index","count":N}` |
| select/choose 대상 없음 | `{"ok":true,"error":"nothing_selected"}` |
| choose name 부재 | `{"ok":true,"error":"no_such_entry"}` |
| choose/select 폴더 | `{"ok":true,"descended":true,"dir":...}` |
| choose/select 파일 해소 | `{"ok":true,"resolved":true,"path":...}` |
| 모달 가드 실패(고아/미소유) | 서버 `tool_gone` (중계 전 차단) |
| file_open wait 알 수 없는 값 | `{"ok":false,"error":"bad_args"}` |
| filedlg 미기동/사망 | 기존 `unknown_app_tool`/`tool_gone` (docs/58 §9) |

## 9. 프로브 계획 (`engine/tools/probes/probe_filedlg_voice.ps1`)

PS5.1 ASCII 전용, `> log 2>&1`, PID-유니크 state 백업/복원, ×2 연속
ALL PASS. 체크(안):

1. 서버 기동+file_open(wait 기본) → 다이얼로그 스폰 확인.
2. `list_app_tools`에 filedlg 3행 + `modal:true` 확인.
3. `app_tool filedlg list` → entries/total/dir/selected.
4. `navigate index:0` → selected 반영 + file 미러.
5. `navigate key:up/down` 경계(클램프).
6. `navigate key:parent` → 상위 폴더.
7. `choose` (파일명) → parked file_open 해소 + `{"ok":true,"path":...}`
   응답 수신(파킹 reply 모드 e2e).
8. 재 file_open(wait:"event") → 즉시 parked ack 확인 → choose 해소 →
   `read_events`에 file.open_result 도착.
9. 모달 가드: 다이얼로그 kill → 도구 소멸(agent.app_tools_changed/
   list_app_tools) → app_tool 호출 `unknown_app_tool`.
10. 고아 가드: 슬롯 없는 수동 filedlg 기동 → params 실패로 미등록 확인
    (결정 2의 부작용이 곧 가드 검증).
11. publish_event `"file.open_result"` → reserved_topic 거부.
12. 회귀: probe_app_tools(61체크), probe_files, probe_agent_chat.

## 10. 범위 밖

- 다이얼로그 필터 콤보 조작 도구(필터 변경) — 필요 시 후속.
- 절대 경로 직접 해소(choose가 아닌 path 지정) — file_open start 인자로
  대체 가능.
- 브로커 tools/list의 modal 노출/모달 전용 네임스페이스 — 소비자가 1개인
  동안 YAGNI.
- 썸네일/알림 센터 연동, 다중 동시 다이얼로그(1-in-flight 유지).