# Linux Port 1단계 — 플랫폼 경계 수술 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/62의 1단계 — 기존 플랫폼 경계의 잔여 흠을 수선해 Win32 동작 변화 0를 증명하고, 2단계(리눅스 구현)가 채울 자리를 확실히 남긴다.

**Architecture:** docs/43 분할 때 이미 만들어진 경계(`IWireTransport`, `JKPlatform` OS별 .cpp, `JKConPtyBridge`) 위에서 잔여 결함 5건을 수선한다: CancelPendingIo 인터페이스 승격, ReadMessage 페이로드 캡, 가드 봉인 뽑아내기, SHA-256 3중 복제 통합, pty 파일 분할. **VideoPresent 어댑터와 파일 대화상자·전체화면 수술은 조사 결과 불요(docs/62 §8) — 이 계획에 없다(YAGNI).**

**Tech Stack:** C++17, CMake+MinGW ninja (`build_with_temp.sh`, TEMP=`/i/temp_jkdesktop`), C++ 콘솔 프로브(`tools/probes/*.cpp`)+PS1 하네스.

**Spec:** `docs/62_linux_port_and_dex.md` (§8 조사 정정 포함 — 스펙과 계획은 함께 읽는다)

## Global Constraints

- **동작 변화 0**: 이 계획의 모든 태스크 완료 후 기존 Win32 회귀 프로브 전부 GREEN (docs/62 §3 1단계 판정). 픽스로 "개선"하더라도 관찰 가능한 동작은 유지.
- **on-wire 포맷 무수정**: 12바이트 `WireHeader{magic=0x4A4B0001, type, length}` `#pragma pack(push,1)` — PS1 프로브들이 .NET으로 직접 이 포맷을 구현해 통과한다 (docs/62 §8.1).
- **지문 포맷 byte-identical**: `"sha256:"+64hex` — jkctl / jktriggers / JKDesktopShell 3곳이 동일 포맷을 요구 (jkctl main.cpp:221-223 주석, JKDesktopShell.cpp:96-97 주석). 통합 후에도 `state\trust.json`의 `EnsureTrustRecord`가 byte-for-byte 일치해야 한다.
- **windows.h-clean TU 규칙**: 수기 WIN32 선언보다 windows.h-clean TU로 함수 이전이 상책 (docs/61 §7 레슨 26). 레거시 typedef+SDK 헤더 공존 TU는 `GetFileAttributesExA` 수기 선언 패턴 (`_WINBASE_` 센티넬).
- **2연속 원칙**: 모든 프로브/회귀는 2연속 GREEN이 판정 (하네스 플레이크 폭로 장치, docs/59 §15).
- **빌드**: `engine/build_with_temp.sh` (TEMP=`/i/temp_jkdesktop`), ninja. 링크 오류는 입력 파일 목록부터 확인 (docs/61 레슨 28). 스테일 obj 주의 — 소스 고친 뒤 반드시 현재 트리 빌드.
- **이번 세션은 계획까지** — 사용자 확정 (2026-09-21): 구현 착수는 별도 시점.

---

### Task 1: CancelPendingIo 인터페이스 승격

`CancelPendingIo()`는 `IWireTransport` 밖의 콘크리트 메서드로 새서 posix 전송이 이 계약을 따를 수 없다 (docs/62 §8.1-①). 인터페이스로 승격하되 Win32 동작은 그대로.

**Files:**
- Modify: `engine/include/ipc/JKWireProtocol.h` (IWireTransport, ~line 255-270)
- Modify: `engine/src/ipc/JKPipeTransport_win32.cpp:172` (`override` 표기)
- Modify: `engine/src/ipc/JKPipeTransport_posix.cpp` (no-op override 추가)

**Interfaces:**
- Consumes: 현행 `JKPipeTransport::CancelPendingIo()` (비가상, `CancelIoEx` 호출)
- Produces: `IWireTransport::CancelPendingIo()` — 가상, 기본 구현 no-op. 2단계 posix 전송이 no-op으로 상속 가능. 호출부 `JKClientConnection::StopReadThread` (src/server/JKClientConnection.cpp:116)는 수정 불요.

- [ ] **Step 1: 기준선 회귀 확보**

