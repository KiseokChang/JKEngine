# Linux 셸 진입 경로 봉합 (F7) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/71 §5의 F7 셸 진입 경로 5종을 봉합해 WSL 리눅스 서버가 데스크톱 셸(taskbar)을 포함한 실제 클라이언트를 스폰·진입하게 만든다.

**Architecture:** posix에 이미 존재하는 `jkapp_*.so` 20종 모듈과 `jk_app_meta`/`jk_app_run_client` ABI 실측(nm 확인) 위에서, win32 전용인 것(dlopen 트리오·모듈 접미 프로브·taskbar 자동스폰·정직 답변)을 플랫폼 분기로 개통한다. 모든 픽스는 **posix leg 신설/플랫폼 상수화** — win32 분기는 바이트 보존.

**Tech Stack:** 플랜 E/F 스택 그대로 — jk::ipc unix socket fold, jk::process::Spawn posix leg, posix_selftest.

**Spec/상위:** docs/71 §5 (F7 잔차 5종 — 최종리뷰 확정), docs/70 §4 (Linux 제외 v1 — jkx **팩/리팩/리스트/익스트랙트 도구**는 v1 유지; 컨테이너 *적재*는 이 플랜의 범위 밖으로 유지). 플랜 F: docs/superpowers/plans/2026-10-05-linux-residue-seal.md + docs/71.

## Global Constraints

