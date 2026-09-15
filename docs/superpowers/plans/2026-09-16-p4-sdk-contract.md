# P4 SDK 계약 (콘솔 앱 + 에이전트 쌍방) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 콘솔 앱 kind(`apps/<name>/manifest.json`)를 정식화하고, `jkctl` CLI(앱→에이전트)와 `run_console_app` 도구(에이전트→앱), 템플릿+샘플 앱을 만든다.

**Architecture:** 데스크탑 셸의 .jkx 스캔 옆에 콘솔 앱 스캔을 추가하고(매니페스트 → 런처 셀), 스폰은 기존 `terminal --shell/--cwd` CLI를 재사용한다. 에이전트 방향은 jkagentd가 서버에 전달하는 `run_console_app` 도구 1종(close_window의 ask 게이트 패턴)이고, 앱 방향은 jkagentd의 control-client 패턴을 CLI로 감싼 `jkctl.exe`(notify/agent/ask)다.

**Tech Stack:** C++ (jkwinserver/jkdesktop), quickjs JSON 파싱(JKTerminalConfig 패턴), JKAgentClient pipe, bcrypt SHA-256, Python/배치 샘플

**Spec:** `docs/superpowers/specs/2026-09-16-p4-sdk-contract-design.md`

## Global Constraints

- 스펙 §3.4: 새 권한 모델 금지 — trust 모델(docs/37) 재사용. 로컬 `apps/`는 신뢰 기본.
- 스펙 §4: jkctl은 동기 원컷 — 스트리밍/세션 YAGNI. §5: 모니터링 YAGNI — 스폰만.
- 상대경로 규칙(docs/44): 앱 디렉토리·바이너리는 jkdesktop cwd(engine/build) 기준 상대경로.
- 빌드: `engine/build_sdl2_jkwindow.bat` (내부에서 build_with_temp.sh — jkapp_*.dll/jkwinserver.exe 복사 픽스 커밋 03a637d/ebf7826 포함). 빌드 전 서버 프로세스 종료 필수 (파일 잠금).
- 검증: 기존 셀프테스트(main.cpp test 모드)는 Win32 스폰 코드 대상이 아니다 — 프로브+서버 로그로 검증(docs/15 패턴). 각 태스크 커밋은 독립 빌드 통과 후.
- 신뢰 지문 형식: `{"records":[{"fingerprint":"sha256:<64hex>","name":…,"source":"user","ts":…}]}` (state/trust.json, jktriggers 로더와 동일 — engine/tools/jktriggers/main.cpp §Trust store).

---

### Task 1: 콘솔 앱 스캔 + 런처 등록 + 스폰 (셸/서버)

**Files:**
- Modify: `engine/include/desktop/JKDesktopShell.h` (LauncherIcon 필드, ShellHost 콜백, ConsoleAppInfo)
- Modify: `engine/src/desktop/JKDesktopShell.cpp` (ScanConsoleApps, Init, LaunchAt)
- Modify: `engine/src/server/JKWindowServer.cpp` (ShellHost wiring, ~150-160행 shellHost 초기화)

**Interfaces:**
- Consumes: 기존 `terminal --shell/--cwd` CLI (docs/44), `LoadImageFile`/`host_.makeTexture`
- Produces: `ShellHost::spawnConsole(const std::string& cmd, const std::string& cwd, const std::string& name)` — Task 2가 재사용. `JKDesktopShell::ConsoleAppInfo(const std::string& name, std::string& cmd, std::string& dir, std::string& fingerprint) const` — Task 2가 재사용. `LauncherIcon`에 `consoleCmd`/`consoleDir` 공백 문자열 기본.

- [ ] **Step 1: LauncherIcon + ShellHost 확장**

`JKDesktopShell.h`의 `LauncherIcon` 구조체(기존 `jkxPath` 필드 옆)에:

```cpp
        // 콘솔 앱 kind (P4 SDK, specs/2026-09-16-p4-sdk-contract §3):
        // 비었으면 .jkx/내장 앱 셀. cmd는 상대경로(서버 cwd 기준).
        std::string consoleCmd;
        std::string consoleDir;
```

`ShellHost` 구조체(기존 `launch` 콜백 옆)에:

```cpp
    // 콘솔 앱 스폰: 터미널 위에 cmd를 띄운다 (cwd = 앱 폴더, 상대경로).
    std::function<void(const std::string& cmd, const std::string& cwd,
                       const std::string& name)> spawnConsole;
```

(헤더에 `<functional>` include 확인 — 기존 launch가 이미 쓰므로 있음.)

- [ ] **Step 2: ScanConsoleApps 구현**

`JKDesktopShell.cpp`의 `ScanJkxApps()`(138행 근처) 아래에 추가:

```cpp
// 콘솔 앱 스캔 (P4 SDK §3): apps/<name>/manifest.json 디렉터리를 .jkx와
// 같은 스캔 대상으로 받는다. JSON 파싱은 quickjs throwaway 런타임
// (JKTerminalConfig::Load 패턴, 1MiB 상한). .jkx와 이름이 겹치면 .jkx 우선.
void JKDesktopShell::ScanConsoleApps() {
    char basePath[MAX_PATH] = {};
    // ScanJkxApps의 exe-dir 산출 코드를 그대로 재사용 (동일 패턴 —
    // GetModuleFileNameA → 디렉토리 절단 → "\\apps\\*").
    std::snprintf(pattern, sizeof(pattern), "%s\\apps\\*", basePath);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            fd.cFileName[0] == '.') continue;
        const std::string dir = std::string("apps\\") + fd.cFileName;
        const std::string manifestPath = basePath + "\\" + fd.cFileName + "\\manifest.json";
        // 1MiB 상한으로 파일을 통째로 읽는다 (JKTerminalConfig::ReadFileBytes 패턴).
        std::vector<uint8_t> bytes;  // ReadFileBytes는 파일-로컬 헬퍼 — 동일 구현 복제
        if (!ReadManifestBytes(manifestPath, bytes)) continue;

        JSRuntime* rt = JS_NewRuntime();
        JSContext* ctx = rt ? JS_NewContext(rt) : nullptr;
        JSValue root = ctx ? JS_ParseJSON(ctx, reinterpret_cast<const char*>(bytes.data()),
                                          bytes.size() - 1, "manifest.json") : JS_UNDEFINED;
        std::string name, cmd, desc;
        if (!JS_IsException(root) && JS_IsObject(root)) {
            // JKTerminalConfig의 getString 람다와 동일 구현 (name/cmd/desc)
        }
        JS_FreeValue(ctx, root); if (ctx) JS_FreeContext(ctx); if (rt) JS_FreeRuntime(rt);
        if (name.empty() || cmd.empty()) continue;

        bool taken = false;  // .jkx 우선: 이름 충돌 시 스킵
        for (const auto& icon : launcherIcons_) taken |= (icon.appName == name);
        if (taken) continue;

        LauncherIcon icon;
        icon.appName = name;
        icon.consoleDir = dir;
        icon.consoleCmd = cmd;
        // 아이콘: apps/<name>/icon@{2x,1x}.png — 없으면 placeholder(null texture)
        char iconPath[600];
        std::snprintf(iconPath, sizeof(iconPath), "%s\\%s\\icon@2x.png", basePath, fd.cFileName);
        jk::LoadedImage img;
        if (jk::LoadImageFile(iconPath, img)) icon.texture = host_.makeTexture(img, "console-icon");
        launcherIcons_.push_back(std::move(icon));
        std::fprintf(stderr, "JKWindowServer: console app '%s' (cmd='%s')\n",
                     name.c_str(), cmd.c_str());
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
```

