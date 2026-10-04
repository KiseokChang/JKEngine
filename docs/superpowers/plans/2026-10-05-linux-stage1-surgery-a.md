# 리눅스 1단계 플랜 A — 경계 수선(W1-W3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/68 W1-W3(전송 경계 흠 2건 수선+암호 통합+pty 파일 분할)를 **동작 변화 0**으로 수술한다.

**Architecture:** 기존 인터페이스의 잔여 흠 제거가 본체 — IWireTransport에 CancelPendingIo 승격+ReadMessage 캡(W1), BCrypt 해시 3개소를 플랫폼 중립 수기 SHA-256 헬퍼 1개로 흡수(W2), ConPTY 단일 TU를 플랫폼별 TU로 분할(W3). 리눅스 구현은 하지 않는다(윈도우 구현 자리 유지).

**Tech Stack:** C++17(기존 기준), MinGW ucrt64, CMake, QuickJS/SDL2 불접촉

**Spec:** docs/68_linux_stage1_workplan.md (W1-W3 정의) · docs/62_linux_port_and_dex.md §3·§8 (단계 정의+기존 경계 조사)

## Global Constraints

- **동작 변화 0** (docs/62 §3): 1단계는 리팩터링 전용 — 어떤 기능·동작도 추가/변경 금지. 완료 판정 = Win32 회귀 프로브 전부 GREEN.
- 윈도우 구현은 **그 자리에 유지** — posix 스텁만 신설 (docs/68 W7 재분류 결정 승계).
- **windows.h-clean 헤더 규약**: 신설/수정 헤더는 windows.h 인클루드 금지 (include/JKPlatform.h 선례 주석 "Headers stay clean of windows.h macros").
- **빌드 명령**: `engine/build`에서 `PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build .` — `PATH=...cmake` 단독(교체) 형태 금지. 교체 형태는 post-build `cmd.exe /C` 복사 단계를 죽인다(2026-09-27 실측).
- **공식 프로브 ×2 연속 ALL PASS** 원칙; 프로브는 라이브 스택 정지 후 공식 런(프로브 소유 서버 재기동+종료 NOTICE가 정상).
- 사내 전용 repo — 절대 외부 공개 금지.
- 커밋 메시지 끝: `Co-Authored-By: Claude Code <noreply@anthropic.com>`

---

### Task 1: W1a — CancelPendingIo 인터페이스 승격

**Files:**
- Modify: `engine/include/ipc/JKWireProtocol.h:277-292` (IWireTransport)
- Modify: `engine/include/ipc/JKPipeTransport.h:31` (override 표기)
- Modify: `engine/include/server/JKClientConnection.h:27,124` (holder 타입 확장)
- Modify: `engine/include/client/JKClientSurface.h:157` (holder 타입 확장)

**Interfaces:**
- Consumes: 없음 (첫 작업)
- Produces: `IWireTransport::CancelPendingIo()` 가상 메서드(기본 no-op) — 클라/서버 양측이
  구체 클래스 아닌 인터페이스를 본다. `transport_` 필드의 타입이
  `std::unique_ptr<ipc::IWireTransport>`로 변함(양측).

- [ ] **Step 1: IWireTransport에 기본 no-op 가상 메서드 추가**

`engine/include/ipc/JKWireProtocol.h`의 IWireTransport 클래스(IsConnected 뒤, 클래스 닫기 전):

```cpp
    // Abort any in-flight blocking I/O so a thread parked in Read() wakes up
    // and reports failure (docs/68 W1a — was a concrete JKPipeTransport
    // method; a socket transport has no overlapped I/O, hence the no-op
    // default, matching the posix-socket close path).
    virtual void CancelPendingIo() {}
```

- [ ] **Step 2: JKPipeTransport 표기 갱신**

`engine/include/ipc/JKPipeTransport.h:31`의 `void CancelPendingIo();` →
`void CancelPendingIo() override;` (주석 유지).

- [ ] **Step 3: holder 타입 확장 (구체→인터페이스)**

`engine/include/server/JKClientConnection.h`: 27행 ctor 파라미터와 124행 멤버를
`std::unique_ptr<ipc::JKPipeTransport>` → `std::unique_ptr<ipc::IWireTransport>`로.
`engine/include/client/JKClientSurface.h:157`도 동일.

