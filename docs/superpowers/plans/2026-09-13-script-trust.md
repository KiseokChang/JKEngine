# 스크립트 신뢰 모델 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** jktriggers가 eval하는 트리거 스크립트에 SHA-256 지문 기반 신뢰 게이트를 둔다 — 패커 출력은 자동 신뢰, 미기록 지문은 서버 승인 파이프라인(채팅창 프롬프트)으로 첫 실행을 통과시킨다.

**Architecture:** 지문은 Windows CNG(BCrypt) SHA-256. 신뢰 저장소는 `state\trust.json`(레코드 upsert, pack/user 2출처). 승인은 close_window 파이프라인의 PendingApproval 일반화(kind 필드) — jktriggers가 `trust_request` 도구 쿼리 → 서버가 파킹 + `agent.approval_request` 방송 → 채팅창 프롬프트 → `approve` 도구로 완결. trust.json 쓰기 권위는 로더(jktriggers).

**Tech Stack:** C++20 (MSYS2 UCRT64 MinGW), BCrypt (`-lbcrypt`), QuickJS, 기존 JKAgentClient/AgentJson/JKJkxFile 스택.

**Spec:** `docs/superpowers/specs/2026-09-13-script-trust-design.md`

## Global Constraints

- 지문 표기: `sha256:<64hex>` — 스펙 §2 verbatim.
- .jkx 지문 = **MANI + SCRI 페이로드를 TOC 순서대로 연결**한 바이트의 SHA-256 (컨테이너 전체 아님) — 스펙 §2.
- dev .js 지문 = 파일 바이트 전체의 SHA-256 — 스펙 §2.
- 신뢰 판정 실패(해시 실패·서버 다운·타임아웃·거부)는 전부 **fail-closed = 스킵** — 스펙 §3/§4.1.
- 패커 upsert는 `source:"pack"` 레코드만 건드리고 `source:"user"` 레코드는 절대 보존 — 스펙 §3.
- trust.json 쓰기는 서버가 아니라 로더(jktriggers)가 한다 — 스펙 §4.2.
- permissions.json `"trust_request"` 기본값 = **"ask"** (파일/키 없음 포함) — 스펙 §4.2.
- 승인 이벤트 페이로드 필드: `kind`, `name`, `origin`("dev"|"package"), `fingerprint` — 스펙 §4.2 (origin과 trust.json의 source는 다른 축).
- JKJkxFile 컨테이너 포맷 무변경 (SIGN 엔트리는 승격 경로로만 남김) — 스펙 §2.
- 빌드: `cmake --build engine/build` 후 `engine/build`에서 실행. 모든 커밋은 repo root에서.
- 기존 회귀는 전부 녹색 유지: `jkdesktop test` 0, `jkagentd --selftest` 0, mcp 5/5, e2e 7/7, palette 4/4, chat 7/7, triggers 7/7.

---

### Task 1: SHA-256 래퍼 + `jktriggers --selftest` 골격

**Files:**
- Modify: `engine/tools/jktriggers/main.cpp` (include 블록 위, `Sha256Hex` 추가 / `main()` 분기)
- Modify: `engine/CMakeLists.txt:527-531` (bcrypt 링크)

**Interfaces:**
- Consumes: 없음 (첫 태스크)
- Produces: `std::string Sha256Hex(const uint8_t* data, size_t len)` — `"sha256:<64hex>"` 반환, 실패 시 `""`. 이후 모든 태스크가 이 서명을 사용.

- [ ] **Step 1: CMake에 bcrypt 링크 추가**

`engine/CMakeLists.txt`의 jktriggers 블록(527-531)을:

```cmake
# M2b trigger-script host: console process, no UI (outside JKWindow/SDL).
# bcrypt: SHA-256 for the script trust model (docs/37 spec, CNG BCrypt).
add_executable(jktriggers tools/jktriggers/main.cpp)
target_link_libraries(jktriggers PRIVATE jkcore bcrypt)
if(WIN32)
    target_link_options(jktriggers PRIVATE -static-libstdc++ -static-libgcc)
endif()
```

- [ ] **Step 2: Sha256Hex + SelfTest 작성 (테스트가 곧 구현 검증)**

`engine/tools/jktriggers/main.cpp` — includes에 추가:

```cpp
#include <bcrypt.h>
```

`namespace {` 안, `HostLog` 정의 뒤에 추가:

```cpp
// ---------------------------------------------------------------------------
// Script trust model (docs/37 spec): SHA-256 fingerprints via Windows CNG.
// ---------------------------------------------------------------------------

// "sha256:<64 hex>" — the identity of a script (spec §2). Returns "" on
// failure; an empty fingerprint never matches a trust record (fail-closed).
std::string Sha256Hex(const uint8_t* data, size_t len) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return "";
    BCRYPT_HASH_HANDLE h = nullptr;
    uint8_t digest[32] = {};
    bool ok = BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0;
    if (ok && len > 0) ok = BCryptHashData(h, (PUCHAR)data, (ULONG)len, 0) == 0;
    if (ok) ok = BCryptFinishHash(h, digest, sizeof(digest), 0) == 0;
    if (h) BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) return "";
    static const char* kHex = "0123456789abcdef";
    std::string out = "sha256:";
    for (uint8_t b : digest) {
        out += kHex[b >> 4];
        out += kHex[b & 0xf];
    }
    return out;
}

int SelfTest() {
    int fails = 0;
    auto check = [&](bool ok, const char* what) {
        HostLog(std::string("[selftest] ") + what + ": " + (ok ? "PASS" : "FAIL"));
        if (!ok) ++fails;
    };
    // NIST FIPS 180-4 known vectors.
    check(Sha256Hex((const uint8_t*)"abc", 3) ==
              "sha256:ba7816bf8f01cfea414140de5dae2223"
              "b00361a396177a9cb410ff61f20015ad",
          "sha256 abc");
    check(Sha256Hex(nullptr, 0) ==
              "sha256:e3b0c44298fc1c149afbf4c8996fb924"
              "27ae41e4649b934ca495991b7852b855",
          "sha256 empty");
    if (fails == 0) HostLog("[selftest] all passed");
    return fails == 0 ? 0 : 1;
}
```

`main()`의 `--pack` 분기(694-696) 직후에 추가:

```cpp
    if (argc >= 2 && std::strcmp(argv[1], "--selftest") == 0) {
        return SelfTest();
    }
```

- [ ] **Step 3: 빌드 + 셀프테스트 실행**

Run: `cmake --build I:/progwork/JKENGINE/engine/build --target jktriggers`
Expected: BUILD 성공

Run: `cd I:/progwork/JKENGINE/engine/build && ./jktriggers.exe --selftest`
Expected: `[selftest] sha256 abc: PASS`, `[selftest] sha256 empty: PASS`, `[selftest] all passed`, exit 0