빌드 후 승인/해체 경로를 통과하는 기존 프로브 1건을 기준선으로:

```bash
bash engine/build_with_temp.sh && powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_approve_self.ps1 > /tmp/gate1_before.txt 2>&1
```

Expected: 빌드 [157/157]성, 프로브 ALL PASS.

- [ ] **Step 2: 인터페이스에 승격**

`engine/include/ipc/JKWireProtocol.h`의 `IWireTransport`(~:255-270)에 기본 구현을 가진 가상으로 추가:

```cpp
// Abort any in-flight I/O so a reader parked in Read() wakes and exits.
// Default: no-op (transports with no abortable blocking read).
// Close() must still only run after the reader thread has joined.
virtual void CancelPendingIo() {}
```

`JKPipeTransport_win32.cpp:172`의 기존 정의에 `override` 표기. `JKPipeTransport_posix.cpp`에:

```cpp
void JKPipeTransport::CancelPendingIo() {} // POSIX stub: no in-flight I/O to abort yet (docs/62 2단계)
```

- [ ] **Step 3: 빌드+회귀**

```bash
bash engine/build_with_temp.sh && powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_approve_self.ps1 > /tmp/gate1_after.txt 2>&1
```

Expected: PASS (기준선과 동일). 실패 시 diff로 수술 원인 확인.

- [ ] **Step 4: Commit**

```bash
git add engine/include/ipc/JKWireProtocol.h engine/src/ipc/JKPipeTransport_win32.cpp engine/src/ipc/JKPipeTransport_posix.cpp
git commit -m "refactor(ipc): CancelPendingIo를 IWireTransport로 승격 (docs/62 §8.1)"
```

---

### Task 2: ReadMessage 페이로드 캡

`ReadMessage`가 길이 상한 검사 없이 `assign(header.length, 0)`한다 (JKWireProtocol.cpp:37, docs/62 §8.1-②). 악성/오염 헤더가 4GiB 할당을 유발 가능. 캡은 전송 계약 일부로 2단계에서도 공용.

**Files:**
- Modify: `engine/include/ipc/JKWireProtocol.h` (상수 추가)
- Modify: `engine/src/ipc/JKWireProtocol.cpp:27` (ReadMessage)
- Test: `engine/tools/probes/wire_cap_probe.cpp` (신설, C++ 콘솔 프로브)

**Interfaces:**
- Consumes: `IWireTransport` 페이크 (probe가 스텁 구현 — `Write/Read/Close/IsConnected` 4메서드)
- Produces: `inline constexpr uint32_t kMaxWirePayload = 8u * 1024 * 1024;` (state 파일 캡 8MiB와 정합). `ReadMessage`가 초과 시 `false` 반환+프레임 소비 안 함. 기존 호출부(서버 ReadLoop/클라)는 시그니처 무수정.

- [ ] **Step 1: 실패 테스트 작성**

`engine/tools/probes/wire_cap_probe.cpp` — 페이크 전송으로 과대 헤더를 흘려 캡 검사:

```cpp
// wire_cap_probe — ReadMessage 페이로드 캡 검증 (docs/62 §8.1-②)
#include <ipc/JKWireProtocol.h>
#include <cstdio>
#include <cstring>
#include <string>
using namespace jk::ipc;

namespace {
struct FakeTransport : IWireTransport {
    std::string bytes; size_t pos = 0;
    bool Write(const void* d, size_t n) override { bytes.append((const char*)d, n); return true; }
    bool Read(void* d, size_t n) override {
        if (pos + n > bytes.size()) return false;
        std::memcpy(d, bytes.data() + pos, n); pos += n; return true;
    }
    void Close() override {}
    bool IsConnected() const override { return pos < bytes.size(); }
};
constexpr uint32_t kMagic = 0x4A4B0001; // kWireMagic과 일치 — 헤더 캡에서 복사하지 말고 그 값을 써라
}

int main() {
    // 정상 프레임은 통과
    {
        FakeTransport t;
        const char payload[] = "hi";
        assert_true(WriteMessage(t, (MsgType)1, payload, 2));
        Message m;
        assert_true(ReadMessage(t, m));
    }
    // 길이 필드만 초과로 조작한 헤더 → ReadMessage가 거부
    {
        FakeTransport t;
        WireHeader h; h.magic = kMagic; h.type = 1; h.length = kMaxWirePayload + 1;
        t.Write(&h, sizeof(h));
        Message m;
        assert_true(!ReadMessage(t, m)); // 8MiB+1 할당 없이 즉시 false
    }
    std::printf("wire_cap_probe ALL PASS\n");
    return 0;
}
```