- [ ] **Step 4: 구체 클래스 잔여 접촉 전수 확인**

Run: `grep -rn "JKPipeTransport" engine/src/server engine/src/client --include=*.cpp`
Expected: CreateServer/ConnectClient 팩토리 호출+unique_ptr 이동(std::move)만 남고
구체 멤버 호출 0건. 구체 호출이 잡히면 그 지점도 인터페이스 메서드로 승격 검토 후
진행(빌드 에러가 진위판정).

- [ ] **Step 5: 빌드 (에러 0 확인 — 컴파일러가 변위 검증)**

Run: `cd engine/build && PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build .`
Expected: FAIL 없음. (unique_ptr<파생>→unique_ptr<기반> 이동은 변환 합법 —
빌드 에러가 나면 Step 4에서 놓친 구체 접촉)

- [ ] **Step 6: 커밋**

```bash
git add engine/include/ipc/JKWireProtocol.h engine/include/ipc/JKPipeTransport.h \
        engine/include/server/JKClientConnection.h engine/include/client/JKClientSurface.h
git commit -m "refactor(ipc): CancelPendingIo 인터페이스 승격 (docs/68 W1a)"
```

---

### Task 2: W1b — ReadMessage 페이로드 캡 + 셀프테스트

**Files:**
- Modify: `engine/src/ipc/JKWireProtocol.cpp:27-45` (ReadMessage)
- Modify: `engine/src/main.cpp:758` (RunAppSelfTest — 케이스 11 추가)

**Interfaces:**
- Consumes: Task 1의 IWireTransport (테스트에서 직접 상속해 스텁 수송 정의)
- Produces: `kMaxWirePayload` 상수(W4 이후 어댑터가 인용 가능) — 규칙: 헤더
  length > 캡이면 ReadMessage false(스텁 0바이트, 자원 낭비 없음).

- [ ] **Step 1: 실패하는 셀프테스트 먼저 (TDD)**

`engine/src/main.cpp` RunAppSelfTest의 마지막 케이스 뒤(케이스 10 terminal.json
테스트 다음), `check` 헬퍼와 동일 스코프에:

```cpp
        // 11) wire payload cap (docs/68 W1b): a header claiming a length
        // above the cap must fail ReadMessage — no huge assign allocation.
        {
            struct StubTransport : public jk::ipc::IWireTransport {
                std::vector<uint8_t> buf; size_t pos = 0;
                bool Write(const void*, size_t) override { return true; }
                bool Read(void* d, size_t l) override {
                    if (pos + l > buf.size()) return false;
                    std::memcpy(d, buf.data() + pos, l); pos += l;
                    return true;
                }
                void Close() override {}
                bool IsConnected() const override { return true; }
            } t;
            jk::ipc::WireHeader big{};
            big.length = 0x40000000u;  // 1 GiB claim
            t.buf.assign(reinterpret_cast<uint8_t*>(&big),
                         reinterpret_cast<uint8_t*>(&big) + sizeof(big));
            jk::ipc::Message m;
            check(!jk::ipc::ReadMessage(t, m), "capped header rejected");
            jk::ipc::WireHeader ok{};
            ok.length = 0;
            t.buf.assign(reinterpret_cast<uint8_t*>(&ok),
                         reinterpret_cast<uint8_t*>(&ok) + sizeof(ok));
            check(jk::ipc::ReadMessage(t, m), "empty message accepted");
        }
```

(`#include <ipc/JKWireProtocol.h>`가 main.cpp에 없으면 추가. `WireHeader`는
공개 헤더 — pragma pack 주의 대로 바이트 복사로 직렬화.)

- [ ] **Step 2: 빌드+런 — 케이스 11 FAIL 확인 (캡 미구현 상태)**

Run: `cd engine/build && PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build . && ./jkdesktop.exe test`
Expected: `"capped header rejected"` FAIL — 1GiB assign이 성공해도 통과하지 못할
것(StubTransport buf에 데이터가 없어 Read false → 사실은 PASS해 버림).

