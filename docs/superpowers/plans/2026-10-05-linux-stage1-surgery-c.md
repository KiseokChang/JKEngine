# Linux stage-1 surgery Plan C (W7-W9) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/68 stage-1 마감 — W7(IME·입력·DPI 경계 확정, docs-only)·W8(tools 5종 잔여 Win32 접촉 정리: fs 흡수 7건 + jk::net 어댑터 신설·jkbridge 흡수 + 2단계 마킹)·W9(공식 회귀 게이트 ×2), 이것으로 1단계 완료 판정.

**Architecture:** 기존 어댑터 규약 승계 — 인터페이스 헤더(engine/include/<ns>/) + `*_win32.cpp`/`*_posix.cpp` 무조건 CMakeLists 등록, TU 내부 `#ifdef _WIN32` 가드, 어댑터 TU만 windows.h 소유. W4-W5와 다른 점 1개: **jk::net의 win32 TU는 windows.h 대신 winsock2.h를 먼저 include**한다(winsock2.h가 windows.h보다 먼저 와야 winsock.h 이중 정의를 피하는 MSDN 규칙 — 같은 TU에 windows.h가 필요 없으면 그냥 안 넣는다).

**Tech Stack:** C++17, Winsock2(socket/bind/listen/accept/recv/send), CMake(ucrt64 gcc 15.2.0), SDD(skill 원문).

**Spec:** docs/68_linux_stage1_workplan.md §W7·§W8·§W9 + docs/62 §3·§4·§8(1단계 정의·공식 회귀 배치) + docs/57 §9(jkbridge bind 계약). Plan A/B의 실행 문서: docs/superpowers/plans/2026-10-05-linux-stage1-surgery-a.md·…-b.md(어댑터 패턴·레슨 원본).

## Global Constraints

