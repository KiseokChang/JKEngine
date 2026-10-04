# 리눅스 1단계 플랜 B(W4-W6 어댑터 신설) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/68 W4-W6 — 프로세스 스폰·fs·시간/스레드의 Win32 접촉을 인터페이스 뒤로 수술한다(동작 변화 0, 리눅스 구현은 스텁만).

**Architecture:** jk::fs(실행경로)와 jk::process(스폰·파이프·Job) 어댑터를 include/ 신설(선례: ipc·crypto — 헤더+_win32/_posix TU, _WIN32 가드는 각 TU 내부), 소비 TU에서 Win32 직접 접촉을 소각. W6은 기계 치환(std::thread/steady_clock은 이미 코드베이스 표준).

**Tech Stack:** C++17/20, Win32(dllimport 선언 방식), CMake 무조건 리스트.

**Spec:** docs/68_linux_stage1_workplan.md §W4-W6 (상우: docs/62 §8) — **원료 실측: .superpowers/sdd/2026-10-05-linux-stage1-surgery-a/plan-b-inventory.md (컨트롤러 검증판이 아닌 조사판 — 콜사이트 라인은 실행자가 접촉 시마다 재확인)**

## Global Constraints

- **동작 변화 0** — 기능 추가 없음, Win32 관측 행동(지문·스폰 계약·파이프 소유·exit 분류) 불변.
- **리눅스 실구현 금지** — posix TU는 스텁만(return false/빈값+TODO 주석). 과잉 추상화 금지(JobObject를 "프로세스 그룹"으로 일반화 금지 — docs/68 §4).
- 헤더 windows.h-clean — Win32는 .cpp TU 내부에만(파일 상단 가드). dllimport 선언 방식 승계.
- 플랜 A carry: **다음 CMakeLists 접촉 시 `if(WIN32) target_link_libraries(jkcore PUBLIC bcrypt) endif()` 추가 후 소비자 4곳 잔존 -lbcrypt 주석 갱신**(최종리뷰 Minor ① — 본 플랜에서 CMakeLists 편집하는 태스크가 반드시 수행).
- 빌드: `cd /i/progwork/JKENGINE/engine/build && PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build .` (PATH **prepend** 필수).
- 셀프테스트 케이스 11(pcx)·12(W1b 캡)·13(SHA-256) 점유 — 신규 케이스 14부터.
- 커밋 main 직접(프로젝트 관례), 메시지 말미 `Co-Authored-By: Claude Code <noreply@anthropic.com>`.
- 사내 전용 repo — 외부 공개 금지.
- 게이트는 Task 6에서 일괄(태스크별 공식 프로브 런은 생략, 빌드+셀프테스트만).

---

### Task 1: jk::fs 어댑터 신설 + exe-dir 흡수

**Files:**
- Create: `engine/include/fs/JKFs.h`, `engine/src/fs/JKFs_win32.cpp`, `engine/src/fs/JKFs_posix.cpp`
- Modify: exe-dir 25콜사이트(11TU — 아래 목록; `JKAppModule_script.cpp`는 **:34 DLL 핸들 케이스 제외**, :40만), `engine/src/JKApplication.cpp:38`
- Test: 셀프테스트 없음 — 흡수 후 전체 빌드+셀프테스트 0 failure가 게이트

**Interfaces:**
- Consumes: 없음(독립)
- Produces: `std::string jk::fs::GetExecutablePath()` — 실행 파일 전체 경로 UTF-8, 크기 무제한(동적 재시도 — MAX_PATH 절단 취약점 동시 소거). 플랜 A의 JKSha256과 동일한 "인터페이스 헤더+플랫폼 TU" 패턴.

- [ ] **Step 1: 헤더 작성**

`engine/include/fs/JKFs.h` (가드 패턴=JKPipeTransport.h 선례 — 1행 `#ifndef JKFS_H`, 네임스페이스 jk::fs, 표준 헤더만):