(실제 코드에는 `assert_true`를 이 파일 안에 `static void assert_true(bool c)` — 실패 시 `std::fprintf(stderr, "FAIL\n"); std::exit(1);` — 로 정의한다. `kMagic`/`kMaxWirePayload`는 헤더의 실제 이름을 사용.)

- [ ] **Step 2: 빌드로 실패 확인**

`kMaxWirePayload`가 아직 없으므로 컴파일 실패가 기대 상태다:

```bash
bash engine/build_with_temp.sh 2>&1 | tail -5
```

Expected: `kMaxWirePayload` 미선언 오류. (프로브를 CMake에 넣는 단계는 Step 4와 함께.)

- [ ] **Step 3: 최소 구현**

`engine/include/ipc/JKWireProtocol.h` (kWireMagic 근처):

```cpp
// Upper bound for a single wire frame payload. Matches the state-file cap
// family (256 KiB state rows / 8 MiB full state) — no legitimate frame
// approaches this; larger length fields are treated as corruption.
inline constexpr uint32_t kMaxWirePayload = 8u * 1024 * 1024;
```

`engine/src/ipc/JKWireProtocol.cpp` `ReadMessage`(~:27), 헤더 유효성 검사(magic 검사 cpp:32) 직후에:

```cpp
if (header.length > kMaxWirePayload) return false; // docs/62 §8.1-②
```

- [ ] **Step 4: 프로브 CMake 등록+통과 확인**

기존 C++ 콘솔 프로브 선례(`terminal_hangul_probe`)를 따라 `engine/CMakeLists.txt` 프로브 목록에 `tools/probes/wire_cap_probe.cpp` 추가(선례 위치 검색: `terminal_hangul_probe` 문자열). 빌드+실행:

```bash
bash engine/build_with_temp.sh && ./engine/build/wire_cap_probe
```

Expected: `wire_cap_probe ALL PASS`.

- [ ] **Step 5: Commit**

```bash
git add engine/include/ipc/JKWireProtocol.h engine/src/ipc/JKWireProtocol.cpp engine/tools/probes/wire_cap_probe.cpp engine/CMakeLists.txt
git commit -m "fix(ipc): ReadMessage 페이로드 상한 캡 8MiB (docs/62 §8.1)"
```

---

### Task 3: SHA-256 통합 + CSPRNG 시드

BCrypt SHA-256이 3곳에 복제돼 있고(jkctl main.cpp:224, jktriggers main.cpp:143, JKDesktopShell.cpp:98) 포맷 교차 일치가 주석으로 강제된다 (docs/62 §8.6). 수기 SHA-256 헬퍼 1개로 통합 — jkbridge 수기 SHA-1+기동 셀프테스트 선례(main.cpp:195/:280)를 따른다. 포맷은 기존 `"sha256:"+64hex` 그대로.

**Files:**
- Create: `engine/include/JKSha256.h`, `engine/src/JKSha256.cpp`
- Test: `engine/tools/probes/sha256_probe.cpp`
- Modify: `engine/tools/jkctl/main.cpp:224`, `engine/tools/jktriggers/main.cpp:143`, `engine/src/desktop/JKDesktopShell.cpp:98`, `engine/tools/jkbridge/main.cpp:118` (RandU32), `engine/CMakeLists.txt` (소스 1줄+프로브 1줄)

**Interfaces:**
- Consumes: (없음 — 자립 모듈)
- Produces:
  - `namespace jk { std::string Sha256Hex(const uint8_t* data, size_t len); }` — 64hex, 접두 없음(호출부가 "sha256:" 붙임 — 기존 형식 유지)
  - `namespace jk { void RandomBytes(uint8_t* out, size_t n); }` — _WIN32: `BCryptGenRandom(BCRYPT_USE_SYSTEM_PREFERRED_RNG)`; 그 외: `/dev/urandom` read(기동 실패 시 `abort()`)
  - `namespace jk { bool Sha256SelfTest(); }` — 벡터 아래
