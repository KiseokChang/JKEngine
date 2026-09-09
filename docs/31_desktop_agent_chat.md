# 31 — Desktop Agent 채팅창 (M2 승인 UX)

- 날짜: 2026-09-10
- 상태: 구현 완료 — 채팅창 MVP + LLM 위임 (스펙 `docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md` §6.2)
- 구현 계획: `docs/superpowers/plans/2026-09-10-agent-chat-mvp.md`, `docs/superpowers/plans/2026-09-10-agent-chat-llm.md`

## 1. 무엇이 생겼나

**jkchat.exe** — JKWindow/SDL 체계 **밖**의 순수 Win32 채팅창 (사용자 제약:
나중에 STT가 붙을 여지를 남기기 위해 별도 프로세스). 슬래시 커맨드로 데스크탑을
조작하고, "ask" 권한으로 걸린 에이전트 조작 요청을 **인라인 승인 프롬프트**
([허용][거부])으로 결정한다.

```
[사용자] → [jkchat.exe — 순수 Win32, 별도 프로세스]
              │  control-only 연결 — M1 JKAgentClient 재사용
              │  AgentQuery(approve) + AgentEventSubscribe(승인 요청 수신)
        [윈도우 서버] — 보류(pendingApprovals_) + agent.approval_request 방송
             ↑ 요청은 블로킹 대기          ↓ 결정 (approve 도구)
        [jkagentd(MCP) / agentctl / 팔레트] ← AgentReply (allow/deny/timeout)
```

- **승인 보류 상태는 서버가 갖는다.** 요청 얼굴(브로커, agentctl, 팔레트)은
  기존 블로킹 계약 그대로 응답을 기다리고, 채팅창은 그냥 이벤트를 받아
  `approve` 도구를 호출하는 클라이언트다 — 브로커 변경은 "ask" 패스스루뿐.
- **하나의 API, 여러 얼굴**: 팔레트 `/chat`도, MCP 에이전트의 `launch_chat`도,
  agentctl의 approve도 같은 서버 핸들러를 친다. 채팅창이 승인하면 그 요청이
  팔레트에서 왔든 MCP에서 왔든 해결된다 (probe가 이 교차 얼굴 경로를 검증).

## 2. 사용법

```bash
cd engine/build
./jkdesktop.exe --server &
./jkdesktop.exe agentctl '{"tool":"launch_chat","args":{}}'   # 서버가 jkchat.exe 스폰
# 또는 팔레트(Alt+Space)에서 /chat
```

슬래시 커맨드 (팔레트와 같은 도구 집합):

| 커맨드 | 도구 | 비고 |
|---|---|---|
| `/list` | list_windows | 창 목록 JSON |
| `/launch <app>` | launch_app | |
| `/close <id>` | close_window | pre_undo 스냅샷 후 전송 — ask면 승인 프롬프트 |
| `/save <name>` / `/restore <name>` / `/undo` | save/restore_layout | |
| `/help` | — | |

- 자연어 입력은 LLM(claude 헤드리스)으로 위임된다 — 아래 §5. 슬래시는
  결정적 커맨드(네트워크/토큰 없음)로 남는다.
- 창 생성/소멸/포커스 이벤트가 트랜스크립트에 시스템 줄로 표시된다.

## 3. 권한 3값 모델

`permissions.json` (exe 옆, 브로커와 서버가 같은 파일을 읽음):

```json
{"close_window": "ask"}
```

| 값 | 의미 |
|---|---|
| `allow` | 즉시 실행 |
| `ask` | **인라인 승인** — 서버가 쿼리를 보류하고 `agent.approval_request` 방송. 채팅창(또는 모든 구독자)의 `{"tool":"approve","args":{"request":N,"decision":"allow"|"deny"}}`로 완결 |
| `deny` / 무기록 | 거부 (`permission_denied`) |

- **구독자 없음** → 즉시 `approval_unavailable`.
- **60초 무응답** → `approval_timeout` (서버 메인 루프의 만료 스캔).
- **거부** → 요청자에게 `denied_by_user`.
- 브로커(jkagentd)는 `ask`를 deny가 아니라 **통과**시킨다 — 서버의 승인
  파이프라인이 절차를 수행하고 브로커 쿼리는 해결될 때까지 블로킹한다.