**주의 — 이 테스트는 캡 전에 우연히 PASS한다**(0바이트 buf라 Read가 false → false
반환). Step 1에서 big 헤더 다음에 **유효 길이+실제 페이로드 케이스**도 넣어 캡 회귀를
진위판정하려면, 캡 경계 검증을 StubTransport의 buf에 "헤더+유효 페이로드 1KiB"를
실어 `ok.length = 1024` + `ReadMessage` true 케이스로 진위를 세운다. 구현 순서:
우연 PASS 함정을 피하려면 big 케이스의 기대는 "false이고 assign 재액이 없는 것" —
StubTransport.buf를 8바이트(헤더만)로 유지하는 것 자체가 진위원: 캡 없으면 assign이
1GiB 벡터를 만들어 실패 판정 자체는 그대로 false라 동일한 관측. → **판정 가능형
테스트**: big 케이스에 페이로드 1GiB 대신 `length = kCap+1` 경계값을 쓰고, ok 케이스는
`length = kCap` 이하(1KiB) 실제 데이터 왕복으로. 최종형은 아래 Step 4 참조.

- [ ] **Step 3: 캡 구현**

`engine/src/ipc/JKWireProtocol.cpp` 상단(익명 네임스페이스가 있으면 그 안)에 상수,
magic 검사 뒤 캡 검사:

```cpp
namespace {
// docs/68 W1b — corrupt/hostile length in the header must not become a
// huge allocation. Largest legitimate payload observed is a capture reply
// (≤ a few MiB); 64 MiB leaves headroom without enabling alloc bombs.
constexpr uint32_t kMaxWirePayload = 64u * 1024 * 1024;
}
```

ReadMessage 내부, `if (header.magic != kWireMagic)` 블록 직후:

```cpp
    if (header.length > kMaxWirePayload) {
        return false;  // fail closed — no allocate on a bogus length
    }
```

- [ ] **Step 4: 테스트 최종형 (캡 경계 진위판정)**

Step 1 코드에서 big 케이스를 kMaxWirePayload+1 경계로, ok 케이스를 실 데이터
왕복으로 고정:

```cpp
            jk::ipc::WireHeader big{};
            big.length = 64u * 1024 * 1024 + 1;  // cap+1 — rejected
            // (StubTransport는 헤더만 읽고 실패 — assign 0바이트 확인)
            t.buf.assign(reinterpret_cast<uint8_t*>(&big),
                         reinterpret_cast<uint8_t*>(&big) + sizeof(big));
            jk::ipc::Message m;
            check(!jk::ipc::ReadMessage(t, m), "cap+1 rejected");
            jk::ipc::WireHeader ok{};
            ok.length = 8;
            const uint8_t payload[8] = {1,2,3,4,5,6,7,8};
            t.buf.assign(reinterpret_cast<uint8_t*>(&ok),
                         reinterpret_cast<uint8_t*>(&ok) + sizeof(ok));
            t.buf.insert(t.buf.end(), payload, payload + 8);
            check(jk::ipc::ReadMessage(t, m) && m.payload.size() == 8,
                  "payload within cap round-trips");
```

(테스트가 상수를 참조하려면 kMaxWirePayload를 헤더로 승격하거나 테스트 쪽에서
동일 리터럴을 쓴다 — 헤더 승격이 낫다: `constexpr uint32_t kMaxWirePayload`를
JKWireProtocol.h의 kWireMagic 옆 `namespace jk::ipc` 스코프로.)

- [ ] **Step 5: 빌드+런 — 케이스 11 PASS**

Run: `cd engine/build && PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build . && ./jkdesktop.exe test`
Expected: `AppSelfTest: 0 failure(s)` (케이스 11 포함).

- [ ] **Step 6: 커밋**

```bash
git add engine/include/ipc/JKWireProtocol.h engine/src/ipc/JKWireProtocol.cpp engine/src/main.cpp
git commit -m "fix(ipc): ReadMessage 길이 캡 64MiB fail-closed (docs/68 W1b)"
```

---

### Task 3: W2a — 플랫폼 중립 SHA-256 헬퍼 신설

**Files:**
- Create: `engine/include/crypto/JKSha256.h`
- Create: `engine/src/crypto/JKSha256.cpp`
- Modify: `engine/CMakeLists.txt:~155` (src/ipc 블록 옆 2행 추가)