(주석의 "동일 구현" 3곳 — `ScanJkxApps`의 exe-dir 코드, `JKTerminalConfig.cpp`의 `getString` 람다와 `ReadFileBytes`를 그대로 옮겨 쓴다. 전부 기존 검증 코드의 복제다.)

`Init()`에서 `ScanJkxApps();` 바로 뒤에 `ScanConsoleApps();` 호출 추가.

- [ ] **Step 3: LaunchAt 분기 + ConsoleAppInfo**

`LaunchAt`(272행 근처)의 스폰 분기를:

```cpp
    const LauncherIcon& item = launcherIcons_[static_cast<size_t>(icon)];
    if (!item.consoleCmd.empty()) {
        if (host_.spawnConsole) host_.spawnConsole(item.consoleCmd, item.consoleDir, item.appName);
        return true;
    }
    if (item.jkxPath.empty()) {
```

새 public 메서드 (Task 2 소비):

```cpp
bool JKDesktopShell::ConsoleAppInfo(const std::string& name, std::string& cmd,
                                    std::string& dir, std::string& fingerprint) const {
    for (const auto& icon : launcherIcons_) {
        if (icon.consoleCmd.empty() || icon.appName != name) continue;
        cmd = icon.consoleCmd;
        dir = icon.consoleDir;
        fingerprint = ConsoleAppFingerprint(cmd);  // "sha256:<64hex>" — Task 1b에서 구현
        return true;
    }
    return false;
}
```

- [ ] **Step 4: 서버 콜백 연결**

`JKWindowServer.cpp` shellHost 초기화(154행 근처)의 `shellHost.launch` 뒤에:

```cpp
    shellHost.spawnConsole = [this](const std::string& cmd, const std::string& cwd,
                                    const std::string& name) {
        // terminal --cwd/--shell CLI (docs/44). SpawnProcess는 cmdLine을
        // 만들어 CreateProcessW에 넘기므로 인용 겹침 주의(453a327 레슨):
        // 뒤따르는 백슬래시가 없는 경로만 인용 — 상대경로 규칙이 이를 보장.
        std::string args = "terminal --cwd \"" + cwd + "\" --shell \"" + cmd + "\"";
        SpawnProcess(clientHostExe_.c_str(), args, name.c_str());
    };
```

- [ ] **Step 5: 빌드 + 프로브 검증**

샘플 매니페스트를 `engine/build/apps/sdkprobe/manifest.json`에 만든다:

```json
{"name":"sdkprobe","cmd":"cmd /c echo SDK probe ok","desc":"스캔 프로브"}
```

Run: `cmd //c "I:\progwork\JKENGINE\engine\build_sdl2_jkwindow.bat"` (서버 종료 상태 확인)
Run: `./jkwinserver.exe` 후 로그에서 `console app 'sdkprobe'` 확인 + 런처에서 클릭 → 터미널 창에 "SDK probe ok"
Expected: 둘 다 성공

- [ ] **Step 6: Commit**

```bash
git add engine/include/desktop/JKDesktopShell.h engine/src/desktop/JKDesktopShell.cpp engine/src/server/JKWindowServer.cpp
git commit -m "feat(sdk): console app kind scan + launcher spawn (P4 SDK task 1)"
```

---

### Task 1b: 스폰 cmd 지문 기록 (trust ledger)

**Files:**
- Modify: `engine/src/desktop/JKDesktopShell.cpp` (ScanConsoleApps 내 기록)

**Interfaces:**
- Consumes: state/trust.json 형식 `{"records":[{"fingerprint":"sha256:<64hex>","name":…,"source":"user","ts":…}]}` (engine/tools/jktriggers/main.cpp:166)
- Produces: `ConsoleAppFingerprint(const std::string& cmd)` — "sha256:"+64hex. Task 2가 ConsoleAppInfo로 간접 재사용.

- [ ] **Step 1: SHA-256 헬퍼**