## 4. 채팅창이 비동기인 이유

채팅창 자신의 `/close`가 ask로 걸리면 응답이 승인 때까지 안 온다. 블로킹
`Query`로 보내면 **자기 UI가 멈춰 자기 승인을 못 누르는 교착**에 빠진다. 그래서
`JKAgentClient`에 `SendQuery`/`PollReply`(쿼리ID 부착 비동기)를 추가했다 —
ping 펌프(400ms)가 보류 응답들을 큐로 플러시하고 UI는 살아 있어 승인 버튼을
누를 수 있다. `pendingReplies_`는 id 매칭으로 바뀌어 기존 `QueryRaw` 계약은
그대로.

## 5. LLM 연동 (claude CLI 서브프로세스)

슬래시가 아닌 입력은 **claude 헤드리스 턴**으로 위임한다 (사용자 결정: A안 —
claude CLI 서브프로세스, `ollama launch claude`로 구동, 참고
`I:\progwork\claude_wrapper\claude_cli_wrapper_guide.md`).

```
[입력줄] Submit() — '/'로 시작하지 않으면
   └ StartLlmTurn(prompt) → 워커 스레드 (UI 논블로킹)
        └ BuildEngineCmd: ollama launch claude --model <m> --
             -p "<prompt>" --output-format json
             --dangerously-skip-permissions [--resume <session_id>]
             --directory <dir>
        └ 익명 파이프로 stdout 수집 (10분 타임아웃, 초과 시 TerminateProcess)
        └ AgentJson 파싱: result / session_id
        └ PostMessage(WM_APP+1) → UI 스레드가 트랜스크립트에 최종 결과 기록
```

**설정** `state\chat.json` (exe 옆, 없으면 기본값):

```json
{"engine":"ollama","ollama_model":"kimi-k2.7-code:cloud","skip_permissions":1,"directory":"I:\\progwork\\JKENGINE"}
```

| 필드 | 기본값 | 의미 |
|---|---|---|
| `engine` | `"ollama"` | `ollama` / `claude`(직접) / `stub`(프로브용) |
| `ollama_model` | `kimi-k2.7-code:cloud` | ollama가 launch할 모델 |
| `skip_permissions` | `1` | claude 헤드리스는 프롬프트에서 **자동 거부**하므로(행 없음) 기본 켬. 0으로 끄면 도구가 자동 거부될 뿐 hang은 없음 — claude 도구 호출의 실제 승인 관문은 **서버 파이프라인**(§3)이 담당 |
| `directory` | `I:\progwork\JKENGINE` | 엔진 프로세스의 **작업 디렉터리** (CreateProcessW lpCurrentDirectory). claude CLI 2.1.x에는 `--directory` 플래그가 없고 세션 히스토리가 cwd에 바인딩되므로 `--resume`에는 매 턴 같은 cwd가 필요. `.mcp.json`(jkagentd)이 있는 위치로 잡을 것 |

- **세션 연속성**: 응답 JSON의 `session_id`를 저장해 다음 입력에 `--resume`.
  `/new`로 리셋.
- **출력은 최종 결과만** (MVP 결정): `--output-format json`의 `result` 필드만
  트랜스크립트에 표시. 스트리밍(`stream-json --include-partial-messages`)은
  후속 과제.
- **claude의 도구 호출 경로**: claude가 MCP(`.mcp.json` → jkagentd)로
  list_windows/close_window 등을 호출하면 기존 권한 관문을 그대로 통과 —
  `close_window`가 ask면 **채팅창 승인 프롬프트가 claude의 도구 호출에 뜬다**.

실엔진 수동 체크리스트 (네트워크 필요 — 자동 프로브는 stub으로 대체):
**2026-09-10 실측 완료** — `tools/probes/smoke_llm_real_a.ps1` / `_b.ps1`이
전 항목을 실엔진으로 자동 구동 검증했다 (실 API 호출 발생하므로 회귀 세트에서는
제외하고 필요시 수동 실행).