- **Windows 무변동 원칙** — 모든 태스크에서 win32 분기의 동작·문자열은 바이트 보존. 픽스는 posix leg 신설 또는 플랫폼 상수(`#ifdef`)로만. 승인 편차는 F의 #1(스탬프)·#2('/') 관측 표면)이고 이 플랜에 신규 win32 관측 변화를 두지 않는다.
- **ec 중립형** — 새 std::filesystem 호출은 전부 `std::error_code` 오버로드. 이 TU들(특히 JKWindowServer.cpp)에는 try/catch가 없으므로 throwing 오버로드는 review r1 HIGH 사망.
- **dlclose 금지** — RunClientModule의 FreeLibrary 주석("unloads corrupt heap")과 동일한 근거로 posix leg도 dlopen 후 닫지 않는다(호스트 프로세스가 곧 종료됨).
- **정직 답변의 플랫폼 한정** — G4의 `ok:false` 정직화는 posix에서만 관측된다(win32 기존 답변 바이트 보존 — win32 SpawnClient 실패 시에도 기존 `{"ok":true}` 유지).
- **jkx v1 경계 유지** — `--jkx` 적재 route와 jkx 팩 계열 도구는 win32 게이트 밖으로 꺼내지 않는다(docs/70 §4 유지). G2는 --client/--filedlg만 개통한다.
- **빌드 명령**: 윈도우 `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH" && /c/msys64/ucrt64/bin/ninja.exe -C build -j3` (engine/에서, 라이브 스택 정지 확인). WSL `wsl.exe -d Ubuntu-24.04 -e sh -c "cd /mnt/i/progwork/JKENGINE/engine/buildwsl && ninja -j4"`.
- 커밋 트레일러 `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

---

### Task G1: 모듈 접미 플랫폼화 + 서버 엔드포인트 경로 보조

**Files:**
- Modify: `engine/include/server/JKWindowServer.h` (clientHostExe_ :623 부근 — 접미 헬퍼 배치)
- Modify: `engine/src/server/JKWindowServer.cpp:4236` (launch_app 프로브), `:492-521` (taskbar 자동스폰 블록)
- Modify: `engine/include/ipc/JKWireEndpoints.h` (신규 inline `DefaultServerEndpointPath()`)

**Interfaces:**
- Consumes: 없음(신규).
- Produces: `namespace jk::ipc { inline const char* DefaultServerEndpointPath(); }` — win32 `kWindowServerPipeName` / posix `"/tmp/JKWindowServerPipe.sock"`(MapEndpointName fold 규칙의 결정론 결과 — JKPipeTransport_posix.cpp:33-43 fold 규칙 인용 주석 필수). `jk::server::AppModuleSuffix()` 또는 파일-로컬 static `AppModuleSuffix()` → win32 `".dll"` / posix `".so"` — **G2·G3가 소비**.

- [ ] **Step 1: 서버 엔드포인트 보조 신설** — JKWireEndpoints.h에 inline 함수 추가:

```cpp
// posix에서 pipe-name 상수가 전송 어댑터의 fold 규칙(JKPipeTransport_posix
// MapEndpointName: pipe-접두 박탈+'/'→'_'+.sock)으로 결정론 결과가
// /tmp/JKWindowServerPipe.sock — 셸 스폰 전 대기 루프 등 "파일 존재" 관측자가
// 그 결과 경로를 알아야 할 때 쓴다(플랜 G3). win32는 파이프 이름 그대로.
inline const char* DefaultServerEndpointPath();
```
구현은 헤더 안 플랫폼 분기(win32: `return kWindowServerPipeName;` / posix: `return "/tmp/JKWindowServerPipe.sock";`) — fold 규칙이 바뀌면 양쪽을 같이 고쳐야 한다는 주석.

- [ ] **Step 2: 접미 헬퍼** — JKWindowServer.h의 clientHostExe_ 선언 부근(플랫폼 기본값 블록 뒤)에:

```cpp
// 클라 모듈 접미 — 플랜 E가 앱 모듈을 posix에 .so로 빌드(buildwsl 실측 20종),
// win32는 .dll(dllimport 계약 유지). 스폰 전 "모듈 있나" 프로브의 관측 동일성
// (존재 bool)은 그대로 — 접미만 플랫폼 값.
inline const char* AppModuleSuffix();
```

- [ ] **Step 3: 프로브 2곳 치환** — `:4234` `exeDir + "/jkapp_" + app + ".dll"` → `AppModuleSuffix()` 사용; `:516` `dirSelf + "/jkapp_taskbar.dll"` → 동일. stderr 문구 "no jkapp_taskbar.dll"은 접미를 포맷에 넣되 **win32에서 바이트 동일 유지**(`"no jkapp_taskbar.dll — desktop runs without a shell"`이던 관측 유지 — 접미가 .dll이므로 자동 유지).
- [ ] **Step 4: 빌드 확인** — 윈도우 ninja RC=0 (라이브 스택 정지 확인) + WSL ninja RC=0.
- [ ] **Step 5: 커밋** `feat(g1): 클라 모듈 접미 플랫폼화+서버 엔드포인트 경로 보조`

### Task G2: posix 클라 모듈 로더(dlopen)+--client/--filedlg route 개통

**Files:**
- Modify: `engine/src/main.cpp` — 상단 플랫폼 lib 선언(:1-32), `RunClientModule`(:460), `RunClientFromJkx`(:496, temp 경로만), route 게이트 :3334-3349(--client)/:3351-3362(--filedlg)

**Interfaces:**
- Consumes: G1 `AppModuleSuffix()`, `jk::ipc::DefaultServerEndpointPath()`.
- Produces: posix에서 `./jkdesktop --client <app>` → dlopen `jkapp_<app>.so` → jk_app_run_client(kPipe) — kPipe는 `jk::ipc::kWindowServerPipeName` 그대로(전송 어댑터가 fold).

- [ ] **Step 1: 플랫폼 lib 트리오** — 상단 `#ifdef _WIN32` 선언들은 그대로; `#else`: `#include <dlfcn.h>`(상단 include 블록에 `_WIN32` 밖으로 무해 추가해도 됨 — MinGW dlopen 아님 주의, include는 #else 안).

- [ ] **Step 2: RunClientModule 플랫폼 분행** — 로드/조회만:

```cpp
static int RunClientModule(const char* modulePath, const char* pipeName) {
    MirrorClientStderr(modulePath);
#ifdef _WIN32
    void* module = LoadLibraryA(modulePath);
#else
    // RTLD_LOCAL: 모듈 심볼을 전역 네임스페이스에 흘려보내지 않는다 —
    // 여러 앱을 한 프로세스가 잡는 일은 없지만(=client 모델) 단일 앱이라도
    // 호스트 main의 심볼과 간섭하지 않는 게 원칙. 닫지 않는다(FreeLibrary
    // 주석 — 힙 손상 선례; posix leg도 동일: 호스트가 곧 종료됨).
    void* module = dlopen(modulePath, RTLD_NOW | RTLD_LOCAL);
#endif
    if (!module) {
#ifdef _WIN32
        std::fprintf(stderr, "Cannot load app module '%s'\n", modulePath);
#else
        std::fprintf(stderr, "Cannot load app module '%s' (%s)\n", modulePath, dlerror());
#endif
        return 1;
    }
#ifdef _WIN32
    auto metaFn = reinterpret_cast<...>(GetProcAddress(module, "jk_app_meta"));
    auto runFn  = reinterpret_cast<...>(GetProcAddress(module, "jk_app_run_client"));
    ... 기존 그대로 ...
#else
    auto metaFn = reinterpret_cast<const jk::JKAppMeta* (*)()>(dlsym(module, "jk_app_meta"));
    auto runFn  = reinterpret_cast<int (*)(const char*)>(dlsym(module, "jk_app_run_client"));
    if (!metaFn || !runFn) { fprintf "no jk_app_meta/jk_app_run_client"; return 1; }
#endif
    ... 이하(meta 인쇄·runFn 호출) 공용 유지 ...
}
```