- 채택 후 3 TU의 BCrypt SHA-256 블록 전량 삭제, 지문 형식/호출부 무수정.

- [ ] **Step 1: 실패 테스트 작성**

`engine/tools/probes/sha256_probe.cpp` — NIST FIPS 180 벡터(기억 말고 이 값을 그대로 쓴다):

```cpp
// sha256_probe — 수기 SHA-256 벡터 검증 (docs/62 §8.6)
#include <JKSha256.h>
#include <cstdio>
#include <cstring>
using namespace jk;

int main() {
    struct { const char* in; const char* hex; } v[] = {
        { "abc",
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
        { "",
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
    };
    for (auto& e : v) {
        std::string got = Sha256Hex((const uint8_t*)e.in, std::strlen(e.in));
        if (got != e.hex) {
            std::fprintf(stderr, "sha256_probe FAIL: %s -> %s\n", e.in, got.c_str());
            return 1;
        }
    }
    if (!Sha256SelfTest()) { std::fprintf(stderr, "sha256_probe FAIL: selftest\n"); return 1; }
    uint8_t rb[4]; RandomBytes(rb, 4); // CSPRNG 시그니처 실측(출력 단정 불가 — 비고장만)
    std::printf("sha256_probe ALL PASS\n");
    return 0;
}
```

- [ ] **Step 2: 빌드로 실패 확인**

```bash
bash engine/build_with_temp.sh 2>&1 | tail -5
```

Expected: `JKSha256.h` 미존재 오류.

- [ ] **Step 3: 구현**

`engine/include/JKSha256.h`:

```cpp
#ifndef JKSHA256_H
#define JKSHA256_H
#include <cstddef>
#include <cstdint>
#include <string>

namespace jk {
// 64 lowercase hex chars, no prefix. Callers prepend "sha256:" — the trust
// fingerprint format must stay byte-identical across jkctl/jktriggers/shell.
std::string Sha256Hex(const uint8_t* data, size_t len);
// CSPRNG. Dies (abort) if the OS entropy source is unavailable — callers
// must not proceed with weak randomness (token generation).
void RandomBytes(uint8_t* out, size_t n);
// FIPS 180 test vectors; boot-time callers gate on this (jkbridge precedent).
bool Sha256SelfTest();
} // namespace jk
#endif
```

`engine/src/JKSha256.cpp`: FIPS 180-2 표준 순수 로직(IV 6a09e667…, 라운드 상수 428a2f98…)으로 `Sha256Hex`+`Sha256SelfTest` 실구현, `RandomBytes`는 `#ifdef _WIN32` 분기(BCryptGenRandom — 단, 이 TU는 windows.h-clean이므로 jkbridge RandU32의 수기 프로토타입 선례: `extern "C" NTSTATUS WINAPI BCryptGenRandom(...)` GetProcAddress 대신 직접 `#include <bcrypt.h>`+`<windows.h>` — 이 TU는 오염 무해한 신규 TU다)/`#else`에서 `/dev/urandom` `fread`+부족 시 `abort()`.

- [ ] **Step 4: 통과 확인**

CMake에 `src/JKSha256.cpp`(코어 lib 소스 목록, :135 인근)+프로브 등록 후:

```bash
bash engine/build_with_temp.sh && ./engine/build/sha256_probe
```

Expected: `sha256_probe ALL PASS`.

- [ ] **Step 5: 4 TU 채택**

- `jkctl/main.cpp:224-235`: `Sha256Hex` BCrypt 블록 삭제 → `return "sha256:" + jk::Sha256Hex((const uint8_t*)s.data(), s.size());`
- `jktriggers/main.cpp:143-153` 동일 (원자적 바이트 인자 버전, 포맷 무수정 — :377/:381 기존 셀프테스트 벡터 유지)
- `JKDesktopShell.cpp:98-111` 동일 (DLL import 프로토타입 :46-58에서 BCrypt 5종 삭제)
- `jkbridge/main.cpp:118` `RandU32` → 내부가 `jk::RandomBytes` 호출하도록 교체 (SHA-1/QR은 그대로)