```cpp
#ifndef JKFS_H
#define JKFS_H
// jk::fs — filesystem boundary adapter (docs/68 W5 stage-1). Win32 impl in
// JKFs_win32.cpp; posix returns false/empty (stage 2: readlink("/proc/self/exe")).
// Supersedes 25 GetModuleFileName call sites (plan-b-inventory §7) — the
// MAX_PATH-truncation variants die together by returning std::string.

#include <string>

namespace jk::fs {
// Full path of the current executable image, UTF-8. Empty string on
// non-Windows (stage 2) — callers keep their existing empty-path fallback.
std::string GetExecutablePath();
}  // namespace jk::fs

#endif  // JKFS_H
```

- [ ] **Step 2: win32 구현**

`engine/src/fs/JKFs_win32.cpp` — `#ifdef _WIN32` 가드(JKPipeTransport_win.cpp 선례), dllimport 선언 방식(JKWindowServer.cpp:118-122 선례 — windows.h include 대신):

```cpp
#ifdef _WIN32
// Win32 impl of jk::fs::GetExecutablePath (docs/68 W5). Hand-declared
// dllimports, JKWindowServer.cpp:118 convention (windows.h-clean consumers).
#include "../../include/fs/JKFs.h"

extern "C" __declspec(dllimport) unsigned long __stdcall
GetModuleFileNameA(void* module, char* out, unsigned long size);
static const unsigned long kMaxDword = 0xFFFFFFFFul;
extern "C" __declspec(dllimport) unsigned long __stdcall
GetLastError();
static const unsigned long kInsufficientBuffer = 122;  // ERROR_INSUFFICIENT_BUFFER

// (GetLastError는 사실 불요 — QueryFullProcessImageName 형태가 아니라
//  크기 재시도만으로 충분: 반환값==size면 절단 의심 → 재시도. 코드는
//  SetLastError 불요 형태로.) 실제 구현:
#endif
```

구현 지침(실행자가 위 스케치를 그대로 쓰지 말고 아래 계약대로 작성):
- 시작 1024, 반환 len==size-1이면 배증 재시도(최대 8회), 실패 또는 0 반환 시 빈 문자열.
- 모듈 핸들 nullptr = 현재 프로세스 exe(A/W 중 A 사용 — 소비자 전원이 A형 경로 소비).

- [ ] **Step 3: posix 스텁**

`engine/src/fs/JKFs_posix.cpp`:

```cpp
#ifndef _WIN32
// Posix stub of jk::fs (stage 2: readlink("/proc/self/exe")).
#include "../../include/fs/JKFs.h"

namespace jk::fs {
std::string GetExecutablePath() { return std::string(); }
}
#endif
```

- [ ] **Step 4: 흡수 — 25콜사이트**

plan-b-inventory §7 표의 각 TU에서 exe-dir 계산 블록(char buf[]→GetModuleFileName→find_last_of→substr)을:

```cpp
#include <fs/JKFs.h>
// ...
const std::string exeDir = [] {
    const std::string exe = jk::fs::GetExecutablePath();
    const size_t cut = exe.find_last_of("\\/");
    return cut == std::string::npos ? std::string() : exe.substr(0, cut + 1);
}();
```

