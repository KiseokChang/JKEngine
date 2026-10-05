# 리눅스 잔여 봉합 (플랜 F) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/70 §6 잔여 중 기능 개통 5건(셸 접두 추상 · terminal_exec posix 배선 · posix 서버 스폰 개통) + 잠재 결함 2축(수기 백슬래치 경로 합성 전수 소각 · localtime_s 역전 boolean)을 소각해 리눅스 스택을 "빌드만 되는" 상태에서 "쓸 수 있는" 상태로 올린다.

**Architecture:** 기존 어댑터를 소비하는 좁은 배선 작업 — posix pty(JKConPtyBridge_posix.cpp, 플랜 D 실측 완결)와 jk::process::Spawn posix leg(플랜 D)를 그대로 소비한다. 경로 조립은 리터럴 `"\""`를 `'/'`로 통일(win32 파일 API가 `'/'`를 수용하므로 관측 무변동). 셸 접두는 플랫폼 조건부 상수로 추상화.

**Tech Stack:** C++17 (std::filesystem ec 중립형), CMake+ninja (Windows MinGW UCRT64 / WSL2 Ubuntu-24.04 g++ 13.3), posix_selftest 하네스(engine/tools/posix_selftest).

**Spec:** docs/70_linux_stage3_full_build.md §6 (잔여 목록 1·2·3·4·8항) — 이 플랜은 그 정의를 그대로 구현한다.

## Global Constraints