- **동작 변화 0** — 1단계 원칙(docs/62 §3). 예외는 반드시 as-built에 명문화. (플랜 B 실적: 예외 2건 — ConsoleAppFingerprint ifdef 제거, e373339 결함 수선.)
- **posix 구현은 스텁만** — 리눅스 실구현은 2단계. 스텁은 헤더 공개 멤버 전수 커버(플랜 B W3 레슨: 스텁 누락 멤버가 원본 잠복 결함이었다).
- **파일명은 `git ls-files` 실측값 그대로** — 플랜 B 핵심 레슨: 플랜 문서 오기(src/crash/)·자체 철자 착각(JKLmEngine vs JKLlmEngine)이 유령 증상 5건을 낳았다. contact 전 항상 `git ls-files`로 실존 경로 확인. 파일명 변수화도 원칙.
- **windows.h-clean 소비 TU 관례** — 소비 TU는 windows.h include 금지, 필요 Win32 상수/함수는 하드코딩+dllimport 수기 선언(어댑터 TU가 windows.h/winsock2.h 소유). 단, tools main.cpp들은 이미 windows를 include하는 전용 TU라 어댑터 include 자체로 windows.h 잔여가 소각될 때만 소각 — 억지 소각 금지(잔여는 마킹+as-built 목록화).
- **CP949·CP_ACP 계약 존중** — fs 어댑터가 이미 명문화한 바: 경로 소비자 전원 A형. tools 흡수도 동일(A-API 사이트만 흡수, UTF-8 재인코딩 도입 금지).
- **게이트는 라이브 스택 정지 후 공식 런 ×2** — docs/62 §4 배치 규약. 빌드: `cd /i/progwork/JKENGINE/engine/build && PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build .` (PREPEND). probe .ps1은 forward-slash 경로.
- **커밋 트레일러:** `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

## 실측 조사판 (컨트롤러 사전 조사, 2026-10-05 — 구현자는 이 값을 신뢰해도 되지만 contact 시 재확인)

- **W8a fs 흡수 대상 7건 / 4TU:** jkagentd `engine/tools/jkagentd/main.cpp` GetModuleFileNameA ×3(:187, :218, :454), jkbridge `engine/tools/jkbridge/main.cpp` :102, jktriggers `engine/tools/jktriggers/main.cpp` :1151, jkctl `engine/tools/jkctl/main.cpp` ×2(:31, :264).
- **W8b jkbridge 소켓 접촉(~20 사이트, 전부 main.cpp):** WSADATA+WSAStartup :2291-2294, listener socket/bind(SO_REUSEADDR)/listen(8) :2297-2316, accept :2335, RecvAll :300-310, WsSendFrame의 send 루프 :311-345, SendAll(HttpReply) :1644-1652, ReadHttpHead(RecvAll 소비) :1603, HandleConn SO_RCVTIMEO/SO_SNDTIMEO :1716-1718 + closesocket :1722, BridgeSession 멤버 SOCKET sock_ + shutdown(SD_BOTH) :1059/1065 + closesocket :1068, PrimaryIp UDP probe :1670-1694(INVALID_SOCKET 비교·connect·getsockname·inet_ntop·closesocket), accept 루프 `std::thread(HandleConn, conn, cfg).detach()`.
- **W8 마킹 대상(흡수 아님):** jkctl CreateProcessW :206 + wmain :774(2단계 CRT 진입·스폰 추상 결부 — CP949 인자 레슨 docs/48), jkchat windows.h :17(의도적 Win32 GUI 앱, docs/31 — 마킹은 "비이식 확정" 기재), jkagentd windows.h 잔여(ConPTY 소유), jktriggers windows.h 잔여(해시 외 Win32 접촉 실측).
- **W7 실측 완료(컨트롤러, 이 플랜의 전제):** `engine/include/JKPlatform.h`(162행)에 PAL 전범위 존재 — InitializeProcessDpiAwareness/IsPerMonitorDpiAware(EnableHighDpiAwareness 대응), GetPhysicalMousePos/GetPhysicalClientMousePos, SendSyntheticKey/SendSyntheticChar(input 대응), ImeMode{Ascii,Hangul,Unknown}/GetCurrentConversionMode/SetConversionMode/CompleteComposition/DetachIme — 전부 "Non-Windows: no-op/Unknown" 헤더 계약 명문화. **신설 0건.** docs/68 W7의 신설 이름(jk::input::/jk::ime::/jk::window::)은 불요 — C-T1이 docs에만 반영.

## Rulings (사전 판정 — 플랜 원문 대비 컨트롤러 결정)

- **R-C1:** docs/68 W8 원문은 "jk::net 인터페이스 **정의만**, 구현은 Winsock 유지"다. 그러나 jkbridge는 이미 전 TU가 tools 전용 windows TU이고 접촉이 Winsock API 11종으로 완결되어 있어, "정의만+Winsock 유지"와 "전면 흡수"의 차이가 사실상 헤더 파일 존재 여부뿐이다. **판정: 전면 흡수로 한다(ListenTcp/Accept/RecvAll/Send/SetTimeouts/ShutdownBoth/Close/PrimaryIp/Startup)** — 이유: 흡수하지 않으면 jkbridge의 SOCKET형이 그대로 남아 어댑터 헤더가 소비자를 못 걷어내고, 2단계의 unix 소켓 작업이 "헤더 신설"과 "TU 치환" 2중이 된다. 동작 변화 0은 각 함수가 원 API 시맨틱의 얇은 래퍼임으로 유지. 비용: 흡수 diff가 커져 리뷰 부담 — 대가는 1회 리뷰, 대응은 C-T2 게이트.
- **R-C2:** send 루프(WsSendFrame/SendAll) 2곳은 실패 의미론이 다르다(bool↔void). 어댑터는 **raw `int Send(Socket, const void*, int)`만** 내보내고 루프는 bridge에 원형 보존한다(원 코드 모양 최소 침해). RecvAll 1곳은 의미론이 단일하므로 어댑터로 통째 이전.
- **R-C3:** WSACleanup은 원문이 부르지 않으므로 어댑터에도 두지 않는다(YAGNI — 존재하지 않는 계약을 신설하지 않는다).

## Files

- Create: `engine/include/net/JKNet.h` — jk::net 인터페이스
- Create: `engine/src/net/JKNet_win32.cpp` — Winsock 얇은 래퍼(TU가 winsock2.h 소유)
- Create: `engine/src/net/JKNet_posix.cpp` — 헤더 전수 커버 스텁
- Modify: `engine/tools/jkbridge/main.cpp` — 전면 흡수 ~20 사이트
- Modify: `engine/tools/jkagentd/main.cpp` — GMFN ×3 흡수 + 잔여 마킹
- Modify: `engine/tools/jktriggers/main.cpp` — GMFN :1151 흡수 + 잔여 마킹
- Modify: `engine/tools/jkctl/main.cpp` — GMFN ×2 흡수 + CreateProcessW·wmain 마킹
- Modify: `engine/tools/jkchat/main.cpp` — windows.h :17 비이식 확정 마킹
- Modify: `engine/CMakeLists.txt` — net 2행 무조건 등록(win32+posix)
- Modify: `engine/tools/diag_selftest.cpp`(정확 경로는 `git ls-files '*selftest*'` 실측) — 케이스 15: net 스모크
- Modify: `docs/68_linux_stage1_workplan.md` — W7·W8·W9 as-built + §3 갱신(마지막 태스크)

---

### Task 1: docs/68 W7 as-built 명문화 (docs-only)

**Files:**
- Modify: `docs/68_linux_stage1_workplan.md` §W7

**Interfaces:**
- Consumes: 컨트롤러 실측(위 Rulings/조사판의 JKPlatform.h 9계약) — 코드 수정 없음
- Produces: W7이 "AS-BUILT 완료" 상태로 docs에 고정(뒤 태스크가 §3 갱신에서 참조)

- [ ] **Step 1: §W7에 AS-BUILT 블록 삽입**

플랜 B의 W4-W6 as-built 블록과 동일 양식:

```markdown
- **실측(플랜 C 태스크 1, 2026-10-05):** 신설 0건 확정 — `jk::Platform`
  (JKPlatform.h, 162행)이 이미 IME·입력·DPI 경계의 단일 PAL이다:
  DPI 2종(InitializeProcessDpiAwareness/IsPerMonitorDpiAware)·입력 2종
  (SendSyntheticKey/SendSyntheticChar)·IME 5종(ImeMode enum+
  GetCurrentConversionMode/SetConversionMode/CompleteComposition/DetachIme),
  전부 "Non-Windows: no-op/Unknown" 헤더 계약 명문화. 원문이 제안한 신설
  이름(jk::input::InjectSyntheticEvent/jk::ime::/jk::window::
  EnableHighDpiAwareness)은 불요 — 어댑터 인터페이스 확정 작업은 실측상
  완결돼 있었던 것으로 판정, 코드·이름 변경 0. 리눅스 IME는 §18 단일 소유
  원칙상 스텁만이 정답이라는 원문 결론도 그대로 유지.
