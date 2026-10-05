// jktriggers — M2b trigger-script host (docs/32).
//
// A control-only agent client (jkchat pattern, no UI) that runs QuickJS
// scripts. Scripts register on(topic, filter, handler) callbacks; the host
// dispatches desktop agent events to them and provides the desktop.* API
// (notify / publish / saveLayout / readFile / log) plus host-side timers.
// Triggers load from <exeDir>\state\triggers\*.js (dev) and
// <exeDir>\apps\triggers\*.jkx containers (packaged, Task 5).

#include <agent/JKAgentClient.h>
#include <agent/JKAgentJson.h>
#include <crypto/JKSha256.h>
#include <fs/JKFs.h>
#include <JKJkxFile.h>
#include <quickjs.h>

// linux stage-3 task 8: windows.h is OUT of this TU. The stage-2 marking
// (docs/68 W8 — CreateDirectoryA·DeleteFileA, FindFirstFileA/FindNextFileA/
// FindClose(WIN32_FIND_DATAA), Sleep) resolved: directory ops → std::filesystem
// error_code overloads (fail-quiet parity — the original IGNORED the
// CreateDirectory/DeleteFile returns and the fail-continue fopen shape), the
// Find*File glob walks → directory_iterator (on Windows libstdc++'s iterator
// delegates to the same FindFirstFile enumeration, so the NTFS name-order the
// packer's TOC/trust-fingerprint depends on is unchanged), Sleep →
// ../ConsoleShim.h.
#include "../ConsoleShim.h"

#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Globals: one agent client, one persistent QuickJS runtime.
// ---------------------------------------------------------------------------
jk::agent::JKAgentClient g_agent;
JSRuntime* g_rt = nullptr;
JSContext* g_ctx = nullptr;

struct TriggerReg {
    std::string topic;  // exact topic, or "prefix.*" glob
    JSValue match;      // RegExp object, or JS_UNDEFINED (match all)
    JSValue handler;    // function(ev)
    std::string source;  // container name ("trig_build") — enable/disable key
    bool enabled = true;
};
std::vector<TriggerReg> g_triggers;

// Container name set while its scripts eval (LoadTriggerContainers → JsOn).
std::string g_currentSource;
// name -> 0/1 from state/triggers.json; absent entry = enabled (docs/34).
std::map<std::string, int> g_enabled;

// Trust store in memory (spec §3) — loaded once at startup; approval
// resolutions upsert into it and the file. (Struct defined in the trust
// block below; vector-of-incomplete-type is fine for a declaration.)
struct TrustRecord;
std::vector<TrustRecord> g_trust;

struct Timer {
    int64_t id;
    uint64_t dueMs;     // steady_clock ms of next fire
    uint64_t interval;  // 0 = one-shot (setTimeout)
    JSValue fn;
};
std::vector<Timer> g_timers;
int64_t g_nextTimerId = 1;

std::string g_exeDir;