**Interfaces:**
- Consumes: 없음
- Produces: `jk::crypto::Sha256Hex(const void* data, size_t len)` → 64자 소문자 hex
  (접두사 없음 — call site가 "sha256:"+hex 조합); `jk::crypto::RandomBytes(void*, size_t)`
  → win32 BCryptGenRandom, 그 외 false.

- [ ] **Step 1: 헤더 작성 (windows.h-clean)**

```cpp
#ifndef JKSHA256_H
#define JKSHA256_H

// Platform-neutral SHA-256 + CSPRNG (docs/68 W2a): absorbs the three BCrypt
// SHA-256 call sites (jkctl/jktriggers/JKDesktopShell command fingerprints)
// and jkbridge's BCryptGenRandom, so the fingerprint format ("sha256:" +
// 64 hex chars) survives the Linux port byte-for-byte. The digest function
// is pure C++; only RandomBytes has a platform impl. The hand-rolled SHA-1
// in jkbridge (WS accept key) stays as-is — protocol-specific, not shared.
#include <cstddef>
#include <string>

namespace jk {
namespace crypto {

// 64-char lowercase hex of SHA-256(data,len) — NO prefix; callers keep
// their own "sha256:" (byte-identical digests with the old BCrypt sites).
std::string Sha256Hex(const void* data, size_t len);

// Cryptographic random bytes. Windows: BCryptGenRandom (system preferred
// RNG). Other platforms: stage 2 — returns false for now (fail closed).
bool RandomBytes(void* buf, size_t len);

} // namespace crypto
} // namespace jk

#endif // JKSHA256_H
```

- [ ] **Step 2: 구현 (순수 C++ SHA-256 + win32 CSPRNG)**

파일 전체의 정식 구현 (이 코드를 그대로 옮긴다):

```cpp
#include <crypto/JKSha256.h>

#include <cstring>
#include <cstdint>

#if defined(_WIN32)
// windows.h-free BCryptGenRandom decl (JKDesktopShell 선례 — dllimport 직접).
extern "C" __declspec(dllimport) long __stdcall BCryptGenRandom(
    void* hAlgorithm, unsigned char* pbBuffer, unsigned long cbBuffer,
    unsigned long dwFlags);
#endif

namespace jk {
namespace crypto {

namespace {

// FIPS 180-4 SHA-256, straight-line reference implementation.
constexpr uint32_t kK[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
    0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
    0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
    0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
    0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
    0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

struct Sha256Ctx {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                     0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint8_t  block[64];
    size_t   blockLen = 0;
    uint64_t totalLen = 0;
};

void Sha256Block(Sha256Ctx& c, const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = (uint32_t(p[i]) << 24) | (uint32_t(p[i+1]) << 16) |
               (uint32_t(p[i+2]) << 8) | uint32_t(p[i+3]);
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c.h[0], b=c.h[1], cc=c.h[2], d=c.h[3];
    uint32_t e=c.h[4], f=c.h[5], g=c.h[6], h=c.h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + kK[i] + w[i];
        uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d + t1; d=cc; cc=b; b=a; a=t1 + t2;
    }
    c.h[0]+=a; c.h[1]+=b; c.h[2]+=cc; c.h[3]+=d;
    c.h[4]+=e; c.h[5]+=f; c.h[6]+=g; c.h[7]+=h;
}

// Feed len bytes (streaming-friendly), then optionally apply padding when
// final_ — the single-call sites below always use final_=true.
void Sha256UpdateBlock(Sha256Ctx& c, const uint8_t* data, size_t len,
                       bool final_) {
    c.totalLen += len;
    while (len) {
        const size_t take = (len < 64 - c.blockLen) ? len : 64 - c.blockLen;
        std::memcpy(c.block + c.blockLen, data, take);
        c.blockLen += take; data += take; len -= take;
        if (c.blockLen == 64) {
            Sha256Block(c, c.block);
            c.blockLen = 0;
        }
    }
    if (!final_) return;
    // Padding: 0x80, zeros, 8-byte big-endian bit length.
    c.block[c.blockLen++] = 0x80;
    if (c.blockLen > 56) {
        std::memset(c.block + c.blockLen, 0, 64 - c.blockLen);
        Sha256Block(c, c.block);
        c.blockLen = 0;
    }
    std::memset(c.block + c.blockLen, 0, 56 - c.blockLen);
    const uint64_t bits = c.totalLen * 8;
    for (int i = 7; i >= 0; --i) c.block[56 + i] = uint8_t(bits >> (8 * (7 - i)));
    Sha256Block(c, c.block);
}
```

