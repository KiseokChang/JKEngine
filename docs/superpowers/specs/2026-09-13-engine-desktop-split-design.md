# engine/desktop 3-lib 분할 설계 (P1)

> 2026-09-13. 디자인 개선 로드맵의 첫 하위 프로젝트. 상위 로드맵 문맥은
> 브레인스토밍 대화(2026-09-13)에서 확정 — 스펙 §8에 결정/직감 장부로 기록.
> 기존 설계 리뷰(2026-09-05, jkdesktop 리네임 시 서브에이전트 리뷰)의 결론을
> 기반으로 하되 점진적 단계로 재구성. 스펙 작성 시점 코드 기준:
> `JKWindowServer.cpp` 2,989행, `main.cpp` 모드 디스패처(툴/단일 앱/서버/클라
> 전부), `jkcore` = 사실상 전 소스를 담는 정적 lib (CMakeLists.txt:121).

---

## 0. 배경과 순서 결정 (상위 로드맵)

사용자가 "window와 데스크탑, 시스템 전반의 디자인 개선"을 원함 → 4층 전부
선택(비주얼/테마, UX/인터랙션, 시스템 아키텍처, 전체 조율) → Win11 모던 룩
확정 → 테마 도달범위는 "단계 확장(셸 먼저)" → **split 먼저** 결정.

| # | 하위 프로젝트 | 내용 | 의존 |
|---|---|---|---|
| **P1** | **engine/desktop split (본 스펙)** | 3-lib 분할 + desktop/ 셸 추출 | 없음 |
| P2 | 테마 시스템 (Win11 모던) | Theme 구조체 중앙화 + 셸 적용 → 네이티브 앱 → ImGui 앱 → 터미널 | P1 |
| P3 | UX 기능군 | 창 스냅 → Alt-Tab → 시작 메뉴 → 전환 애니메이션 | P2 |
| P4 | 앱/클라 개발 환경 | SDK 표면(헤더/링크), 앱 템플릿, 빌드/디버그 워크플로, 앱 디렉터리 정리(MineSweeper↔TetrisApp.cpp 커플링 포함) | P1 마무리 후 고민 |

---

## 1. 목표와 성공 기준

**목표**

1. `jkdesktop.exe`에 섞인 서버/셸/클라 코드를 라이브러리 경계로 분리해,
   P2(테마)·P3(UX)·P4(앱 개발 환경)가 각자 맞는 표면 위에서 진행되도록 한다.
2. 셸(launcher/배경/스폰)이 서버 코어에서 떼어져 "서버 = 어리석은 컴포지터"
   원칙(docs/28)이 코드 구조로 실현된다.
3. 언제 멈춰도 빌드 그린·프로브 회귀 PASS인 완성 상태를 유지한다 (B안 —
   점진적 split의 본질).

**성공 기준**

- 각 단계(§3 ①②③)마다: 빌드 그린 + `jkdesktop test` 0 failures + 기존
  프로브 13종 전부 회귀 PASS.
- 최종 상태: `JKWindowServer.cpp`에는 컴포지터/연결 관리만 남고, 런처 코드는
  desktop/로 이동, 스폰 경로는 주입형.
- 기존 스모크가 전부 여전히 동작 (exe 모드 이름 불변).

---

## 2. 최종 구조 (3-lib)