uint64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void HostLog(const std::string& line) {
    std::fputs(line.c_str(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);  // redirected stdout is fully buffered
}

void LogJsException(const std::string& where) {
    JSValue ex = JS_GetException(g_ctx);
    const char* msg = JS_ToCString(g_ctx, ex);
    HostLog(std::string("[triggers] JS exception (") + where + "): " +
            (msg ? msg : "?"));
    if (msg) JS_FreeCString(g_ctx, msg);
    JS_FreeValue(g_ctx, ex);
}

// ---------------------------------------------------------------------------
// Rate limiter (docs/38 spec §2): per-source fixed-window budget. A trusted
// script that runs away (publish→on self-loop) is throttled instead of
// flooding the bus. Convenience guard — not a security boundary (spec §1).
// Kept as one block (struct + constants + core with the injected clock) so
// the throttle reads as a unit. It sits above SelfTest (which consumes
// RateAllowAt) and above Publish, whose definition is below — the one-time
// notify path calls it through the hoisted declaration.
// ---------------------------------------------------------------------------
struct SourceBudget {
    uint64_t windowStartMs = 0;
    int count = 0;
    bool notified = false;  // one notify + one log per window
};
constexpr int kRateLimitCount = 60;
constexpr uint64_t kRateLimitWindowMs = 60000;
constexpr size_t kMaxTimers = 64;
std::map<std::string, SourceBudget> g_budget;

bool Publish(const std::string& topic, const std::string& dataRawJson);
std::string JsonEscapeStr(const std::string& s);  // defined below (trust block)

// Core with injected clock (selftest). Returns true when this invocation may
// proceed; consumes 1 budget unit only when allowed — a throttled call
// returns false without touching the count.
bool RateAllowAt(const std::string& source, uint64_t nowMs) {
    SourceBudget& b = g_budget[source];
    if (nowMs - b.windowStartMs >= kRateLimitWindowMs) {
        b.windowStartMs = nowMs;
        b.count = 0;
        b.notified = false;
    }
    if (b.count >= kRateLimitCount) {
        if (!b.notified) {
            b.notified = true;
            HostLog("[triggers] rate limit: " + source + " (" +
                    std::to_string(kRateLimitCount) + " fires/" +
                    std::to_string(kRateLimitWindowMs / 1000) + "s)");
            Publish("agent.notify",
                    "{\"title\":\"트리거 발화 제한\",\"body\":\"" +
                        JsonEscapeStr(source) + " — 이벤트 발화 상한 도달\"}");
        }
        return false;
    }
    ++b.count;
    return true;
}
bool RateAllow(const std::string& source) { return RateAllowAt(source, NowMs()); }

// ---------------------------------------------------------------------------
// Script trust model (docs/37 spec): SHA-256 fingerprints.
// ---------------------------------------------------------------------------

// "sha256:<64 hex>" — the identity of a script (spec §2). docs/68 W2b: digest
// is the platform-neutral jkcore helper (was Windows CNG BCrypt — digests are
// byte-identical, selftest vectors prove it). The ""-on-failure contract is
// now unreachable (pure function), kept so an empty fingerprint still never
// matches a trust record (fail-closed).
std::string Sha256Hex(const uint8_t* data, size_t len) {
    return "sha256:" + jk::crypto::Sha256Hex(data, len);
}

// ---------------------------------------------------------------------------
// Trust store — state\trust.json (spec §3).
// {"records":[{"fingerprint":"sha256:…","name":…,"source":"pack"|"user","ts":…}]}
// Missing/corrupt file = everything untrusted (fail-closed). ts is epoch
// seconds (long long — the file format is unchanged; only the load path
// widened, see ReadTrustTimestamps below).
struct TrustRecord {
    std::string fingerprint;  // "sha256:<64hex>" — the identity key
    std::string name;         // display name ("trig_build", "state/x.js")
    std::string source;       // "pack" (packer self-attestation) | "user" (approval)
    long long ts = 0;         // epoch seconds
};

// Defined below with the JSON string helpers (used by SaveTrustRecords).
std::string JsonEscapeStr(const std::string& s);

// ts (epoch seconds) reader: AgentJson has no int64 array accessor —
// GetArrInt truncates to int (post-2038 timestamps would clip) and
// GetArrStr only accepts string values (JS_IsString check), so a strtoll
// fallback would silently zero every numeric ts. Read ts through a direct
// QuickJS parse of the same buffer instead: one extra throwaway parse per
// file at startup, full 64-bit precision, missing/malformed -> 0.
std::vector<long long> ReadTrustTimestamps(const std::string& text) {
    std::vector<long long> out;
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = rt ? JS_NewContext(rt) : nullptr;
    if (!ctx) {
        if (rt) JS_FreeRuntime(rt);
        return out;
    }
    JSValue root = JS_ParseJSON(ctx, text.c_str(), text.size(), "trust");
    if (JS_IsException(root)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        root = JS_UNDEFINED;
    }
    JSValue arr = JS_IsObject(root) ? JS_GetPropertyStr(ctx, root, "records")
                                    : JS_UNDEFINED;
    if (JS_IsArray(arr)) {
        int64_t len = 0;
        if (JS_GetLength(ctx, arr, &len) == 0) {
            out.assign(static_cast<size_t>(len), 0);
            for (int64_t i = 0; i < len; ++i) {
                JSValue item =
                    JS_GetPropertyUint32(ctx, arr, static_cast<uint32_t>(i));
                if (JS_IsObject(item)) {
                    JSValue v = JS_GetPropertyStr(ctx, item, "ts");
                    int64_t ts = 0;
                    if (!JS_IsException(v) && !JS_IsUndefined(v) &&
                        JS_ToInt64(ctx, &ts, v) == 0)
                        out[static_cast<size_t>(i)] = ts;
                    JS_FreeValue(ctx, v);
                }
                JS_FreeValue(ctx, item);
            }
        }
    }
    JS_FreeValue(ctx, arr);
    JS_FreeValue(ctx, root);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return out;
}

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
    const std::vector<long long> tsList = ReadTrustTimestamps(buf);
    for (int i = 0; i < n && i < 512; ++i) {
        TrustRecord r;
        if (!json.GetArrStr("records", i, "fingerprint", r.fingerprint) ||
            r.fingerprint.empty())
            continue;
        json.GetArrStr("records", i, "name", r.name);
        json.GetArrStr("records", i, "source", r.source);
        r.ts = i < static_cast<int>(tsList.size()) ? tsList[static_cast<size_t>(i)]
                                                   : 0;
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
        std::error_code ec;  // CreateDirectoryA 반환 무시 계약 — fail-quiet 원문
        std::filesystem::create_directories(dir, ec);
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
// writes source:"user" (spec §3). keepUserRecord (packer path) skips the
// upsert when the same fingerprint already has a source:"user" record —
// a user who explicitly approved a hash keeps their record; the packer
// never flips a user record to "pack".
void TrustUpsert(std::vector<TrustRecord>* recs, const TrustRecord& r,
                 bool keepUserRecord = false) {
    for (auto& existing : *recs) {
        if (existing.fingerprint == r.fingerprint) {
            if (keepUserRecord && existing.source == "user") return;
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
    r.ts = static_cast<long long>(std::time(nullptr));
    TrustUpsert(&g_trust, r);
    SaveTrustRecords(g_exeDir + "\\state\\trust.json", g_trust);
    HostLog("[triggers] trusted + recorded: " + name);
    return true;
}

int SelfTest() {
    int fails = 0;
    int total = 0;
    auto check = [&](bool ok, const char* what) {
        HostLog(std::string("[selftest] ") + what + ": " + (ok ? "PASS" : "FAIL"));
        ++total;
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
        // A user approval of the same fingerprint replaces the pack record;
        // a later repack must not flip it back (spec §3 — packer never
        // touches user records).
        TrustRecord userSame = pack;
        userSame.name = "state/y.js";
        userSame.source = "user";
        userSame.ts = 400;
        TrustUpsert(&recs, userSame);
        check(recs.size() == 2 && recs[0].source == "user",
              "user approval replaces pack record");
        TrustRecord pack3 = pack;
        pack3.ts = 500;  // repack, same fingerprint — must be skipped
        TrustUpsert(&recs, pack3, /*keepUserRecord=*/true);
        check(recs.size() == 2 && recs[0].source == "user",
              "repack preserves user record");
        check(IsTrusted(recs, "sha256:aaa1") && IsTrusted(recs, "sha256:bbb2"),
              "is_trusted recorded");
        check(!IsTrusted(recs, "sha256:ccc3") && !IsTrusted(recs, ""),
              "is_trusted unknown fail-closed");
        check(SaveTrustRecords(path, recs), "trust save");
        // ts int64: a value beyond INT32_MAX survives the save/load roundtrip
        // through the direct-QuickJS ts reader (GetArrInt would truncate it —
        // 2200000000 pins the widening, not just a wide-but-in-int32 value).
        {
            std::vector<TrustRecord> big;
            TrustRecord t64;
            t64.fingerprint = "sha256:ccc3";
            t64.name = "big_ts";
            t64.source = "user";
            t64.ts = 2200000000;
            TrustUpsert(&big, t64);
            const std::string tsPath = g_exeDir + "\\state\\_selftest_ts.json";
            check(SaveTrustRecords(tsPath, big), "ts int64 save");
            std::vector<TrustRecord> tsBack;
            LoadTrustRecords(tsPath, &tsBack);
            check(tsBack.size() == 1 && tsBack[0].ts == 2200000000,
                  "ts int64 roundtrip (2200000000 > INT32_MAX)");
            std::error_code dec; // DeleteFileA 반환 무시 — fail-quiet 원문
            std::filesystem::remove(tsPath, dec);
        }
        std::vector<TrustRecord> back;
        LoadTrustRecords(path, &back);
        // Order: [0] = user-approved-over-pack record (ts 400 — the skipped
        // repack's ts 500 must NOT have landed), [1] = the loader user record.
        check(back.size() == 2 && back[0].source == "user" &&
                  back[0].ts == 400 && back[1].source == "user",
              "trust roundtrip + user record preserved");
        std::error_code dec; // DeleteFileA 반환 무시 — fail-quiet 원문
        std::filesystem::remove(path, dec);
    }
    // Container fingerprint: MANI+SCRI in TOC order, deterministic, and
    // sensitive to script content (spec §2).
    {
        const std::string path = g_exeDir + "\\state\\_selftest_fp.jkx";
        const char* maniText = "name=demo\ntrigger=a.js\n";
        std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
        entries.emplace_back(
            "manifest.txt",
            std::vector<uint8_t>(maniText, maniText + std::strlen(maniText)));
        entries.emplace_back("a.js",
                             std::vector<uint8_t>{'l','o','g','(',')',';'});
        check(jk::JKJkxFile::Write(path, entries), "fp test container write");
        std::string fp1;
        {
            // Inner scopes so each container's FILE* is closed before the
            // next Write/DeleteFileA — an open handle makes both fail
            // silently on Windows (fopen shares, but not for delete).
            jk::JKJkxFile jkx;
            check(jkx.Open(path), "fp test container open");
            fp1 = FingerprintContainer(jkx);
            const std::string fp2 = FingerprintContainer(jkx);
            check(!fp1.empty() && fp1 == fp2, "container fp deterministic");
        }
        // Same manifest + changed script -> different fingerprint.
        entries[1].second = {'l','o','g','(','1',')',';'};
        check(jk::JKJkxFile::Write(path, entries), "fp test rewrite");
        {
            jk::JKJkxFile jkx2;
            check(jkx2.Open(path), "fp test reopen");
            check(FingerprintContainer(jkx2) != fp1, "container fp content-bound");
        }
        std::error_code dec; // DeleteFileA 반환 무시 — fail-quiet 원문
        std::filesystem::remove(path, dec);
    }
    // Rate limiter (docs/38): fixed-window boundary with the injected clock.
    // The 61st hit latches notified=true and fires the one-time notify —
    // Publish fails here (no server) and logs, which is the expected path.
    {
        const std::string src = "selftest_rate";
        g_budget.clear();
        const uint64_t now = 1000000;  // arbitrary steady-clock ms
        bool earlyDeny = false;
        for (int i = 0; i < kRateLimitCount; ++i)
            earlyDeny |= !RateAllowAt(src, now);
        check(!earlyDeny, "rate: 60 within window allowed");
        check(!RateAllowAt(src, now), "rate: 61st denied");
        check(g_budget[src].notified, "rate: notify latched on denial");
        check(!RateAllowAt(src, now + 1000),
              "rate: still denied in same window");
        check(RateAllowAt(src, now + kRateLimitWindowMs),
              "rate: window rollover re-allowed");
        check(!g_budget[src].notified && g_budget[src].count == 1,
              "rate: budget reset (notified=false, count=1)");
        bool reDeny = false;
        for (int i = 1; i < kRateLimitCount; ++i)
            reDeny |= !RateAllowAt(src, now + kRateLimitWindowMs);
        check(!reDeny, "rate: re-exhaust after rollover allowed");
        check(!RateAllowAt(src, now + kRateLimitWindowMs),
              "rate: re-exhausted denied again");
        g_budget.clear();
    }
    if (fails == 0)
        HostLog("[selftest] all passed (" + std::to_string(total) + " checks)");
    return fails == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// desktop.* implementation helpers
// ---------------------------------------------------------------------------

// Publish {topic, dataRawJson} through the server's publish_event tool.
// Blocking Query is fine here: the host is a headless console process and
// Query's pump also flushes queued events/replies.
bool Publish(const std::string& topic, const std::string& dataRawJson) {
    std::string args = "{\"topic\":\"" + topic + "\",\"data\":" + dataRawJson + "}";
    std::string reply;
    const bool ok = g_agent.Query("publish_event", args, reply);
    if (!ok) HostLog("[triggers] publish failed (pipe down?)");
    return ok;
}

// exeDir-relative file read for trigger configuration.
std::string ReadFileRelative(const std::string& rel, bool& ok) {
    const std::string path = g_exeDir + "\\" + rel;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        ok = false;
        return "";
    }
    std::string data;
    char buf[4096];
    size_t got;
    while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, got);
    std::fclose(f);
    if (data.size() > 65536) data.resize(65536);
    ok = true;
    return data;
}

// ---------------------------------------------------------------------------
// QuickJS C functions
// ---------------------------------------------------------------------------

// Minimal JSON string escaping for script-provided strings embedded into
// publish_event args (quotes/backslashes/control chars would otherwise
// corrupt the envelope).
std::string JsonEscapeStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

JSValue JsOn(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 3 || !JS_IsString(argv[0]) || !JS_IsFunction(ctx, argv[2]))
        return JS_ThrowTypeError(ctx, "on(topic, filter, handler)");
    TriggerReg t;
    const char* topic = JS_ToCString(ctx, argv[0]);
    t.topic = topic ? topic : "";
    if (topic) JS_FreeCString(ctx, topic);
    // filter is {match: /regex/} — null/undefined/other non-objects mean
    // "match all" (GetPropertyStr on null would throw and poison the reg).
    JSValue match = JS_IsObject(argv[1]) && !JS_IsNull(argv[1])
                        ? JS_GetPropertyStr(ctx, argv[1], "match")
                        : JS_UNDEFINED;
    t.match = JS_IsUndefined(match) ? JS_UNDEFINED : JS_DupValue(ctx, match);
    JS_FreeValue(ctx, match);
    t.handler = JS_DupValue(ctx, argv[2]);
    t.source = g_currentSource;   // enable/disable key (docs/34)
    g_triggers.push_back(std::move(t));
    return JS_UNDEFINED;
}

JSValue JsNotify(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    // desktop.notify(title, body?)
    const char* title = argc > 0 && JS_IsString(argv[0])
                            ? JS_ToCString(ctx, argv[0])
                            : nullptr;
    const char* body = argc > 1 && JS_IsString(argv[1])
                           ? JS_ToCString(ctx, argv[1])
                           : nullptr;
    std::string data = "{\"title\":\"";
    data += title ? JsonEscapeStr(title) : "";
    data += "\",\"body\":\"";
    data += body ? JsonEscapeStr(body) : "";
    data += "\"}";
    if (title) JS_FreeCString(ctx, title);
    if (body) JS_FreeCString(ctx, body);
    Publish("agent.notify", data);
    return JS_UNDEFINED;
}

JSValue JsPublish(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    // desktop.publish(topic, dataObj)
    if (argc < 2 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "publish(topic, dataObj)");
    const char* topic = JS_ToCString(ctx, argv[0]);
    std::string t = topic ? topic : "";
    if (topic) JS_FreeCString(ctx, topic);
    JSValue raw = JS_JSONStringify(ctx, argv[1], JS_UNDEFINED, JS_UNDEFINED);
    std::string data;
    if (JS_IsString(raw)) {
        const char* c = JS_ToCString(ctx, raw);
        if (c) data = c;
        JS_FreeCString(ctx, c);
    }
    JS_FreeValue(ctx, raw);
    if (data.empty()) data = "{}";
    Publish(t, data);
    return JS_UNDEFINED;
}

JSValue JsSaveLayout(JSContext* ctx, JSValueConst, int argc,
                     JSValueConst* argv) {
    // desktop.saveLayout(name) — blocking query; reply lands in the log.
    const char* name = argc > 0 && JS_IsString(argv[0])
                           ? JS_ToCString(ctx, argv[0])
                           : nullptr;
    std::string args = "{\"name\":\"";
    args += name ? JsonEscapeStr(name) : "";
    args += "\"}";
    if (name) JS_FreeCString(ctx, name);
    std::string reply;
    if (g_agent.Query("save_layout", args, reply)) {
        HostLog("[triggers] save_layout -> " + reply);
    } else {
        HostLog("[triggers] save_layout failed (pipe down?)");
    }
    return JS_UNDEFINED;
}

JSValue JsLog(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc > 0 && JS_IsString(argv[0])) {
        const char* s = JS_ToCString(ctx, argv[0]);
        HostLog("[triggers] " + std::string(s ? s : ""));
        if (s) JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

JSValue JsReadFile(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "readFile(relPath)");
    const char* rel = JS_ToCString(ctx, argv[0]);
    bool ok = false;
    std::string data = ReadFileRelative(rel ? rel : "", ok);
    if (rel) JS_FreeCString(ctx, rel);
    if (!ok) return JS_Throw(ctx, JS_NewString(ctx, "readFile: not found"));
    return JS_NewStringLen(ctx, data.c_str(), data.size());
}

JSValue JsSetTimeout(JSContext* ctx, JSValueConst, int argc,
                     JSValueConst* argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "setTimeout(fn, ms)");
    int64_t ms = 0;
    if (argc > 1) JS_ToInt64(ctx, &ms, argv[1]);
    if (g_timers.size() >= kMaxTimers) {
        HostLog("[triggers] timer cap reached (" +
                std::to_string(kMaxTimers) + ") — timer dropped");
        return JS_UNDEFINED;
    }
    Timer t;
    t.id = g_nextTimerId++;
    t.dueMs = NowMs() + static_cast<uint64_t>(ms < 0 ? 0 : ms);
    t.interval = 0;
    t.fn = JS_DupValue(ctx, argv[0]);
    g_timers.push_back(std::move(t));
    return JS_NewInt64(ctx, t.id);
}

JSValue JsSetInterval(JSContext* ctx, JSValueConst, int argc,
                      JSValueConst* argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "setInterval(fn, ms)");
    int64_t ms = 0;
    if (argc > 1) JS_ToInt64(ctx, &ms, argv[1]);
    if (ms <= 0) ms = 1;
    if (g_timers.size() >= kMaxTimers) {
        HostLog("[triggers] timer cap reached (" +
                std::to_string(kMaxTimers) + ") — timer dropped");
        return JS_UNDEFINED;
    }
    Timer t;
    t.id = g_nextTimerId++;
    t.dueMs = NowMs() + static_cast<uint64_t>(ms);
    t.interval = static_cast<uint64_t>(ms);
    t.fn = JS_DupValue(ctx, argv[0]);
    g_timers.push_back(std::move(t));
    return JS_NewInt64(ctx, t.id);
}