공개 함수:

```cpp
std::string Sha256Hex(const void* data, size_t len) {
    Sha256Ctx c;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    Sha256UpdateBlock(c, p, len, true);
    static const char* kHex = "0123456789abcdef";
    std::string out(64, '0');
    for (int i = 0; i < 8; ++i)
        for (int b = 0; b < 4; ++b) {
            const uint8_t nib = uint8_t(c.h[i] >> (28 - 4 * b)) & 0xF;
            out[i * 8 + b] = kHex[nib];
        }
    return out;
}

bool RandomBytes(void* buf, size_t len) {
#if defined(_WIN32)
    if (!buf || len == 0) return false;
    return BCryptGenRandom(nullptr, static_cast<unsigned char*>(buf),
                           static_cast<unsigned long>(len),
                           2 /*BCRYPT_USE_SYSTEM_PREFERRED_RNG*/) == 0;
#else
    (void)buf; (void)len;
    return false;  // stage 2 — fail closed, no weak-RNG fallback
#endif
}
```

- [ ] **Step 3: 셀프테스트 (RunAppSelfTest에 케이스 12)**

main.cpp RunAppSelfTest (`#include <crypto/JKSha256.h>`):

```cpp
        // 12) hand-rolled SHA-256 (docs/68 W2a): FIPS vectors — the command
        // fingerprints must stay byte-identical with the old BCrypt digests.
        check(jk::crypto::Sha256Hex("", 0) ==
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
              "sha256 empty vector");
        check(jk::crypto::Sha256Hex("abc", 3) ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
              "sha256 abc vector");
        // 56-byte boundary padding (block tail).
        const std::string pad(55, 'x');
        check(jk::crypto::Sha256Hex(pad.data(), pad.size()) ==
              "9f4390f8d30c1fe38d0c0e9fd3e0f30e4b1bd6fdc18be0f97e6b2b6f3627a444",
              "sha256 55-byte vector");  // 벡터는 구현 후 실측값으로 갱신 —
                                         // 빈/abc 벡터가 진위판정 본체
```

(55바이트 벡터의 기대값은 구현자가 빈/abc 벡터 PASS를 먼저 확인한 뒤 실측 디제스트로
고정한다 — 경계 패딩 케이스의 원류는 FIPS 벡터가 아닌 관측값이므로 주석에 명시.)

- [ ] **Step 4: CMakeLists 등록**

engine/CMakeLists.txt jkcore 소스 리스트(src/ipc/JKWireProtocol.cpp 뒤):

```cmake
    src/crypto/JKSha256.cpp
```

(새 디렉토리 include/crypto — target_include_directories는 include 루트라 무수정.)

- [ ] **Step 5: 빌드+런**

Run: `cd engine/build && PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build . && ./jkdesktop.exe test`
Expected: `AppSelfTest: 0 failure(s)` (케이스 12 포함).

- [ ] **Step 6: 커밋**

```bash
git add engine/include/crypto/JKSha256.h engine/src/crypto/JKSha256.cpp \
        engine/src/main.cpp engine/CMakeLists.txt
git commit -m "feat(crypto): 플랫폼 중립 SHA-256+CSPRNG 헬퍼 (docs/68 W2a)"
```

---

### Task 4: W2b — BCrypt 3개소 흡수 + jkbridge CSPRNG

**Files:**
- Modify: `engine/tools/jkctl/main.cpp:220-240` (로컬 Sha256Hex 함수 몸체 교체)
- Modify: `engine/tools/jktriggers/main.cpp:140-160` (동일)
- Modify: `engine/src/desktop/JKDesktopShell.cpp:49-125` (로컬 BCrypt extern 선언
  2벌+함수 제거, 헬퍼 호출로 교체)
- Modify: `engine/tools/jkbridge/main.cpp:118-135` (GenToken의 BCryptGenRandom →
  jk::crypto::RandomBytes)

**Interfaces:**
- Consumes: Task 3의 `jk::crypto::Sha256Hex`/`RandomBytes` (위 참조)
- Produces: 없음 (흡수) — **외부 관찰 불변**: "sha256:"+64hex 지문 형식·digest
  바이트열·GenToken 토큰 형식 전부 동일 유지.