`JKDesktopShell.cpp` 파일-로컬 네임스페이스에 (bcrypt.h include 추가):

```cpp
// cmd 지문 (P4 SDK §3.4): docs/37 스킴 재사용 — jktriggers ComputeSha256Hex와
// 동일 형식("sha256:"+64hex), bcrypt 구현.
std::string ConsoleAppFingerprint(const std::string& cmd) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return "";
    std::vector<uint8_t> hash(32);
    BCryptHashData(alg, (PUCHAR)cmd.data(), (ULONG)cmd.size(), 0);
    BCryptFinishHash(alg, hash.data(), (ULONG)hash.size(), 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    std::string out = "sha256:";
    char hex[3];
    for (uint8_t b : hash) { std::snprintf(hex, 3, "%02x", b); out += hex; }
    return out;
}
```

- [ ] **Step 2: trust.json 레코드 보증**

`ScanConsoleApps()`에서 매니페스트 수용 후(`push_back` 앞):

```cpp
        // 스폰 cmd 지문 보증 (docs/37 형식). 이미 있으면 건드리지 않는다 —
        // trust.json의 유일 작성자는 이 스캔(콘솔)과 스크립트 로더(스크립트).
        const std::string fp = ConsoleAppFingerprint(cmd);
        if (!fp.empty()) { /* state/trust.json 읽기→records에 (fingerprint,name,source="user",ts) 없으면 append→쓰기 */ }
```

(JSON append는 AgentJson으로 읽고 문자열로 재조립 — jktriggers `SaveTrustRecords`의 문자열 조립 방식과 동일. 레코드 객체에 `kind` 필드는 추가하지 않는다 — 기존 로더가 모르는 필드 추가는 스킴 오염.)

- [ ] **Step 3: 빌드 + 검증 + Commit**

빌드 후 서버 기동 → `engine/build/state/trust.json`에 sdkprobe 지문 레코드 확인:

```bash
git add engine/src/desktop/JKDesktopShell.cpp
git commit -m "feat(sdk): record spawn cmd fingerprint in trust ledger (P4 SDK §3.4)"
```

---

### Task 2: 에이전트→앱 `run_console_app` 도구

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (도기 dispatch 블록, 1672행 close_window 앞/뒤)
- Modify: `engine/include/server/JKWindowServer.h:183` (PendingApproval 주석의 kind 목록 갱신)
- Modify: `engine/tools/jkagentd/main.cpp:46` (kToolsListJson + IsKnownTool)

**Interfaces:**
- Consumes: Task 1의 `ConsoleAppInfo(name→cmd,dir,fingerprint)`, `shellHost.spawnConsole` 경로(서버 메서드로 승격 필요 — Step 1), close_window 게이트 패턴(1672-1738), approve 해소(2356 근처)
- Produces: 서버 도구 `run_console_app {name}` → `{"ok":true,"surface":<id>}` / ask 게이트는 kind "run_console_app"

- [ ] **Step 1: 스폰 함수 승격**

ShellHost.spawnConsole 람다 본문을 `JKWindowServer::SpawnConsoleApp(const std::string& cmd, const std::string& cwd, const std::string& name)` private 메서드로 추출(콜백과 도구가 공유).

- [ ] **Step 2: 도구 dispatch (close_window 패턴)**

`launch_app` 블록(1829행) 뒤에:

```cpp
    } else if (tool == "run_console_app") {
        // P4 SDK §5: 에이전트가 콘솔 앱을 스폰. ask 기본 게이트(close_window
        // 패턴) — target 창이 없으므로 targetId=0, 식별자는 name.
        std::string name;
        req.GetObjStr("args", "name", name);
        std::string cmd, dir, fp;
        if (name.empty() || name.size() > 64 ||
            !shell_ || !shell_->ConsoleAppInfo(name, cmd, dir, fp)) {
            reply = "{\"ok\":false,\"error\":\"unknown_app\"}";
        } else {
            switch (AgentToolAllowed("run_console_app")) {
                case AgentDecision::Allow:
                    SpawnConsoleApp(cmd, dir, name);
                    reply = "{\"ok\":true}";
                    break;
                case AgentDecision::Ask:
                    // close_window의 Ask 분기와 동일 — subscriber 확인, 파킹,
                    // agent.approval_request 방송. kind="run_console_app",
                    // name=앱 이름 (approve 해소에서 재사용).
                    /* close_window 1690-1733 코드와 동형 — 버퍼 포맷만:
                       "{\"topic\":\"agent.approval_request\",\"request\":%u,"
                       "\"tool\":\"run_console_app\",\"name\":\"%s\",\"ts\":%lld}" */
                    replied = false;
                    break;
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    }
```

- [ ] **Step 3: approve 해소 분기**

`approve` 블록의 `if (allow && it->kind == "close_window")` 뒤에:

```cpp
            if (allow && it->kind == "run_console_app" && shell_) {
                std::string cmd, dir, fp;
                if (shell_->ConsoleAppInfo(it->name, cmd, dir, fp)) {
                    SpawnConsoleApp(cmd, dir, it->name);
                }
            }
```

(`PendingApproval.kind` 주석에 `"run_console_app"` 추가.)

- [ ] **Step 4: jkagentd 도구 표면**

`kToolsListJson`에 추가:

```cpp
"{\"name\":\"run_console_app\",\"description\":\"Spawn an installed console app (apps/<name>/manifest.json) in a terminal (permission-gated)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}},\"required\":[\"name\"]}},"
```

`IsKnownTool`의 kNames 배열에 `"run_console_app"` 추가.

- [ ] **Step 5: 빌드 + e2e 검증**

빌드 → 서버 기동 → `jkdesktop agentctl '{"tool":"run_console_app","args":{"name":"sdkprobe"}}'` → sdkprobe 터미널 스폰 확인. ask 게이트 동작은 permissions.json에서 run_console_app을 ask로 두고 jkchat 승인창 경로 확인(사용자 확인).

- [ ] **Step 6: Commit**

```bash
git add engine/src/server/JKWindowServer.cpp engine/include/server/JKWindowServer.h engine/tools/jkagentd/main.cpp
git commit -m "feat(agent): run_console_app tool — agent spawns console apps (P4 SDK §5)"
```

---

### Task 3: 앱→에이전트 `jkctl` CLI

**Files:**
- Create: `engine/tools/jkctl/main.cpp`
- Modify: `engine/CMakeLists.txt` (jkchat 타깃 블록 옆에 동일 패턴)
- Modify: `engine/src/main.cpp:2615` 부근 (도움말에 jkctl 언급 — 선택)

**Interfaces:**
- Consumes: `JKAgentClient::Query` (notify 등 서버 도구), jkchat의 claude 래퍼 관례(state/chat.json + `ollama launch claude --model <m> -- -p <q>`)
- Produces: `jkctl.exe notify "<msg>"`, `jkctl agent '<json>'`, `jkctl ask "<질문>"` — Task 4 샘플/lfrc가 호출

- [ ] **Step 1: main.cpp 작성**

