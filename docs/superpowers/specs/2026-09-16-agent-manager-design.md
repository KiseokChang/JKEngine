# 에이전트 관리자 앱 설계 (specs/2026-09-16-agent-manager)

- 날짜: 2026-09-16
- 배경: 플랫폼 설계 spec §7 "에이전트 관리자 — 설치된 에이전트·권한·자율성 사다리
  설정" (단계 3). §7 잔여 앱 중 사용자 선정.
- 상태: 설계 승인 (2026-09-16, 채팅에서 A안 승인) → 구현 플랜 대기
- 참조: docs/29 (M1 브로커+permissions), docs/31 (승인 파이프라인),
  docs/33 (notify 앱 = 템플릿), docs/34 (trigger 도구), docs/37 (trust),
  docs/38 (rate limit), docs/51 (P4 콘솔 앱), docs/52 (theme_set/write_failed 선례)

## 0. 범위와 비범위

**넣는다** — 4탭 GUI:
1. 권한 매트릭스 (도구 × allow/ask/deny, 쓰기 = 승인 파이프라인 게이트)
2. 트리거 관리 (목록 + on/off)
3. 신뢰 지문 목록 (조회 + 해지)
4. 설치 앱 + receipts (콘솔 앱/.jkx/실행 중 창 통합 목록, receipts 꼬리)

**안 넣는다 (YAGNI, 명시)**:
- 자율성 사다리(§6 신뢰 점수·도메인 자율성 캡) — 미구현 그대로. 매트릭스 MVP가
  그 전제 데이터(도구별 결정)를 만들므로 이후 확장 지점.
- 브라우저 "팝업-페이지 더블파이어" 등 docs/49 MINOR와 무관.
- 알림 센터 재사용(토스트) — 매니저는 요청-응답 패널 앱, 이벤트 구독 불필요.
- 채팅 /agentmgr 슬래시 — 팔레트 + launch_app으로 충분(LLM은 launch_app으로
  열 수 있다). 이월 없이 그냥 안 함.

## 1. 앱 (A안: ImGui 클라 앱)

- `jkapp_agentmgr.dll` + `agentmgr.jkx` — `ClientNotifyApp` 템플릿
  (ClientAgentMgrApp.cpp/.h, jkapp_notify와 동일 구조: 16ms 타이머 프레임 게이트,
  NotifyRoot식 어두운 루트, `WA_CHROMELESS`, 560×640, 타이틀 "Agent Manager").
- 한글 폰트: malgun.ttf + GetGlyphRangesKorean (notify 선례, docs/33).
- 테마: `ApplyImGuiTheme()` + `OnThemeChanged` 훅 (docs/52, 0101d55 신설 가상 훅).
- 통신: 자기 창 연결에서 `SendAgentQuery`/`PollReply` (팔레트 선례 — 블로킹
  JKAgentClient 모델 아님, 클라 read 루프는 프레임 루프 결합). **이벤트 구독
  불필요** — 트리거 on/off, 권한 변경의 결과는 요청-응답으로 충분하고 패널
  갱신은 (a) 탭 진입 시, (b) 새로고침 버튼, (c) 자기 요청의 reply 도착 시에만.
- 타이틀: `SendWindowTitle` (MsgType 21) 불필요 — 타이틀 고정 "Agent Manager".
- 아이콘: 다크 슬레이트 타일 + 관리자 글리프 (레슨 20 규약: launcher_agentmgr).
- 런처 셀 = ScanJkxApps가 자동. 팔레트 `/agentmgr` → launch_app(app:"agentmgr").

### 1.1 탭 상세

**권한 탭**: 테이블 행 = 도구 1개, 열 = [도구 | gate 뱃지 | 현재 결정 | 기본값 |
출처(file/default) | 변경 버튼들]. gate 뱃지는 server/server(fixed)/broker/none
(§2.1 표) — none 행은 "파일 기록만" 안내. 셀 변경은 allow/ask/deny 3버튼 —
현재 값과 다른 것만 활성화. 누르면 `permission_set`
쿼리 → 파킹 → 채팅 승인 → reply 오면 매트릭스 재조회. 결정 상태 표시
("승인 대기"). 알려진 도구 순서는 서버가 돌려주는 배열 순서 그대로.