- [ ] **Step 4: Commit**

```bash
git add engine/CMakeLists.txt engine/tools/jktriggers/main.cpp
git commit -m "feat(trust): SHA-256 fingerprint wrapper (BCrypt) + jktriggers --selftest"
```

---

### Task 2: 신뢰 저장소 — trust.json 레코드 (순수 함수 + 셀프테스트)

**Files:**
- Modify: `engine/tools/jktriggers/main.cpp` (SelfTest 위에 trust store 함수 추가, SelfTest에 케이스 추가)

**Interfaces:**
- Consumes: `Sha256Hex` (Task 1), `jk::agent::AgentJson` (GetArraySize/GetArrStr/GetArrInt)
- Produces:
  - `struct TrustRecord { std::string fingerprint, name, source; int ts = 0; }`
  - `bool LoadTrustRecords(const std::string& path, std::vector<TrustRecord>* out)`
  - `bool SaveTrustRecords(const std::string& path, const std::vector<TrustRecord>& recs)`
  - `void TrustUpsert(std::vector<TrustRecord>* recs, const TrustRecord& r)` — fingerprint 매칭 교체, 없으면 push
  - `bool IsTrusted(const std::vector<TrustRecord>& recs, const std::string& fp)`
  - `int SelfTest()` (확장)

- [ ] **Step 1: trust store 함수 작성**

`Sha256Hex` 아래에 추가:

```cpp
// ---------------------------------------------------------------------------
// Trust store — state\trust.json (spec §3).
// {"records":[{"fingerprint":"sha256:…","name":…,"source":"pack"|"user","ts":…}]}
// Missing/corrupt file = everything untrusted (fail-closed). ts is epoch
// seconds (int — AgentJson's int accessor bound; ms would overflow).
struct TrustRecord {
    std::string fingerprint;  // "sha256:<64hex>" — the identity key
    std::string name;         // display name ("trig_build", "state/x.js")
    std::string source;       // "pack" (packer self-attestation) | "user" (approval)
    int ts = 0;               // epoch seconds
};

bool LoadTrustRecords(const std::string& path, std::vector<TrustRecord>* out) {
    out->clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::string buf;
    char chunk[4096];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
        buf.append(chunk, got);
    std::fclose(f);
    jk::agent::AgentJson json(buf);
    int n = 0;
    if (!json.ok() || !json.GetArraySize("records", n)) return false;
    for (int i = 0; i < n && i < 512; ++i) {
        TrustRecord r;
        if (!json.GetArrStr("records", i, "fingerprint", r.fingerprint) ||
            r.fingerprint.empty())
            continue;
        json.GetArrStr("records", i, "name", r.name);
        json.GetArrStr("records", i, "source", r.source);
        json.GetArrInt("records", i, "ts", r.ts);
        out->push_back(std::move(r));
    }
    return true;
}

bool SaveTrustRecords(const std::string& path,
                      const std::vector<TrustRecord>& recs) {
    std::string dir = path;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) {
        dir = dir.substr(0, slash);
        CreateDirectoryA(dir.c_str(), nullptr);
    }
    std::string out = "{\"records\":[";
    bool first = true;
    for (const auto& r : recs) {
        if (!first) out += ",";
        first = false;
        out += "{\"fingerprint\":\"" + JsonEscapeStr(r.fingerprint) +
               "\",\"name\":\"" + JsonEscapeStr(r.name) +
               "\",\"source\":\"" + JsonEscapeStr(r.source) +
               "\",\"ts\":" + std::to_string(r.ts) + "}";
    }
    out += "]}";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return true;
}

// Upsert by fingerprint — replace in place or append. Callers own the
// preservation rule: the packer only writes source:"pack", the loader only
// writes source:"user" (spec §3).
void TrustUpsert(std::vector<TrustRecord>* recs, const TrustRecord& r) {
    for (auto& existing : *recs) {
        if (existing.fingerprint == r.fingerprint) {
            existing = r;
            return;
        }
    }
    recs->push_back(r);
}

// A fingerprint is trusted only via an explicit record (spec §3 — no
// "trusted by absence").
bool IsTrusted(const std::vector<TrustRecord>& recs, const std::string& fp) {
    if (fp.empty()) return false;
    for (const auto& r : recs) {
        if (r.fingerprint == fp) return true;
    }
    return false;
}
```

- [ ] **Step 2: SelfTest에 trust store 케이스 추가**

`SelfTest()`의 sha256 empty 체크 뒤에 추가:

```cpp
    // Trust store: upsert idempotency + user-record preservation (spec §3).
    {
        const std::string path = g_exeDir + "\\state\\_selftest_trust.json";
        std::vector<TrustRecord> recs;
        TrustRecord pack;
        pack.fingerprint = "sha256:aaa1";
        pack.name = "pack_bundle";
        pack.source = "pack";
        pack.ts = 100;
        TrustUpsert(&recs, pack);
        TrustUpsert(&recs, pack);  // idempotent
        TrustRecord user;
        user.fingerprint = "sha256:bbb2";
        user.name = "state/x.js";
        user.source = "user";
        user.ts = 200;
        TrustUpsert(&recs, user);
        TrustRecord pack2 = pack;
        pack2.ts = 300;  // repack bumps ts, same fingerprint
        TrustUpsert(&recs, pack2);
        check(recs.size() == 2, "upsert idempotent");
        check(IsTrusted(recs, "sha256:aaa1") && IsTrusted(recs, "sha256:bbb2"),
              "is_trusted recorded");
        check(!IsTrusted(recs, "sha256:ccc3") && !IsTrusted(recs, ""),
              "is_trusted unknown fail-closed");
        check(SaveTrustRecords(path, recs), "trust save");
        std::vector<TrustRecord> back;
        LoadTrustRecords(path, &back);
        check(back.size() == 2 && back[0].source == "pack" &&
                  back[1].source == "user",
              "trust roundtrip + user record preserved");
        DeleteFileA(path.c_str());
    }
```

- [ ] **Step 3: 빌드 + 셀프테스트 실행**

Run: `cmake --build I:/progwork/JKENGINE/engine/build --target jktriggers && cd I:/progwork/JKENGINE/engine/build && ./jktriggers.exe --selftest`
Expected: 기존 2건 + 신규 4건 전부 PASS, exit 0

- [ ] **Step 4: Commit**

```bash
git add engine/tools/jktriggers/main.cpp
git commit -m "feat(trust): trust store records (state/trust.json) — load/save/upsert/is-trusted"
```

---

### Task 3: 지문 계산 — dev 파일 + 컨테이너 (MANI+SCRI, TOC 순)