```

- [ ] **Step 2: 커밋**

```bash
git add docs/68_linux_stage1_workplan.md
git commit -m "docs(spec): docs/68 W7 as-built — jk::Platform PAL이 경계, 신설 0건 확정 (플랜 C 태스크 1)"
```

---

### Task 2: W8a — GetModuleFileNameA 잔여 7건 jk::fs 흡수

**Files:**
- Modify: `engine/tools/jkagentd/main.cpp`(:187·:218·:454 부근)
- Modify: `engine/tools/jkbridge/main.cpp`(:102 부근)
- Modify: `engine/tools/jktriggers/main.cpp`(:1151 부근)
- Modify: `engine/tools/jkctl/main.cpp`(:31·:264 부근)

**Interfaces:**
- Consumes: `jk::fs::GetExecutablePath()` — `#include <fs/JKFs.h>`, `std::string`, CP_ACP 바이트, 실패/비윈도우 빈 문자열. 기존 콜사이트 계약(W5 as-built): 파생 규약(뒤 "\\" 유무·파일명)은 콜사이트 원문 유지.
- Produces: 없음(리프 태스크). 뒤 C-T3의 W8 as-built가 "26건 이후 2차 흡수 7건" 수치를 참조.

원칙: **순수 치환** — `GetModuleFileNameA(h, buf, N)` + 버퍼 사전 준비 코드가 콜사이트마다 있으므로, 어댑터가 바로 std::string을 내놓는 만큼 버퍼 선언· GetLastError 분기·절단 처리가 같이 사라진다. 각 콜사이트에서 (1) 사용 전후 의미 동일성을 주석 1줄로 남기고 (2) 절단·실패 폴백이 원문에 있었다면 **어댑터의 빈 문자열 폴백으로 대체된다는 점**을 그 주석에 명시한다(동일 관측 유지 원칙 — 원문 폴백과 빈 문자열 폴백이 관측적으로 같은 결과인 사이트만 치환하고, 다른 동작이 있던 사이트는 as-built에 widening 기록).