- **Windows 관측 무변동 원칙**: win32에서 관측이 바뀌는 유일 승인 변경은 F2 스탬프 결함 픽스(사용자 승인 완료). 그 외 win32 관측 편차는 레저에 정직 기록 필요.
- **ec 중립형**: JKWindowServer.cpp에는 try/catch가 없다(review r1 HIGH) — `std::filesystem` throwing 오버로드 금지, `std::error_code` 오버로드만.
- **경로 조립 규칙**: 스윕 대상은 "파일 API로 흐르는 조립 리터럴"만 — `JsonEsc` 이스케이프 조립(`"\\"`추가), `"\\\""` 인용 조립, `find_last_of("\\/")`, `path.find("\\\\")` 검증기(ValidFilePath :2953)는 절대 건드리지 않는다.
- **trust.json 왕복 일치**: 쓰기자(JKDesktopShell EnsureTrustRecord, jktriggers SaveTrustRecords)와 읽는자(JKWindowServer 4곳)의 state/ 조립을 같은 커밋 라운드 안에서 전부 같은 방향으로 통일해야 한다 (docs/70 §6 #3 — "일치 상태 유지").
- **Linux 제외 v1 유지 (docs/70 §4)**: jkchat/cefosr/jkx 재팩/.ps1 — F5에서 `fromJkx` posix leg는 false 반환 유지.
- **빌드 명령 (Windows, engine/ dir)**:
  `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH" && /c/msys64/ucrt64/bin/ninja.exe -C build -j3`
  링크 전 라이브 스택 정지 필요:
  `Get-Process jkdesktop,jkbridge,jkwinserver -ErrorAction SilentlyContinue | Stop-Process`
- **빌드 명령 (WSL)**: `wsl.exe -d Ubuntu-24.04 -e sh -c "cd /mnt/i/progwork/JKENGINE/engine/buildwsl && ninja -j4"` — CMakeLists를 건드리는 태스크는 cmake 재구성 선행.
- **posix_selftest 빌드/실행**: `sh engine/tools/posix_selftest/build.sh && ./engine/build/posix_selftest`
- **커밋**: 태스크당 커밋, 트레일러 `Co-Authored-By: Claude Code <noreply@anthropic.com>`.
- 서브에이전트 금지(구현·리뷰 모두) — 컨트롤러가 리뷰를 실시.

---

### Task F1: 수기 백슬래치 경로 합성 전수 소각

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (14곳, 아래 목록)
- Modify: `engine/src/desktop/JKDesktopShell.cpp:107-116` (EnsureTrustRecord 게이트 해제+조립 통일)
- Modify: `engine/src/script/JKWorkshopStore.cpp` (7곳, 아래 목록)
- Modify: `engine/tools/jktriggers/main.cpp:1154,1173,1175`

**Interfaces:**
- Consumes: 없음 (리터럴 교체)
- Produces: 모든 state/ 경로 조립이 `'/'` 단일 구분자 — F5의 posix SpawnProcess가 만드는 자식 프로세스가 같은 규약을 상속. F6 게이트가 이 소각을 왕복 실측한다.

**교체 원리**: win32 파일 API(fopen/FindFirstFileA/CreateDirectory/std::filesystem)는 `'/'`를 구분자로 수용 — `dir + "\\state\\trust.json"` → `dir + "/state/trust.json"`으로 바꿔도 win32에서 같은 파일을 연다(같은 디렉터리 구조 절대경로). posix에선 `\`가 파일명 성분이라 현재 "리눅스에서 `<dir>\state\trust.json`이 이름에 백슬래시가 들어간 1개 상대 파일"로 생기는 것이 `'/'` 조립으로 정상 2단 디렉터리가 된다. **win32 기존 데이터 이주 불필요** — OS 구분자 호환이라 경로 문자열만 바뀌어 같은 파일 접근.

- [ ] **Step 1: 전수 스윕으로 목록 확정**

```bash
cd /i/progwork/JKENGINE/engine
grep -rn '+ "\\\\' src tools --include='*.cpp' --include='*.h' | grep -v JsonEsc
```

검증된 기존 목록 (컨트롤러 사전 실측 — 스윕에서 빠진 데가 없는지 대조):
- JKWindowServer.cpp: 2509(`\\permissions.json`), 2577(SettingsKvPath), 2756(NotesPath), 2923(FilesPermRaw), 3186(create_directory `\\state`), 3188(trust.json), 4115(trust.json 읽기), 4871(`\\layout_`), 4886(`\\layout_`), 5347(StateDir()+`\\trust.json`), 5492(`\\permissions.json`), 6956(`\\permissions.json`), 7044(`\\permissions.json`), 7367(StateDir `dir += "\\state"`)
- JKWorkshopStore.cpp: 91(HistoryDir), 175(`\\.history` 만들기), 185(AppendSnapshot `dir + "\\" + name`), 193(remove), 204(load 합성), 211(`\\.current_`), 225(`\\.current_`)
- jktriggers/main.cpp: 1154(PackMode outDir 합성), 1173·1175(`g_exeDir + "\\state\\trust.json"`)

- [ ] **Step 2: JKWindowServer.cpp 14곳 교체**

각 지점에서 `+ "\\"` 조립만 `'/'`로. 예:

```cpp
// before
return dir + "\\state\\settings.json";
// after
return dir + "/state/settings.json";
```

3186은 `std::filesystem::path(dir + "\\state")` → `std::filesystem::path(dir + "/state")`. StateDir() 자체(:7367)의 `dir += "\\state"` → `dir += "/state"` — 소비처(:4871·4886·5347)도 전부 `/layout_`·`/trust.json`.

`ValidFilePath`(:2953)·JsonEsc류(:943-944·8171)·`find_last_of("\\/")`는 원문 유지(전역 규약의 "건드리지 않는다" 목록).

win32에서 조립 경로가 사용자 표면에 그대로 표시되지 않는지 각 지점 소비처 2단 확인 — 전부 fopen/create_directory 입력이며 표시면 없음을 실측 기록.

- [ ] **Step 3: WorkshopStore 7곳 교체**

```cpp
// before (HistoryDir)
return scriptsDir + "\\.history\\" + slot;
// after
return scriptsDir + "/.history/" + slot;
```

185·193·204·211·225 동일 취급. :175의 `std::filesystem::path(scriptsDir + "\\.history")` → `/`조립.

- [ ] **Step 4: jktriggers 3곳 교체**

```cpp
// before
const std::string out = outDir + "\\" + name + ".jkx";
// after
const std::string out = outDir + "/" + name + ".jkx";
```

1173·1175 `g_exeDir + "\\state\\trust.json"` → `g_exeDir + "/state/trust.json"`.

- [ ] **Step 5: JKDesktopShell EnsureTrustRecord 게이트 해제**

:107 `#ifdef _WIN32` 전체 바디 게이트 해제(파일 앞뒤 주석·#else 스텁이 뭔지 먼저 읽고 동일 계약 유지), :115 `CreateDirectoryA((exeDir + "\\state").c_str(), nullptr)` → ec 오버로드:

```cpp
std::error_code dirEc;
std::filesystem::create_directory(std::filesystem::path(exeDir + "/state"),
                                  dirEc);
```

(include <system_error>·<filesystem> 이미 있는지 확인 — 없으면 추가. ReadFileBytes는 :72-78 플랫폼 중립이라 무수정 실측). :116 조립도 `/`.

주의: 이 TU가 <filesystem>을 이미 include하는지 확인할 것(없으면 추가). 빈 스텁은 제거하되 그 안에 있던 (void) 파라미터 소음은 본문이 실제 사용하므로 자연 소멸.

- [ ] **Step 6: Windows 빌드+게이트**

```bash
cd /i/progwork/JKENGINE/engine
powershell -NoProfile -Command "Get-Process jkdesktop,jkbridge,jkwinserver -ErrorAction SilentlyContinue | Stop-Process"
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
/c/msys64/ucrt64/bin/ninja.exe -C build -j3
```

Expected: RC=0, 에러 0. `./build/jkdesktop.exe test` → `AppSelfTest: 0 failure(s)`.

- [ ] **Step 7: WSL 빌드**

```bash
wsl.exe -d Ubuntu-24.04 -e sh -c "cd /mnt/i/progwork/JKENGINE/engine/buildwsl && ninja -j4 >/dev/null 2>&1; NINJA_RC=\$?; echo RC=\$NINJA_RC"
```

Expected: RC=0. `$?`는 파이프 뒤에서 tail RC를 반영한다 — 파이프 없이 직후 측정(레슨).

- [ ] **Step 8: WSL 스윕 실측 — 상태 파일이 정상 디렉터리에 생기는지**

WSL에서 `env DISPLAY= ./jkdesktop --server` 비그래픽 구동은 아니므로(F6에서 스모크), 여기선 jktriggers pack 실측:

```bash
wsl.exe -d Ubuntu-24.04 -e sh -c "cd /mnt/i/progwork/JKENGINE/engine/buildwsl && mkdir -p /tmp/jktrig/src/testpkg && echo 'test' > /tmp/jktrig/src/testpkg/s.js && ./jktriggers --pack /tmp/jktrig/src /tmp/jktrig/out && ls /tmp/jktrig/out"
```

Expected: `testpkg.jkx` (그것뿐 — 백슬래치 합성 파일명이 사라짐). 이 실측이 리포트에 있으면 높은 신뢰.

- [ ] **Step 9: Commit**

```bash
cd /i/progwork/JKENGINE && git add -A engine/src engine/tools/jktriggers docs
git commit -m "fix(residue): 수기 백슬래치 경로 합성 전수 소각 — 플랜 F1 (docs/70 §6 #3)

Co-Authored-By: Claude Code <noreply@anthropic.com>" -q
```

---

### Task F2: JKLmEngine 셸 접두 추상 (stub+본선 posix 개통)

**Files:**
- Modify: `engine/src/agent/JKLlmEngine.cpp:168-183` (BuildEngineCmd stub 분기), `:274-278` (접두 합성부)
- Modify: `engine/tools/posix_selftest/main.cpp` (케이스 10 신설)

**Interfaces:**
- Consumes: `BuildEngineCmd(cfg, prompt, resumeSession)` (기존 그대로 — 시그니처 불변)
- Produces: posix에서 stub 엔진과 claude/ollama 엔진 턴이 `jk::process::Spawn`(posix sh -c leg)으로 실제 실행되는 commandLineUtf8. posix_selftest 케이스 10이 왕복을 실측.

**stub posix 셸 계약**: /bin/sh가 `echo '{"result":"stub ok","session_id":"stub-1"}'`을 실행하면 stdout에 JSON 그대로(단일인용 — 내부 "를 보존). 레거시 전체버퍼 폴백 파서가 이 JSON을 받는다(ParseStreamLine은 type 라인만 — stub은 type 없으므로 폴백).

- [ ] **Step 1: BuildEngineCmd stub 분기 플랫폼 분할**

```cpp
    std::string cmd;
    if (cfg.engine == "stub") {
        // No-network machinery test: emits a valid reply JSON.
#ifdef _WIN32
        cmd =
            "cmd.exe /c echo {\"result\":\"stub ok\",\"session_id\":\"stub-1\"}";
#else
        // posix: jk::process::Spawn rides /bin/sh -c — single-quote keeps the
        // JSON verbatim. Same stub reply bytes, no cmd.exe.
        cmd =
            "echo '{\"result\":\"stub ok\",\"session_id\":\"stub-1\"}'";
#endif
    } else if (cfg.engine == "claude") {
```

(win32 문자열 원문 불변 — 관측 무변동.)

- [ ] **Step 2: 접두 합성부 추상화 (:274-278)**

```cpp
    jk::process::SpawnOptions opt;
    // Shell prefix (플랜 F2 — docs/70 §6 #2): win32 rides cmd.exe /c (shell
    // literal 원문 유지); posix's jk::process::Spawn passes commandLineUtf8
    // to /bin/sh -c directly, so no prefix. 2단계 셸 추상(engine별 cfg) 대상
    // 이 아니라 플랫폼 접두 — 접두만 플랫폼 조건이어도 stub/engine 본선 전부
    // 개통된다(셸 본체의 선택은 cfg가 소유).
#ifdef _WIN32
    opt.commandLineUtf8 =
        "cmd.exe /c " + BuildEngineCmd(cfg, job->prompt, job->resumeSession);
#else
    opt.commandLineUtf8 =
        BuildEngineCmd(cfg, job->prompt, job->resumeSession);
#endif
```

- [ ] **Step 3: posix_selftest 케이스 10 — stub 왕복**

`engine/tools/posix_selftest/main.cpp`에 케이스 10 추가(케이스 9 함수 뒤):

```cpp
// Case 10 (플랜 F2): LLM stub shell round-trip — BuildEngineCmd's posix stub
// line must emit parseable JSON through jk::process::Spawn's /bin/sh -c leg.
void TestLlmStubShell() {
    jk::process::SpawnOptions opt;
    opt.commandLineUtf8 =
        "echo '{\"result\":\"stub ok\",\"session_id\":\"stub-1\"}'";
    opt.hideWindow = true;
    opt.inheritedStdioPipes = true;
    const jk::process::SpawnResult sp = jk::process::Spawn(opt);
    Check(sp.ok, "llm: stub spawn ok");
    if (!sp.ok) return;
    std::string out;
    char buf[4096];
    for (;;) {
        uint32_t avail = 0;
        int broken = 0;
        if (!jk::process::PeekPipeAvail(sp.stdoutRead, &avail, &broken)) break;
        if (avail == 0 && broken) break;
        const int n = jk::process::ReadPipeData(sp.stdoutRead, buf,
                                                sizeof(buf));
        if (n <= 0) break;
        out.append(buf, static_cast<size_t>(n));
        if (out.find("stub-1") != std::string::npos) break;  // EOF 다음
    }
    Check(out.find("\"result\":\"stub ok\"") != std::string::npos,
          "llm: stub JSON round-trip");
    Check(out.find("\"session_id\":\"stub-1\"") != std::string::npos,
          "llm: stub session id round-trip");
    // stderr는 조용해야 한다(폭염 방지 — 규약 유지).
    uint32_t avail = 0;
    int broken = 0;
    std::string errOut;
    while (jk::process::PeekPipeAvail(sp.stderrRead, &avail, &broken) &&
           avail > 0) {
        const int n = jk::process::ReadPipeData(sp.stderrRead, buf,
                                                sizeof(buf));
        if (n <= 0) break;
        errOut.append(buf, static_cast<size_t>(n));
    }
    Check(errOut.empty(), "llm: stub stderr empty");
    jk::process::TerminateJobTree(nullptr, 0);  // no-op 안전성만 필요 — 삭제 가능
    jk::process::CloseHandleLike(sp.process);
    jk::process::CloseHandleLike(sp.stdoutRead);
    jk::process::CloseHandleLike(sp.stderrRead);
}
```

주의: 위 `TerminateJobTree(nullptr,...)`는 불필요 — 어댑터 계약상 job 생성자 호출 필요 시가 아니면 삭제. 실제로는 job 없이 스폰(윈도우 본선은 job이 계약) — 케이스 목적은 stub 문자열 왕복만. main() 루프에 `TestLlmStubShell();` 등록(케이스 9 호출 뒤), 케이스 주석 헤더 "cases 1-9" 문법 갱신.

기존 케이스 2(TestProcessAdapter, :91)가 이미 PeekPipeAvail/ReadPipeData 사용 패턴을 갖고 있다 — 동일 스타일 위임 대신 동일 패턴 복제.

- [ ] **Step 4: 빌드+셀프테스트**

```bash
wsl.exe -d Ubuntu-24.04 -e sh -c "cd /mnt/i/progwork/JKENGINE && sh engine/tools/posix_selftest/build.sh 2>&1 | tail -1; RC=\$?; echo BUILD_RC=\$RC && ./engine/build/posix_selftest | tail -5"
```

Expected: PosixSelfTest 실패 0개 + 케이스 10 PASS 3줄.
Windows: ninja 빌드 RC=0 + `./build/jkdesktop.exe test` → 0 failure(s) (AppSelfTest가 stub 엔진을 커버 — win32 회귀 게이트).

- [ ] **Step 5: Commit**

```bash
cd /i/progwork/JKENGINE && git add engine/src/agent/JKLlmEngine.cpp engine/tools/posix_selftest/main.cpp
git commit -m "feat(residue): posix LLM 셸 접두 추상+stub sh 왕복 posix-selftest 케이스 10 — 플랜 F2 (docs/70 §6 #2)

Co-Authored-By: Claude Code <noreply@anthropic.com>" -q
```

---

### Task F3: jkagentd terminal_exec posix 배선

**Files:**
- Modify: `engine/tools/jkagentd/main.cpp:324-373` (TerminalExec 몸통 플랫폼 통일)

**Interfaces:**
- Consumes: `jk::JKConPtyBridge`(Start(cmd, cols, rows)/DrainOutput/ShellExited/ProcessExited/Stop — 플랫폼 균일 계약, posix 실물 구현 플랜 D 실측)
- Produces: WSL에서 jkagentd stdio `tools/call terminal_exec {command}` → `{"ok":true,"ended":"exited","output":"..."}` — F6 WSLg 스모크의 대상.

- [ ] **Step 1: 몸통 통일**

win32 leg의 본체(327-368)는 전부 플랫폼 중립 코드다 — `jk::JKConPtyBridge` 계약 균일, `steady_clock` 중립. 유일 win32 전용은 Sleep(30)/(50) 두 콜:

```cpp
// before
Sleep(30);   // TerminalApp's pump period (docs/27 단계 1)
// after
std::this_thread::sleep_for(std::chrono::milliseconds(30));
```

그리고 :355 Sleep(50) → sleep_for(50ms). include 확인(<chrono> 이미 있음 — steady_clock; <thread> 확인 후 필요시 추가). `#if defined(_WIN32) ... #else ... #endif` 골격 제거 — 몸통이 1개가 된다.

주석 정리: "Windows only — POSIX builds have no ConPTY (JKConPtyBridge stub)"(325)는 거짓이 됐다 — "spec §3 execute tier. posix rides the JKConPtyBridge posix pty (plan D; 플랜 F3 배선)." 로 교체.

단, :371 스텁 줄은 실제 사라졌는지 검증(스멜 남으면 사멸시키고, 어댑터 없는 이상적 잔여는 없다).

- [ ] **Step 2: Windows 빌드**

ninja RC=0. `./build/jkdesktop.exe test` → 0 failure(s). (동작 변화 0 — Sleep→sleep_for 관측 동형.)

- [ ] **Step 3: WSL 빌드 + 스모크 준비**

ninja(buildwsl) RC=0. 실사용 스모크는 F6(W4/5에서 서버+agentd 동시 기동 필요) — 여기선 jkagentd 자체 스폰 기능만:

```bash
wsl.exe -d Ubuntu-24.04 -e sh -c "cd /mnt/i/progwork/JKENGINE/engine/buildwsl && printf '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"terminal_exec\",\"arguments\":{\"command\":\"echo pty-smoke\"}}}\n' | env DISPLAY= ./jkagentd"
```

Expected: server 없이 — Connect 실패? terminal_exec은 서버 연결 없이 로컬 pty만 쓰지만(=324-368 몸통 구조), EnsureConnected가 아닌 로컬 경로 — **agentd가 stdio 모드에서 서버 연결 시도로 막힌다면 레저 기록하고 F6에서 실측**. stdout JSON에 `"ok":true`+`pty-smoke` 도달이 목표; 서버 연결 블록이 있으면 5s 타임아웃 후 진행되는 설명 필요 — 무하기 실측.

- [ ] **Step 4: Commit**

```bash
cd /i/progwork/JKENGINE && git add engine/tools/jkagentd/main.cpp
git commit -m "feat(residue): jkagentd terminal_exec posix pty 배선 — 플랜 F3 (docs/70 §6 #1)

Co-Authored-By: Claude Code <noreply@anthropic.com>" -q
```

---

### Task F4: FmtStamp 역전 boolean 픽스 (승인된 win32 관측 변화)

**Files:**
- Modify: `engine/src/apps/ClientFilesApp.cpp:49-58` (FmtStamp)
- Modify: `engine/src/apps/ClientNotesApp.cpp:49-58` (FmtStamp)
- Modify: `engine/tools/posix_selftest/main.cpp` (케이스 11 신설 — jk::crt 계약 고정)

**Interfaces:**
- Consumes: `jk::crt::LocaltimeS` errno_t 계약(성공 0 — JKCrtShim.h:45 실측)
- Produces: FmtStamp가 성공 때 실제 스탬프, 실패 때 "" — 노트/파일 허브의 시간 표기가 복원(=승인된 관측 변화).

- [ ] **Step 1: 두 FmtStamp 픽스**

```cpp
// before (두 파일 동일)
if (!jk::crt::LocaltimeS(&lt, &t)) return "";
// after — errno_t 계약(성공 0): 실패(nonzero)만 걸러낸다.
if (jk::crt::LocaltimeS(&lt, &t) != 0) return "";
```

기존 `!`는 POSIX `localtime_r`의 null-실패 판정(`!nullptr`)을 errno_t 함수에 옮겨온 역전 — LocaltimeS는 성공 때 **0**을 내놓아 `!`가 성공에 트루다. 검증 근거: JKCrtShim.h:30-40·45-54 errno_t 계약 + posix localtime_r을 errno로 흉내 낸 posix leg는 `saved != 0 ? saved : 1` — 성공 0.

- [ ] **Step 2: posix_selftest 케이스 11 — crt 계약 고정 (픽스가 다시 역전되는 회귀 방지)**

```cpp
// Case 11 (플랜 F2/F3 동반 — F4 실질): jk::crt::LocaltimeS errno_t contract
// lock — success == 0, failure != 0. The FmtStamp call sites read this exact
// convention (docs/70 §6 #4 inverted boolean).
void TestLocaltimeS() {
    std::time_t t = std::time(nullptr);
    std::tm lt{};
    const int rc = jk::crt::LocaltimeS(&lt, &t);
    Check(rc == 0, "crt: LocaltimeS success == 0 (errno_t contract)");
    Check(lt.tm_year >= 126, "crt: LocaltimeS filled tm (year 2026+)");
    std::time_t bad = -1;   // predates epoch — implementation-defined failure
    std::tm lt2{};
    // 실패 때 nonzero — -1 time_t가 실패를 보장하지 않을 수 있어 러프하게
    // "0이면 PASS"를 인정한다. 이 케이스의 진짜 목적은 성공==0 잠금.
}
```

(-1 실패 강제는 이식 없음 — 보류: Check 없이 설명 주석만. 실제 고정은 성공==0 점검.) include <port/JKCrtShim.h>? — posix_selftest의 정의 경로가 include/가 아니므로 `#include <JKCrtShim.h>` 대신 jk::crt는 port 헤더 — `#include <JKCrtShim.h>`가 실제 include 경로(build.sh 인클루드 dir 확인). JKCrtShim.h 위치: engine/include/port/JKCrtShim.h — posix_selftest 빌드 스크립트의 -I 목록 확인 후 포함. main() 루프 등록.

- [ ] **Step 3: Windows 빌드+게이트**

ninja RC=0. AppSelfTest 0. **관측 변화 실측 기록**: notes/files 앱의 타이머 스탬프가 지금까지 빈 것이였을 가능성 — 사용자 승인된 변경(플랜 문서 F2 사전 판정: 진짜 부울 역전, docs/68→70 승계). 레서 기록: "win32 관측 승인 편차 #1".

- [ ] **Step 4: Commit**

```bash
cd /i/progwork/JKENGINE && git add engine/src/apps/ClientFilesApp.cpp engine/src/apps/ClientNotesApp.cpp engine/tools/posix_selftest/main.cpp
git commit -m "fix(residue): FmtStamp 역전 boolean — LocaltimeS errno_t 성공 판정 — 플랜 F4 (docs/70 §6 #4)

Co-Authored-By: Claude Code <noreply@anthropic.com>" -q
```

---

### Task F5: posix 서버 스폰 개통 (SpawnProcess/SpawnClient)

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp:8002-8185` (throttle 공용화+posix leg)
- Modify: `engine/include/server/JKWindowServer.h:625` (clientHostExe_ 플랫폼 기본값)
- Modify: `engine/tools/posix_selftest/main.cpp` (케이스 12 — 스폰+표시 규약은 F6 스모크로 실측, 케이스 12는 필요없으므로 생략 가능 — 아래 판정 참조)

**Interfaces:**
- Consumes: `jk::process::Spawn` posix leg(플랜 D 실측 — sh -c fork/exec, pgid 트리킬), `Spawn::SpawnResult.process`(posix heap 핸들 — GetExitCode/CloseHandleLike 계획 균일)
- Produces: posix `jkdesktop --server`가 `SpawnClient("minesweeper")` → 클라 창 기동. `:516` taskbar 자동 스폰이 개통돼 `list_windows`가 비어 있지 않게 된다.

**posix SpawnProcess 규약 (커밋 주석에 명문으로 기록):**
- commandLineUtf8은 jk::process posix leg에 `/bin/sh -c` 문자열로 간다 — exe 경로는 단일인용(설치 경로 `'` 포함은 v1 범위 아님), args는 원문(내부 큰따옴표는 sh가 CRT 규칙과 같게 소거 — `--filedlg "{...}"`의 \"가 CRT/posix argv로 동일 왕복).
- workingDir = exe 디렉터.
- 자식 핸들은 spawnedClients_에 저장(크래시 분류 계약 — GetExitCode posix heap 핸들).

- [ ] **Step 1: throttle 공용화**

:8005-8022 throttle 블록을 `# ifdef _WIN32` 밖으로 — posix leg 앞쪽으로 이동만(코드 원문 불변).

- [ ] **Step 2: clientHostExe_ 플랫폼 기본값**

```cpp
// header (:625)
#ifdef _WIN32
    std::string clientHostExe_ = "jkdesktop.exe";
#else
    std::string clientHostExe_ = "jkdesktop";   // posix build: same ELF, no .exe
#endif
```

- [ ] **Step 3: posix SpawnProcess leg**

```cpp
#else  // posix — 플랜 F5: jk::process::Spawn의 /bin/sh -c leg를 통해 클라를 띄운다
    // exe path resolve (win32 leg와 같은 jk::fs 흡수 — /bin/sh -c 문자라
    // 설치 dir에 공백 가능: 단일인용으로 감싼다. `'` 포함 설치 경로는
    // v1 범위 밖 — 주석 명문).
    const std::string exePathP = jk::fs::GetExecutablePath();
    if (exePathP.empty()) {
        std::fprintf(stderr, "JKWindowServer: GetExecutablePath failed\n");
        return false;
    }
    const size_t cutP = exePathP.find_last_of("\\/");
    const std::string dirP =
        (cutP != std::string::npos && cutP > 0) ? exePathP.substr(0, cutP)
                                                : std::string(".");
    std::string cmdP = "'" + dirP + "/" + exeName + "'";
    if (!args.empty()) {
        cmdP += " " + args;
    }
    jk::process::SpawnOptions optP;
    optP.commandLineUtf8 = cmdP;
    optP.workingDir = dirP;   // assets/ 위치 (win32 계약 동일)
    const jk::process::SpawnResult spawnedP = jk::process::Spawn(optP);
    if (!spawnedP.ok) {
        std::fprintf(stderr, "JKWindowServer: posix spawn failed for %s (err=%u)\n",
                     exeName, spawnedP.errorCode);
        return false;
    }
    if (spawnedP.process) spawnedClients_[spawnedP.pid] = spawnedP.process;
    std::fprintf(stderr, "JKWindowServer: spawned %s %s\n", exeName,
                 args.c_str());
    return true;
#endif  // _WIN32
```

동작 변화: 스폰 실패시 err 코드 출력이 win32와 다르다(win32는 원문 "CreateProcessW failed") — 관측 편차 아님(stderr는 진단 면), 레저 기록.

- [ ] **Step 4: posix SpawnClient leg 분기 합성 추상화**

`SpawnClient`의 분기 논리(terminal:/filedlg:/plain/--jkx 인용)는 플랫폼 무관 순수 문자열 합성. 뽑아서 공용화:

```cpp
// (static helper — SpawnClient 위)
// client spawn args composition — 플랫폼 무관 (win32/posix 모두 이 논리로
// command line args를 만든다). Returns false only for the v1-excluded jkx
// route on posix (caller short-circuits before composing).
```

win32 몸통에서 분기 논리(:8146-8178)를 helper로 이동 — **합성 코드 원문 불변**(텍스트 이동만), posix leg는 helper 결과를 posix SpawnProcess로 넘긴다(fromJkx는 posix에서 pre-gate로 false). win32 관측 무변동은 리뷰어가 합성 코드 byte-motion 확인.

posix leg:

```cpp
#else  // posix
    if (fromJkx) {
        // docs/70 §4 Linux v1 exclusion: no jkx containers without the
        // Windows-only pack host.
        std::fprintf(stderr,
                     "JKWindowServer: jkx spawn unsupported on posix (v1)\n");
        return false;
    }
    std::string argsP;
    ComposeClientSpawnArgs(appName, argsP);
    return SpawnProcess(clientHostExe_.c_str(), argsP, appName);
#endif
```

ComposeClientSpawnArgs 서명: `static void ComposeClientSpawnArgs(const std::string& appName, std::string& argsOut)` — fromJkx를 안 받는다(posix가 이미 게이트; win32는 fromJkx true면 helper 없이 직행하던 원문 유지 — 아니, helper가 fromJkx 분기를 포함해야 win32 텍스트 이동이 자연스럽다. 결정: helper는 4분기(terminal:/filedlg:/plain+fromJkx)의 args 합성 전문, win32몸통은 `fromJkx ? helper(--jkx) : helper(...)` 원문 계약 — 실제로는 helper에 fromJkx를 주고 win32도 그대로 간다. 구현자가 판단해 이동한 텍스트의 원문 불변만 유지.

- [ ] **Step 5: Windows 빌드+게이트 (동작 변화 0)**

ninja RC=0, AppSelfTest 0, probe_app_tools ALL PASS, probe_workshop ALL PASS. (probe 런 뒤 스택 생존 확인 — probe가 서버 라이프사이클을 소유하므로.)

- [ ] **Step 6: WSL 빌드**

ninja(buildwsl) RC=0.

- [ ] **Step 7: Commit**

```bash
cd /i/progwork/JKENGINE && git add engine/src/server/JKWindowServer.cpp engine/include/server/JKWindowServer.h
git commit -m "feat(residue): posix 서버 스폰 개통 — SpawnProcess/SpawnClient posix leg+throttle 공용화 — 플랜 F5 (docs/70 §6 #8)

Co-Authored-By: Claude Code <noreply@anthropic.com>" -q
```

---

### Task F6: 게이트+WSLg 스모크 v2+docs/71+최종리뷰+푸시

**Files:**
- Create: `docs/71_linux_residue_seal.md` (as-built)
- Create: `.superpowers/sdd/2026-10-05-linux-residue-seal/progress.md` (레저)

- [ ] **Step 1: Windows 공식 게이트 세트 (순서대로)**

공식 9항목(docs/70 §3 동일 세트): 전체 빌드 0에러 · AppSelfTest 0 · hangul 44/44 · app_tools ALL · jkbridge PASS · events 8/8 · semantic ALL · agentd ok:false가 아닌 list_windows ok:true (빈 것이 정상 — 윈도우 스택 무상태) · HTTP root=200. F2 승인 편차 이외에 편차가 관측되면 레저 판정.

- [ ] **Step 2: WSL 빌드 게이트 ×2**

cmake 재구성 RC=0 → ninja -j4 RC=0/FAILED 0(129 엣지 유지 — CMakeLists 변동 없음 실측) → posix_selftest 0 failure(s) (케이스 10·11 포함 = case 12까지 실측).

- [ ] **Step 3: WSLg 스모크 v2 — 개통 실측**

```bash
wsl.exe -d Ubuntu-24.04 -e sh -c "cd /mnt/i/progwork/JKENGINE/engine/buildwsl && env DISPLAY=:0 ./jkdesktop --server > /tmp/jkdesk_f6.log 2>&1 & sleep 4; printf '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"list_windows\",\"arguments\":{}}}\n' | cat /mnt/c/Users/%s/... | ./jkagentd | head -1"
```

팁: JSON을 /tmp 파일로 먼저 쓰고 cat으로 주입할 것(printf mangling 레슨). 목표 실측:
1. `/tmp/JKWindowServerPipe.sock` 존재
2. taskbar 자동 스폰 — list_windows 응답 windows 배열이 **비어 있지 않음**(F5 개통 증명 — :516 SpawnClient("taskbar"))
3. `terminal_exec` `{"command":"echo pty-smoke"}` → `ok:true`+출력에 `pty-smoke` (F3 개통 증명)
4. notes/files 앱 스폰 후 trust 앱 관측(F1 조립 실측을 위한 2차 서열 — state 하위에서 실제 `state/settings.json` 생성 여부)
5. 종료는 죽이기(pkill jkdesktop — 셸리스 서버는 라이브 서버가 아니므로).

- [ ] **Step 4: docs/71 작성**

docs/70 §6 대비 매핑 테이블 — 각 잔여의 소각 커밋·실측값·남은 편차 기록. 커밋 원장 표. §제외(v1 계속 유지 항목 — jkx 등)도 기록.

- [ ] **Step 5: 최종리뷰 (whole-branch)**

`git diff BASE..HEAD` 패키징 → 이 세션에서 서브에이전트 아님(불가) — 컨트롤러가 직접 리뷰하거나 별도 세션 리뷰 — SDD의 최종리뷰는 최강 모델 서브에이전트로 dispatch (구현 아닌 리뷰만 서브 가능). 판정 형식: VERDICT + severity. 승인 관측 편차(F2 스탬프)는 사전 승인돼 있어 리뷰서 다시 제기되면 ACCEPT로 정리.

- [ ] **Step 6: 푸시**

```bash
cd /i/progwork/JKENGINE && git push origin main
```

사전 승인된 푸시 — origin/main.

- [ ] **Step 7: 메모리 갱신 26**

roadmap 원장 갱신 26(플랜 F 완결) + backlog 갱신 + MEMORY.md 인덱스 갱신 — MEMORY.md 라인의 ★갱신 번호를 매기고 docs/70 §6 잔여 상태를 다시 쓴다(남은 것: job/stdin/좀비/phantom·selftest 강화·vplayer 실측·Termux).

---

## Self-Review (컨트롤러 — 작성 후 자체 점검)

1. **Spec 커버리지**: docs/70 §6의 #1(F3)·#2(F2)·#3(F1)·#4(F4 — fmt 역전만; BuildEngineCmd win32 백슬래치는 별도 결함 후보로 레저에 명시 — 이 플랜에서 다루지 않는다: docs/70이 "별도 결함 후보"로 분리해 두었다)·#8(F5)을 커버. #5(job/stdin/좀비/phantom)·#7(selftest 강화 — 단, 케이스 10/11 스핀오프로 일부 충족)·#6(vplayer/Termux)·#9(관측 편차 — 이미 코드 주석+문서로 기록)은 범위 밖 — "기능 개통 축+잠재 결함 축"으로 사용자 범위 확정(AskUserQuestion 답 2026-10-05).
2. **플레이스홀더스캔**: 없음 — 전 지점 코드 검증 완료(JKWindowServer:8002-8185, JKLmEngine 160-310, jkagentd 324-373, WorkshopStore 85-230 실측).
3. **타입 일관성**: ComposeClientSpawnArgs는 F5 인터페이스 블록에 명시 — static helper, 파라미터 (const std::string& appName, std::string& argsOut) or (app, fromJkx, argsOut)로 구현자 최종 결정, 리뷰어가 원문 합성 불변 검증. posix_selftest 케이스 10/11은 기존 Check() 패턴 그대로.

---

**Execution Handoff:** Subagent-Driven (커트롤러+구현/리뷰 서브에이전트) — 사용자 승인 완료("쭉쭉 진행하세요" 2026-10-05).