JSValue JsClearTimer(JSContext* ctx, JSValueConst, int argc,
                     JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    int64_t id = 0;
    if (JS_ToInt64(ctx, &id, argv[0]) != 0) return JS_UNDEFINED;
    for (size_t i = 0; i < g_timers.size(); ++i) {
        if (g_timers[i].id == id) {
            JS_FreeValue(ctx, g_timers[i].fn);
            g_timers.erase(g_timers.begin() + static_cast<long>(i));
            break;
        }
    }
    return JS_UNDEFINED;
}

// Run microtasks (spec examples use async handlers).
void RunPendingJobs() {
    JSContext* c = nullptr;
    while (JS_ExecutePendingJob(g_rt, &c) > 0) {
    }
}

// ---------------------------------------------------------------------------
// Enable flags + loaded manifest (docs/34)
// ---------------------------------------------------------------------------

// state/triggers.json: {"triggers":[{"name":"trig_build","enabled":0},...]}
// — the server writes it (trigger_toggle); we re-read on triggers.reload.
void ReloadTriggerFlags() {
    g_enabled.clear();
    FILE* f = std::fopen((g_exeDir + "\\state\\triggers.json").c_str(), "rb");
    if (!f) return;
    std::string buf;
    char chunk[4096];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
        buf.append(chunk, got);
    std::fclose(f);
    // JS_ParseJSON requires a NUL-terminated buffer — std::string guarantees
    // one via c_str() semantics (docs/27 lesson 3).
    jk::agent::AgentJson json(buf);
    int n = 0;
    if (!json.ok() || !json.GetArraySize("triggers", n)) return;
    for (int i = 0; i < n && i < 64; ++i) {
        std::string name;
        int en = 1;
        if (!json.GetArrStr("triggers", i, "name", name) || name.empty())
            continue;
        json.GetArrInt("triggers", i, "enabled", en);
        g_enabled[name] = en;
    }
    for (auto& t : g_triggers) {
        auto it = g_enabled.find(t.source);
        t.enabled = (it == g_enabled.end()) || it->second != 0;
    }
    HostLog("[triggers] flags reloaded (" + std::to_string(g_enabled.size()) +
            ")");
}

