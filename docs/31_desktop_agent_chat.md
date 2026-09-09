# 31 — Desktop Agent 채팅창 (M2 승인 UX)

- 날짜: 2026-09-10
- 상태: 구현 완료 (스펙 `docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md` §6.2)
- 구현 계획: `docs/superpowers/plans/2026-09-10-agent-chat-mvp.md`

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

- 자연어 입력은 LLM 연동(다음 계획)까지 안내 메시지.
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

## 5. 테스트

```powershell
./build/jkdesktop.exe test                            # 0 failures
./build/jkagentd.exe --selftest                       # 0 failures
powershell -File tools/probes/probe_agent_mcp.ps1     # 5/5 PASS
powershell -File tools/probes/probe_agent_e2e.ps1     # 7/7 PASS
powershell -File tools/probes/probe_agent_palette.ps1 # 4/4 PASS
powershell -File tools/probes/probe_agent_chat.ps1    # 7/7 PASS (신규)
```

`probe_agent_chat.ps1`: launch_chat 스폰 → ask close(블로킹 job) → 이벤트로
request id 캡처 → **approve allow** → close 완결 + 창 소멸 → **deny** →
`denied_by_user` + 창 생존. (타임아웃 경로는 수동 검증: 60초 후
`approval_timeout` 확인.) 프로브 관례 주의: push 이벤트는 재생되지 않으므로
이벤트 구독 job이 close job보다 **먼저** 연결돼야 한다.

## 6. 제한 (채팅창 MVP 스코프)

- 자연어/LLM 위임 미구현 — 다음 계획 (network 권한 + API 키 UI 포함).
- 타임아웃 60초 고정, 승인 프롬프트 1건 동시 처리 (채팅창 UI 기준).
- Win32 기본 폰트라 한글 표시는 OK지만 시각 디자인은 최소 수준.
- STT/마이크는 4단계(장치 게이트) 과제 — 별도 프로세스라 채팅창 안에서만
  확장하면 된다.
- 이벤트는 created/destroyed/focused + approval 2종 (스펙 §4 전체 세트는 4단계).

## 7. 다음 단계와의 연결

- **LLM 연동**: 채팅창의 비동기 채널 위에 자연어 → 도구 위임. network 권한
  설계가 선행 (스펙 §5 network 행).
- **M2b 트리거 스크립트**: QuickJS `on(topic, filter, handler)` — 알림의
  단일 수신처는 채팅창 트랜스크립트 또는 그때까지 승격된 알림 센터.
- 앱 정복 사다리(§6.6): 승인 UX가 생겨 close_window의 control 등급이
  "인라인 승인" 스펙(§5)을 처음으로 충족했다.