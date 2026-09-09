# 29 — Desktop Agent M1: 에이전트가 jkdesktop을 만지는 첫 길

- 날짜: 2026-09-10
- 상태: 구현 완료 (스펙 `docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md` §6.1)
- 구현 계획: `docs/superpowers/plans/2026-09-10-agent-m1.md`

## 1. 무엇이 생겼나

Claude Code 같은 외부 에이전트가 MCP(stdio)로 접속해 jkdesktop의 **구조화된
상태**를 읽고 조작한다 — 스크린샷 추측 없이.

```
[Claude Code]  [agentctl CLI]  [probes/*.ps1]
      └────────── MCP / agentctl ─────────┘
                    │
              [jkagentd 사이드카]          ← 권한 관문 + receipt 기록
                    │
        [윈도우 서버 (jkdesktop --server)]
         AgentQuery/Reply/Event (와이어 17~20)
```

- **제어 전용 연결**: 에이전트 클라이언트는 표면/공유메모리 없이 파이프로만
  대화한다. 서버 acceptor는 두 번째 메시지(`AgentEventSubscribe`)로 연결
  종류를 판별한다.
- **와이어 포맷**: 고정 POD 헤더(queryId, jsonLen) + UTF-8 JSON 본문 —
  `CommitSurfaceHeader`의 "뒤에 N개 엔트리" 관행의 일반화.
- **브로커는 사이드카**: jkagentd가 죽어도 데스크탑은 정상 동작한다.

## 2. 서버 기동과 agentctl

```bash
cd engine/build
./jkdesktop.exe --server &        # 윈도우 서버
./jkdesktop.exe agentctl '{"tool":"ping","args":{}}'
# {"ok":true,"pong":true}
./jkdesktop.exe agentctl '{"tool":"list_windows","args":{}}'
# {"ok":true,"windows":[{"id":3,"title":"Minesweeper","pid":11048,
#   "x":500,"y":170,"w":320,"h":380,"focused":true,"minimized":false}]}
./jkdesktop.exe agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}'
./jkdesktop.exe agentctl '{"tool":"save_layout","args":{"name":"work"}}'
./jkdesktop.exe agentctl '{"tool":"restore_layout","args":{"name":"work"}}'
./jkdesktop.exe agentctl '{"tool":"close_window","args":{"id":3}}'   # 기본 deny
./jkdesktop.exe agent-events 5     # 이벤트 스트림 5초 관찰
```

`save_layout`/`restore_layout`은 `<exeDir>/state/layout_<name>.json`에 스냅샷을
남긴다. 복원 매칭은 **타이틀** 기준 (surface id는 재시작마다 바뀐다).

## 3. Claude Code 연결 (claude-wrapper)

저장소 루트의 `.mcp.json`이 jkagentd를 프로젝트 스코프 MCP 서버로 등록한다:

```json
{ "mcpServers": { "jkdesktop": {
    "command": "I:\\progwork\\JKENGINE\\engine\\build\\jkagentd.exe" } } }
```

Claude Code 세션에서 `jkdesktop` 서버의 도구로 노출된다:
`list_windows`, `launch_app`, `focus_window`, `close_window`, `save_layout`,
`restore_layout`, `read_log`, `read_events`, `terminal_exec`.

## 4. 권한과 receipt

- **permissions.json** (jkagentd.exe 옆): `{"close_window":"allow"}` 식으로
  도구별 allow/deny. 기본값은 전부 allow, **close_window만 deny** — M1엔
  승인 UI가 없으므로 파일 수정이 곧 승인 행위다 (스펙 §5).
- **receipts.jsonl** (`<exeDir>/state/`): 모든 tools/call이
  `{ts, tool, args, result}`로 append-only 기록된다. 타임라인 질의의 백엔드.
- `read_events`: 윈도우 created/destroyed/focused 이벤트 큐를 비워준다.
  브로커 연결은 서버가 push하는 이벤트를 큐에 적립한다.
- `terminal_exec`: ConPTY로 명령을 실행하고 출력을 돌려준다 (`ended`:
  "exited"|"timeout"). Windows 전용.

## 5. 테스트

```powershell
./build/jkdesktop.exe test                     # 앱 셀프테스트
./build/jkagentd.exe --selftest                # MCP 라우터 셀프테스트
powershell -File tools/probes/probe_agent_mcp.ps1    # launch/list/save/deny/receipt
powershell -File tools/probes/probe_agent_e2e.ps1    # 정복 사이클 전체
```

## 6. 제한 (M1 스코프)

- 승인 프롬프트 UI 없음 — 2단계(커맨드 바) 과제. 그때까지 close_window는
  permissions.json 수정으로만 허용된다.
- 서버가 죽으면 쿼리는 블로탁 후 `pipe_error` — 타임아웃 없음 (M1, 스펙 §11).
- POSIX 빌드에서 terminal_exec 미지원 (ConPTY 스텁).
- 이벤트 rate limit/dead-letter 없음 — 스펙 §4의 4단계 과제.
- **내부 에이전트 채팅창(2단계)은 JKWindow/SDL 체계 밖**에 만든다는 사용자
  제약이 계획 문서에 기록되어 있다 (plans/2026-09-10-agent-m1.md 말미).

## 7. 다음 단계와의 연결

스펙 §6.2의 커맨드 바가 같은 도구 집합을 호출한다 ("하나의 API, 여러 얼굴").
**M2a 커맨드 바(Alt+Space 팔레트) 구현 완료 (2026-09-10)** —
`docs/30_desktop_agent_m2a.md`. 서버 관문(permissions.json)이 팔레트
얼굴까지 확장되었다. 앱 정복 사다리(§6.6)의 L0(minesweeper)는
`probe_agent_e2e.ps1`이 이미 수행한다 — 정복 = launch/observe/drive/verify/recover 완주.