- [ ] **Step 1: 4TU contact 실측** — `git ls-files`로 실제 경로 확인 후, 7 콜사이트 각각 전후 20줄 읽기. 각 사이트 기록: 버퍼 방식(MAYBE static/스택/MAX_PATH)·절단 처리 유무·반환값 소비 방식.
- [ ] **Step 2: 순수 치환** — 각 TU `#include <fs/JKFs.h>`(없으면) + 7 사이트 치환. windows.h include는 잔여 접촉이 있으면 유지(무조건 소각 금지), 잔여 접촉에 "stage-2 marking: docs/68 W8" 주석.
- [ ] **Step 3: 전체 빌드** — `cmake --build .` 에러 0.
- [ ] **Step 4: 게이트** — jkagentd 실측 liveness(라이브 스택 기동 후 `printf '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}\n' | ./jkagentd.exe` → ok:true) + probe_app_tools 1회.
- [ ] **Step 5: 커밋**

```bash
git add engine/tools/jkagentd/main.cpp engine/tools/jkbridge/main.cpp \
        engine/tools/jktriggers/main.cpp engine/tools/jkctl/main.cpp
git commit -m "refactor(plat): tools GMFN 잔여 7건 jk::fs::GetExecutablePath 흡수 (docs/68 W8a, 플랜 C 태스크 2)"
```

---

### Task 3: W8b — jk::net 어댑터 신설 + jkbridge 전면 흡수

**Files:**
- Create: `engine/include/net/JKNet.h`
- Create: `engine/src/net/JKNet_win32.cpp`
- Create: `engine/src/net/JKNet_posix.cpp`
- Modify: `engine/tools/jkbridge/main.cpp`(~20 사이트)
- Modify: `engine/CMakeLists.txt`
- Test: 셀프테스트 파일(`git ls-files '*selftest*'` 실측) — 케이스 15

**Interfaces:**
- Produces(2단계가 소비할 계약 — 헤더 주석에 전부 명문화):

```cpp
// jk::net — Winsock boundary adapter (docs/68 W8 stage-1). Winsock impl in
// JKNet_win32.cpp (this TU owns winsock2.h — it must precede windows.h);
// posix stubs in JKNet_posix.cpp (stage 2: unix sockets). Socket handles are
// u64 (SOCKET) so consumers stay windows.h-clean.
#include <cstdint>
#include <string>

namespace jk::net {
using Socket = std::uint64_t;
// Winsock INVALID_SOCKET == (SOCKET)(~0) — same value for posix failure.
constexpr Socket kInvalidSocket = 0xFFFFFFFFFFFFFFFFull;

// WSADATA/WSAStartup(MAKEWORD(2,2)) — false on failure. Non-Windows: true.
bool Startup();
// socket(AF_INET,SOCK_STREAM)+SO_REUSEADDR(TRUE)+bind+listen(backlog).
// bindIp empty = INADDR_ANY (LAN+loopback, token gate — docs/57 §9);
// non-empty = inet_addr(bindIp). kInvalidSocket on any failure.
Socket ListenTcp(const std::string& bindIp, std::uint16_t port, int backlog);
// accept(listener) — kInvalidSocket on failure. Consumer loop stays
// detached-thread-per-conn (docs/57 as-built).
Socket Accept(Socket listener);
// Blocking recv-all loop: recv()==<=0 on EOF/error → false (original
// RecvAll(main.cpp:300) semantics verbatim).
bool RecvAll(Socket, void* buf, std::size_t n);
// Raw send() passthrough: >=1 bytes sent, <=0 EOF/error — send-loop
// callers (WsSendFrame, SendAll) keep their own loop shape (R-C2).
int Send(Socket, const void* data, int len);
// SO_RCVTIMEO/SO_SNDTIMEO both set to timeoutMs (30s slowloris/dead-phone
// guard, opus MAJOR-3). Errors ignored — original ignored them too.
void SetTimeouts(Socket, std::uint32_t timeoutMs);
// shutdown(SD_BOTH).
void ShutdownBoth(Socket);
// closesocket().
void Close(Socket);
// Primary LAN IP via UDP-connect trick (no packets sent) — "?" fallback,
// original PrimaryIp(main.cpp:1670) verbatim.
std::string PrimaryIp();
}  // namespace jk::net
```

- Consumes: jkbridge의 Socket형은 전부 `jk::net::Socket`로. `BridgeSession` 멤버 `SOCKET sock_` → `jk::net::Socket sock_`, HandleConn 시그니처 동형 치환.

**TDD 순서**(필수 — 플랜 B 선례: RED가 원문 유실 계약을 포획했다):