// Startup manifest for trigger_list (server merges with triggers.json):
// one flat row per container×topic so AgentJson array access reads it
// without nested paths: {"triggers":[{"name":..,"topic":..},...]}.
void WriteLoadedManifest() {
    std::string out = "{\"triggers\":[";
    bool first = true;
    for (const auto& t : g_triggers) {
        if (!first) out += ",";
        first = false;
        out += "{\"name\":\"" + JsonEscapeStr(t.source) + "\",\"topic\":\"" +
               JsonEscapeStr(t.topic) + "\"}";
    }
    out += "]}";
    std::error_code ec;  // CreateDirectoryA 반환 무시 계약 — fail-quiet 원문
    std::filesystem::create_directories(g_exeDir + "\\state", ec);
    FILE* f =
        std::fopen((g_exeDir + "\\state\\triggers_loaded.json").c_str(), "wb");
    if (!f) return;
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    HostLog("[triggers] loaded manifest written (" +
            std::to_string(g_triggers.size()) + " reg(s))");
}

// ---------------------------------------------------------------------------
// Event dispatch
// ---------------------------------------------------------------------------

bool TopicMatches(const std::string& pattern, const std::string& topic) {
    if (pattern == topic) return true;
    // "prefix.*" glob: everything under the namespace.
    if (pattern.size() >= 2 && pattern[pattern.size() - 1] == '*' &&
        pattern[pattern.size() - 2] == '.') {
        const size_t prefixLen = pattern.size() - 1;  // keep the dot
        return topic.size() > prefixLen &&
               topic.compare(0, prefixLen, pattern, 0, prefixLen) == 0;
    }
    return false;
}