```cpp
// jkctl — P4 SDK 앱→에이전트 원컷 CLI (specs/2026-09-16-p4-sdk-contract §4).
// notify/agent: 서버 도구 쿼리(JKAgentClient, 권한은 서버 파이프라인).
// ask: 로컬 LLM CLI 호출(jkchat의 claude 래퍼 관례, chat.json 설정 재사용).
#include <agent/JKAgentClient.h>

#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::string ExeDirA() {
    char path[1024] = {};
#ifdef _WIN32
    GetModuleFileNameA(nullptr, path, sizeof(path));
    const std::string full(path);
    return full.substr(0, full.find_last_of("\\/") + 1);
#else
    return "./";
#endif
}

// chat.json에서 model 읽기 (jkchat LoadChatConfig의 최소형 — 기본값 폴백).
std::string LoadModel() {
    std::string model = "glm-5.3-flash:cloud";
    std::FILE* f = std::fopen((ExeDirA() + "..\\state\\chat.json").c_str(), "rb");
    if (!f) return model;
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    const char* key = std::strstr(buf, "\"model\"");
    if (key && (key = std::strchr(key + 7, '"'))) {
        const char* end = std::strchr(key + 1, '"');
        if (end) model.assign(key + 1, static_cast<size_t>(end - key - 1));
    }
    return model;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: jkctl notify \"<msg>\" | agent '<json>' | ask \"<question>\"\n");
        return 2;
    }
    const std::string sub = argv[1];

    if (sub == "notify" && argc >= 3) {
        jk::agent::JKAgentClient c;
        if (!c.Connect()) { std::fprintf(stderr, "jkctl: server not running\n"); return 1; }
        std::string reply;
        const std::string args = "{\"text\":\"" + std::string(argv[2]) + "\"}";
        // 서버 notify 도구의 인자 이름은 기존 open_notify/notify 도구 스키마 참조
        return c.Query("notify", args, /*out*/*(new std::string)) ? 0 : 1;
    }
    if (sub == "agent" && argc >= 3) {
        jk::agent::JKAgentClient c;
        if (!c.Connect()) { std::fprintf(stderr, "jkctl: server not running\n"); return 1; }
        std::string reply;
        if (!c.QueryRaw(argv[2], reply)) return 1;
        std::printf("%s\n", reply.c_str());
        return 0;
    }
    if (sub == "ask" && argc >= 3) {
        // 로컬 LLM 원컷 — 응답을 stdout으로 통과한다 (동기 원컷, §4).
        std::string cmd = "ollama launch claude --model \"" + LoadModel() +
                          "\" -- -p \"" + std::string(argv[2]) + "\"";
        return std::system(cmd.c_str()) == 0 ? 0 : 1;
    }
    std::fprintf(stderr, "unknown subcommand: %s\n", sub.c_str());
    return 2;
}
```

(주의: `notify` 분기의 `*(new std::string)` 자리는 실제 지역변수 `std::string reply;`로 고쳐 씀 — 위 스케치의 오타. 서버 notify 도구의 실제 인자 스키마는 구현 시 JKWindowServer의 notify 도구 블록에서 확인해 그대로 맞춘다.)

- [ ] **Step 2: CMake 타깃**

`engine/CMakeLists.txt`에서 `jkchat` 타깃 블록을 찾아 동일 패턴으로 `jkctl` 추가 (sources `tools/jkctl/main.cpp`, 링크 `jkagent` 클라 스택 — jkchat이 링크하는 것과 동일, `set_target_properties(... PREFIX "")` 포함).

- [ ] **Step 3: 빌드 + 검증**

Run: 빌드 → `jkwinserver.exe` 기동 상태에서
Run: `engine/build/jkctl.exe notify "SDK e2e"` → 데스크탑 알림 확인
Run: `engine/build/jkctl.exe agent '{"tool":"ping","args":{}}'` → JSON 응답 stdout
Run: `engine/build/jkctl.exe ask "안녕을 한 문장으로 설명해줘"` → LLM 응답 stdout

- [ ] **Step 4: Commit**

```bash
git add engine/tools/jkctl/main.cpp engine/CMakeLists.txt
git commit -m "feat(sdk): jkctl CLI — console apps call the agent (P4 SDK §4)"
```

---

### Task 4: 템플릿 + 샘플 앱 (C 씨앗)

**Files:**
- Create: `engine/templates/console-app/manifest.json`
- Create: `engine/templates/console-app/README.md`
- Create: `engine/apps/sampletodo/manifest.json`
- Create: `engine/apps/sampletodo/sampletodo.cmd`
- Modify: `engine/build_with_temp.sh` (apps 디렉터리 동기화 추가)