- [ ] **Step 1: 셀프테스트 케이스 15 RED** — net 스모크: `jk::net::Startup()` true → `ListenTcp("127.0.0.1", 0, 1)`이 kInvalidSocket 아님 → 백그라운드 스레드에서 connect+송신(원시 Winsock 1회 — 테스트 TU는 winsock 허용) → Accept → RecvAll 5바이트 일치 → Close 양쪽. plus posix 스텁 컴파일 증명은 빌드만(posix TU가 무조건 로드됨). RED 진단: 케이스 함수는 신설되지만 어댑터 미신설 컴파일 실패가 RED(케이스 13 선례 — 어댑터 본체 신설이 곧 GREEN).
- [ ] **Step 2: 어댑터 3파일 신설** — 헤더(위 코드 verbatim)+win32 본체+posix 스텁(멤버 전수 커버 — W3 스텁 누락 레슨). win32 TU 최상단: `#ifndef _WIN32 #error` 아니고 `#ifdef _WIN32` 전통 가드 + winsock2.h 먼저 include, `#include <ws2tcpip.h>`(inet_ntop·inet_addr).
- [ ] **Step 3: CMakeLists 등록** — net 2행 무조건 리스트(플랜 B 등록 방식과 동일 — 기존 fs/process 행 옆 참조).
- [ ] **Step 4: GREEN 확인** — 케이스 15 PASS + 전체 빌드 에러 0.
- [ ] **Step 5: jkbridge 흡수** — WSAStartup→Startup, listener 4종→ListenTcp(원문 주석 "bind 미지정 = INADDR_ANY…봉쇄"는 어댑터 헤더 계약에 이미 봉합됐으므로 콜사이트 주석은 1줄 축약, docs/57 §9 링크 유지), accept→Accept, RecvAll→어댑터(this→static 이동 원문 유지 의미), send 루프 2곳→`jk::net::Send` 소비 원형 보존(R-C2), SetTimeouts(30*1000), shutdown(SD_BOTH)×2→ShutdownBoth, closesocket ×N→Close, INVALID_SOCKET 비교→kInvalidSocket, PrimaryIp 총체→삭제+어댑터 호출. SOCKET 잔여 0 확인(`grep -n "SOCKET" main.cpp` → 주석 외 0).
- [ ] **Step 6: 게이트** — `cmake --build .` 에러 0 + 셀프테스트 0 failure(케이스 15 포함) + **probe_jkbridge PASS**(라이브 스택 정지 후 ×2) + jkbridge URL 인쇄·HTTP 웹 루트 회수 실측(브릿지 기동 → curl http 1회 → ok 관측 → 정지).
- [ ] **Step 7: 커밋**(RED 커밋 없음 — 케이스 13 선례: 원문 코드 결함이 아니므로 어댑터 신설 커밋에 RED→GREEN 동반)

```bash
git add engine/include/net/JKNet.h engine/src/net/JKNet_win32.cpp \
        engine/src/net/JKNet_posix.cpp engine/tools/jkbridge/main.cpp \
        engine/CMakeLists.txt <selftest file>
git commit -m "feat(plat): jk::net 어댑터 신설+셀프테스트 케이스 15+jkbridge 전면 흡수 (docs/68 W8b, 플랜 C 태스크 3)"
```

---

### Task 4: W8 잔여 마킹 (2단계 대상 명시)

**Files:**
- Modify: `engine/tools/jkctl/main.cpp`(:206 CreateProcessW, :774 wmain)
- Modify: `engine/tools/jkchat/main.cpp`(:17 windows.h)

**Interfaces:** Consumes 없음. Produces: as-built §W8이 참조하는 마킹 주석 표준형: `// stage-2 marking: docs/68 W8 — <대상>, <연결 이유>`.

- [ ] **Step 1: jkctl 마킹** — CreateProcessW 콜사이트 주석: `// stage-2 marking: docs/68 W8 — jk::process::Spawn 흡수 대상(wmain/CP949 인자 레슨 docs/48, CRT 진입 문제와 결부 — docs/68 W4 셸 추상과 동일 결정 시점)`. wmain 위 동일 톤.
- [ ] **Step 2: jkchat 마킹** — windows.h include 위: `// Non-portable by design: Win32 GUI app (docs/31) — not a stage-1 target (docs/68 W8).`
- [ ] **Step 3: 빌드+커밋** — 주석만이므로 빌드 1회+커밋.