**트리거 탭**: `trigger_list` 행 (name/topics/enabled) + on/off 버튼 →
`trigger_toggle`. 비활성 항목 흐리게. topics 없는 행(플래그만 있는)도 표시.

**신뢰 탭**: `trust_list` 행 (name/source/ts/지문 앞 15자 — 서버가 이미 자름) +
해지 버튼 → `trust_revoke` → 승인 → 재조회. "변경은 jktriggers 재시작 후
적용" 안내 문구 상시 (docs/37 규약 — reload는 enable 플래그만 재적용).

**설치/로그 탭**: 위쪽 = `installed_list` 테이블 [이름 | kind | 실행 중] —
실행 중 열은 `list_windows` 결과를 이름으로 조인(대소문자 무시 exact). 아래쪽 =
`read_receipts` 최근 N행 [ts | tool | ok/error] + 새로고침 버튼.

## 2. 서버 도구 (신설 5종)

모두 `HandleAgentQuery`의 else-if 체인에 추가 (lesson 35: 이미
clientsMutex_ 보유 중 — 도구 내부에서 클라 락 재획득 금지, Unsafe 관용구).

### 2.1 `agent_permissions` (읽기, 무게이트 — safe tier)

**게이트의 실체 (코드 조사 2026-09-16, 설계 정정 사유)**: 서버는
`AgentToolAllowed`를 3개 도구에서만 검사한다(close_window/trust_request/
run_console_app — JKWindowServer.cpp:1691/1786/1866). 그 외 서버 도구의
permissions.json 값은 **서버 경로에서 무력**하고, 브로커 `LoadPermissions`
의 bool 맵이 MCP 에이전트 경로만 걸러낸다. capture_window 도구의
"permissions.json can deny" 주석(JKWindowServer.cpp:2090)은 실제 체크가 없는
부정확한 주석 — 구현 시 주석만 정정(게이트 추가는 범위 밖, 부수 발견으로 기록).
매트릭스는 이 이분법을 숨기지 않는다:

| 행 그룹 | 도구 | gate 열 표기 | 서버 effective |
|---|---|---|---|
| 서버 3치 게이트 | close_window, trust_request, run_console_app, (신규) trust_revoke | `server` | 파일값/기본값 3치 결정 |
| 서버 고정 게이트 | permission_set | `server(fixed)` | 항상 ask |
| 브로커 전용 bool | read_log, read_events, terminal_exec | `broker` | 서버 미게이트(allow) |
| 무게이트 서버 도구 | list_windows, focus_window, launch_app, save_layout, restore_layout, publish_event, capture_window, capture_region, trigger_toggle, theme_set, open_notify, launch_chat, approve, file_open, file_dialog_params, (신규) agent_permissions, installed_list, read_receipts | `none` | allow (파일값 무력, 브로커에만 유효) |

응답 형식 (행마다 `gate` 필드 추가):
```json
{"ok":true,"perms":[
  {"tool":"close_window","gate":"server","file":"deny|ask|allow|null",
   "effective":"deny","default":"deny"}, ...]}
```
- `file`: permissions.json에 키가 있으면 그 값, 없으면 null.
- 파일 unreadable이면 `{"ok":false,"error":"permissions_unreadable"}` —
  파일 없음(기본값)과 파싱 실패(사용자 편집 실수)를 구분 (trust_list의
  docs/38 선례).
- 브로커 `LoadPermissions`와 서버 `AgentToolAllowed`가 같은 파일을 읽지만
  로직이 이분화돼 있어(bool 맵 vs 3치 결정) 이 도구는 **서버 논리 단일 사실**로
  렌더링하고 broker 행은 gate 표기로만 구분한다.