void DispatchEvent(const std::string& topic, const std::string& json) {
    JSValue ev = JS_ParseJSON(g_ctx, json.c_str(), json.size(), "event");
    if (JS_IsException(ev)) {
        LogJsException("event parse");
        return;
    }
    // Convenience: hoist data.text to the top level so bundle scripts can
    // read e.text directly (terminal.output feed).
    JSValue data = JS_GetPropertyStr(g_ctx, ev, "data");
    if (JS_IsObject(data)) {
        JSValue text = JS_GetPropertyStr(g_ctx, data, "text");
        if (JS_IsString(text)) {
            JS_SetPropertyStr(g_ctx, ev, "text", JS_DupValue(g_ctx, text));
        }
        JS_FreeValue(g_ctx, text);
    }
    JS_FreeValue(g_ctx, data);

    JSValue evArgs[1] = {ev};
    for (auto& t : g_triggers) {
        if (!t.enabled) continue;   // disabled via state/triggers.json
        if (!TopicMatches(t.topic, topic)) continue;
        if (!JS_IsUndefined(t.match)) {
            // RegExp.test(JSON text of the event) — the spec's filter shape.
            // Note: test() gets the STRING, not the parsed object (calling it
            // with evArgs[0] would stringify to "[object Object]" and never
            // match — caught by the Task 8 probe).
            JSValue evStr =
                JS_NewStringLen(g_ctx, json.c_str(), json.size());
            JSValue strArgs[1] = {evStr};
            JSValue testFn = JS_GetPropertyStr(g_ctx, t.match, "test");
            JSValue r = JS_IsFunction(g_ctx, testFn)
                            ? JS_Call(g_ctx, testFn, t.match, 1, strArgs)
                            : JS_UNDEFINED;
            JS_FreeValue(g_ctx, testFn);
            int hit = 0;
            const bool isBool = !JS_IsException(r) && JS_IsBool(r);
            if (isBool) hit = JS_ToBool(g_ctx, r);
            JS_FreeValue(g_ctx, evStr);
            if (!isBool) {
                LogJsException("filter.test");
                if (!JS_IsException(r)) JS_FreeValue(g_ctx, r);
                continue;
            }
            JS_FreeValue(g_ctx, r);
            if (!hit) continue;
        }
        if (!RateAllow(t.source)) continue;  // docs/38: skip quietly (notify once)
        JSValue r = JS_Call(g_ctx, t.handler, JS_UNDEFINED, 1, evArgs);
        if (JS_IsException(r)) {
            LogJsException("handler");
        } else {
            JS_FreeValue(g_ctx, r);
        }
    }
    JS_FreeValue(g_ctx, ev);
    RunPendingJobs();
}