형태로 교체 — **기존 경로 파생 규약(뒤 붙는 "\\" 유무, settings.json/permission/trust/state 파일명)은 원문 렌들레다.** TU별: JKWindowServer 13(단 :8029는 W형 — 같은 헬퍼로 절단하되 원문 W형의 의미(non-ANSI dir 안전)는 A형 전환 시 무손실, 주석 남김), JKDesktopShell 3, JKLlmEngine :44-51 소각(ExeDirA 삭제), ClientBrowserApp 2(`ExeDirSlash()`는 슬래시 정규화 변형 — 반환 후 `\\`→`/` 치환 유지), JKClientApplication 1, JKThemeConfig 1, JKTextAtlas 1(단 #else 스텁 케이스는 어댑터가 대체 — 기존 "비윈도우 빈값" 계약과 일치), main.cpp 1, JKTerminalConfig 1, ClientNotifyApp 1, JKAppModule_script :40만.

- [ ] **Step 5: JKApplication.cpp:38 소각**

`C:\temp_jkwin_verify\mouse.log` → `jk::fs::GetExecutablePath()` exe-dir 기반 `<exeDir>\state\mouse_verify.log`(state 디렉터리 없으면 기존과 동일 fopen 실패 조용히 무시 — 동작 계약 보존 주석).

- [ ] **Step 6: CMakeLists + bcrypt 예방(플랜 A carry)**

engine/CMakeLists.txt — jkcore 리스트에 `src/fs/JKFs_win32.cpp`/`_posix.cpp` 2행 추가(무조건, :156-161 패턴) + **`if(WIN32) target_link_libraries(jkcore PUBLIC bcrypt) endif()` 추가** + jkdesktop_shell/jkbridge/jktriggers/jkctl의 잔존 `-lbcrypt`에 주석 갱신(주석 유지 조건상 남아있어도 무해 — W2 as-built 기재).

- [ ] **Step 7: 빌드+셀프테스트+커밋**

빌드 에러 0 + `jkdesktop.exe test` 0 failure + `git add -A && git commit -m "refactor(fs): jk::fs::GetExecutablePath 어댑터 신설+exe-dir 25콜사이트 흡수 (docs/68 W5)"`.

### Task 2: jk::process 어댑터 신설

**Files:**
- Create: `engine/include/process/JKProcess.h`, `engine/src/process/JKProcess_win32.cpp`, `engine/src/process/JKProcess_posix.cpp`
- Modify: `engine/CMakeLists.txt`(2행)
- Test: 셀프테스트 케이스 14(스폰 경로 스모크 — 실제 자식 없이 계약 검증 형태)

**Interfaces:**
- Consumes: 없음
- Produces(spawn 2계열 흡수 — stdio 상속형[JKLlmEngine]+GUI 무stdio형[JKWindowServer]; **ConPTY 계열은 흡수 불가, 배제** — inventory §3 실측):

```cpp
#ifndef JKPROCESS_H
#define JKPROCESS_H
// jk::process — process/pipe boundary adapter (docs/68 W4 stage-1).
// Win32 impl JKProcess_win32.cpp; posix stubs (stage 2: posix_spawn+poll).
// Design contracts carried from the absorbed call sites (do not change):
//  (a) InheritedStdioPipes: parent keeps READ ends only — the write ends are
//      closed immediately after spawn, else the child's stdout never EOFs.
//  (b) JobHandle is RAII-bound to "handle close == tree death"
//      (JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE) — CloseJob IS the kill.

#include <cstdint>
#include <string>

namespace jk::process {

struct SpawnOptions {
    std::string commandLineUtf8;   // full command line, UTF-8 (adapter widens)
    std::string workingDir;        // empty = inherit
    bool hideWindow = false;       // CREATE_NO_WINDOW + STARTF_USESHOWWINDOW/SW_HIDE
    bool inheritedStdioPipes = false;  // create stdout/stderr parent-read pipes
};

struct SpawnResult {
    bool ok = false;
    std::string error;
    void* process = nullptr;       // PROCESS_INFORMATION::hProcess (opaque)
    uint32_t pid = 0;              // dwProcessId
    void* stdoutRead = nullptr;    // parent read end — valid iff inheritedStdioPipes
    void* stderrRead = nullptr;
};

SpawnResult Spawn(const SpawnOptions& options);

// Pipe — PeekPipeData fills available; ReadPipeData returns bytes read
// (-1 error). Peer-closed detection is the caller's job via read==0/err —
// same observation as ERROR_BROKEN_PIPE today.
bool  PeekPipeAvail(void* pipe, uint32_t* available, int* brokenError);
int   ReadPipeData(void* pipe, char* buffer, int cap);
void  CloseHandleLike(void* handle);

// Job (KILL_ON_CLOSE contract) — LlmEngine only consumer. Interface only;
// win32 impl keeps JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, posix returns nullptr.
void* CreateKillOnCloseJob();
bool  AssignToJob(void* job, const SpawnResult& proc);
bool  TerminateJobTree(void* job, uint32_t exitCode);

// Single-process kill (JKWindowServer::KillServerHolders consumer — the
// "TerminateProcessTree" docs/68 wording was a survey miscount: 1 real tree
// site + this single-site consumer).
bool KillProcess(void* process, uint32_t exitCode);
// Exit classification (JKWindowServer crash path; kStillActiveExit=259 stays).
bool GetExitCode(void* process, uint32_t* exitCode);

}  // namespace jk::process

#endif  // JKPROCESS_H
```

- [ ] **Step 2: win32 구현** — JKLlmEngine.cpp:253-390을 계약(a)(b)을 그대로 승계하는 형태로 이식(상속형: `SECURITY_ATTRIBUTES{.., TRUE}` 2쌍+`SetHandleInformation(read*, INHERIT, 0)`+스폰 직후 write 끝 CloseHandle; GUI형: inherit FALSE+stdio 없음); dllimport 선언 방식(JKWindowServer.cpp:97-107 형태 — CreateProcessW·CreatePipe·SetHandleInformation·SetInformationJobObject·AssignProcessToJobObject·TerminateJobObject·TerminateProcess·GetExitCodeProcess·PeekNamedPipe·ReadFile·CloseHandle). posix = 전원 false/nullptr 스텁.
- [ ] **Step 3: 셀프테스트 케이스 14** — stub 자식(`cmd.exe /c echo {"ok":true}`)을 Spawn(hide)+Peek/Read로 왕복 검증+Job RAII 스폰 후 close 검증(동작 관측: 스폰 성공+출력 1줄+process 종료). **Stub 소프트웨어는 테스트 리터럴이라 윈도우 전용 허용**(셀프테스트 자체가 win32 전용 명문화).
- [ ] **Step 4: 빌드+셀프테스트+커밋** — `refactor(process): jk::process 어댑터 신설 (docs/68 W4)`.

### Task 3: JKLlmEngine 흡수 (windows.h 탈착)

**Files:**
- Modify: `engine/src/agent/JKLlmEngine.cpp`(:11 include 소각 대상 — 흡수 후 잔여 Win32 접촉 없어야 함), 셀프테스트

**Interfaces:** Consumes: Task 2 계약 전부. Produces: windows.h-free JKLlmEngine.

- [ ] **Step 1:** CreatePipe/PeekNamedPipe/ReadFile(:334-371 루프 — ERROR_BROKEN_PIPE 판정은 ReadPipeData 반환·PeekPipeAvail brokenError로 이동)/Job 4함수(:309-390 — RAII 계약: `CloseJob`=킬)/CreateProcess(:273-302)/Utf8ToWide·WideToUtf8(:23-42 — 어댑터 내부화로 소각, UTF-16 계약은 commandLineUtf8 단일 문자열로)/ExeDirA(Task 1 소각 완료 상태)를 jk::process/jk::fs 호출로 교체. `BuildEngineCmd`·stub 리터럴 :167·셸 접두 :273 **은 건드리지 않는다**(2단계 셸 추상 — 주석 "stage-1 marking: shell literal, docs/68 W4"만).
- [ ] **Step 2:** windows.h include 소각 후 빌드 — 에러 0이면 봉합(CMakeLists 타깃 변화 불요 — kernel32 dllimport는 어댑터 TU가 소유).
- [ ] **Step 3:** 커밋 — `refactor(llm): JKLlmEngine 스폰 접촉을 jk::process로 흡수·windows.h 탈착 (docs/68 W4)`.

### Task 4: JKWindowServer 흡수 (수기 선언 소각)

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` — :97-160의 CreateProcessW·LauncherStartupInfoW·LauncherProcessInformation·GetExitCodeProcess 선언 블록(~50행) 소각 대상. **Toolhelp 선언(:155-178)은 스폰이 아니라 보유자 스캔(서버 후보탐색)이므로 보존.**
- [ ] **Step 1:** SpawnProcess(:8003-8106)를 jk::process::Spawn(GUI형)으로, CleanupDisconnectedClients(:7927-7943)의 GetExitCodeProcess→jk::process::GetExitCode, KillServerHolders(:413-422)→jk::process::KillProcess. `spawnedClients_` 맵 소유 구조(void*로 그대로 — 어댑터가 관리하는 것은 핸들 이소가 아니다) 유지.
- [ ] **Step 2:** 빌드+셀프테스트+커밋 — `refactor(server): SpawnProcess·KillServerHolders를 jk::process로 흡수·수기 선언 소각 (docs/68 W4)`.

### Task 5: 시간·스레드 표준화 (W6)

**Files:**
- Modify: `engine/src/agent/JKLlmEngine.cpp`:324·:376(GetTickCount64→`std::chrono::steady_clock::now()` 기반 ms 계산 — kTurnIdleKillMs 의미 불변), :428-436(CreateThread→`std::thread(LlmTurnThread, job)` detach — **Busy 롤백 계약(:431-433 "no thread, no turn")은 try/catch(std::system_error)로 보존** — inventory §12 실측), `engine/src/crash/JKCrashHandler_win32.cpp`:157-158(→`std::thread(MirrorThread, ctx).detach()` — daemon 계약 보존)
- [ ] **Step 1:** 치환+빌드+셀프테스트+커밋 — `refactor(time): GetTickCount64→steady_clock·CreateThread→std::thread (docs/68 W6)`.

### Task 6: 완료 게이트 + docs/68 as-built

- [ ] **Step 1:** 전체 빌드 에러 0.
- [ ] **Step 2:** 공식 프로브 ×2: probe_app_tools(launch_app 스폰 스모크)·probe_jkbridge(LLM 턴 경로)·probe_agent_events + terminal_hangul_probe(회귀) + `jkdesktop.exe test` 0 failure(케이스 14 포함).
- [ ] **Step 3:** docs/68 W4-W6 as-built 블록 기재+커밋(마킹 사항: cmd.exe :273 1단계 생존·ClientFilesApp C:\ 스펙 §2.3 선결·폰트 리졸버 재용은 docs/63 §223 스코프로 2단계 — 이 3건을 "2단계 결정 대기" 명문화).

## Self-Review 노트 (작성자 판정)

1. **W5 폰트는 본 플랜 스코프 외**: jk::text::ResolveDesktopFontPath/FallbackPath가 이미 어댑터 자리(inventory §8) — ImGui 앱 10파일 흡수는 docs/63 §223("ImGui 앱 경로 미변경") 스코프 변경+라이브 settings 오버라이드로 관측 변화 가능성이 있어 **2단계 결정 대기로 기재(as-built)**. C:\ ClientFilesApp도 스펙 §2.3 충돌로 마킹만.
2. **SpawnResult의 process/stdoutRead 등 void*는 플랫폼 투명 핸들** — JKConPtyBridge.h 선례(멤버 전원 void*)와 동일 계약.
3. **type 일치**: PeekPipeAvail(void*, uint32_t*, int*) / ReadPipeData → int / KillProcess vs TerminateJobTree — 소비처 1:1(inventory §2 정정 반영).
4. TDD: 인터페이스 신설은 셀프테스트 케이스 14가 RED→GREEN 본체이고, 흡수 태스크는 관측 불변(기존 스모크)이 진위판정.