**Files:**
- Modify: `engine/tools/jktriggers/main.cpp` (IsTrusted 아래에 지문 함수, SelfTest에 케이스)

**Interfaces:**
- Consumes: `Sha256Hex` (Task 1), `jk::JKJkxFile` (Open/EntryCount/Entries/ReadEntry)
- Produces:
  - `std::string FingerprintBytes(const std::vector<uint8_t>& bytes)` — 빈 입력은 `""` (fail-closed)
  - `std::string FingerprintContainer(jk::JKJkxFile& jkx)` — MANI+SCRI 페이로드 TOC 순 연결

- [ ] **Step 1: 지문 함수 작성**

`IsTrusted` 아래에 추가:

```cpp
// Fingerprint of raw bytes — empty input yields "" (an empty script never
// gets a trust record; the loader skips it anyway).
std::string FingerprintBytes(const std::vector<uint8_t>& bytes) {
    return bytes.empty() ? std::string()
                         : Sha256Hex(bytes.data(), bytes.size());
}

// Container fingerprint (spec §2): SHA-256 over the MANI + SCRI payloads
// concatenated in TOC order — icon/metadata-only changes do NOT re-trigger
// approval. Any read failure yields "" (fail-closed).
std::string FingerprintContainer(jk::JKJkxFile& jkx) {
    std::vector<uint8_t> blob;
    for (int i = 0; i < jkx.EntryCount(); ++i) {
        const auto& e = jkx.Entries()[i];
        if (std::strcmp(e.type, "MANI") != 0 && std::strcmp(e.type, "SCRI") != 0)
            continue;
        std::vector<uint8_t> payload;
        if (!jkx.ReadEntry(i, payload)) return "";
        blob.insert(blob.end(), payload.begin(), payload.end());
    }
    return FingerprintBytes(blob);
}
```

- [ ] **Step 2: SelfTest에 컨테이너 지문 케이스 추가**

SelfTest의 trust store 블록 뒤에 추가:

```cpp
    // Container fingerprint: MANI+SCRI in TOC order, deterministic, and
    // sensitive to script content (spec §2).
    {
        const std::string path = g_exeDir + "\\state\\_selftest_fp.jkx";
        const char* maniText = "name=demo\ntrigger=a.js\n";
        std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
        entries.emplace_back(
            "manifest.txt",
            std::vector<uint8_t>(maniText, maniText + std::strlen(maniText)));
        entries.emplace_back("a.js", {'l','o','g','(',')',';'});
        check(jk::JKJkxFile::Write(path, entries), "fp test container write");
        jk::JKJkxFile jkx;
        check(jkx.Open(path), "fp test container open");
        const std::string fp1 = FingerprintContainer(jkx);
        const std::string fp2 = FingerprintContainer(jkx);
        check(!fp1.empty() && fp1 == fp2, "container fp deterministic");
        // Same manifest + changed script -> different fingerprint.
        entries[1].second = {'l','o','g','(','1',')',';'};
        check(jk::JKJkxFile::Write(path, entries), "fp test rewrite");
        jk::JKJkxFile jkx2;
        check(jkx2.Open(path), "fp test reopen");
        check(FingerprintContainer(jkx2) != fp1, "container fp content-bound");
        DeleteFileA(path.c_str());
    }
```

- [ ] **Step 3: 빌드 + 셀프테스트 실행**

Run: `cmake --build I:/progwork/JKENGINE/engine/build --target jktriggers && cd I:/progwork/JKENGINE/engine/build && ./jktriggers.exe --selftest`
Expected: 기존 6건 + 신규 5건 전부 PASS, exit 0

- [ ] **Step 4: Commit**

```bash
git add engine/tools/jktriggers/main.cpp
git commit -m "feat(trust): container fingerprint — MANI+SCRI payloads in TOC order"
```

---

### Task 4: 로더 게이트 + 승인 요청 (trust_request 클라이언트)

**Files:**
- Modify: `engine/tools/jktriggers/main.cpp` (TrustGate 함수, LoadJsDir/LoadTriggerContainers 게이트, main() connect-first 재구성, 런타임 trust 전역)

**Interfaces:**
- Consumes: Task 2/3의 `LoadTrustRecords/TrustUpsert/IsTrusted/FingerprintBytes/FingerprintContainer`, 서버의 `trust_request` 도구 (Task 6에서 구현 — 이 태스크 단독으로는 서버가 `not_implemented` 회신 → TrustGate가 fail-closed로 스킵. 실측은 Task 10 프로브)
- Produces:
  - `bool TrustGate(const std::string& name, const char* origin, const std::string& fp)` — true면 eval 진행
  - 전역 `std::vector<TrustRecord> g_trust` — 시작 시 `state\trust.json`에서 로드

- [ ] **Step 1: 런타임 전역 + TrustGate 작성**

전역 섹션(`std::map<std::string,int> g_enabled;` 아래)에:

```cpp
// Trust store in memory (spec §3) — loaded once at startup; approval
// resolutions upsert into it and the file.
std::vector<TrustRecord> g_trust;
```

`FingerprintContainer` 아래에:

```cpp
// Spec §4.1: fingerprint → IsTrusted → trust_request approval → record.
// Returns false to skip the script. Every failure path (hash failure,
// server down, deny, timeout, permission) is fail-closed.
bool TrustGate(const std::string& name, const char* origin,
               const std::string& fp) {
    if (fp.empty()) {
        HostLog("[triggers] untrusted (fingerprint failed): " + name);
        return false;
    }
    if (IsTrusted(g_trust, fp)) return true;
    HostLog("[triggers] trust approval needed: " + name + " (" +
            fp.substr(0, 15) + "…)");
    std::string args = "{\"name\":\"" + JsonEscapeStr(name) +
                       "\",\"origin\":\"" + origin +
                       "\",\"fingerprint\":\"" + fp + "\"}";
    std::string reply;
    if (!g_agent.Query("trust_request", args, reply)) {
        HostLog("[triggers] trust_request failed (server down) — skip: " +
                name);
        return false;
    }
    if (reply.find("\"ok\":true") == std::string::npos) {
        HostLog("[triggers] trust denied: " + name + " -> " + reply);
        return false;
    }
    TrustRecord r;
    r.fingerprint = fp;
    r.name = name;
    r.source = "user";
    r.ts = static_cast<int>(std::time(nullptr));
    TrustUpsert(&g_trust, r);
    SaveTrustRecords(g_exeDir + "\\state\\trust.json", g_trust);
    HostLog("[triggers] trusted + recorded: " + name);
    return true;
}
```

- [ ] **Step 2: 두 로딩 경로에 게이트 적용**

`LoadJsDir()` — 파일 읽고 `code.empty()` 검사 사이(541행 근처)에:

```cpp
        if (!code.empty()) {
            if (!TrustGate(std::string("state/") + fd.cFileName, "dev",
                           FingerprintBytes(std::vector<uint8_t>(
                               code.begin(), code.end()))))
                continue;
            EvalScript(code, std::string("state/") + fd.cFileName);
        }
```

`LoadTriggerContainers()` — manifest 읽기 성공 후, `trigger=` 파싱 루프 앞에 (컨테이너 전체가 신뢰 단위):

```cpp
        // The container is the trust unit (spec §2): one fingerprint per
        // .jkx, gated once before any of its scripts eval.
        if (!TrustGate(container, "package", FingerprintContainer(jkx)))
            continue;
```

- [ ] **Step 3: main() connect-first 재구성**

`main()`의 로딩 블록(698-705)을:

```cpp
    if (!InitRuntime()) {
        HostLog("jktriggers: QuickJS init failed");
        return 1;
    }
    // Spec §4.1: first-run approvals need the server — connect (best effort,
    // ~5 s) BEFORE loading. Trusted records still load offline; untrusted
    // scripts fail closed when the server never appears.
    for (int i = 0; i < 10 && !g_agent.Connect(); ++i) Sleep(500);
    if (g_agent.IsConnected()) {
        g_agent.SubscribeEvents(true);
        HostLog("[triggers] connected to the window server");
    }
    LoadTrustRecords(g_exeDir + "\\state\\trust.json", &g_trust);
    LoadJsDir();
    LoadTriggerContainers();
    ReloadTriggerFlags();   // apply state/triggers.json before first dispatch
    WriteLoadedManifest();
```

- [ ] **Step 4: 빌드 + 셀프테스트 + 기존 회귀**

Run: `cmake --build I:/progwork/JKENGINE/engine/build --target jktriggers && cd I:/progwork/JKENGINE/engine/build && ./jktriggers.exe --selftest`
Expected: 전부 PASS (gate는 서버 미구현이라 실서버 동작은 Task 10에서 실측)

Run: `powershell -File tools/probes/probe_agent_triggers.ps1` (repo root 기준 `engine/tools/probes/`)
Expected: 7/7 PASS — 기존 번들 3종은 이 시점엔 trust.json 기록이 없어 **프롬프트 없이 로드되어야 하는데 gate가 막는다**. 서버에 trust_request가 아직 없으므로 `not_implemented` → fail-closed 스킵 → 이 프로브는 FAIL이 예상된다.

중요: 이것은 정상이다. 다음 둘 중 택일 — (a) 이 커밋 전에 `--pack` 재실행으로 pack 레코드를 먼저 만든다(단, PackMode 업서트는 Task 5). 실용적으로는 (b) Task 5를 먼저 빌드에 포함해 패커가 pack 레코드를 기록하게 한 뒤 회귀를 돌린다. **그래서 Task 4와 5는 한 커밋으로 묶는다.**

- [ ] **Step 5: Commit (Task 5와 묶음 — Task 5 완료 후)**

---

### Task 5: 패커 자기-증명 (source:"pack" upsert)

**Files:**
- Modify: `engine/tools/jktriggers/main.cpp` (`PackMode` 성공 분기)

**Interfaces:**
- Consumes: `TrustUpsert/LoadTrustRecords/SaveTrustRecords` (Task 2), `FingerprintBytes` (Task 3)
- Produces: 패킹 성공 시 `<exeDir>\state\trust.json`에 `source:"pack"` 레코드. CMake ALL 타깃(`triggers`)이 재빌드마다 재팩하므로 부팅부터 기존 번들 3종이 무프롬프트.

- [ ] **Step 1: PackMode에 자기-증명 추가**

`PackMode()`의 `if (jk::JKJkxFile::Write(out, entries))` 분기(668-673)를:

```cpp
        const std::string out = outDir + "\\" + fd.cFileName + ".jkx";
        if (jk::JKJkxFile::Write(out, entries)) {
            HostLog("jktriggers: packed " + out);
            ++packed;
            // Spec §3 self-attestation: the packer records its output's
            // fingerprint as source "pack" (upsert — user records untouched).
            // Entries order == TOC order == the MANI+SCRI stream the loader
            // hashes, so the fingerprints agree by construction.
            std::vector<uint8_t> blob;
            for (const auto& e : entries)
                blob.insert(blob.end(), e.second.begin(), e.second.end());
            TrustRecord r;
            r.fingerprint = FingerprintBytes(blob);
            r.name = fd.cFileName;
            r.source = "pack";
            r.ts = static_cast<int>(std::time(nullptr));
            if (!r.fingerprint.empty()) {
                std::vector<TrustRecord> recs;
                LoadTrustRecords(g_exeDir + "\\state\\trust.json", &recs);
                TrustUpsert(&recs, r);
                if (SaveTrustRecords(g_exeDir + "\\state\\trust.json", recs))
                    HostLog("jktriggers: trust record (pack): " + r.name);
            }
        } else {
            HostLog("jktriggers: FAILED packing " + out);
        }
```

주의: 지문 일치의 전제는 **Write가 entries 순서대로 TOC를 쓴다**는 것(JKJkxFile::Write 계약)과 **PackMode의 entries가 MANI+SCRI뿐**이라는 것(manifest.txt + *.js만 팩). 둘 다 성립한다. loader 쪽 `FingerprintContainer`는 TOC 순으로 MANI+SCRI를 연결 — 동일 스트림.

- [ ] **Step 2: 빌드 + 재팩 + pack 레코드 확인**

Run: `cmake --build I:/progwork/JKENGINE/engine/build`
Expected: `Packing trigger containers` + `jktriggers: trust record (pack): trig_build/trig_idle/trig_crash` 로그

Run: `cd I:/progwork/JKENGINE/engine/build && ./jktriggers.exe --selftest && cat state/trust.json`
Expected: 셀프테스트 PASS + records에 trig_build/trig_idle/trig_crash 3종(`"source":"pack"`)

- [ ] **Step 3: 기존 회귀 (trust 게이트가 이제 pack 레코드로 통과)**

Run: `powershell -File engine/tools/probes/probe_agent_triggers.ps1`
Expected: 7/7 PASS

- [ ] **Step 4: Commit (Task 4 포함)**

```bash
git add engine/tools/jktriggers/main.cpp
git commit -m "feat(trust): loader gate + trust_request approval + packer self-attestation

jktriggers: fingerprint gate on both load paths (dev .js, trigger .jkx),
first-run approval via server trust_request (fail-closed), connect-first
startup, packer writes source:pack records."
```

---

### Task 6: 서버 — PendingApproval 일반화 + trust_request + approve + trust_list