1. [x] `state\chat.json` 없이 기동 → 기본값(ollama/kimi) 사용 확인
2. [x] "열려 있는 창을 목록으로 보여줘" → claude가 `list_windows`를 호출하고
   목록이 결과로 표시
3. [x] 후속 "그 목록에서 지뢰찾기는 몇 번 id야?" → `--resume`로 맥락 유지
4. [x] `permissions.json {"close_window":"ask"}`에서 "지뢰찾기 닫아줘" → 채팅창
   승인 프롬프트가 claude의 도구 호출에 뜸 → 허용 → 창 소멸 + LLM이 결과 보고
5. [x] `/new` → "새 LLM 세션" 표시, 세션 리셋

실측에서 고친 것: (a) claude CLI에 `--directory` 플래그가 없어 인자에서 제거하고
워커 프로세스 cwd로 대체; (b) stderr를 stdout 파이프에서 분리 — claude의
`[claude-code:unrecognized_model]` 경고가 stderr로 나와 합치면 JSON 파싱이
깨진다 (파싱 실패 시 stderr 꼬리를 에러로 표시).

## 6. 테스트

```powershell
./build/jkdesktop.exe test                            # 0 failures
./build/jkagentd.exe --selftest                       # 0 failures
powershell -File tools/probes/probe_agent_mcp.ps1     # 5/5 PASS
powershell -File tools/probes/probe_agent_e2e.ps1     # 7/7 PASS
powershell -File tools/probes/probe_agent_palette.ps1 # 4/4 PASS
powershell -File tools/probes/probe_agent_chat.ps1    # 7/7 PASS
powershell -File tools/probes/probe_agent_chat_llm.ps1 # 2/2 PASS (신규, stub)
```

`probe_agent_chat.ps1`: launch_chat 스폰 → ask close(블로킹 job) → 이벤트로
request id 캡처 → **approve allow** → close 완결 + 창 소멸 → **deny** →
`denied_by_user` + 창 생존. (타임아웃 경로는 수동 검증: 60초 후
`approval_timeout` 확인.) 프로브 관례 주의: push 이벤트는 재생되지 않으므로
이벤트 구독 job이 close job보다 **먼저** 연결돼야 한다.

`probe_agent_chat_llm.ps1`: stub 엔진(`state\chat.json`에 `"engine":"stub"`)
으로 네트워크 없이 자연어 → 엔진 턴 → 트랜스크립트 표시까지 기계부(E2E 배선)
를 검증. UI 드라이빙 교훈: 버튼 텍스트는 GetWindowTextW로 읽고(버튼 WM_GETTEXT
교차프로세스는 모지바케), multiline EDIT은 WM_GETTEXTLENGTH가 과소 보고하므로
버퍼를 넉넉히 잡아야 한다.

## 7. 제한 (채팅창 MVP 스코프)

- LLM 출력은 최종 결과만, 타임아웃 10분 고정, 스트리밍 미구현 (후속).
- 타임아웃 60초 고정, 승인 프롬프트 1건 동시 처리 (채팅창 UI 기준).
- Win32 기본 폰트라 한글 표시는 OK지만 시각 디자인은 최소 수준.
- STT/마이크는 4단계(장치 게이트) 과제 — 별도 프로세스라 채팅창 안에서만
  확장하면 된다.
- 이벤트는 created/destroyed/focused + approval 2종 (스펙 §4 전체 세트는 4단계).

## 8. 다음 단계와의 연결

- **LLM 스트리밍**: `--output-format stream-json --include-partial-messages`
  로 부분 출력 표시 (MVP 이후).
- **M2b 트리거 스크립트**: 구현 완료 (`docs/32`) — QuickJS 호스트 jktriggers.exe,
  첫 번들 3종. 알림 수신처는 이 문서의 `agent.notify` 브랜치(`[알림]` 줄, §6)로
  연결됐고, 알림 센터 앱은 스펙 §7 후속.
- 앱 정복 사다리(§6.6): 승인 UX가 생겨 close_window의 control 등급이
  "인라인 승인" 스펙(§5)을 처음으로 충족했다.