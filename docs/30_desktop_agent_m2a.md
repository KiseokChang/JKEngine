# 30 — Desktop Agent M2a: 커맨드 바 (Alt+Space 팔레트)

- 날짜: 2026-09-10
- 상태: 구현 완료 (스펙 `docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md` §6.2)
- 구현 계획: `docs/superpowers/plans/2026-09-10-agent-m2a.md`

## 1. 무엇이 생겼나

**Alt+Space**로 열리는 ImGui 커맨드 팔레트 — 스펙 §2의 "커맨드 바/패널창" 얼굴.

```
[사용자] --Alt+Space--> [Command Palette (jkapp_palette.dll, ImGui)]
                              │  AgentQuery/Reply/Event (와이어 17~20)
                              │  ── 자기 창 연결 위에서 ──
              [윈도우 서버] HandleAgentQuery (MCP/jkagentd와 같은 도구)
```

핵심은 "하나의 API, 여러 얼굴"(스펙 §2): 팔레트의 `/close`와 Claude Code의
`close_window`는 **같은 서버 핸들러**(`JKWindowServer::HandleAgentQuery`)를
친다. 팔레트는 control-only 연결이 아니라 **일반 창 클라이언트**다 — 표면을
갖고(520×360, chromeless), 같은 파이프 위에서 M1의 에이전트 프로토콜을
쓴다. 서버의 `ProcessClientMessage`는 애초에 모든 클라이언트의 AgentQuery를
받아주므로 클라 측에 필요했던 것은 `JKClientSurface`에 채널 추가뿐.

## 2. 사용법

```bash
cd engine/build
./jkdesktop.exe --server &        # Alt+Space → 팔레트 등장 (다시 → 재활성화)
# 또는 직접: ./jkdesktop.exe --client palette
```

팔레트에 입력하는 결정적 커맨드:

| 커맨드 | 도구 | 비고 |
|---|---|---|
| `/list` | list_windows | 창 목록 JSON 표시 |
| `/launch <app>` | launch_app | 예: `/launch minesweeper` |
| `/close <id>` | close_window | **서버 관문** — 기본 deny, permissions.json 편집이 승인 |
| `/save <name>` | save_layout | `<exeDir>/state/layout_<name>.json` |
| `/restore <name>` | restore_layout | 타이틀 매칭 |
| `/undo` | restore_layout "pre_undo" | 직전 상태 복원 |
| `/help` | — | 커맨드 표 |

- **undo 칩**: `/close`, `/restore`는 먼저 자동으로 `save_layout "pre_undo"`
  스냅샷을 찍고, 그 응답이 도착해야 파괴적 커맨드를 보낸다 (스펙 §9 undo의
  첫 칸). 스냅샷이 실패하면 파괴적 커맨드는 실행되지 않는다.
- **이벤트 패널**: 팔레트 하단에 window.created/destroyed/focused가 실시간
  표시된다 — 스펙 §7 알림 센터의 최소판 (독립 앱은 M2b 트리거와 함께).
- 자연어 입력은 결정적 커맨드가 자리 잡은 뒤 파서→에이전트 위임으로 연결된다
  (현재는 안내 메시지).

## 3. 서버 측 변경 두 가지

1. **이벤트 구독 완화**: `PushAgentEvent`가 control-only 전용에서
   `AgentEventSubscriber()` 플래그만 보도록 — 창 클라이언트도
   `AgentEventSubscribe`를 보내면 이벤트를 받는다 (Task 5의 전제).
2. **서버 측 close_window 관문** (`JKWindowServer::AgentToolAllowed`):
   브로커(jkagentd)의 관문은 jkagentd를 거치는 호출만 지켜보지만, 팔레트처럼
   직접 연결하는 얼굴은 우회할 수 있다. 그래서 **같은
   `<exeDir>\permissions.json`(두 exe가 같은 빌드 디렉터리 → 같은 파일)을
   서버가 읽어** close_window를 게이트한다. 기본 deny, 파일 편집 = 승인 행위
   (M1 모델의 연속). 클라이언트 측 채널: `JKClientSurface`에
   `SendAgentQuery`/`SendAgentEventSubscribe`/`PollAgentReply`/
   `DrainAgentEvents` + `JKEventType::AgentReply` (큐 폴 모델 — WindowList와
   같은 패턴).

## 4. 테스트

```powershell
./build/jkdesktop.exe test                            # 0 failures
./build/jkagentd.exe --selftest                       # 0 failures
powershell -File tools/probes/probe_agent_mcp.ps1     # 5/5 PASS (M1 회귀)
powershell -File tools/probes/probe_agent_e2e.ps1     # 7/7 PASS (M1 회귀)
powershell -File tools/probes/probe_agent_palette.ps1 # 4/4 PASS (신규)
```

`probe_agent_palette.ps1`: 팔레트 스폰 → list_windows 표시 확인 → 서버 관문
deny 확인(agentctl은 브로커를 거치지 않으므로 순수 서버 관문 테스트) →
permissions.json 허용 후 팔레트 자신을 close → 목록에서 소멸 확인.

## 5. 제한 (M2a 스코프)

- **ImGui 폰트는 ASCII만** — 팔레트 UI 문자열은 전부 영어. 한글 IME/폰트는
  터미널 백로그(docs/26)와 같은 과제.
- 자연어 위임 미구현 (스펙 §6.2 후반부; 외부 에이전트 연동은 MCP 얼굴이 담당).
- 실행 전 레이아웃 미리보기 오버레이 없음 — undo 칩(pre_undo 스냅샷)만.
- Alt+Space 스폰은 타이틀 `"Command Palette"` 매칭 — 팔레트를 직접 닫은 뒤
  Alt+Space하면 새로 스폰된다.
- 이벤트는 created/destroyed/focused 3종 (스펙 §4 전체 세트는 4단계).

## 6. 다음 단계와의 연결

- **M2b 트리거 스크립트**: QuickJS `on(topic, filter, handler)` + .jkx
  매니페스트 `triggers:` — "빌드 실패 알림"이 이 팔레트의 이벤트 패널(또는
  그때까지 승격된 알림 센터 앱)로 도달한다.
- **내부 에이전트 채팅창**: JKWindow/SDL 체계 밖 (사용자 제약, M1 계획
  말미) — 승인 UX(인라인 프롬프트)와 함께 별도 계획.
- 앱 정복 사다리(스펙 §6.6)의 L1(테트리스)은 그대로 다음 대상.