# 43. engine/desktop 3-lib 분할 (P1) as-built

- 날짜: 2026-09-13
- 상태: 구현 완료 (커밋 08c5829 → 5659b50, 게이트 GATE_GREEN)
- 선행: docs/superpowers/specs/2026-09-13-engine-desktop-split-design.md (스펙),
  docs/42 (리플로우 — P1 직전 상태)

## 1. 개요

`jkdesktop.exe` 하나에 섞여 있던 서버/셸/클라 코드를 라이브러리 경계로
분리했다. 3개 정적 lib(jkcore/jkclient/jkserver) + desktop 셸 유닛 추출 +
`jkwinserver.exe` thin 서버 호스트. 컴포지터/연결 관리는 `src/server`로,
런처 셀/배경/스폰은 `desktop/`으로 이동하고 스폰 경로는 주입형이 되었다 —
스펙 §1 "서버 = 어리석은 컴포지터"(docs/28) 원칙을 코드 구조로 실현한 단계.

어느 단계서든 멈춰도 빌드 그린·프로브 회귀 PASS인 상태(스펙 B안)를 유지했고,
최종 게이트는 GATE_GREEN. 로드맵 위치: P1 완료 → 다음은 P2 테마 시스템
(Win11 모던, Theme 구조체 중앙화 — 셸/서버가 분리된 덕에 어디에 둘지가
이제 결정 가능).

## 2. 단계별 커밋 (BASE 6ee11b1 → HEAD 5659b50)

| 커밋 | 내용 | 스펙 단계 |
|---|---|---|
| 08c5829 | refactor(ipc): 파이프명 통일 → `jk::ipc::kWindowServerPipeName` (`include/ipc/JKWireEndpoints.h`) | ① |
| ea15ae4 | build: jkserver static lib 추출 (4 server 소스, PUBLIC jkcore) | ① |
| 6d991a7 | build: jkclient static lib 추출 (6 파일); 22개 jkapp_* 전부 jkclient 링크 | ② |
| 1688af2 | refactor(server): desktop 셸 추출 (`JKDesktopShell` + `ShellHost` 주입) | ③a |
| 5659b50 | feat(build): jkwinserver.exe thin 서버 호스트 | ③b |

## 3. lib 경계

| 유닛 | 소유 | 위치 |
|---|---|---|
| **jkcore** (static) | 위젯(JKControl 계열)/DC/한글/오디오/ipc(wire+pipe+shmem)/agent client+json/이벤트 | include/ 루트 + src/ipc, src/agent |
| **jkclient** (static, jkcore 위) | JKClientSurface/JKClientApplication + 터미널 스택(파서/그리드/아틀라스/컨피그; ConPTY 브리지는 jkcore 잔존 — §5 R2) | src/client, src/terminal |
| **jkserver** (static, jkcore 위) | JKWindowServer/JKCompositor/JKCompositorLayer/JKCompositorOutput/JKClientConnection | src/server |
| **desktop 셸** (유닛) | launcher 셀/배경/스폰 (`JKDesktopShell` + `ShellHost` 주입) | desktop/ |

의존 규칙 (엄수):

- `jkwinserver → desktop → jkserver → jkcore`, `jkclient → jkcore`, `앱 → jkclient`.
- **jkserver는 jkclient를 링크하지 않는다** — 서버는 표면 픽셀만 받고, 서버-클라
  교신은 와이어 IPC.
- 앱 모듈(22개 jkapp_*)은 jkclient를 링크 — 서버 lib에는 의존하지 않는다.
- **jkagentd / jkchat / jktriggers는 jkcore만 링크** — 서버 lib이 생겼어도
  에이전트 계열은 클라 스택(터미널 포함)에 의존하지 않는다.

## 4. 빌드 매트릭스 변화

분할의 실용 효과는 "무엇을 고쳤느냐에 따라 무엇을 다시 빌드하는가"가
쪼개진 것이다.

**이전**: jkcore가 사실상 전 소스를 흡수 → 서버 소스 한 줄 수정이
26타깃 전부 재링크(풀 리빌드)로 번졌다.

**이후** (변경 유형별):

| 수정 대상 | 다시 빌드/링크 | 앱 DLL 재링크 |
|---|---|---|
| 서버 소스 (`src/server/*`) | `--target jkwinserver` (+ 필요 시 jkdesktop 재링크) | 없음 |
| 클라/터미널 스택 | jkclient 재빌드 + 앱 DLL 재링크 + **jkx_packages 재포장** (레슨 18) | 있음 |
| 셸 (`desktop/`) | jkdesktop_shell + jkwinserver/jkdesktop 재링크 | 없음 |
| 풀 빌드 | `cmake --build .` (변함없음) — | — |

전제와 관례:

- exe mtime > 소스 mtime 확인(레슨 37) — 리링크 후 살아있는 exe로 게이트가
  가려지는 재발 방지.
- PATH 전제: `export PATH="/c/msys64/ucrt64/bin:$PATH"` (cc1plus DLL 로드).

## 5. 노출 API 표면

분할은 컴파일 경계를 넘어 소비자가 붙는 **공개 API 표면**을 만든다. 사용자
지시("노출하는 API가 생기겠네요")에 따라 기록:

| 계층 | 헤더/심볼 | 소비자 |
|---|---|---|
| 앱 SDK | `include/apps/JKAppModule.h` (C ABI, 불변) + JKClientApplication/JKClientSurface + JKControl 위젯/한글/플랫폼 (jkcore) | 앱 모듈 (jkclient 링크) |
| 터미널 스택 | `include/terminal/*` (파서/그리드/아틀라스/컨피그; 브리지는 jkcore) | 터미널 계열 앱, jkagentd |
| 셸/서버 | `JKWindowServer` 공개 메서드 (Init/Run/StartAcceptor/**SetClientHostExe**) + `desktop/JKDesktopShell.h`의 `ShellHost{renderer,outputScale,makeTexture,launch}` | jkwinserver main (향후 별도 프로세스 셸) |
| 와이어 | `include/ipc/JKWireProtocol.h` + `JKWireEndpoints.h` (`kWindowServerPipeName`) | 서버↔클라↔에이전트 |

P4 연결: 헤더 위생(앱 SDK가 서버 헤더를 노출받지 않게 jkclient PUBLIC
include 좁히기), 앱 템플릿 스니펫, ShellHost 계약 문서화 — P4(앱 개발
환경)의 첫 과제로 이어진다.

## 6. 실행 중 결정 (스펙 §8 D9–D12)

- **R1 — `jkserver PUBLIC jkdesktop_shell` 링크 방향**: 스펙 §2 표의
  "desktop → jkserver"와 링크 방향이 반대. 판정: **인클루드 방향**(desktop이
  jkserver 헤더를 include하지 않음)은 유지됐다 — ShellHost 콜백 구조체로
  역결합. 링크를 서버가 소유하는 것은 셸 유닛의 수명을 서버가 관리하기
  때문. 스펙 §8 직감("경계를 CMake가 안 존중한다")이 실제로 재현된 지점.
  틀리면: 셸 수정마다 서버 재링크 — 현재 매트릭스(§4)가 정확히 그 비용.
- **R2 — JKConPtyBridge.cpp는 jkcore 잔존**: 브리프 이동 목록에 있었으나
  jkagentd의 terminal_exec가 브리지를 직접 구동
  (`engine/tools/jkagentd/main.cpp:248`). 브리지는 "클라 스택"이 아니라
  양측이 공유하는 ConPTY 프로세스 I/O 기반시설. 틀리면: jkclient에 두면
  jkagentd가 jkclient 링크를 강요당함 — 에이전트가 클라 스택 의존. P4 재검토.
- **R3 — `LaunchAt(x,y)` 추가**: 셸로 launcherIcons_가 이동하면서 스폰 키
  (appName/jkxPath)가 서버 손 밖으로 감 → HitTest만으론 스폰 불가. 히트테스트와
  스폰을 한 메서드로 캡슐화, 기존 분기 순서와 동일(리뷰어 대조 확인). 틀리면:
  서버가 셸 내부 상태(아이콘 표)를 알아야 함 — 역결합 붕괴.
- **R4 — `SetClientHostExe`**: SpawnProcess의 `jkdesktop.exe --client`
  하드코딩 제거. 셸 초기화 인자(스펙 ③ 원안)가 아니라 **서버 멤버 + 세터**로 —
  jkwinserver thin main이 서버 구동 후 주입. 틀리면: 셸이 호스트 exe를
  알게 되어 별도 프로세스 셸 전환(D7) 경로가 막힌다.

## 7. 검증

- **풀 빌드 exit 0**; 두 exe가 engine/src+include 전체보다 최신 (jkdesktop
  초기 mtime 지연은 Task 5 빌드 순서 유물 — relink로 해소).
- **`jkdesktop test` → 0 failures**.
- **프로브 15/15 PASS 무리트라이** (`engine/tools/probes/`): mcp, e2e,
  palette, chat, chat_llm, triggers, trust, ratelimit, notify, triggerctl,
  shot, maximize, desktop_resize, terminal_mouse, terminal_select.
- **스모크 2종**: `engine/tmp/smoke_jkwinserver.ps1` (jkwinserver.exe로
  서버 창 + agentctl launch_app minesweeper) PASS; `jkdesktop --server`
  셀프스폰 PASS (teardown clean).
- 비고: probe_agent_ratelimit 3m02s 소요(문서 ~70s) — 전 게이트 PASS,
  머신 sleep 누적으로 추정.

## 8. 후속 과제

미해결 마이너 (P1 범위 유지로 남김):

- EOF 개행 없음 4종: JKWireEndpoints.h, JKDesktopShell.h/.cpp,
  jkwinserver_main.cpp
- CMakeLists jkapp_terminal 블록 구식 코멘트 ("parser/grid/atlas/bridge
  are in jkcore" — 브리지는 jkcore 잔존이 맞지만 문구가 구식)
- LaunchAt의 `if (host_.launch)` 방어 가드 (와이어링 순서상 무의미)
- mtime 게이트 상설화 시 의존 순서 빌드 필요
- probe_agent_ratelimit 3m02s (문서 ~70s) — 원인 규명

다음: **P2 테마 시스템** (Win11 모던). 이후 P4(앱 개발 환경)에서 헤더
위생/앱 템플릿/ShellHost 계약 문서화(§5 P4 연결)와 R2 브리지 소속 재검토.