**Files:**
- Modify: `engine/include/server/JKWindowServer.h:167-173` (PendingApproval 필드)
- Modify: `engine/src/server/JKWindowServer.cpp` (close_window 뒤 trust_request 브랜치, approve 도구 kind 가드, AgentToolAllowed, trigger_list 뒤 trust_list)

**Interfaces:**
- Consumes: 없음 (서버 자체 완결)
- Produces:
  - 도구 `trust_request` — args `{name, origin, fingerprint}`; 응답 `{"ok":true}`(allow/승인됨) 또는 `{"ok":false,"error":…}` (permission_denied/approval_unavailable/missing_name)
  - `agent.approval_request` 이벤트에 `kind`/`name`/`origin`/`fingerprint` 필드
  - 도구 `trust_list` — `{"ok":true,"records":[{"fingerprint":"sha256:xxxx…"(15자), "name", "source", "ts"}]}`

- [ ] **Step 1: PendingApproval 일반화 (헤더)**

`engine/include/server/JKWindowServer.h`의 `struct PendingApproval`(167-173)을:

```cpp
    struct PendingApproval {
        uint32_t requestId = 0;
        uint32_t queryId = 0;      // AgentQuery to complete on resolution
        uint32_t requesterId = 0;  // requesting connection's id
        uint32_t targetId = 0;     // window the request would touch
                                   // (trust_request: 0 — no target window)
        time_t expiresAt = 0;
        // Script trust model (docs/37 spec): one pipeline, two kinds.
        std::string kind = "close_window";  // "close_window" | "trust_request"
        std::string name;          // trust_request: script display name
        std::string origin;        // trust_request: "dev" | "package"
        std::string fingerprint;   // trust_request: "sha256:<64hex>"
    };
```

- [ ] **Step 2: trust_request 도구 브랜치**

`JKWindowServer.cpp`의 close_window 브랜치 끝(1427행 `}` 뒤, `launch_app` 앞)에:

```cpp
    } else if (tool == "trust_request") {
        // Script trust gate (docs/37 spec): jktriggers asks before the first
        // eval of an unknown fingerprint. Default permission is "ask" — the
        // same inline-approval pipeline as close_window. The server only
        // relays the decision; the loader owns trust.json.
        std::string name, origin, fingerprint;
        req.GetObjStr("args", "name", name);
        req.GetObjStr("args", "origin", origin);
        req.GetObjStr("args", "fingerprint", fingerprint);
        if (name.empty() || fingerprint.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            switch (AgentToolAllowed("trust_request")) {
                case AgentDecision::Allow:
                    // permissions.json says allow — every script is trusted.
                    reply = "{\"ok\":true}";
                    break;
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "trust_request";
                    p.name = name;
                    p.origin = origin;
                    p.fingerprint = fingerprint;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[1024];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"trust_request\","
                                  "\"kind\":\"trust_request\",\"name\":\"%s\","
                                  "\"origin\":\"%s\",\"fingerprint\":\"%s\","
                                  "\"ts\":%lld}",
                                  p.requestId, JsonEsc(name).c_str(),
                                  JsonEsc(origin).c_str(),
                                  fingerprint.c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
```

(fingerprint는 hex 전용이므로 JsonEsc 불필요.)

- [ ] **Step 3: approve 도구의 kind 가드**

`approve` 도구(1871-1879)의 Close 전송을 close_window에만:

```cpp
            if (allow && it->kind == "close_window") {
                for (auto& c : clients_) {
                    if (c && c->Id() == it->targetId &&
                        !c->IsDisconnected()) {
                        ipc::WriteMessage(c->Transport(), ipc::MsgType::Close,
                                          std::vector<uint8_t>{});
                        break;
                    }
                }
            }
```

요청자 회신(`{"ok":true}` / `denied_by_user`)은 종류 무관 동일 — jktriggers가 ok=true를 allow로 해석.

- [ ] **Step 4: AgentToolAllowed — trust_request 기본 ask**

`AgentToolAllowed`(2007-2034)를:

```cpp
AgentDecision JKWindowServer::AgentToolAllowed(const std::string& tool) const {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    const std::string path = dir + "\\permissions.json";
    // Missing entry defaults: close_window denies (M1 rule), trust_request
    // ASKS (the gate would be pointless if unknown scripts loaded silently),
    // everything else allows. "ask" pipelines: close_window + trust_request;
    // other tools degrade to allow since nothing parks them.
    const bool askCapable = (tool == "close_window" || tool == "trust_request");
    auto defaultDecision = [&]() -> AgentDecision {
        if (tool == "close_window") return AgentDecision::Deny;
        if (tool == "trust_request") return AgentDecision::Ask;
        return AgentDecision::Allow;
    };
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return defaultDecision();
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson perm(buf);
    std::string value;
    if (!perm.ok() || !perm.GetStr(tool.c_str(), value)) {
        return defaultDecision();
    }
    if (value == "allow") return AgentDecision::Allow;
    if (value == "ask") {
        return askCapable ? AgentDecision::Ask : AgentDecision::Allow;
    }
    if (value == "deny") return AgentDecision::Deny;
    return defaultDecision();
}
```

- [ ] **Step 5: trust_list 도구**

`trigger_list` 브랜치 끝(1785행 `reply = out + "]}"` 뒤, `events_list` 앞)에:

```cpp
    } else if (tool == "trust_list") {
        // Script trust store (docs/37 spec): the loader's trust.json records,
        // fingerprints truncated to 15 chars ("sha256:"+8hex) for display.
        std::string out = "{\"ok\":true,\"records\":[";
        bool first = true;
        std::FILE* f =
            std::fopen((StateDir() + "\\trust.json").c_str(), "rb");
        if (f) {
            char buf[65536] = {};
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            jk::agent::AgentJson json(buf);
            int cnt = 0;
            if (json.ok() && json.GetArraySize("records", cnt)) {
                for (int i = 0; i < cnt && i < 512; ++i) {
                    std::string fp, name, source;
                    int ts = 0;
                    if (!json.GetArrStr("records", i, "fingerprint", fp))
                        continue;
                    json.GetArrStr("records", i, "name", name);
                    json.GetArrStr("records", i, "source", source);
                    json.GetArrInt("records", i, "ts", ts);
                    if (!first) out += ",";
                    first = false;
                    out += "{\"fingerprint\":\"" +
                           (fp.size() > 15 ? fp.substr(0, 15) : fp) +
                           "\",\"name\":\"" + JsonEsc(name) +
                           "\",\"source\":\"" + JsonEsc(source) +
                           "\",\"ts\":" + std::to_string(ts) + "}";
                }
            }
        }
        out += "]}";
        reply = out;
    }
```

- [ ] **Step 6: 빌드 + 서버 셀프테스트 + 수동 실측**