- [ ] **Step 6: 회귀 (지문 경로!)**

```bash
powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_jkctl_init.ps1 > /tmp/gate3.txt 2>&1
```

Expected: 27/27 PASS ×2 — 지문 경로(TrustPreRecord) 실측. jkbridge 회귀도: `probe_jkbridge.ps1` 28×2.

- [ ] **Step 7: Commit**

```bash
git add engine/include/JKSha256.h engine/src/JKSha256.cpp engine/tools/probes/sha256_probe.cpp engine/tools/jkctl/main.cpp engine/tools/jktriggers/main.cpp engine/src/desktop/JKDesktopShell.cpp engine/tools/jkbridge/main.cpp engine/CMakeLists.txt
git commit -m "refactor(crypto): 수기 SHA-256 헬퍼 통합 — BCrypt 3중 복제 소각 (docs/62 §8.6)"
```

---

### Task 4: 단일 인스턴스 가드 뽑아내기

`TryAcquireSingleInstanceGuard`(JKWindowServer.cpp:296)가 수기 `WaitNamedPipeA`/`CreateMutexA` 선언(:111-134)을 TU 안에 숨긴다 (docs/62 §8 — 뮤텍스→flock 교체 목표). `JKPlatform`으로 뽑아내고 posix 스텁을 남긴다. **flock 실구현은 2단계** — 여기서는 경계만.

**Files:**
- Modify: `engine/include/JKPlatform.h` (선언 추가), `engine/src/JKPlatform_win32.cpp` (실구현 이전)
- Modify: `engine/src/server/JKWindowServer.cpp:296` (호출부로 축소), `engine/CMakeLists.txt` (없음 — 기존 파일)

**Interfaces:**
- Consumes: (없음)
- Produces: `JKPlatform::AcquireServerGuard(const std::string& pipeName) -> void*` — 성공 시 가드 핸들(소유자: 호출자가 파괴 시 `JKPlatform::ReleaseServerGuard(void*)`), 실패 시 `nullptr`. 기존 mutex `Local\jkdesktop-server-<pipeName>` 명명+already-exists 거부+WaitNamedPipeA(50) 유산 프로브 로직 **전부** 이동, 동작 무수정.

- [ ] **Step 1: 이동**

- `include/JKPlatform.h`: `static void* AcquireServerGuard(const std::string& name);` + `static void ReleaseServerGuard(void* guard);` 선언.
- `src/JKPlatform_win32.cpp`: JKWindowServer.cpp:296-341의 로직(CreateMutexA/initial owner/ERROR_ALREADY_EXISTS/WaitNamedPipeA 유산 프로브) 통째 이전, `ReleaseServerGuard` = ReleaseMutex+CloseHandle.
- `JKWindowServer.cpp`: `TryAcquireSingleInstanceGuard`를 `return JKPlatform::AcquireServerGuard(pipeName) != nullptr;` 1줄로, 수기 선언(:125-131) 중 가드 전용분 삭제 — 단, `StartAcceptor`의 `WaitNamedPipeA` 스핀(:365)은 파이프 생존 확인 용도로 남긴다(전송 계약).
- 소멸 경로 `~JKWindowServer`(:186-189)가 `ReleaseServerGuard`를 부르도록.

- [ ] **Step 2: 빌드+가드 회귀**

```bash
bash engine/build_with_temp.sh && powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_desktop_resize.ps1 > /tmp/gate4.txt 2>&1
```

Expected: PASS(서버 기동/정지 생존).

- [ ] **Step 3: 제2 인스턴스 봉쇄 실측**

```bash
powershell -ExecutionPolicy Bypass -File engine/tools/probes/spin_load.ps1   # 이미 존재하는 기동 프로브 활용 가능
```

+ 수기: 라이브 서버 상태에서 `jkwinserver.exe` 제2 기동 → exit 1 즉답(테마 1줄+가드 메시지, docs/59 §11.1 as-built)을 2회 확인.

- [ ] **Step 4: Commit**

```bash
git add engine/include/JKPlatform.h engine/src/JKPlatform_win32.cpp engine/src/server/JKWindowServer.cpp
git commit -m "refactor(platform): 단일 인스턴스 가드 JKPlatform 이전 (docs/62 §8 — 2단계 flock 대비)"
```

