# 트리거 활성/비활성 UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 트리거(jktriggers 번들)를 에이전트 도구와 팔레트 슬래시 커맨드로 on/off 토글한다.

**Architecture:** `state/triggers.json`이 활성 플래그의 단일 진실원. 서버가 trigger_toggle(파일 갱신 + `triggers.reload` publish)·trigger_list(두 state 파일 병합) 도구를 제공하고, jktriggers는 reload 이벤트에서 C 레벨로 플래그를 재적재해 DispatchEvent에서 enabled=false를 스킵한다. 팔레트는 /triggers·/trigger 명령으로 도구를 호출한다.

**Tech Stack:** C++ (MinGW/ucrt64), QuickJS (quickjs-ng), JKAgentJson (quickjs throwaway-runtime reader), ImGui (팔레트), PowerShell 5.1 probes.

**Spec:** docs/32_desktop_agent_triggers.md §8 "트리거 활성/비활성 UI" 항목 + 2026-09-11 브레인스토밍 결정(팔레트 + 도구, 러버밴드/매니저 앱 아님).

## Global Constraints

- 커밋은 태스크 단위로 main에 직접, 즉시 push. 메시지 끝에 `Co-Authored-By: Claude Code <noreply@anthropic.com>`.
- 빌드: `cd /i/progwork/JKENGINE/engine && export PATH="/c/msys64/ucrt64/bin:$PATH" && cmake --build build` — **빌드 출력을 grep으로 필터하지 말 것** (레슨 37: Permission denied 가려짐 → 스테일 바이너리). 빌드 전 `Get-Process jkdesktop,jktriggers,jkchat | Stop-Process -Force`.
- agentctl은 네이티브 argv — **프로브 JSON 페이로드에 공백 금지** (레슨 32). 프로브는 `Invoke-Agentctl` 함수로 raw JSON 전달.
- PS 스크립트는 반드시 Write 툴로 .ps1 파일로 쓰고 `-File`로 실행 (레슨 27), UTF-8 BOM 필요.
- ImGui/Win32 앱은 WM_GETTEXT로 읽을 수 없음 — 판정은 agentctl reply + state 파일 + list_windows (레슨 34).
- AgentJson: `explicit AgentJson(const std::string&)`, `GetArraySize(key,int&)`, `GetArrStr(key,idx,field,std::string&)`, `GetArrInt(key,idx,field,int&)` — 전부 참조형. JS_ParseJSON은 NUL 종단 필요 (std::string이면 OK).
- jktriggers는 컨트롤 전용 JKAgentClient, 이벤트는 `g_agent.PollEvents(events)` 루프에서 `DispatchEvent(ev.topic, ev.json)`로 전달.

---

### Task 1: jktriggers — 트리거 소스 추적 + 플래그 적재/재적재 + loaded 매니페스트

**Files:**
- Modify: `engine/tools/jktriggers/main.cpp` (TriggerReg, JsOn, EvalScript 주변, DispatchEvent, main)

**Interfaces:**
- Produces: `TriggerReg.source`(컨테이너명, 예 "trig_build")·`TriggerReg.enabled`; C 함수 `ReloadTriggerFlags()`, `WriteLoadedManifest()`; `state/triggers_loaded.json` = `{"triggers":[{"name":"trig_build","topics":["terminal.output",...]},...]}`. Task 2의 trigger_list가 이 파일을 읽는다. 파일은 jktriggers가 **쓰는** 유일한 state 파일 (플래그 파일은 서버가 쓴다).

- [ ] **Step 1: 구현**

`TriggerReg` 확장 + 전역 (g_triggers 선언 근처):

```cpp
struct TriggerReg {
    std::string topic;
    JSValue match;      // {match: /regex/} or JS_UNDEFINED
    JSValue handler;    // function(ev)
    std::string source; // container name ("trig_build") — enable/disable key
    bool enabled = true;
};
std::vector<TriggerReg> g_triggers;
// Container name set while its scripts eval (LoadTriggerContainers → JsOn).
std::string g_currentSource;
// name -> 0/1 from state/triggers.json; absent entry = enabled (1).
std::map<std::string, int> g_enabled;
```

`JsOn`에 `t.source = g_currentSource;` 추가 (`t.handler = JS_DupValue...` 줄 위).