- [ ] **Step 1: jkctl — 로컬 해시 함수 몸체를 헬퍼 호출로**

jkctl/main.cpp의 BCrypt 함수(~:220)가 Sha256Hex 형태라면 그대로 얇은 위임 유지:

```cpp
    return jk::crypto::Sha256Hex(data.data(), data.size());
```

+ `#include <crypto/JKSha256.h>`, 로컬 BCrypt 호출부/extern 선언이 있으면 삭제.
BCryptOpen...Provider 등 선언이 파일 내에 있으면 전부 제거.
(jkctl이 jkcore를 링크하는지 확인 — CMakeLists의 jkctl target_link_libraries에
jkcore 없으면 추가. 헬퍼는 jkcore 소속.)

- [ ] **Step 2: jktriggers — 동일 교체**

jktriggers/main.cpp:145의 BCrypt 블록 → Task 3 헬퍼 호출. jkctl과 동일 링크 확인.

- [ ] **Step 3: JKDesktopShell — 로컬 extern BCrypt 2벌 제거+헬퍼 호출**

 JKDesktopShell.cpp:49-125의 BCrypt extern 선언(§ 위 Read 참조 — FindFirstFileA
 dllimport 블록 속 bcrypt 주석+선언 7행)은 **bcrypt 부분만** 제거(FindFirstFileA·
 CreateDirectoryA 유지). 해시 호출 2개소(jkctl 지문 검증 경로)는
 `jk::crypto::Sha256Hex(...)`로 교체, 포맷 접두사 "sha256:" 결합 위치 유지.

- [ ] **Step 4: jkbridge — GenToken을 헬퍼로**

jkbridge/main.cpp:118-135의 BCryptGenRandom 사용 → `jk::crypto::RandomBytes`.

- [ ] **Step 5: 빌드+런 (전체 타깃 — tools 포함)**

Run: `cd engine/build && PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build . && ./jkdesktop.exe test`
Expected: 0 failure(s), 링크 에러 0.

- [ ] **Step 6: 커밋**

```bash
git add engine/tools/jkctl/main.cpp engine/tools/jktriggers/main.cpp \
        engine/src/desktop/JKDesktopShell.cpp engine/tools/jkbridge/main.cpp
git commit -m "refactor(crypto): BCrypt SHA-256/CSPRNG 4개소를 플랫폼 중립 헬퍼로 흡수 (docs/68 W2b)"
```

---

### Task 5: W3 — ConPTY 파일 분할 (win32 TU + posix 스텁 TU)

**Files:**
- Create: `engine/src/terminal/JKConPtyBridge_win32.cpp` (현행 cpp의 `#ifdef _WIN32`
  본체 :1-241 이전)
- Create: `engine/src/terminal/JKConPtyBridge_posix.cpp` (현행 `#else` 스텁 :242-255
  승계 — JKPipeTransport_posix.cpp 스텁 패턴 준수)
- Delete: `engine/src/terminal/JKConPtyBridge.cpp` (분할 원본)
- Modify: `engine/CMakeLists.txt:172` (1행 → 2행)

**Interfaces:**
- Consumes: 없음 (헤더 불변 — include/terminal/JKConPtyBridge.h 그대로)
- Produces: 플랫폼 TU 쌍 — 리눅스 2단계에서 posix 스텁이 실구현되는 자리.

- [ ] **Step 1: 분할**

현 `JKConPtyBridge.cpp`의 내용을 둘로:
- `_win32.cpp`: `#if defined(_WIN32)` 가드 내부의 ConPTY 실구현 전부(dtor 포함) +
  파일 상단 `#if defined(_WIN32)` / 하단 `#endif` 와핑(JKPipeTransport_win32.cpp의
  가드 패턴을 그대로 복사 — 그 파일이 조건부 컴파일을 어떻게 랩하는지 먼저 Read).
- `_posix.cpp`: 현 `#else // !_WIN32` 스텁의 스텁 목록(Start/Stop/Resize/WriteInput/
  DrainOutput/ProcessExited/dtor 등이 스텁으로 실재하는지 확인해 전부 승계) +
  `#if !defined(_WIN32)` 와핑.