### 2.2 `permission_set {tool, decision}` — **결정 상태에서 항상 Ask (하드코딩)**

- 검증(파킹 전): tool은 위 표의 알려진 도구 배열 멤버(trust_request의
  bad_fingerprint 선례 — 파킹 전 검증), decision ∈ {allow, ask, deny}. 위반 시
  `missing_tool`/`bad_decision`/`unknown_tool` 즉답.
- 쓰기 의미: `gate:"server"` 행은 실제 게이트 변경, `gate:"broker"` 행은 브로커
  bool 변경, `gate:"none"` 행은 **파일 기록만**(현재 서버에서 무력 — UI는
  "브로커 전용/서버 미게이트" 뱃지로 표시). permission_set은 행 그룹을 판별하지
  않고 전부 쓰기를 허용한다 — 매트릭스 표기가 정직하면 사용자가 판단한다.
- 게이트: `AgentToolAllowed("permission_set")`은 **파일 값을 무시하고 항상
  Ask**. 이유: 매트릭스에서 permission_set 자체를 allow로 바꿔두면 이후 모든
  권한 변경이 무승인이 되는 2단 우회 — 파일은 이 도구의 결정을 바꿀 수 없다
  (설계의 핵심 안전 결정).
- Ask 경로: PendingApproval **신규 kind `"permission_set"` + 신규 필드
  `permTool`/`permDecision`** (close_window의 targetId처럼 kind 전용 페이로드
  — name 필드 재용용 금지). 승인 요청 이벤트에
  `"tool":"permission_set","target_tool":"close_window","decision":"ask"` 포함
  → 채팅 승인 스트립이 "close_window → allow 로 바꿀까요?" 형태로 보여야 하므로
  jkchat approval 프롬프트에 permTool/decision 렌더링 추가.
- 승인 해소(`approve` kind 분기): permissions.json RMW 쓰기 — 기존 파일을
  AgentJson으로 읽고(없으면 기본값으로 시작), 해당 도구 키를 decision으로
  대체, **알려진 도구 키 전부 기록**(파일에 없던 키도 현재 effective 값으로
  명시 쓰기 — 다음 편집자가 기본값을 온전히 보도록), 쓰기 실패는
  `write_failed`로 요청자 reply에 표면화(docs/52 선례).
- 거부/타임아웃: 파일 불변, 요청자에게 `denied_by_user`/`approval_timeout`.
- events_list 카탈로그에 `agent.approval_request` 행 이미 있으므로 별도 신규
  토픽 없음 (permission_set은 approval_request를 재사용).

### 2.3 `trust_revoke {fingerprint}` — askCapable에 추가 (기본 Ask, 파일로
allow/deny 변경 가능)

- 검증(파킹 전): 지문 형식 `sha256:[0-9a-f]{64}` (trust_request의
  ValidFingerprint 람다 재사용) → `bad_fingerprint`. 대상 레코드 존재 여부는
  파킹 전 실측(trust.json 읽기) → 없으면 `{"ok":false,"error":"not_found"}`.
- Ask 경로: 신규 kind `"trust_revoke"`, 페이로드 필드 `fingerprint` + display
  name. 승인 시 trust.json에서 해당 지문 레코드 제거 RMW (write_failed 표면화,
  .bak 규약 불변 — docs/49는 북마크, docs/37은 .bak 없음 → **.bak 규약 신규
  적용**: 최초 덮어쓰기 1회 보존, 북마크 선례).
- 결과 reply에 `"restart_needed":true` — jktriggers는 부팅 1회 로드라 재시작까지
  메모리 신뢰 유지 (docs/37 §4.1). UI는 이 필드로 안내 문구 갱신.
- 방향성 논의(설계 결정 기록): 해지는 안전 방향이라 무게이트 후보였으나,
  **게이트 없는 trust_revoke 도구는 악성 트리거가 신뢰 저장소를 전면 소각하는
  DoS 통로**가 되므로 Ask 기본 + 파일 오버라이드 허용(allow로 바꿔도 권한을
  부여하지 않으므로 2단 우회 위험 없음).