| 유닛 | 소유 | 현재 위치 |
|---|---|---|
| **jkcore** (static) | 위젯(JKControl 계열)/DC/한글/오디오/ipc(wire+pipe+shmem)/agent client+json/이벤트 | include/ 루트 + src/ipc, src/agent |
| **jkclient** (static, jkcore 위) | JKClientSurface/JKClientApplication + 터미널 스택(JKVtParser/JKTerminalGrid/JKGlyphAtlas/JKConPtyBridge/JKTerminalConfig) | src/client, src/terminal |
| **jkserver** (static, jkcore 위) | JKWindowServer/JKCompositor/JKCompositorLayer/JKCompositorOutput/JKClientConnection | src/server |
| **jkwinserver.exe** | thin 호스트 — 서버 모드 전용 main | src/main.cpp의 `--server` 경로 분리 |
| **desktop/** (셸 유닛) | launcher 셀/배경/스폰 로직 (JKWindowServer.cpp 2,676–2,860행) | JKWindowServer.cpp에서 추출 |

불변(건드리지 않음):

- 앱 DLL·.jkx 토폴로지 — 앱은 jkclient(+jkcore)를 링크, 서버만 jkserver에
  의존. C ABI(`include/apps/JKAppModule.h`) 경계 유지.
- imgui/implot/quickjs/FFmpeg 토폴로지 — jkcore 밖 규칙 유지.
- 스모크/프로브의 exe 모드 이름 (`jkdesktop`).

의존 방향 (엄수): `jkwinserver → desktop → jkserver → jkcore`,
`jkclient → jkcore`, `앱 → jkclient`. jkserver는 jkclient를 링크하지 않는다
(서버는 표면 픽셀만 받는다 — 실제 서버-클라 교신은 와이어 IPC이므로 이미 그렇다).

---

## 3. 진행: 점진적 3단계

### ① jkserver 추출 (핵심, 1–1.5세션)

- `src/server/*` 5개 파일 → 신규 `jkserver` static lib (jkcore 링크).
  CMakeLists.txt jkcore 소스 목록(:170-173)에서 제거, `jkdesktop`이 둘 다 링크.
- **파이프명 통일**: `\\.\pipe\JKWindowServerPipe`가 main.cpp 3곳(:2789,
  :2795, :2889)에 하드코딩 → 하나의 상수(공개 헤더, 예: `include/ipc/`의
  와이어 상수와 함께)로.
- exe 분리는 이 단계에서 하지 않음 — 링크 경계만.

### ② jkcore/jkclient 경계 정리 (1세션)

- jkcore에서 `src/client/*`, `src/terminal/*`를 `jkclient` lib으로.
- `JKENGINE_FONT_DIR`/`JK_SCRIPTS_DIR` 컴파일 def(:190-193)는 jkcore에 유지
  하되 목적 명확화: 터미널/스크립트가 실제로 필요로 하는 것만 전파. P1 범위에
  서는 동작 변경 없음 (런타임 결정으로의 전환은 P4 과제로 넘김 — §5 리스크 2).
- 검증: 앱 DLL 전부 리링크 + 터미널/스크립트 셀프테스트(`jkdesktop test`).

### ③ desktop/ 셸 추출 + jkwinserver.exe (1세션)

- `InitLauncher`/`DrawLauncher*`/런처 셀/배경(약 300행) → `src/desktop/`.
  셸 렌더·스폰은 JKWindowServer가 제공하는 공개 인터페이스(레이어 추가, 스폰
  훅, 이벤트 구독)를 통해 호출 — 셸은 jkserver 위에 얹는 클라이언트-격녀
  유닛 — 실제 연결이 아니라 프로세스 내 특권 클라이언트, taskbar와 동일 원칙).
- `SpawnProcess`의 `jkdesktop.exe --client` 하드코딩 → **호스트 exe명 주입**
  (셸 초기화 인자로 받음). 셸이 자기 호스트를 알아야 하는 유일한 지점.
- main.cpp 최종 분리: `jkwinserver.exe`(서버 모드만) + `jkdesktop.exe`
  (기존 모드 전부 유지 — 스모크 회귀 보호). `jkdesktop`에 `--server` 모드는
  남겨도 무방하나, 스펙 결정: 남긴다 (파워셸 스크립트 호환).
- 검증: 서버/taskbar/terminal/launcher 실스폰 스모크 (스폰 경로 주입 전환).

---

## 4. 검증

- 각 단계 끝: `jkdesktop test` + 프로브 13종 회귀
  (`engine/tools/probes/probe_*.ps1` — mcp/e2e/palette/chat/chat_llm/triggers/
  trust/ratelimit/notify/triggerctl/shot/maximize/desktop_resize 등).
- ③ 후 신규: `probe_desktop_resize.ps1` + 런처 스폰 스모크를 조합해
  주입형 스폰 경로 검증.
- 빌드 산출물 mtime 확인 관례(레슨 37) 유지 — 리링크 후 살아있는 exe로 인한
  가려짐 재발 방지.

---

## 5. 리스크 / 커플링 (기존 리뷰 3대 + 신규)

1. **파이프명 하드코딩** — ①에서 상수 통일로 해소.
2. **`JKENGINE_FONT_DIR`/`JK_SCRIPTS_DIR` 소스 절대경로 def** — ②에서
   전파 범위만 정리하고, 런타임(exe-dir 상대) 결정 전환은 **P4로 연기**
   (동작 변경을 P1에 섞지 않는다 — 결정 §8-D4).
3. **스모크의 exe 모드 의존** — exe 분리는 ③ 맨 끝 + `jkdesktop` 클라/스모크
   모드 유지로 해소.
4. **MineSweeper가 TetrisApp.cpp를 컴파일하는 소스 커플링** — P1 범위 밖,
   P4(앱 개발 환경)에서 정리 (결정 §8-D5).

---

## 6. P2 이후 표면 (미리보기 — 본 스펙 범위 밖)

- P2 테마: `Theme` 구조체가 jkserver(크롬/배경)와 desktop/(런처)·taskbar가
  공유하는 단일 진실원이 되어야 함 → split 후 desktop/·jkserver 표면이 정리돼
  있어 어디에 두는지 자연히 결정됨. ①-③ 동안 크롬/런처 색 하드코딩을 **건드리지
  않는다** — P2에서 한 번에 중앙화 (이중작업 방지, 결정 §8-D6).
- P3 UX: 창 스냅은 chrome grab machinery(jkserver)와 최대화 트래킹
  (preMaxRects_, docs/39) 위에서 — ①로 jkserver가 분리된 뒤 진행.

---

## 7. 검증 플레이북 참조

- docs/15_verification_playbook.md — 프로브/스모크 규약.
- 레슨 37 (빌드 산출물 grep 필터 금지, mtime 확인) — 모든 단계에 적용.

---

## 8. 결정/직감 장부 (문서 = 산출물)

| # | 결정/직감 | 근거 |
|---|---|---|
| D1 | **P1을 먼저, 테마/UX 나중** (사용자 결정 2026-09-13) | 기능이 붙기 전에 경계를 찢으면 나중 고통이 덜함 — 테마·UX 작업이 정리된 표면 위에서 진행 |
| D2 | **접근안 B(점진적 split)** (사용자 결정) | A(풀 split)의 최종 구조 + C(최소 split)의 속도 사이; 어느 단계서든 멈춰도 완성 상태 유지. A/C는 폐기하지 않고 B의 단계 목표로 흡수 |
| D3 | **exe 분리(jkwinserver)를 ①이 아니라 ③과 함께** | 직감: main.cpp 이동/분리는 파일 이동을 한 번으로 몰아주는 게 낫다. ①은 링크 경계만 — 중간 상태의 임시 경계 최소화 |
| D4 | **font/scripts dir의 런타임 결정 전환은 P4로 연기** | P1은 구조 이동만, 동작 변경은 안 섞는다 — 회귀 원인 규명 비용 분리 (직감: "구조"와 "동작" 변경은 커밋도 세션도 섞지 않는다) |
| D5 | **MineSweeper↔TetrisApp 커플링은 P4로** | split 범위 밖. P4 = 앱 개발 환경의 첫 과제로 자연 배치 (사용자 지시 "P1 마무리되면 CLIENT(앱) 개발 환경 구성도 고민") |
| D6 | **크롬/런처 색 하드코딩은 P1에서 건드리지 않는다** | P2 테마 작업에서 Theme 구조체로 한 번에 중앙화 — P1에서 고치면 P2에서 이중작업 |
| D7 | **셸(desktop/)은 "프로세스 내 특권 클라이언트" 모델** | taskbar(docs/28)의 "서버는 어리석은 컴포지터" 원칙을 구조로 실현 — 셸이 서버 공개 인터페이스만 쓰면 추후 별도 프로세스 셸로도 전환 가능 (직감: 지금은 in-process, 경계만 지금 그리는 것) |
| D8 | **문서 = 산출물 원칙** (사용자 지시 2026-09-13) | 결정과 직감을 장부로 기록 — 본 §8이 그 실천. 이후 P2/P3/P4 스펙에도 동일 섹션 유지 |
| 직감 | jkserver가 jkclient를 링크하지 않는 경계는 **이미 사실** (서버-클라 교신이 와이어 IPC) — "경계가 없다"가 아니라 "경계를 CMake가 안 존중한다"가 현실. lib으로 찢는 것은 사실상 서술화 | 설계 리뷰 + 와이어 프로토콜 구조 |
| 직감 | desktop/ 추출의 진짜 가치는 런처 코드 이동이 아니라 **P2 테마가 셸과 서버에 각각 접근할 수 있게 되는 것** | 섹션 6 — P2 표면 미리보기 |
| D9 | **`jkserver PUBLIC jkdesktop_shell` 링크 방향은 스펙 §2 표와 반대로 확정** (실행 중, docs/43 §6 R1) | 인클루드 방향(desktop이 jkserver 헤더를 include하지 않음)은 유지 — ShellHost 콜백 구조체로 역결합. 링크를 서버가 소유하는 이유는 셸 유닛의 수명을 서버가 관리하기 때문. 스펙 §8 직감("경계를 CMake가 안 존중한다"가 현실)이 재현된 지점 |
| D10 | **JKConPtyBridge.cpp는 jkclient가 아니라 jkcore 잔존** (실행 중, docs/43 §6 R2) | jkagentd의 terminal_exec가 브리지를 직접 구동(engine/tools/jkagentd/main.cpp:248) — 브리지는 "클라 스택"이 아니라 양측 공유 ConPTY 프로세스 I/O 기반시설. jkclient로 옮기면 jkagentd가 jkclient 링크를 강요당함. P4에서 재검토 |
| D11 | **셸 스폰을 `LaunchAt(x,y)` 한 메서드로 캡슐화** (실행 중, docs/43 §6 R3) | 셸로 launcherIcons_가 이동하면서 스폰 키(appName/jkxPath)가 서버 손 밖으로 감 → HitTest만으론 스폰 불가. 히트테스트+스폰을 캡슐화해 서버가 셸 내부 상태(아이콘 표)를 모르게 유지 |
| D12 | **호스트 exe 주입을 셸 초기화 인자가 아니라 서버 멤버 + `SetClientHostExe` 세터로** (실행 중, docs/43 §6 R4) | SpawnProcess의 `jkdesktop.exe --client` 하드코딩 제거 — jkwinserver thin main이 서버 구동 후 주입. 셸이 호스트 exe를 모르면 별도 프로세스 셸 전환(D7) 경로가 열린 채 유지됨 |
| 직감 | **링크 방향과 인클루드 방향은 별개의 결정** — "누가 헤더를 보는가"(역결합 설계)와 "누가 lib을 링크하는가"(수명 소유)가 다른 답을 내놓을 수 있다. 스펙 표는 둘을 하나로 그리지 말 것 (D9에서 실제로 갈라짐) | docs/43 §6 R1 실행 결과 |