void FireTimers() {
    const uint64_t now = NowMs();
    for (size_t i = 0; i < g_timers.size();) {
        Timer& t = g_timers[i];
        if (now < t.dueMs) {
            ++i;
            continue;
        }
        const uint64_t interval = t.interval;
        const int64_t id = t.id;
        JSValue r = JS_Call(g_ctx, t.fn, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(r)) {
            LogJsException("timer");
        } else {
            JS_FreeValue(g_ctx, r);
        }
        RunPendingJobs();
        if (interval == 0) {
            // One-shot: the callback may have cleared/inserted timers.
            for (size_t j = 0; j < g_timers.size(); ++j) {
                if (g_timers[j].id == id) {
                    JS_FreeValue(g_ctx, g_timers[j].fn);
                    g_timers.erase(g_timers.begin() + static_cast<long>(j));
                    break;
                }
            }
        } else {
            // Repeating: re-arm (callback may have cleared this very timer).
            bool still = false;
            for (auto& t2 : g_timers) {
                if (t2.id == id && t2.interval == interval) {
                    t2.dueMs = now + interval;
                    still = true;
                    break;
                }
            }
            if (!still) continue;  // removed during the call
            ++i;
        }
    }
}

// ---------------------------------------------------------------------------
// Runtime bootstrap + script loading
// ---------------------------------------------------------------------------

bool InitRuntime() {
    g_rt = JS_NewRuntime();
    if (!g_rt) return false;
    g_ctx = JS_NewContext(g_rt);
    if (!g_ctx) {
        JS_FreeRuntime(g_rt);
        g_rt = nullptr;
        return false;
    }
    JSValue global = JS_GetGlobalObject(g_ctx);

    // Registration idiom follows JKScriptHost: one JS_NewCFunction per entry
    // (JS_CFUNC_def is not part of quickjs-ng's public header).
    JS_SetPropertyStr(g_ctx, global, "on",
                      JS_NewCFunction(g_ctx, JsOn, "on", 3));
    JS_SetPropertyStr(g_ctx, global, "setTimeout",
                      JS_NewCFunction(g_ctx, JsSetTimeout, "setTimeout", 2));
    JS_SetPropertyStr(g_ctx, global, "setInterval",
                      JS_NewCFunction(g_ctx, JsSetInterval, "setInterval", 2));
    JS_SetPropertyStr(g_ctx, global, "clearTimeout",
                      JS_NewCFunction(g_ctx, JsClearTimer, "clearTimeout", 1));
    JS_SetPropertyStr(g_ctx, global, "clearInterval",
                      JS_NewCFunction(g_ctx, JsClearTimer, "clearInterval", 1));

    JSValue desktop = JS_NewObject(g_ctx);
    JS_SetPropertyStr(g_ctx, desktop, "notify",
                      JS_NewCFunction(g_ctx, JsNotify, "notify", 2));
    JS_SetPropertyStr(g_ctx, desktop, "publish",
                      JS_NewCFunction(g_ctx, JsPublish, "publish", 2));
    JS_SetPropertyStr(g_ctx, desktop, "saveLayout",
                      JS_NewCFunction(g_ctx, JsSaveLayout, "saveLayout", 1));
    JS_SetPropertyStr(g_ctx, desktop, "log",
                      JS_NewCFunction(g_ctx, JsLog, "log", 1));
    JS_SetPropertyStr(g_ctx, desktop, "readFile",
                      JS_NewCFunction(g_ctx, JsReadFile, "readFile", 1));
    JS_SetPropertyStr(g_ctx, global, "desktop", desktop);
    JS_FreeValue(g_ctx, global);
    return true;
}

void EvalScript(const std::string& code, const std::string& label) {
    JSValue r = JS_Eval(g_ctx, code.c_str(), code.size(), label.c_str(),
                        JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r)) {
        LogJsException("load " + label);
    } else {
        JS_FreeValue(g_ctx, r);
        HostLog("[triggers] loaded " + label);
    }
    RunPendingJobs();
}

// FindFirstFile glob-tail filter (task 8): the "*.js"/".jkx" pattern match,
// case-insensitive like the FindFirstFile matcher. The dropped residual is
// 8.3 short-name matching only (e.g. "x.js.bak" whose TRIGGER~1.JS short name
// matched "*.js" — a matcher quirk, not a contract; real entries carry the
// true suffix on both legs).
bool GlobTail(const std::string& name, const char* tail) {
    const size_t n = std::strlen(tail);
    if (name.size() < n) return false;
    const size_t base = name.size() - n;
    for (size_t i = 0; i < n; ++i) {
        if (std::tolower(static_cast<unsigned char>(name[base + i])) !=
            std::tolower(static_cast<unsigned char>(tail[i])))
            return false;
    }
    return true;
}