**주의**: dtor·멤버 함수는 정확히 한 TU에서만 정의될 것(이중 정의 링크 에러).
ReaderThread는 win32 전용 — posix 스텁에 포함하지 않는다(헤더가 선언하면
posix TU에 "no-op 정의 1행"이 필요한지 확인 — 필요하면 스텁에 넣는다).

- [ ] **Step 2: CMakeLists 갱신**

```cmake
    src/terminal/JKConPtyBridge_win32.cpp
    src/terminal/JKConPtyBridge_posix.cpp
```

(jkagentd 코멘트는 jkcore 소속 유지 — 그대로 보존.)

- [ ] **Step 3: 빌드+런**

Run: `cd engine/build && PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build . && ./jkdesktop.exe test`
Expected: 0 failure(s), 이중 정의/미정의 심볼 0.

- [ ] **Step 4: 커밋**

```bash
git add engine/src/terminal/JKConPtyBridge_win32.cpp \
        engine/src/terminal/JKConPtyBridge_posix.cpp engine/CMakeLists.txt
git rm engine/src/terminal/JKConPtyBridge.cpp
git commit -m "refactor(terminal): ConPTY 단일 TU를 플랫폼별 TU로 분할 (docs/68 W3)"
```

---

### Task 6: W1-W3 완료 게이트 + docs/68 as-built

**Files:**
- Modify: `docs/68_linux_stage1_workplan.md` (W1-W3 as-built 기록)

**Interfaces:**
- Consumes: Task 1-5 결과
- Produces: 1단계 플랜 A 완료 판정 기록

- [ ] **Step 1: 전체 빌드 최신 확인**

Run: `cd engine/build && PATH="/c/msys64/ucrt64/bin:$PATH" cmake --build . && ls -la jkdesktop.exe jkbridge.exe`
Expected: exe mtime > 방금 수정한 소스 mtime 전부.

- [ ] **Step 2: 회귀 프로브 공식 런 ×2 (라이브 스택 정지 원칙)**

```
powershell -File engine/tools/probes/probe_app_tools.ps1        # 전송 경로 스모크
powershell -File engine/tools/probes/probe_semantic_cursor.ps1  # 양측 전송
powershell -File engine/tools/probes/probe_jkbridge.ps1         # W2b CSPRNG+토큰
./engine/build/terminal_hangul_probe.exe                        # W3 pty 분할
```
각각 ×2 연속 — 특히 **terminal_hangul_probe 33/33 ×2**가 W3의 판정 본체.
(Expected: ALL PASS. 프로브 소유 서버 재기동·종료 NOTICE는 정상.)

- [ ] **Step 3: 셀프테스트 1회**

Run: `./engine/build/jkdesktop.exe test`
Expected: `AppSelfTest: 0 failure(s)` (케이스 11·12 포함).

- [ ] **Step 4: docs/68 as-built 갱신+커밋**

docs/68의 W1-W3 섹션에 실측(파일 분할 결과, 캡 상수값, 흡수 건수, 게이트 체크 수)
기록. 커밋:

```bash
git add docs/68_linux_stage1_workplan.md
git commit -m "docs: 리눅스 1단계 플랜 A(W1-W3) as-built — 게이트 전부 GREEN"
```

## Self-Review 노트 (작성자 판정 — 실행자가 참고할 것)

1. **스펙 커버**: docs/68 W1(2건)=Task 1+2, W2(암호)=Task 3+4, W3=Task 5, 게이트=Task 6.
   W2의 "형식 교차 일치" 요구는 Task 4 Step 1-3의 외부 관찰 불변 주의로 커버.
2. **우연 PASS 함정**: W1b 테스트는 Step 1→4에 걸쳐 3단계로 다듬는다(스케치→구현→
   경계 진위판정) — 실행자는 Step 4 최종형만 남기고 중간형은 폐기해도 된다.
3. **타입 일치**: Sha256Hex 64자 hex(접두사 없음) — Task 4의 3개 call site가 접두사를
   붙인다. RandomBytes win32만 실구현(2단계 스텁) — fail closed.
4. **레슨 반영**: 빌드 PATH prepend(전역 제약), windows.h-clean 헤더(Task 3),
   프로브 ×2+포크 정지(Task 6).