**Interfaces:**
- Consumes: Task 3의 `jkctl.exe`, Task 1의 매니페스트 스캔
- Produces: 개발자 시작점 템플릿 + 살아있는 SDK 예제

- [ ] **Step 1: 템플릿**

`engine/templates/console-app/manifest.json`:

```json
{
  "name": "myapp",
  "cmd": "python myapp.py",
  "desc": "설명 한 줄 (선택)"
}
```

`engine/templates/console-app/README.md`:

```markdown
# 콘솔 앱 템플릿 (P4 SDK)

이 폴더를 복사해 `engine/build/apps/<name>/`에 넣으면 런처에 앱으로 등록된다.

- `manifest.json`의 `name`은 폴더명과 같게, `cmd`는 터미널에서 실행할 명령줄
  (앱 폴더가 작업 디렉토리 — 상대경로 사용)
- 콘솔 앱 본체는 아무 언어든 가능 (exe, py, cmd, …)
- 에이전트 호출: `<엔진 폴더>\jkctl.exe notify "메시지"` / `ask "질문"`
- 아이콘(선택): `icon@1x.png`(16px), `icon@2x.png`(32px) — 없으면 placeholder
- 스폰 cmd는 설치 시 SHA-256 지문으로 state/trust.json에 기록된다 (docs/37 스킴)
```

- [ ] **Step 2: 샘플 앱**

`engine/apps/sampletodo/sampletodo.cmd`:

```bat
@echo off
REM sampletodo — P4 SDK 샘플 콘솔 앱 (specs/2026-09-16-p4-sdk-contract §6).
REM 할 일 파일을 보여주고, 인자가 있으면 jkctl로 에이전트에 전달한다.
if "%~1"=="" goto :list
"<엔진빌드절대경로는 금지 — 상대경로>" 2>nul
set HERE=%~dp0
call "%HERE%..\..\jkctl.exe" ask "할 일 목록을 검토하고 우선순위를 정해줘: %~1"
goto :eof
:list
type "%~dptodo.txt" 2>nul || echo (할 일이 없다 - todo.txt에 한 줄씩 적는다)
```

`engine/apps/sampletodo/manifest.json`:

```json
{
  "name": "sampletodo",
  "cmd": "sampletodo.cmd",
  "desc": "SDK 샘플 — 할 일 뷰어 + jkctl ask"
}
```

(참고: sampletodo.cmd는 자기 폴더에서 상대적으로 `..\..\jkctl.exe`를 찾는다 —
apps/<name>이 engine/build/apps 아래 2단계이므로 정확하다. 위 스케치의 이상한
한 줄은 정리해서 커밋할 것.)

- [ ] **Step 3: 빌드 스크립트에 apps 동기화**

`engine/build_with_temp.sh`의 assets 복사 근처에:

```bash
# 콘솔 앱 폴더(kind) 동기화 (P4 SDK): apps/<dir>/manifest.json 앱을
# 실행 폴더로 복사 — .jkx 패커 산출물과 같은 자리.
if [ -d "$SRC/apps" ]; then
    for d in "$SRC"/apps/*/; do
        [ -d "$d" ] && cp -R "$d" "$SRC/build/apps/"
    done
fi
```

- [ ] **Step 4: 빌드 + e2e + Commit**

빌드 → 서버 기동 → 런처에 sampletodo 셀 확인 → 클릭 → 터미널에서 todo 안내 출력 확인 → `sampletodo.cmd review`로 jkctl ask 경로 확인.

```bash
git add engine/templates/console-app engine/apps/sampletodo engine/build_with_temp.sh
git commit -m "feat(sdk): console-app template + sampletodo sample (P4 SDK §6)"
```

---

### Task 5: lfrc jkctl 교체 + docs/51 as-built