Run: `cmake --build I:/progwork/JKENGINE/engine/build && cd I:/progwork/JKENGINE/engine/build && ./jkdesktop.exe test`
Expected: 0 failures

Run (실측, 서버 재시작 후):
```bash
cd I:/progwork/JKENGINE/engine/build
./jkdesktop.exe --server &
sleep 3
echo 'console.log("probe")' > state/triggers/trust_probe.js 2>/dev/null || true
./jkdesktop.exe agentctl '{"tool":"trust_list","args":{}}'
```
Expected: `trust_list` 응답에 pack 레코드 3종. (trust_request 승인 전체 경로는 Task 10 프로브가 자동 실측.)

Run: `powershell -File tools/probes/probe_agent_chat.ps1`
Expected: 7/7 PASS — approve 도구의 kind 가드가 close_window 경로를 안 깼는지 확인.

- [ ] **Step 7: Commit**

```bash
git add engine/include/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp
git commit -m "feat(trust): server trust_request tool + approval pipeline generalization + trust_list"
```

---

### Task 7: jkagentd — trust_list 카탈로그 등록

**Files:**
- Modify: `engine/tools/jkagentd/main.cpp:48-59` (kToolsListJson), `62-72` (IsKnownTool), `92-99` (LoadPermissions kNames)

**Interfaces:**
- Consumes: 서버의 `trust_list` (Task 6)
- Produces: MCP/agentctl에서 `trust_list` 호출 가능

- [ ] **Step 1: 세 곳에 trust_list 등록**

kToolsListJson — terminal_exec 엔트리 뒤(58행 끝의 `,"` 연결)에 추가:

```cpp
"{\"name\":\"trust_list\",\"description\":\"List trusted script fingerprints from state/trust.json\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
```

IsKnownTool의 kNames 배열과 LoadPermissions의 kNames 배열 모두에 `"trust_list"` 추가 (기본 allow — 루프가 전부 true로 초기화하므로 별도 기본값 불필요).

- [ ] **Step 2: 빌드 + 브로커 셀프테스트 + MCP 실측**

Run: `cmake --build I:/progwork/JKENGINE/engine/build --target jkagentd && cd I:/progwork/JKENGINE/engine/build && ./jkagentd.exe --selftest`
Expected: 0 failures

Run: `./jkdesktop.exe agentctl '{"tool":"trust_list","args":{}}'`
Expected: `{"ok":true,"records":[…]}` — permission_denied 없음

- [ ] **Step 3: Commit**

```bash
git add engine/tools/jkagentd/main.cpp
git commit -m "feat(trust): trust_list in jkagentd tool catalog"
```

---

### Task 8: jkchat — 신뢰 승인 프롬프트 + /trust

**Files:**
- Modify: `engine/tools/jkchat/main.cpp` (ShowTrustApproval 추가, HandleEvent approval_request 분기, /trust 커맨드, /help)

**Interfaces:**
- Consumes: Task 6의 `agent.approval_request` kind/name/origin/fingerprint 필드, `trust_list` 도구
- Produces: trust_request 이벤트 시 인라인 프롬프트(기존 [허용][거부] 버튼 공유)

- [ ] **Step 1: ShowTrustApproval 추가**

`ShowApproval`(196-206) 아래에:

```cpp
// Script trust prompt (docs/37 spec): same strip, different copy. The
// fingerprint shows as "sha256:"+8 hex — enough to eyeball against
// /trust output, full value in the transcript log.
static void ShowTrustApproval(const std::string& name, const std::string& origin,
                              const std::string& fingerprint, uint32_t request) {
    g_approvalRequest = request;
    const std::string fp8 =
        fingerprint.size() > 15 ? fingerprint.substr(0, 15) : fingerprint;
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE,
                 L"[신뢰 요청] %s (%s) 해시 %s… 승인할까요?",
                 Utf8ToWide(name).c_str(), Utf8ToWide(origin).c_str(),
                 Utf8ToWide(fp8).c_str());
    SetWindowTextW(g_hPrompt, buf);
    ShowWindow(g_hPrompt, SW_SHOWNORMAL);
    ShowWindow(g_hAllow, SW_SHOWNORMAL);
    ShowWindow(g_hDeny, SW_SHOWNORMAL);
}
```

- [ ] **Step 2: HandleEvent 분기**

`HandleEvent`의 `agent.approval_request` 브랜치(494-504)를:

```cpp
    if (ev.topic == "agent.approval_request") {
        jk::agent::AgentJson e(ev.json);
        std::string kind;
        int request = 0;
        e.GetInt("request", request);
        e.GetStr("kind", kind);
        if (kind == "trust_request") {
            std::string name, origin, fp;
            e.GetStr("name", name);
            e.GetStr("origin", origin);
            e.GetStr("fingerprint", fp);
            ShowTrustApproval(name, origin, fp, static_cast<uint32_t>(request));
            Log("[신뢰 요청] " + name + " (" + origin + ") — 해시 " +
                (fp.size() > 15 ? fp.substr(0, 15) : fp) + "…");
        } else {
            std::string title;
            int target = 0;
            e.GetInt("target_id", target);
            e.GetStr("title", title);
            ShowApproval(title, static_cast<uint32_t>(target),
                         static_cast<uint32_t>(request));
            Log("[승인 요청] close_window → " + title + " (#" +
                std::to_string(target) + ")");
        }
    }
```

- [ ] **Step 3: /trust 커맨드 + /help 갱신**

`/events` 커맨드(485-487) 뒤에:

```cpp
    } else if (cmd == "trust") {
        // Script trust store (docs/37, palette parity).
        SendTool("trust_list", "{}", "trust");
    }
```

도움말 줄(416)에 `/trust` 추가: `"/list /launch <app> /close <id> /chat /notify /shot /triggers"` 뒤에 `" /trust"`.

- [ ] **Step 4: 빌드 + 채팅 프로브**

Run: `cmake --build I:/progwork/JKENGINE/engine/build --target jkchat && powershell -File engine/tools/probes/probe_agent_chat.ps1`
Expected: 7/7 PASS (기존 close 프롬프트 경로 회귀 — UI의 실제 프롬프트 렌더링은 Task 10 프로브가 approval 이벤트로 간접 검증)

- [ ] **Step 5: Commit**

```bash
git add engine/tools/jkchat/main.cpp
git commit -m "feat(trust): jkchat inline trust-approval prompt + /trust"
```

---

### Task 9: 팔레트 — /trust (팔레트 패리티)

**Files:**
- Modify: `engine/src/apps/ClientPaletteApp.cpp:217-220` (/help), `232-234` 근처 (/triggers 뒤 /trust)

**Interfaces:**
- Consumes: 서버의 `trust_list` (Task 6)
- Produces: Alt+Space 팔레트에서 `/trust`로 신뢰 기록 열람