### 2.4 `installed_list` (읽기, 무게이트)

- `JKDesktopShell`에 신규 메서드 `ListInstalled()` — launcherIcons_ 순회:
  `{name, kind}` (kind = `"console"`(consoleCmd 비지 않음) | `"jkx"`(jkxPath
  비지 않음) | `"builtin"`). 서버 도구가 shell_에 위임, 매니페스트 cmd는
  **반환하지 않는다** (지문/cmd 노출 = run_console_app 승인 경로의 관심사,
  관리자 뷰엔 불필요 — 최소 노출).
- 응답: `{"ok":true,"installed":[{"name":"sampletodo","kind":"console"},...]}`
- shell_이 null(셸 미기동)이면 `{"ok":true,"installed":[]}` (빈 배열 — 읽기
  도구의 실패는 corrupt/unreadable만).

### 2.5 `read_receipts {limit?}` (읽기, 무게이트)

- `StateDir()+"\\receipts.jsonl"` 꼬리 읽기 — 파일 뒤에서 최대 256KiB, 행 역순,
  `limit` 기본 50·상한 200. 행 파싱은 raw 스캔(`"ts":`,`"tool":`,`"result"`
  내 `"ok":true/false`) — AgentJson 2레벨 접근자로 충분.
- 응답: `{"ok":true,"rows":[{"ts":123,"tool":"close_window","ok":false},...]}`
  (result JSON 전문은 노출하지 않는다 — args에 경로/명령어가 실릴 수 있음).
- 파일 없음 = `{"ok":true,"rows":[]}` (브로커 미사용 설치는 정상 상태).

## 3. 브로커(jkagentd) 갱신

- `kToolsListJson`/`IsKnownTool`에 5종 추가: agent_permissions, installed_list,
  read_receipts (인자 없음/limit), trust_revoke, permission_set (schema 포함).
- `LoadPermissions` kNames 갱신 + 기본값: 읽기 3종 = allow, **trust_revoke =
  deny, permission_set = deny** (run_console_app 선례 — "permissions.json에
  ask를 넣는 것"이 승인 행위; 서버 Ask 파이프라인이 실 게이트이므로 MCP 에이전트
  기본 차단은 정직한 기본값). ask-capable 목록(브로커)은 bool 맵이라 ask가
  통과 — 파일에 `"trust_revoke":"ask"`를 넣은 설치에서만 MCP 에이전트가
  해지 요청을 승인 파이프라인으로 보낼 수 있다.
- argsJson 빌더: trust_revoke{fingerprint}, permission_set{tool,decision},
  read_receipts{limit}.
- `--selftest` 갱신: unknown_tool 회귀 + permission_set/trust_revoke args 재빌드
  케이스.

## 4. 데이터/파일

| 파일 | 소유 | 매니저의 행위 |
|---|---|---|
| `permissions.json` (exeDir) | 사용자 편집 + permission_set 승인 시 서버 RMW | 표시 + 승인 통과 변경 |
| `state/trust.json` | jktriggers 로더 + trust_request 승인 | 표시 + 승인 통과 해지 |
| `state/receipts.jsonl` | jkagentd 전용 쓰기 | 읽기만 |
| `state/triggers.json` | 서버 (trigger_toggle) | 표시 + 재사용 토글 |
| `state/triggers_loaded.json` | jktriggers | 읽기만 (trigger_list 경유) |

## 5. 에러 처리

- state 파일 unreadable/corrupt: 해당 패널에 에러 박스(한국어, docs/38
  `trust_store_unreadable` 메시지 그대로) — 앱이 죽지 않고 다른 탭은 동작.
- 쓰기 실패(write_failed): 요청자 reply에 실리므로 패널에 그대로 표시 —
  조용한 성공 라인 금지 (docs/52 ec8083e 선례).
- 서버 미연결/not_connected: 상태줄 한 줄, 자동 재시도 없음(수동 새로고침).

## 6. 테스트