**Files:**
- Modify: `%APPDATA%\lf\lfrc` (git 밖)
- Create: `docs/51_p4_sdk_contract.md`

**Interfaces:**
- Consumes: Task 3의 jkctl.exe — lfrc `A`가 임시 LLM 직접 호출(docs/44)을 갈아탐

- [ ] **Step 1: lfrc 갈아타기**

`%APPDATA%\lf\lfrc`의 `cmd agent` 블록을:

```
cmd agent &{{
    "I:\progwork\JKENGINE\engine\build\jkctl.exe" ask "선택한 파일/디렉토리를 검토하고 한국어로 요약해줘: %fx%"
}}
```

- [ ] **Step 2: 프로브 갱신**

`engine/tools/probes/probe_lf_ops.ps1` — 기존 PASS 항목 유지(jkctl 경로는 절대경로라 프로브가 파일 존재만 확인):

```powershell
$jkctl = "I:\progwork\JKENGINE\engine\build\jkctl.exe"
if (-not (Test-Path $jkctl)) { Write-Host "FAIL: jkctl.exe missing"; exit 1 }
```

- [ ] **Step 3: docs/51 as-built**

`docs/51_p4_sdk_contract.md` — 스펙 §2~§7 as-built + 층위 2(DLL)/3(.jkx) 문서화 섹션:

```markdown
# 45 — P4 SDK 계약 as-built (콘솔 앱 + 에이전트 쌍방)

- 날짜: 2026-09-16
- 스펙: docs/superpowers/specs/2026-09-16-p4-sdk-contract-design.md

## As-built
<!-- 태스크별 실제 결과, 계획과 다른 점. -->

## DLL 계약 (문서화 — 현재 동작이 계약)
<!-- jkapp_<name>.dll 모듈 계약: JKAPP_MODULE_BUILD, ShellHost 스폰(--client
     <name>), 클라 meta 등. docs/43 §5 참조. 코드 변경 없음 — 사실 기술. -->

## .jkx 계약 (문서화 — 기존)
<!-- docs/21 컨테이너 + docs/37 trust. 이번 변경 없음. -->

## C 후보 (풀 SDK로 가는 다음 단계)
<!-- 템플릿 생성기(jkctl init), 패키지 매니저, 샘플 라인업. 이번에 심은 씨앗:
     engine/templates/console-app, sampletodo. -->
```

- [ ] **Step 4: Commit**

```bash
git add docs/51_p4_sdk_contract.md engine/tools/probes/probe_lf_ops.ps1
git commit -m "docs(45): P4 SDK as-built + DLL/.jkx contract docs + C candidates"
```

---

## Self-Review 결과

- **스펙 커버리지**: §3 콘솔 kind → Task 1, §3.4 지문 → Task 1b, §4 jkctl → Task 3, §5 도구 → Task 2, §6 씨앗 → Task 4, lf 교체+문서 → Task 5. §7 검증 → 각 태스크 Step. 커버됨.
- **플레이스홀더**: 스케치의 3곳("동일 구현 복제", close_window Ask 본문, notify 인자)은 **기존 검증 코드의 정확한 복제 지점을 파일:행으로 명시**한 것 — 새 설계 TBD가 아니라 기존 코드 재사용 지시다. Task 4의 sampletodo.cmd 스케치 오류는 Step 2에 "정리해서 커밋"으로 명시했다 — 실행자가 5분 안에 다듬을 크기.
- **타입 일관성**: `ConsoleAppInfo(name→cmd,dir,fingerprint)`와 `spawnConsole(cmd,cwd,name)` 시그니처가 Task 1 정의와 Task 2 소비에서 동일. trust 지문 형식 "sha256:"+64hex 전 태스크 동일.
- **빌드 간섭**: 각 태스크 검증에 서버 기동이 필요 — 실행자는 빌드 전 `taskkill //IM jkwinserver.exe //F` 습관화 (파일 잠금 레슨).