- [ ] **Step 1: /trust 커맨드**

`/triggers` 브랜치(232-234) 뒤에:

```cpp
    } else if (cmd == "trust") {
        // Script trust store (docs/37).
        SendTool("trust_list", "{}");
    }
```

/help 두 번째 줄(219)에 ` /trust` 추가:

```cpp
        AppendLog("  /shot  /triggers  /trigger <name> on|off  /events  /trust");
```

- [ ] **Step 2: 빌드 + 팔레트 프로브**

Run: `cmake --build I:/progwork/JKENGINE/engine/build && powershell -File engine/tools/probes/probe_agent_palette.ps1`
Expected: 4/4 PASS

- [ ] **Step 3: Commit**

```bash
git add engine/src/apps/ClientPaletteApp.cpp
git commit -m "feat(trust): palette /trust command (palette parity)"
```

---

### Task 10: probe_agent_trust.ps1 — 전 경로 실측 + 회귀 전수

**Files:**
- Create: `engine/tools/probes/probe_agent_trust.ps1`

**Interfaces:**
- Consumes: Task 1-9 전부. `agent-events` CLI (이벤트 관찰), `agentctl` approve/trust_list, jktriggers stdout 로그 파일 (probe_agent_triggers.ps1의 `-RedirectStandardOutput` 관례)

- [ ] **Step 1: 프로브 작성**

```powershell
# Script trust model probe (docs/37 spec 완료 조건): untrusted dev script ->
# trust_request -> chat approval pipeline (allow) -> script runs + user
# record; restart -> no re-prompt; second script denied -> never runs;
# packer records are trusted from boot.
$ErrorActionPreference = "Continue"
$exe   = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig  = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
$state = "I:\progwork\JKENGINE\engine\build\state"
$trustFile = "$state\trust.json"
$devDir = "$state\triggers"

Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# Start from a clean trust store (the packer will re-add pack records).
Remove-Item $trustFile -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $devDir | Out-Null
Remove-Item "$devDir\trust_probe.js","$devDir\trust_probe2.js" -ErrorAction SilentlyContinue

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

# events subscription FIRST (pushes are not replayed — docs/31 lesson)
$evJob = Start-Job -ScriptBlock {
    param($e)
    (& $e agent-events 25) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2

# untrusted dev trigger: publishes a marker at eval time
'desktop.publish("trust.probe", {"n":1});' | Set-Content -Path "$devDir\trust_probe.js" -Encoding ASCII

function Start-TriggerHost {
    Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) `
        -WindowStyle Hidden `
        -RedirectStandardOutput "$env:TEMP\trust_probe.log" `
        -RedirectStandardError "$env:TEMP\trust_probe.err"
}

Start-TriggerHost

# wait for the approval request in the event stream
$reqId = $null
foreach ($i in 1..10) {
    $events = Receive-Job -Job $evJob -ErrorAction SilentlyContinue | Out-String
    # the envelope has "request" before "kind" (docs/31 shape) — take the
    # last request id on the stream, probe_agent_chat.ps1 convention
    if ($events -match '"kind\\?":"trust_request"' -and $events -match '"request\\?":(\d+)') {
        $ids = [regex]::Matches($events, '"request\\?":(\d+)')
        $reqId = $ids[$ids.Count - 1].Groups[1].Value
        break
    }
    Start-Sleep -Seconds 1
}
if ($reqId) { Write-Host "approval-request: PASS (request $reqId)" }
else { Write-Host "approval-request: FAIL"; Get-Content "$env:TEMP\trust_probe.log" -ErrorAction SilentlyContinue; exit 1 }

$approve = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
if ($approve -match 'approved\\?":true') { Write-Host "approve: PASS" }
else { Write-Host "approve: FAIL $approve"; exit 1 }

# script ran (marker event) + user record written
$ran = $false
foreach ($i in 1..6) {
    $events = Receive-Job -Job $evJob -ErrorAction SilentlyContinue | Out-String
    if ($events -match 'trust\.probe') { $ran = $true; break }
    Start-Sleep -Seconds 1
}
if ($ran) { Write-Host "script-ran: PASS" }
else { Write-Host "script-ran: FAIL"; Get-Content "$env:TEMP\trust_probe.log"; exit 1 }

$trust = Get-Content $trustFile -Raw -ErrorAction SilentlyContinue
if ($trust -match '"source":"user"' -and $trust -match 'trust_probe') {
    Write-Host "user-record: PASS"
} else { Write-Host "user-record: FAIL ($trust)"; exit 1 }

