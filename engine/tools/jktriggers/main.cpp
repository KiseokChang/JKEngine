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
#include <JKJkxFile.h>
#include <quickjs.h>

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstring>
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
    CreateDirectoryA((g_exeDir + "\\state").c_str(), nullptr);
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

// Dev path: loose .js files next to the exe.
void LoadJsDir() {
    const std::string dir = g_exeDir + "\\state\\triggers\\*.js";
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(dir.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::string path = g_exeDir + "\\state\\triggers\\" + fd.cFileName;
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) continue;
        std::string code;
        char buf[8192];
        size_t got;
        while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0)
            code.append(buf, got);
        std::fclose(f);
        if (!code.empty()) EvalScript(code, std::string("state/") + fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// Packaged path: <exeDir>\apps\triggers\*.jkx containers. Each container's
// manifest.txt carries "name=<pkg>" and "trigger=<script.js>[,more.js]".
// The manifest is parsed locally (name/trigger only) — the server never
// sees these containers, so they stay out of the launcher grid.
void LoadTriggerContainers() {
    const std::string dir = g_exeDir + "\\apps\\triggers\\*.jkx";
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(dir.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        // Container name (sans .jkx) tags every trigger its scripts register
        // — the enable/disable key for state/triggers.json (docs/34).
        std::string container = fd.cFileName;
        const size_t dot = container.rfind(".jkx");
        if (dot != std::string::npos) container.resize(dot);
        g_currentSource = container;
        const std::string path = g_exeDir + "\\apps\\triggers\\" + fd.cFileName;
        jk::JKJkxFile jkx;
        if (!jkx.Open(path)) {
            HostLog(std::string("[triggers] cannot open ") + fd.cFileName);
            continue;
        }
        // Read manifest.txt (type MANI per the writer's convention).
        std::vector<uint8_t> mani;
        const int mi = jkx.FindEntry(nullptr, "manifest.txt");
        if (mi < 0 || !jkx.ReadEntry(mi, mani)) {
            HostLog(std::string("[triggers] no manifest in ") + fd.cFileName);
            continue;
        }
        const std::string maniText(mani.begin(), mani.end());
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
                                   std::string(fd.cFileName) + "/" + script);
                    } else {
                        HostLog(std::string("[triggers] entry not found: ") +
                                fd.cFileName + "/" + script);
                    }
                }
                start = comma + 1;
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
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
    CreateDirectoryA(outDir.c_str(), nullptr);
    const std::string glob = srcDir + "\\*";
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(glob.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        // Empty bundle is fine (pre-Task-6 builds) — just leave a marker.
        HostLog("jktriggers: no trigger sources under " + srcDir);
        return 0;
    }
    int packed = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            std::strcmp(fd.cFileName, ".") == 0 ||
            std::strcmp(fd.cFileName, "..") == 0)
            continue;
        const std::string base = srcDir + "\\" + fd.cFileName;
        std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
        // manifest.txt + every *.js in the subdirectory.
        const std::string inner = base + "\\*";
        WIN32_FIND_DATAA fd2{};
        HANDLE h2 = FindFirstFileA(inner.c_str(), &fd2);
        if (h2 == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            const std::string path = base + "\\" + fd2.cFileName;
            FILE* f = std::fopen(path.c_str(), "rb");
            if (!f) continue;
            std::vector<uint8_t> bytes;
            char buf[8192];
            size_t got;
            while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0)
                bytes.insert(bytes.end(), buf, buf + got);
            std::fclose(f);
            entries.emplace_back(fd2.cFileName, std::move(bytes));
        } while (FindNextFileA(h2, &fd2));
        FindClose(h2);

        const std::string out = outDir + "\\" + fd.cFileName + ".jkx";
        if (jk::JKJkxFile::Write(out, entries)) {
            HostLog("jktriggers: packed " + out);
            ++packed;
        } else {
            HostLog("jktriggers: FAILED packing " + out);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    HostLog("jktriggers: packed " + std::to_string(packed) + " container(s)");
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    // exeDir for state/triggers and apps/triggers.
    char modulePath[1024] = {};
    if (GetModuleFileNameA(nullptr, modulePath, sizeof(modulePath))) {
        char* lastSlash = modulePath;
        for (char* p = modulePath; *p; ++p) {
            if (*p == '\\' || *p == '/') lastSlash = p;
        }
        *lastSlash = '\0';
        g_exeDir = modulePath;
    }

    if (argc >= 4 && std::strcmp(argv[1], "--pack") == 0) {
        return PackMode(argv[2], argv[3]);
    }

    if (!InitRuntime()) {
        HostLog("jktriggers: QuickJS init failed");
        return 1;
    }
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
                Sleep(1000);
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
        Sleep(50);
    }
}