// Dev path: loose .js files next to the exe.
void LoadJsDir() {
    const std::string dir = g_exeDir + "\\state\\triggers";
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) return;  // 원문: FindFirstFile INVALID_HANDLE → 조용히 반환
    for (const std::filesystem::directory_entry& entry : it) {
        std::error_code ec2;
        if (!entry.is_regular_file(ec2) || ec2) continue;
        const std::string fileName = entry.path().filename().string();
        if (!GlobTail(fileName, ".js")) continue;
        const std::string path = entry.path().string();
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) continue;
        std::string code;
        char buf[8192];
        size_t got;
        while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0)
            code.append(buf, got);
        std::fclose(f);
        if (!code.empty()) {
            if (!TrustGate(std::string("state/") + fileName, "dev",
                           FingerprintBytes(std::vector<uint8_t>(
                               code.begin(), code.end()))))
                continue;
            EvalScript(code, std::string("state/") + fileName);
        }
    }
}

// Packaged path: <exeDir>\apps\triggers\*.jkx containers. Each container's
// manifest.txt carries "name=<pkg>" and "trigger=<script.js>[,more.js]".
// The manifest is parsed locally (name/trigger only) — the server never
// sees these containers, so they stay out of the launcher grid.
void LoadTriggerContainers() {
    const std::string dir = g_exeDir + "\\apps\\triggers";
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) return;  // 원문: FindFirstFile INVALID_HANDLE → 조용히 반환
    for (const std::filesystem::directory_entry& entry : it) {
        std::error_code ec2;
        if (!entry.is_regular_file(ec2) || ec2) continue;
        const std::string fileName = entry.path().filename().string();
        if (!GlobTail(fileName, ".jkx")) continue;
        // Container name (sans .jkx) tags every trigger its scripts register
        // — the enable/disable key for state/triggers.json (docs/34).
        std::string container = fileName;
        const size_t dot = container.rfind(".jkx");
        if (dot != std::string::npos) container.resize(dot);
        g_currentSource = container;
        const std::string path = entry.path().string();
        jk::JKJkxFile jkx;
        if (!jkx.Open(path)) {
            HostLog(std::string("[triggers] cannot open ") + fileName);
            continue;
        }
        // Read manifest.txt (type MANI per the writer's convention).
        std::vector<uint8_t> mani;
        const int mi = jkx.FindEntry(nullptr, "manifest.txt");
        if (mi < 0 || !jkx.ReadEntry(mi, mani)) {
            HostLog(std::string("[triggers] no manifest in ") + fileName);
            continue;
        }
        const std::string maniText(mani.begin(), mani.end());
        // The container is the trust unit (spec §2): one fingerprint per
        // .jkx, gated once before any of its scripts eval.
        if (!TrustGate(container, "package", FingerprintContainer(jkx)))
            continue;
        // Parse "trigger=" values (comma list), then eval each script entry.
        for (size_t p = 0; p < maniText.size();) {
            size_t eol = maniText.find('\n', p);
            if (eol == std::string::npos) eol = maniText.size();
            const std::string line = maniText.substr(p, eol - p);
            p = eol + 1;
            if (line.rfind("trigger=", 0) != 0) continue;
            const std::string list = line.substr(8);
            size_t start = 0;
            while (start < list.size()) {
                size_t comma = list.find(',', start);
                if (comma == std::string::npos) comma = list.size();
                std::string script = list.substr(start, comma - start);
                // trim
                while (!script.empty() &&
                       (script.front() == ' ' || script.front() == '\r'))
                    script.erase(script.begin());
                while (!script.empty() &&
                       (script.back() == ' ' || script.back() == '\r'))
                    script.pop_back();
                if (!script.empty()) {
                    std::vector<uint8_t> code;
                    const int idx = jkx.FindEntry(nullptr, script);
                    if (idx >= 0 && jkx.ReadEntry(idx, code) && !code.empty()) {
                        EvalScript(std::string(code.begin(), code.end()),
                                   std::string(fileName) + "/" + script);
                    } else {
                        HostLog(std::string("[triggers] entry not found: ") +
                                fileName + "/" + script);
                    }
                }
                start = comma + 1;
            }
        }
    }
}

void Shutdown() {
    for (auto& t : g_timers) JS_FreeValue(g_ctx, t.fn);
    g_timers.clear();
    for (auto& t : g_triggers) {
        JS_FreeValue(g_ctx, t.match);
        JS_FreeValue(g_ctx, t.handler);
    }
    g_triggers.clear();
    if (g_ctx) JS_FreeContext(g_ctx);
    if (g_rt) JS_FreeRuntime(g_rt);
    g_ctx = nullptr;
    g_rt = nullptr;
}