# restart: trusted now — no new approval, script runs again
Get-Process jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$evJob2 = Start-Job -ScriptBlock {
    param($e)
    (& $e agent-events 12) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2
Start-TriggerHost
$noPrompt = $true; $ran2 = $false
foreach ($i in 1..8) {
    $events2 = Receive-Job -Job $evJob2 -ErrorAction SilentlyContinue | Out-String
    if ($events2 -match 'trust\.probe') { $ran2 = $true }
    if ($events2 -match '"kind\\?":"trust_request"') { $noPrompt = $false; break }
    Start-Sleep -Seconds 1
}
Start-Sleep -Seconds 3   # let the host finish loading before judging
$events2 = Receive-Job -Job $evJob2 -ErrorAction SilentlyContinue | Out-String
if ($events2 -match 'trust\.probe') { $ran2 = $true }
if ($ran2 -and $noPrompt) { Write-Host "restart-trusted: PASS" }
else { Write-Host "restart-trusted: FAIL (ran=$ran2 noPrompt=$noPrompt)"; exit 1 }

# deny path: second script -> deny -> never runs
'desktop.publish("trust.probe2", {"n":1});' |
    Set-Content -Path "$devDir\trust_probe2.js" -Encoding ASCII
Get-Process jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$evJob3 = Start-Job -ScriptBlock {
    param($e)
    (& $e agent-events 20) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2
Start-TriggerHost
$req2 = $null
foreach ($i in 1..10) {
    $events3 = Receive-Job -Job $evJob3 -ErrorAction SilentlyContinue | Out-String
    if ($events3 -match '"request\\?":(\d+)') { $req2 = $Matches[1]; break }
    Start-Sleep -Seconds 1
}
if ($req2) {
    $deny = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $req2 + ',"decision":"deny"}}')
    if ($deny -match 'approved\\?":false') { Write-Host "deny-decision: PASS" }
    else { Write-Host "deny-decision: FAIL $deny"; exit 1 }
} else { Write-Host "deny-decision: FAIL (no request)"; exit 1 }
Start-Sleep -Seconds 3
$events3 = Receive-Job -Job $evJob3 -ErrorAction SilentlyContinue | Out-String
if ($events3 -notmatch 'trust\.probe2') { Write-Host "deny-skipped: PASS" }
else { Write-Host "deny-skipped: FAIL"; exit 1 }

# pack records trusted from boot: trust_list shows the 3 bundles as pack
$tl = Invoke-Agentctl '{"tool":"trust_list","args":{}}'
$packCount = ([regex]::Matches($tl, '"source\\?":"pack"')).Count
if ($packCount -ge 3) { Write-Host "pack-records: PASS ($packCount)" }
else { Write-Host "pack-records: FAIL ($tl)"; exit 1 }

# cleanup
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item "$devDir\trust_probe.js","$devDir\trust_probe2.js" -ErrorAction SilentlyContinue
Remove-Item $trustFile -ErrorAction SilentlyContinue

Write-Host "PASS: agent trust"
exit 0
```

- [ ] **Step 2: 프로브 실행**

Run: `powershell -File engine/tools/probes/probe_agent_trust.ps1`
Expected: 7 체크 전부 PASS, `PASS: agent trust`, exit 0

실패 시 디버깅 순서: `$env:TEMP\trust_probe.log`([triggers] 로그 — trust approval needed/denied/trusted+recorded), 이벤트 job 출력(approval_request 페이로드), `state\trust.json` 내용.

- [ ] **Step 3: 회귀 전수**

Run:
```bash
cd I:/progwork/JKENGINE/engine/build
./jkdesktop.exe test
./jkagentd.exe --selftest
./jktriggers.exe --selftest
powershell -File tools/probes/probe_agent_mcp.ps1       # 5/5
powershell -File tools/probes/probe_agent_e2e.ps1       # 7/7
powershell -File tools/probes/probe_agent_palette.ps1   # 4/4
powershell -File tools/probes/probe_agent_chat.ps1      # 7/7
powershell -File tools/probes/probe_agent_triggers.ps1  # 7/7
powershell -File tools/probes/probe_agent_events.ps1    # 5/5
powershell -File tools/probes/probe_agent_trust.ps1     # PASS
```
Expected: 전부 통과

- [ ] **Step 4: Commit**

```bash
git add engine/tools/probes/probe_agent_trust.ps1
git commit -m "test(trust): probe_agent_trust — approval/deny/restart/pack-records paths"
```

---

### Task 11: 문서화 — docs/37 + 메모리 갱신

**Files:**
- Create: `docs/37_desktop_agent_trust.md`
- Modify: `docs/32_desktop_agent_triggers.md:169-172` (§8 "스크립트 서명/신뢰 모델은 후속" 갱신)
- Modify: `C:\Users\kisoc\.claude\projects\I--progwork-JKENGINE\memory\MEMORY.md` (roadmap 라인 갱신)

- [ ] **Step 1: docs/37 작성**

`docs/32` 문서 관례(상태/아키텍처 ASCII/도구 표/테스트/제한)를 따라 작성:

```markdown
# 37. Desktop Agent 스크립트 신뢰 모델 — 지문 + trust.json + 승인 일반화

- 날짜: 2026-09-13
- 상태: 구현 완료. 스펙 `docs/superpowers/specs/2026-09-13-script-trust-design.md`

## 1. 모델 (요약)
- 지문 = SHA-256 (BCrypt): dev .js는 파일 전체, .jkx는 MANI+SCRI 페이로드 TOC 순 연결.
- 저장소 state\trust.json — source:"pack"(패커 자기-증명)/"user"(승인 기록). 없음/파손 = 전부 비신뢰 (fail-closed).
- 승인 = close_window 파이프라인 일반화: trust_request 도구 → agent.approval_request(kind/name/origin/fingerprint) → jkchat 프롬프트 → approve. trust.json 쓰기 권위는 로더.

## 2. 서버 변경점
- PendingApproval kind 일반화, approve의 Close는 close_window만.
- AgentToolAllowed: trust_request 기본 ask (close_window는 기존대로 deny, 나머지 allow).
- trust_list 도구 (지문 15자 표시).

## 3. jktriggers 변경점
- connect-first 시작(승인 쿼리를 위해), TrustGate가 두 로딩 경로를 게이트.
- PackMode가 source:"pack" upsert (user 레코드 보존, 지문 불일치 재승인).
- jktriggers --selftest: SHA-256 벡터 + trust store + 컨테이너 지문.

## 4. 얼굴
- jkchat [신뢰 요청] 프롬프트 + /trust, 팔레트 /trust, jkagentd trust_list.

## 5. 테스트
- probe_agent_trust.ps1 (7 체크): approval-request → approve → script-ran →
  user-record → restart-trusted(무재승인) → deny-skipped → pack-records.

## 6. 제한
- 내용 변경(재빌드 포함) → 지문 변경 → 재승인. 로컬 기준선은 편의 모델(보안 경계 아님).
- reload 중 승인 대기는 블로킹(최대 60초). 실서명 승격 경로는 스펙 §7.
```

- [ ] **Step 2: docs/32 §8 후속 표기 갱신**

`docs/32_desktop_agent_triggers.md`의 169-172행을:

```markdown
- **M2b 남은 것**: ~~트리거 활성/비활성 UI~~ — **구현됨 (2026-09-11,
  docs/34)**: trigger_toggle/trigger_list 도구 + 팔레트 /triggers·/trigger +
  state/triggers.json 플래그 + triggers.reload 재적재. ~~스크립트 서명/신뢰
  모델~~ — **구현됨 (2026-09-13, docs/37)**: SHA-256 지문 + state/trust.json
  + trust_request 승인 파이프라인 일반화.
```

- [ ] **Step 3: 메모리 갱신**

`C:\Users\kisoc\.claude\projects\I--progwork-JKENGINE\memory\MEMORY.md`의 roadmap 라인에서
`**NEXT: 스크립트 서명/신뢰 모델** (user-picked, docs/32 §8 마지막 M2b 항목 — roadmap memory에 착수 맥락 기록)` 부분을
`**스크립트 신뢰 모델 done (2026-09-13, docs/37)** — SHA-256 지문 + trust.json + trust_request 승인 일반화` 로 교체.

- [ ] **Step 4: Commit**

```bash
git add docs/37_desktop_agent_trust.md docs/32_desktop_agent_triggers.md
git commit -m "docs(37): script trust model — fingerprint store + approval generalization complete"
```

---

## Task 의존성

- Task 1 → 2 → 3 → 4 → 5 (같은 파일 체인; 4+5는 한 커밋)
- Task 6 (서버)는 Task 4의 trust_request 수신처 — 4/5 먼저 또는 병렬 가능하나 실측은 6 후
- Task 7/8/9는 Task 6에만 의존 (병렬 가능)
- Task 10은 전부 의존. Task 11은 10 통과 후.