```bash
git add engine/tools/jkctl/main.cpp engine/tools/jkchat/main.cpp
git commit -m "docs(code): W8 2단계 마킹 — jkctl CreateProcessW·wmain, jkchat Win32 GUI 비이식 확정 (플랜 C 태스크 4)"
```

---

### Task 5: W9 — 공식 회귀 게이트 ×2 + docs/68 as-built 완결 (컨트롤러 직접)

**Files:**
- Modify: `docs/68_linux_stage1_workplan.md` — §W8·§W9 AS-BUILT 블록 + §3 갱신 + 잔여 목록화

**Interfaces:** Consumes: T3·T4 실측값. Produces: 1단계 완료 판정 기록 + 2단계 착수 판단 메모.

**게이트 세트**(docs/62 §4 배치 — 플랜 B T6 게이트 세트 승계 + 이번 신설 스모크):

1. 라이브 스택 정지 → 전체 빌드 → 셀프테스트(케이스 15 포함 0 failure) → terminal_hangul_probe 44/44 → probe_app_tools → probe_jkbridge → probe_agent_events → probe_semantic_cursor — **이 세트 ×2 연속**.
2. jkagentd liveness 실측(printf stdin check → ok:true) — W8a 흡수 후 매 런.
3. jkbridge URL 인쇄+curl HTTP 스모크(플랜 B에 없던 신규 — net 흡수의 실재 게이트).

**as-built 기재 항목:** W8a 흡수 수치(실측 콜사이트 수·폴백 widening 유무 — 플랜 사전 조사판은 7건이었지만 실측 우선)·jk::net 계약 목록·R-C1/R-C2/R-C3 판정·jkbridge SOCKET 잔여=0 실측·마킹 목록(jkctl 2·jkchat 1·jkagentd/jktriggers 잔여 실측)·게이트 값 ×2.

- [ ] **Step 1: 게이트 ×2** — 위 세트 2회 연속 공식 런(각 런 사이 스택 완전 정지). 실패 시 결함 수선 후 재런(수선 커밋은 동작 변화 0 예외 3번째가 될 수 있음 — as-built에 명문화).
- [ ] **Step 2: docs/68 §W8·§W9 as-written + §3 갱신** — 플랜 B 양식 그대로. §3에 "플랜 C 착수·완료 실측(커밋 목록·게이트 값)" 블록 + **1단계 완료 판정 문장**.
- [ ] **Step 3: 커밋**

```bash
git add docs/68_linux_stage1_workplan.md
git commit -m "docs(spec): docs/68 W7-W9 as-built 완결 — 리눅스 1단계 경계 수술 완료 판정 (플랜 C)"
```

---

## 2단계 이관 목록 (완료 시 docs에 이대로 흡수 — 새 문서 아님)

- 잔여 Win32 소비 TU: jkctl(CreateProcessW·wmain)·jkagentd(ConPTY·WaitForSingleObject dllimport)·jktriggers(잔여 실측)·jkchat(비이식 확정)
- 폰트 경로 추상·ClientFilesApp C:\ 마킹·JKLlmEngine cmd.exe 셸 리터럴 — W5/W4 as-built에 기록된 2단계 결정 대기 건 그대로
- 플랜 B parked findings: ReadPipeData 부분 실패 관측 차이·close IS kill 직접 관측 케이스·NIT-6 dirW 경계

## Self-Review (컨트롤러 자체 검증 — 작성 완료 시점 수행)

1. **스펙 커버리지:** docs/68 W7(→T1)·W8(→T2 fs 7건+T3 net+T4 마킹)·W9(→T5 게이트) 전부 태스크 배정. W8 원문 "정의만" 대비 전면 흡수는 R-C1이 판정 문서화 — 스펙과의 유일한 변위이고 스펙 목적(소비자 windows.h-clean)은 오히려 완성 쪽.
2. **플레이스홀더:** 없음 — 헤더 코드 verbatim 제공, 콜사이트는 실측 조사판 행번호+치환 원칙 전수 기재.
3. **타입 일관성:** `jk::net::Socket`=u64, kInvalidSocket=~0. BridgeSession 멤버·HandleConn·accept 루프 전부 동형 치환으로 일관. T2는 jk::fs 기존 계약 재사용(plans/…-b.md의 as-built와 동일) — 신설 인터페이스 없음.