`LoadTriggerContainers`: 컨테이너명 추출 + eval 전 설정 — `do {` 루프 맨 앞:

```cpp
std::string container = fd.cFileName;
const size_t dot = container.rfind(".jkx");
if (dot != std::string::npos) container.resize(dot);
g_currentSource = container;
```

`DispatchEvent` 루프 맨 앞에 게이트:

```cpp
for (auto& t : g_triggers) {
    if (!t.enabled) continue;   // disabled via state/triggers.json (docs/34)
    if (!TopicMatches(t.topic, topic)) continue;
```

새 C 레벨 함수 2개 (`DispatchEvent` 위, `// Event dispatch` 섹션 앞):

```cpp
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
    HostLog("[triggers] flags reloaded (" + std::to_string(g_enabled.size()) + ")");
}

// Startup manifest for trigger_list (server merges with triggers.json):
// {"triggers":[{"name":"trig_build","topics":["terminal.output"]},...]}
void WriteLoadedManifest() {
    std::map<std::string, std::vector<std::string>> bySource;
    for (const auto& t : g_triggers) bySource[t.source].push_back(t.topic);
    std::string out = "{\"triggers\":[";
    bool first = true;
    for (const auto& kv : bySource) {
        if (!first) out += ",";
        first = false;
        out += "{\"name\":\"" + JsonEscapeStr(kv.first) + "\",\"topics\":[";
        for (size_t i = 0; i < kv.second.size(); ++i) {
            if (i) out += ",";
            out += "\"" + JsonEscapeStr(kv.second[i]) + "\"";
        }
        out += "]}";
    }
    out += "]}";
    CreateDirectoryA((g_exeDir + "\\state").c_str(), nullptr);
    FILE* f = std::fopen((g_exeDir + "\\state\\triggers_loaded.json").c_str(),
                         "wb");
    if (!f) return;
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
}
```

필요 헤더 확인: `<agent/JKAgentJson.h>`가 이미 포함돼 있는지 확인하고 없으면 추가 (JKAgentClient/AgentEvent는 이미 jk::agent — 아마 같은 헤더 체인). `<map>` 추가.

`main()` 시작 시퀀스:

```cpp
LoadTriggerContainers();
ReloadTriggerFlags();    // apply state/triggers.json before first dispatch
WriteLoadedManifest();
```

메인 루프 이벤트 처리 변경:

```cpp
for (const auto& ev : events) {
    if (ev.topic == "triggers.reload") {
        ReloadTriggerFlags();   // server publishes after trigger_toggle
        continue;
    }
    DispatchEvent(ev.topic, ev.json);
}
```

- [ ] **Step 2: 빌드**

Run: `cmake --build build` — 에러 없음 확인 (출력 필터 금지; jktriggers.exe mtime 확인).

- [ ] **Step 3: 실측** — Write 툴로 임시 .ps1: 서버+jktriggers 기동 → 3초 → `state\triggers_loaded.json` 내용 출력 (trig_build/trig_idle/trig_crash + topics) → 프로세스 정리. 예상: 3 컨테이너, trig_build의 topics에 terminal.output.

- [ ] **Step 4: 커밋** — `feat(triggers): per-container enable flags + loaded manifest`

### Task 2: 서버 — trigger_toggle / trigger_list 도구

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (HandleAgentQuery, publish_event 분기 뒤)

**Interfaces:**
- Consumes: Task 1의 `state/triggers_loaded.json`; 기존 `StateDir()`, `AgentJson`, publish_event 인벨롭 경로.
- Produces: 도구 `trigger_toggle {"name":<str>,"on":<0|1>}` → `{"ok":true}` + `triggers.reload` publish; 도구 `trigger_list` → `{"ok":true,"triggers":[{"name","topics":[...],"enabled":0|1},...]}` (loaded 파일에만 있고 flags에 없으면 enabled=1).

- [ ] **Step 1: trigger_toggle 구현** (publish_event 분기 뒤에 삽입; 권한 게이트 밖 — launch_app 등급):

읽기-수정-쓰기:

```cpp
} else if (tool == "trigger_toggle") {
    // docs/34: write state/triggers.json (single source of truth) then
    // publish triggers.reload — jktriggers re-reads the file on the event.
    std::string name;
    int on = -1;
    req.GetObjStr("args", "name", name);
    req.GetObjInt("args", "on", on);
    if (name.empty() || on < 0) {
        reply = "{\"ok\":false,\"error\":\"missing_name\"}";
    } else {
        const std::string path = StateDir() + "\\triggers.json";
        std::map<std::string, int> flags;
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (f) {
            char buf[4096] = {};
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            jk::agent::AgentJson json(buf);
            int cnt = 0;
            if (json.ok() && json.GetArraySize("triggers", cnt)) {
                for (int i = 0; i < cnt && i < 64; ++i) {
                    std::string en;
                    int ev = 1;
                    std::string nm;
                    if (json.GetArrStr("triggers", i, "name", nm)) {
                        json.GetArrInt("triggers", i, "enabled", ev);
                        flags[nm] = ev;
                    }
                }
            }
        }
        flags[name] = on;
        std::string out = "{\"triggers\":[";
        bool first = true;
        for (const auto& kv : flags) {
            if (!first) out += ",";
            first = false;
            out += "{\"name\":\"" + JsonEsc(kv.first) + "\",\"enabled\":" +
                   std::to_string(kv.second) + "}";
        }
        out += "]}";
        reply = "{\"ok\":true}";
        if (std::FILE* w = std::fopen(path.c_str(), "wb")) {
            std::fwrite(out.data(), 1, out.size(), w);
            std::fclose(w);
        } else {
            reply = "{\"ok\":false,\"error\":\"write_failed\"}";
        }
        if (reply.find("\"ok\":true") != std::string::npos) {
            const long long ts = static_cast<long long>(std::time(nullptr)) * 1000;
            PushAgentEventJson("{\"topic\":\"triggers.reload\",\"data\":{},\"ts\":" +
                               std::to_string(ts) + "}");
        }
    }
}
```

(주의: 실행 시 publish_event 분기의 실제 인벨롭 생성 코드를 보고 동일한 헬퍼/형식 재사용 — 위 PushAgentEventJson 페이로드가 그 형식과 일치하면 그대로, 다르면 publish_event의 문자열 빌드를 그대로 복제.)

- [ ] **Step 2: trigger_list 구현** (trigger_toggle 뒤):

```cpp
} else if (tool == "trigger_list") {
    // merge: names+topics from jktriggers' manifest, enabled from the flags
    // file (missing entry = enabled).
    std::map<std::string, std::vector<std::string>> loaded;
    std::map<std::string, int> flags;
    auto readState = [&](const char* file,
                         std::function<void(jk::agent::AgentJson&, int)> fn) {
        std::FILE* f = std::fopen((StateDir() + "\\" + file).c_str(), "rb");
        if (!f) return;
        char buf[8192] = {};
        const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
        std::fclose(f);
        jk::agent::AgentJson json(buf);
        int cnt = 0;
        if (json.ok() && json.GetArraySize("triggers", cnt)) {
            for (int i = 0; i < cnt && i < 64; ++i) fn(json, i);
        }
    };
    readState("triggers_loaded.json", [&](jk::agent::AgentJson& j, int i) {
        std::string nm;
        if (!j.GetArrStr("triggers", i, "name", nm)) return;
        int tn = 0;
        j.GetArraySize(("triggers." + std::to_string(i) + ".topics").c_str(), tn);
        ...
    });
```

**주의 — 위 스케치의 topics 중첩 접근은 AgentJson가 지원하지 않을 수 있다.** 실행 시 `JKAgentJson.h`를 먼저 읽고 지원 API 확인. 중첩 배열 접근이 없으면 (1) topics 생략(이름+enabled만), 또는 (2) triggers_loaded.json을 `{"triggers":[{"name":...,"topic":"terminal.output"},...]}` **평면 배열**(Task 1에서 컨테이너×토픽 각 행)로 바꾸고 `GetArrStr("triggers",i,"topic")`로 읽는다. 실행 시 (2)가 가능하면 (2)로 확정 — Task 1의 WriteLoadedManifest를 그에 맞게 수정. reply 형식: `{"ok":true,"triggers":[{"name":"x","topic":"y","enabled":1},...]}`.

- [ ] **Step 3: 빌드** (필터 금지, mtime 확인).

- [ ] **Step 4: 실측** — 임시 .ps1: 서버+jktriggers 기동 → `agentctl trigger_list` 출력 → `agentctl trigger_toggle {"name":"trig_idle","on":0}` → `state\triggers.json` 내용 확인 → `trigger_list`에서 enabled 0 확인. cleanup: state\triggers.json 삭제.