**probe_agentmgr.ps1** (PS5.1 ASCII-only + BOM, lesson 50; Heisenberg 클린
바이너리 공식런, lesson 61; dll/jkx mtime > 소스 mtime 게이트, lesson 37):
1. 스폰: launch_app(agentmgr) → list_windows에 "Agent Manager" 행.
2. `agent_permissions` shape: close_window의 default=deny, effective=deny,
   file=null (permissions.json 부재 상태 보장 후).
3. permission_set E2E: `permission_set{close_window, allow}` → 파킹 → 승인 →
   (a) 요청자 reply ok, (b) **파일 실측** — permissions.json에
   `"close_window":"allow"`, (c) agent_permissions 재조회 file="allow".
   클린업: 파일 삭제(테스트가 만든 것임을 보장 — 스폰 전 존재 검사, 존재 시
   abort).
4. trust_revoke E2E: trust.json에 가짜 레코드 시딩 → trust_revoke → 승인 →
   trust.json에서 제거 실측 + .bak 1회 보존 실측. (시딩 전 trust.json 실측 백업)
5. trust_revoke not_found / bad_fingerprint (파킹 없이 즉답).
6. installed_list: sampletodo(console) + minesweeper(jkx) 포함.
7. read_receipts: receipts.jsonl 부재 → rows:[] / 시딩 파일 → 역순 꼬리+캡.
8. 트리거 회귀: trigger_toggle on→off→list (probe_agent_triggerctl의 축소판).

**회귀**: probe_agent_mcp(tools/list 변경 — 카운트 갱신 필요), probe_agent_trust,
probe_agent_triggerctl, probe_agent_chat(승인 스트립에 permission_set 렌더링
추가로 chat 코드 손대면), jkdesktop test 0, jkagentd --selftest 0.

**jkchat**: approval_request 이벤트에 kind=permission_set/trust_revoke가 오면
"[권한 변경] close_window → allow", "[신뢰 해지] <name>" 형태의 스트립 렌더링
추가 — approve 도구는 kind 불문 공용이라 서버 변경 없이 해소 가능.

## 7. 리스크와 방어

| 리스크 | 방어 |
|---|---|
| permission_set의 2단 우회(자신을 allow로) | AgentToolAllowed에서 파일값 무시 + 하드코딩 Ask |
| 악성 스크립트의 신뢰 전면 해지 | trust_revoke 기본 Ask |
| 승인 대기 중 이중 파킹 | 파킹 전 not_found/bad_* 즉답 — 대기열 오염 없음 |
| permissions.json 파손(수동 편집) | agent_permissions가 permissions_unreadable로 구분, RMW는 유효 키 전재기록으로 복구 효과 |
| HandleAgentQuery 내 재획득 데드락 | 도구 구현은 락 프리 (lesson 35, Unsafe 관용구) |
| receipts에 민감 args 노출 | read_receipts는 ts/tool/ok만 반환 |

## 8. 구현 표기 (완료 시 기입)

- 커밋: de7d2b9(읽기 3종) → 72c5355(permission_set) → 7cd4393(trust_revoke) →
  d542415(브로커 5종) → bcee1e6(jkchat 스트립 + /agentmgr) → 9528113(jkapp_agentmgr)
- docs: docs/53_desktop_agent_mgr.md
- 프로브: probe_agentmgr.ps1 (15/15 PASS) + 회귀 GREEN (mcp/trust/triggerctl/chat,
  jkagentd --selftest 0)
- 델타 (구현과 정합, §2.1/§2.5 보완): agent_permissions의 `file`은 파일 행 부재 시
  `""`(규약). read_receipts 행의 ok는 bool 접근자 부재로 `"0"/"1"` 문자열, ts는
  epoch 초(ms→1000 나눔). trust_list 행에 전체 지문 `fp` 필드 추가(표시용 15자
  truncated와 병행) — UI 해지가 전체 지문으로 동작하게. 트리거 탭은 2레벨 리더
  제약으로 topics 열 생략(name/enabled만).