---

### Task 5: JKConPtyBridge 파일 분할

단일 TU가 `#ifdef`로 양 플랫폼을 흡수 중 — `_win32`/`_posix` 파일 분할 패턴(src/ipc 선례)으로 통일. **posix pty 실구현(openpty)은 2단계** — 여기서는 스텁 분리만.

**Files:**
- Create: `engine/src/terminal/JKConPtyBridge_win32.cpp` (현행 파일의 _WIN32 부), `engine/src/terminal/JKConPtyBridge_posix.cpp` (현행 :246-251 스텁 확장)
- Modify: `engine/src/terminal/JKConPtyBridge.cpp` 삭제, `engine/CMakeLists.txt` (소스 목록 교체)

**Interfaces:**
- Consumes: `include/terminal/JKConPtyBridge.h` 무수정 (Start/DrainOutput/WriteInput/Resize/Stop/IsValid/ShellExited/ProcessExited)
- Produces: 동일 클래스, 플랫폼별 TU. 2단계 posix TU가 `posix_openpt`/`fork`로 채울 자리만 남김(스텁+명확한 not-implemented 메시지 유지).

- [ ] **Step 1: 분할**

현행 `JKConPtyBridge.cpp`를: win32 TU = `#ifndef _WIN32 → #endif` 반전으로 순수 윈도우 코드, posix TU = 로더/스텁 부+`posix_openpt` 계획 주석 실체화는 2단계 문서 참조. 헤더 무수정.

- [ ] **Step 2: 빌드+터미널 회귀**

```bash
bash engine/build_with_temp.sh
powershell -ExecutionPolicy Bypass -File engine/tools/probes/terminal_hangul_probe.ps1 > /tmp/gate5a.txt 2>&1
powershell -ExecutionPolicy Bypass -File engine/tools/probes/terminal_hangul_view_probe.ps1 > /tmp/gate5b.txt 2>&1
```

Expected: 33×2 + 18×2 ALL PASS (pty 경로 무변 증명).

- [ ] **Step 3: Commit**

```bash
git add engine/src/terminal/ engine/CMakeLists.txt
git commit -m "refactor(terminal): JKConPtyBridge _win32/_posix 파일 분할 (docs/62 §8.4)"
```

---

### Task 6: 1단계 회귀 전량 게이트

**Files:** (없음 — 판정만)

**Interfaces:** Consumes: Task 1-5 완료. Produces: 1단계 완료 판정.

- [ ] **Step 1: 표준 회귀 세트 ×2**

```bash
for p in probe_approve_self probe_jkctl_init terminal_hangul_probe terminal_hangul_view_probe jkedit_probe probe_jkbridge probe_workshop vpt13 vpt14 probe_settings probe_files probe_app_tools; do
  powershell -ExecutionPolicy Bypass -File "engine/tools/probes/$p.ps1" > "/tmp/s1_$p.txt" 2>&1
done
```

Expected: 전부 ALL PASS. 실패 시 해당 태스크로 되돌아가 수술.

- [ ] **Step 2: 2연속 반복**

동일 루프 1회 더 실행 — 2연속 GREEN 확정 (플레이크 폭로).

- [ ] **Step 3: docs/62 as-built 기록**

docs/62 §8 말미에 1단계 완료 기록(커밋 범위+회귀 결과) 추가 후 커밋.

---

## Self-Review 결과

1. **Spec 커버리지**: docs/62 §8 잔여 흠 5건 → Task 1(CancelPendingIo)·2(페이로드 캡)·3(암호)·4(가드)·5(pty 분할). §8.5(VideoPresent 불요)·§8.2/3(대화상자·전체화면 불요)은 태스크 없음이 정답(YAGNI 명시). 2단계 항목(Unix socket/flock/posix pty)은 스펙 §3-2에 위임 — 계획에 넣지 않음.
2. **Placeholder**: 없음 — 모든 코드 단계에 실제 코드, 모든 판정 단계에 실측 명령.
3. **타입 일관성**: `Sha256Hex(const uint8_t*, size_t)`/`RandomBytes`/`AcquireServerGuard` 선언과 채택부 호출이 일치. `kMaxWirePayload` 상수명이 probe와 구현에서 동일.