- [ ] **Step 5: 커밋** — `feat(agent): trigger_toggle + trigger_list tools`

### Task 3: 팔레트 — /triggers, /trigger

**Files:**
- Modify: `engine/src/apps/ClientPaletteApp.cpp` (Submit의 /notify 분기 뒤)

**Interfaces:**
- Consumes: Task 2 도구. 기존 `SendTool(tool, argsJson)`, `AppendLog`, `Trim`, `EscapeJson`.

- [ ] **Step 1: Submit에 추가:**

```cpp
} else if (cmd == "triggers") {
    SendTool("trigger_list", "{}");
} else if (cmd == "trigger") {
    // /trigger <name> on|off
    const size_t sp2 = arg.find(' ');
    if (sp2 == std::string::npos || arg.empty()) {
        AppendLog("  usage: /trigger <name> on|off");
    } else {
        const std::string name = Trim(arg.substr(0, sp2));
        const std::string mode = Trim(arg.substr(sp2 + 1));
        if (mode != "on" && mode != "off") {
            AppendLog("  usage: /trigger <name> on|off");
        } else {
            SendTool("trigger_toggle",
                     "{\"name\":\"" + EscapeJson(name) + "\",\"on\":" +
                         (mode == "on" ? "1" : "0") + "}");
        }
    }
}
```

`/help` 줄에 `  /triggers  /trigger <name> on|off` 추가.

- [ ] **Step 2: 빌드** (팔레트 UI는 WM_GETTEXT 불가 — 회귀 probe_agent_palette로 도구 경로만 확인).

- [ ] **Step 3: 커밋** — `feat(palette): /triggers list + /trigger on|off`

### Task 4: 프로브 + docs/34 + 회귀

**Files:**
- Create: `engine/tools/probes/probe_agent_triggerctl.ps1` (UTF-8 BOM)
- Create: `docs/34_desktop_agent_triggerctl.md`
- Modify: `docs/32_desktop_agent_triggers.md` §8, memory roadmap

- [ ] **Step 1: probe 작성** — 체크 5종, 전부 파일/agentctl 판정:
  1. loaded-manifest: 서버+jktriggers 기동 3초 → `state\triggers_loaded.json`에 trig_build/trig_idle/trig_crash 전부 존재.
  2. list: `trigger_list` → ok + 3개, enabled 1.
  3. off-blocks: launch_app notify (이벤트 구독자) → `trigger_toggle trig_build on=0` → publish terminal.output 에러 → 4초 → notify_history 엔트리 수 **증가 없음**.
  4. on-resumes: `trigger_toggle trig_build on=1` → publish 에러 → 4초 → 엔트리 +1 (title="빌드 실패 감지" 계열).
  5. state-file: `state\triggers.json`에 `{"name":"trig_build","enabled":1}` 존재.
  - cleanup: state\triggers.json, triggers_loaded.json, notify_history.json 삭제 + 프로세스 kill (캡처는 cleanup 전).
  - 페이로드 공백 금지, BOM 1회.

- [ ] **Step 2: probe 실행** — PASS 5/5.

- [ ] **Step 3: 전체 회귀** — `jkdesktop test` 0 + probe_agent_mcp/e2e/palette/chat/chat_llm/triggers/notify 전부 PASS (기존 triggers 프로브는 플래그 파일 없는 상태도 통과해야 함 — cleanup에서 state\triggers.json 삭제 필수).

- [ ] **Step 4: docs/34** — 요약/구성/state 파일/도구/팔레트/검증/레슨 + docs/32 §8 갱신.

- [ ] **Step 5: 커밋+푸시** — `feat(triggers): probe + docs/34 — trigger on/off complete`

## Self-Review

- 스펙 커버리지: 플래그 파일(1,2)/재적재(1)/도구(2)/팔레트(3)/검증·문서(4) — 커버됨.
- 플레이스홀더: Task 2의 topics 중첩 접근은 API 확인 후 확정하도록 **명시적 분기 지시**로 기록 (플레이스홀더 아님 — 실제 대안 2개 제시).
- 타입 일관성: `trigger_toggle {"name","on"}` int 0/1 / 팔레트도 1·0 전송 / 프로브도 0·1 — 일치.