// --pack <srcDir> <outDir>: pack <srcDir>/<name>/ into <outDir>/<name>.jkx.
int PackMode(const std::string& srcDir, const std::string& outDir) {
    std::error_code ec;  // CreateDirectoryA 반환 무시 계약 — fail-quiet 원문
    std::filesystem::create_directories(outDir, ec);
    ec.clear();
    std::filesystem::directory_iterator it(srcDir, ec);
    if (ec) {
        // Empty bundle is fine (pre-Task-6 builds) — just leave a marker.
        HostLog("jktriggers: no trigger sources under " + srcDir);
        return 0;
    }
    int packed = 0;
    // directory_iterator doesn't yield "."/".." — the strcmp guards die with
    // the Find* walk; the file filter below keeps the directory-only rule.
    for (const std::filesystem::directory_entry& entry : it) {
        std::error_code ec2;
        if (!entry.is_directory(ec2) || ec2) continue;
        const std::string name = entry.path().filename().string();
        const std::string base = entry.path().string();  // 원문: srcDir\name
        std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
        // manifest.txt + every file in the subdirectory (원문 glob은 "*" —
        // 디렉터만 제외하고 전부 수집).
        std::error_code ec3;
        std::filesystem::directory_iterator inner(base, ec3);
        if (ec3) continue;  // 원문: h2 INVALID_HANDLE → continue
        for (const std::filesystem::directory_entry& innerEntry : inner) {
            std::error_code ec4;
            if (!innerEntry.is_regular_file(ec4) || ec4) continue;
            FILE* f = std::fopen(innerEntry.path().string().c_str(), "rb");
            if (!f) continue;
            std::vector<uint8_t> bytes;
            char buf[8192];
            size_t got;
            while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0)
                bytes.insert(bytes.end(), buf, buf + got);
            std::fclose(f);
            entries.emplace_back(innerEntry.path().filename().string(),
                                 std::move(bytes));
        }

        const std::string out = outDir + "\\" + name + ".jkx";
        if (jk::JKJkxFile::Write(out, entries)) {
            HostLog("jktriggers: packed " + out);
            ++packed;
            // Spec §3 self-attestation: the packer records its output's
            // fingerprint as source "pack" (upsert — user records untouched:
            // keepUserRecord skips a same-fingerprint source:"user" record).
            // Entries order == TOC order == the MANI+SCRI stream the loader
            // hashes, so the fingerprints agree by construction.
            std::vector<uint8_t> blob;
            for (const auto& e : entries)
                blob.insert(blob.end(), e.second.begin(), e.second.end());
            TrustRecord r;
            r.fingerprint = FingerprintBytes(blob);
            r.name = name;
            r.source = "pack";
            r.ts = static_cast<long long>(std::time(nullptr));
            if (!r.fingerprint.empty()) {
                std::vector<TrustRecord> recs;
                LoadTrustRecords(g_exeDir + "\\state\\trust.json", &recs);
                TrustUpsert(&recs, r, /*keepUserRecord=*/true);
                if (SaveTrustRecords(g_exeDir + "\\state\\trust.json", recs))
                    HostLog("jktriggers: trust record (pack): " + r.name);
            }
        } else {
            HostLog("jktriggers: FAILED packing " + out);
        }
    }
    HostLog("jktriggers: packed " + std::to_string(packed) + " container(s)");
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    // exeDir for state/triggers and apps/triggers.
    // docs/68 W8a: GMFN 1024 버퍼 → jk::fs::GetExecutablePath 치환 — 버퍼·
    // 수동 슬래시 스캔 소각. 실패 시 빈 문자열 폴백: 원문은 g_exeDir 변경 없음
    // (초기값 "") — 관측 동일. 절단 규약 원문 유지(마지막 슬래시에서 끊어 뒤
    // "\\" 제거; 원문은 슬래시 부재 시 첫 문자 절단 = 빈 문자열, 동일 재현).
    const std::string exePath = jk::fs::GetExecutablePath();
    const size_t lastSlash = exePath.find_last_of("\\/");
    if (lastSlash != std::string::npos) g_exeDir = exePath.substr(0, lastSlash);
    else g_exeDir = "";  // 원문: 슬래시 부재 경로는 첫 문자 절단 = 빈 dir

    if (argc >= 4 && std::strcmp(argv[1], "--pack") == 0) {
        return PackMode(argv[2], argv[3]);
    }

    if (argc >= 2 && std::strcmp(argv[1], "--selftest") == 0) {
        return SelfTest();
    }

    if (!InitRuntime()) {
        HostLog("jktriggers: QuickJS init failed");
        return 1;
    }
    // Spec §4.1: first-run approvals need the server — connect (best effort,
    // ~5 s) BEFORE loading. Trusted records still load offline; untrusted
    // scripts fail closed when the server never appears.
    for (int i = 0; i < 10 && !g_agent.Connect(); ++i) jkconsole::SleepMs(500);
    if (g_agent.IsConnected()) {
        g_agent.SubscribeEvents(true);
        HostLog("[triggers] connected to the window server");
    }
    LoadTrustRecords(g_exeDir + "\\state\\trust.json", &g_trust);
    LoadJsDir();
    LoadTriggerContainers();
    ReloadTriggerFlags();   // apply state/triggers.json before first dispatch
    WriteLoadedManifest();

    HostLog("jktriggers: running (" +
            std::to_string(g_triggers.size()) + " trigger(s), " +
            std::to_string(g_timers.size()) + " timer(s))");

    // Main loop: reconnect-on-timer, ping pump (also flushes replies),
    // event drain, timer fire. 50 ms tick for timer granularity.
    bool subscribed = false;
    uint64_t lastPingMs = 0;
    while (true) {
        if (!g_agent.IsConnected()) {
            if (g_agent.Connect()) {
                subscribed = g_agent.SubscribeEvents(true);
                if (subscribed)
                    HostLog("[triggers] connected to the window server");
            } else {
                jkconsole::SleepMs(1000);
                continue;
            }
        }
        const uint64_t now = NowMs();
        if (now - lastPingMs >= 400) {  // ping pump flushes parked replies
            std::string pong;
            g_agent.Query("ping", "{}", pong);
            lastPingMs = now;
        }
        std::vector<jk::agent::AgentEvent> events;
        g_agent.PollEvents(events);
        for (const auto& ev : events) {
            if (ev.topic == "triggers.reload") {
                ReloadTriggerFlags();  // server publishes after trigger_toggle
                continue;
            }
            DispatchEvent(ev.topic, ev.json);
        }
        FireTimers();
        jkconsole::SleepMs(50);
    }
}