- [ ] **Step 3: RunClientFromJkx — temp 관련만 플랫폼 분기** (route는 v1 게이트 유지 — Step 4에서 --jkx는 그대로 두고 여기 몸통만 이식성 확보):

```cpp
// temp: win32 GetTempPathA/GetCurrentProcessId → 플랫폼 분기
#ifdef _WIN32
    ... 기존 그대로 ...
#else
    std::error_code tempEc;
    std::string tempDirStr = std::filesystem::temp_directory_path(tempEc).string();
    if (tempEc) tempDirStr = ".";
    if (!tempDirStr.empty() && tempDirStr.back() != '/') tempDirStr += '/';
    long pid = static_cast<long>(getpid());
    // tempPath 조립이 기존 snprintf("%sjkapp_%s_%lu.dll")와 동일 형태
#endif
```
`#include <unistd.h>`(posix include는 기존 main.cpp posix include 블록에 이미 있는지 확인 — 없으면 추가).

- [ ] **Step 4: route 게이트 개통** — :3334-3349(--client): `#else` 스텁 삭제 → posix leg:

```cpp
#else
        // posix 클라 route 개통(플랜 G2): 플랜 E가 앱 모듈 .so 20종을 이미
        // 빌드하고 ABI jk_app_meta/jk_app_run_client 노출을 실측(nm -D GREEN).
        const std::string soName =
            std::string("jkapp_") + clientApp + jk::server::AppModuleSuffix();
        return RunClientModule(soName.c_str(), kPipe);
#endif
```
:3351-3362(--filedlg) 동일(`RunClientModule(std::string("jkapp_filedlg") + suffix)`, kPipe). **:3453(--jkx)은 v1 게이트 유지** — 건드리지 않는다. main.cpp 안 `jk::server::AppModuleSuffix()` 가시성 확인(JKWindowServer.h include 여부 — 없으면 main.cpp에 include 추가하거나, 루프 방복을 피해 접미 문자열을 JKAppModule.h 쪽 헬퍼로 옮겨도 허용 — 헬퍼 위치 최종 결정은 구현자 몫, G1 인터페이스 소비가 어긋나지 않게 **단일 정의 원칙** 유지).
- [ ] **Step 5: 검증** — 윈도우 ninja RC=0 + `./build/jkdesktop.exe --client minesweeper` 수동 스모크(실행→창 뜸→수동 종료; 자동화엔 AppSelfTest가 대체) + 스택 재기동. WSL: `sh -c "cd buildwsl && timeout 5 ./jkdesktop --client minesweeper"` — `[client] module loaded` 인쇄 + 서버 없이 connect 실패 관측(0이 아니어도 로드 성공이면 이 태스크 관측 충족).
- [ ] **Step 6: 커밋** `feat(g2): posix 클라 모듈 dlopen 로더+--client/--filedlg route 개통`

### Task G3: taskbar 자동 스폰 posix 개통

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp:492-521` — `#ifdef _WIN32` 게이트 소멸, 플랫폼 분할은 블록 내부의 대기 수단만

**Interfaces:**
- Consumes: G1 `AppModuleSuffix()`/`DefaultServerEndpointPath()`, G2가 개통한 `--client taskbar` posix route(SpawnClient("taskbar") → `sh -c '<dir>/jkdesktop --client taskbar'`).

- [ ] **Step 1: 블록 개통** — :492 `#ifdef _WIN32` / :521 `#endif` 소멸. 대기 소수만 분기:

```cpp
    // 기존 win32 대기: WaitNamedPipeA ×20 — 그대로 win32 leg에 남긴다
#ifdef _WIN32
    for (int i = 0; i < 20; ++i) {
        if (WaitNamedPipeA(pipeName_.c_str(), 20)) break;
        Sleep(10);
    }
#else
    // posix(플랜 G3): unix socket 파일의 존재를 같은 상한(~200ms)으로 폴링 —
    // 클라에 connect 재시도가 없는 것(win32 관측)을 동형으로 보존.
    for (int i = 0; i < 20; ++i) {
        std::error_code waitEc;
        if (std::filesystem::exists(jk::ipc::DefaultServerEndpointPath(), waitEc)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#endif
```
`exePathAutoSpawn`·`find_last_of("\\/")`(허용 사이트)·dll 프로브(AppModuleSuffix)·SpawnClient("taskbar")·stderr 문구는 기존 그대로.
- [ ] **Step 2: 빌드** — 양쪽 RC=0.
- [ ] **Step 3: 검증 스모크 (WSL)** — `engine/tmp/f6_smoke.sh` 기반 `f7_smoke.sh` 신설: 서버 기동 시 서버 로그에 taskbar 스폰 라인이 나고, `list_windows` 응답 **windows 배열 비어 있지 않음**(F7 왕복 증명 — docs/70 §6 #8의 원래 관측 목표). 라인 예 `JKWindowServer: spawned jkdesktop --client taskbar`.
- [ ] **Step 4: 커밋** `feat(g3): taskbar 자동 스폰 posix 개통 — WSLg 셸 부팅`

### Task G4: 정직 답변(posix 한정) + launch_chat 플랫폼 인지

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` — launch_app 핸들러 jkx 분기 2곳(:4243 jkxCandidate 폴백, :4300대 jkx 도구 분기), launch_chat(:5915)

- [ ] **Step 1: jkx 경로에서 SpawnClient 실패 정직화 — posix 분기:**  두 곳 모두 `SpawnClient(..., true)` 반환값을 **posix에서만** 검사:

```cpp
                bool spawned = SpawnClient(resolved.c_str(), true);
#ifdef _WIN32
                (void)spawned;                    // win32 관측 바이트 보존
                reply = "{\"ok\":true}";
#else
                // posix(플랜 F5/F7): fromJkx는 v1 프리게이트로 false — 거짓
                // ok:true 대신 도구에 실패를 보고한다(posix만).
                reply = spawned ? "{\"ok\":true,\"via\":\"jkx\"}"
                    : std::string("{\"ok\":false,\"error\":\"spawn_failed\","
                                  "\"jkx\":\"") + resolved + "\"}";
#endif
```
(jkxCandidate 폴백 가지는 `via:"jkx"` 없는 기존 리plies 형태 그대로 존중 — 두 분기 각각의 기존 성공 reply 문자열을 보존하고 실패 reply만 posix에서 신설.)
- [ ] **Step 2: launch_chat** — :5915:

```cpp
    } else if (tool == "launch_chat") {
        // M2 chat (approval surface). posix는 jkchat을 CMake 게이트로
        // 빌드하지 않았다(docs/70 §2 T1) — 거짓 ok 대신 플랫폼을 보고한다.
#ifdef _WIN32
        SpawnProcess("jkchat.exe", "");
        reply = "{\"ok\":true}";
#else
        reply = "{\"ok\":false,\"error\":\"unavailable_on_platform\"}";
#endif
```
- [ ] **Step 3: jkx 인수 정규기 실측(G5 흡수 — 판정 기록)** — `'/'→'\\'` 폴드 :4262-4277에 대해 후보 4종을 종이에서 전수 대조해 **posix 미스 케이스가 실존하는지** 확정: 원문(candidate 1)·exeDir 기준 원문(2)·정규화(3)·정규화 exeDir(4). "apps/workshop.jkx"·"workshop.jkx"·"apps\\workshop.jkx"·"sub/apps/w.jkx" 입력별로 어느 후보가 잡히는지 표로 만들어 보고에 첨부. 미스가 실존하면 폴드를 플랫폼 분기(win32만 '\'폴드, posix는 '/' 보존+strip 대상 "'apps/'"도 rfind로)로 소각 — 실존하지 않으면 수정 없음·판정만 기록.
- [ ] **Step 4: 빌드 + 커밋** `fix(g4): posix 정직 답변(jkx 스폰 실패·launch_chat)+jkx 인수 정규기 실측`

### Task G5: stub 리터럴 공용화

**Files:**
- Modify: `engine/src/agent/JKLmEngine.cpp` (stub 리터럴 2곳 — win32 문자열 그대로 상수로)
- Modify: `engine/tools/posix_selftest/main.cpp` (케이스 10 — 같은 상수 참조)

- [ ] **Step 1: 공용 상수 신설** — JKLmEngine.cpp 상단 파일-로컬:

```cpp
// stub LLM 턴의 셸 명령 — win32는 cmd.exe 접두(posix는 접두 없는 sh echo —
// 플랜 F2). selftest 케이스 10이 같은 리터럴을 재실행하므로 한 곳에서 관리
// (docs/71 리뷰 MEDIUM — 3처 복제 소각).
```
win32 리터럴 `"cmd.exe /c echo {\"result\":\"stub ok\",\"session_id\":\"stub-1\"}"`과 posix 리터럴 각각을 상수로; posix_selftest는 posix 상수만 사용하므로 JKLmEngine.cpp 파일-로컬은 소용없다 — **공용 헤더 `engine/include/agent/JKLmEngine.h`에 inline constexpr** 두 개로 올린다(이름 `kStubShellCmdWin32`/`kStubShellCmdPosix`).
- [ ] **Step 2: 소비치 3곳 치환 + posix_selftest build.sh 재빌드 확인** — `sh engine/tools/posix_selftest/build.sh && ./engine/build/posix_selftest` → 케이스 10 PASS 유지.
- [ ] **Step 3: 커밋** `refactor(g5): stub 셸 리터럴 공용화 — 3처 복제 소각`

### Task G6: 마감 — 게이트+docs/72+최종리뷰+푸시+메모리 갱신 27

- [ ] **Step 1: Windows 공식 9항목 1회차** (docs/70 §3 세트).
- [ ] **Step 2: WSL 빌드+posix_selftest**(0 failures, 케이스 수 변동 없음).
- [ ] **Step 3: WSLg 스모크 v3** — `engine/tmp/f7_smoke.sh`: 서버 기동→taskbar 자동스폰 관측→list_windows **windows 비어 있지 않음**→`launch_app {"app":"minesweeper"}`(plain route — G1+G2+G4 개통 증명)→windows에 minesweeper 나타남→pkill 정리.
- [ ] **Step 4: docs/72_linux_shell_entry_seal.md** — docs/71 §5 5종 → 소각 커밋·실측 매핑 + F7 잔차 체계 종결 + 남은 docs/70 §6 상태 재정리.
- [ ] **Step 5: 최종리뷰** — BASE..HEAD 패키지, 최강 모델 서브에이전트(opus), 판정 형식 VERDICT+severity. 라이더 있으면 반영.
- [ ] **Step 6: 푸시**(사전 승인) + **메모리 갱신 27**(roadmap 원장+backlog+MEMORY.md 인덱스).

---

## Self-Review

1. **Spec 커버리지**: docs/71 §5 5종 — #1(.so 프로브)=G1, #2(posix --client route)=G2, #3(taskbar 자동스폰)=G3, #3의 jkx 핸들러 false 무시=G4, #4(launch_chat)=G4, #5(jkx 인수 폴드)=G4 Step 3. F7 체계 전부. G5 stub 공용화=docs/71 리뷰 MEDIUM 후속. vplayer/Termux/#5(job·stdin)는 범위 밖(유지).
2. **플레이스홀더 스캔**: G2 Step 5의 검증 기준 "모듈 로드 성공"은 `[client] module loaded` 인쇄로 관측 가능 — 구현자 재충 정의 없음. G4 Step 3의 표 작성은 실측 산출물(보고 첨부)로 명확.
3. **타입 일관성**: `AppModuleSuffix()`·`DefaultServerEndpointPath()`가 G1 Produce이고 G2·G3 Consume — 헬퍼 위치(JKAppModule.h vs JKWindowServer.h)는 구현자가 결정 가능하되 단일 정의. dlopen RTLD 상수·dlerror 문법은 실제 코드 블록을 제공.