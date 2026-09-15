# 에이전트 관리자 앱 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 스펙 §7 "에이전트 관리자" — 4탭 ImGui 앱(권한 매트릭스/트리거/신뢰/설치+receipts) + 서버 도구 5종 + 브로커/jkchat/팔레트 연결.

**Architecture:** 정규 클라 앱 `jkapp_agentmgr.dll`(notify 템플릿)이 자기 창 연결로 `SendAgentQuery`/`PollAgentReply`(팔레트 선례). 서버는 HandleAgentQuery에 도구 5종 신설 — 쓰기 2종(`permission_set`/`trust_revoke`)은 기존 `pendingApprovals_` 파킹+`approve` 기계 재사용. 브로커는 kToolsListJson/게이트 기본값만 갱신.

**Tech Stack:** C++17 MinGW (msys2 ucrt64), ImGui 1.92 (`imgui` 타깃), JKAgentJson(quickjs 2레벨 리더), CMake ninja, PowerShell 5.1 프로브.

**Spec:** `docs/superpowers/specs/2026-09-16-agent-manager-design.md` (플랜은 스펙과 함께 읽는다)

**Spec deltas (구현 결정, docs 태스크에서 스펙에 반영):**
- §2.1의 `"file":null` → `"file":""` (빈 문자열 = 오버라이드 없음 — AgentJson 2레벨 리더가 null을 못 읽는다, docs/38 선례).
- §2.3 trust_revoke 전제로 `trust_list` 응답에 전체 지문 필드 `"fp"` 신설 (기존 15자 절단 `fingerprint`는 표시용으로 유지 — 해지는 전체 지문 필요).
- §1.1 설치 탭의 "실행 중" 열 = `list_windows` 타이틀에 앱 이름이 포함되는 완화 매칭 (타이틀↔앱 이름은 1:1이 아님).
- §2.5 read_receipts rows: `ok`는 `"0"/"1"` 문자열 + `ts`는 epoch **초** (AgentJson 2레벨 리더 — bool 접근자 없음, GetArrInt 경유).
- §1.1 트리거 탭의 topics 열 포기 (2레벨 리더가 3레벨 배열을 못 읽음) — name/enabled만 표시.

## Global Constraints

- 빌드: `cd I:/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin cmake --build . --target <T>`
- **레슨 57**: 앱 DLL 신규 추가 시 해당 타겟을 최소 1회 빌드(`--target jkapp_agentmgr`), 안 하면 형제 타겟 빌드가 링크를 건너뜀.
- **레슨 18**: DLL 재빌드 후 `--target jkx_packages` 재팩 필수 — 클라는 .jkx에서 추출해 실행.
- **레슨 35**: HandleAgentQuery는 clientsMutex_ 보유 중 — 도구 구현 내부에서 클라 락 재획득 금지.
- **레슨 37**: 빌드 출력을 grep으로 필터하지 말 것; exe/dll/jkx mtime > 최신 소스 mtime 확인.
- **레슨 27/50**: PowerShell 스크립트/프로브는 Write 도구로 파일 생성, 본문 ASCII-only (BOM 의존 제거).
- **레슨 61**: 임시 계측 → 클린바이너리 공식런 3단. 공식 판정은 최종 프로브.
- **레슨 33**: 프로브는 프로세스 종료 전 트랜스크립트/파일을 먼저 읽는다.
- 최종리뷰: 최강 모델(opus) — 마지막 태스크에서 전체-브랜치 리뷰.
- 커밋 트레일러: `Co-Authored-By: Claude Code <noreply@anthropic.com>`

---

### Task 1: 서버 읽기 도구 3종 (agent_permissions / installed_list / read_receipts)

**Files:**
- Modify: `engine/src/desktop/JKDesktopShell.h` (ListInstalled 선언)
- Modify: `engine/src/desktop/JKDesktopShell.cpp` (ListInstalled 구현)
- Modify: `engine/src/server/JKWindowServer.cpp` (HandleAgentQuery else-if 체인 — `events_list` 블록 뒤, `launch_chat` 앞에 3분기 삽입; 파일 스코프에 kPermMatrix 정적 테이블)

**Interfaces:**
- Consumes: `JKDesktopShell::launcherIcons_` (기존 private 멤버), `StateDir()`, `AgentJson` (`ok/GetStr/GetArraySize/GetArrStr/GetObjInt`), `JsonEsc`.
- Produces: 서버 도구 `agent_permissions`(응답 `{"ok":true,"perms":[{"tool","gate","file","effective","default"}]}`), `installed_list`(`{"ok":true,"installed":[{"name","kind"}]}`), `read_receipts`(`{"ok":true,"rows":[{"ts","tool","ok"}]}`). `kPermMatrix` 정적 테이블 — Task 2/3이 재사용(파일 RMW 행 나열). `JKDesktopShell::ListInstalled(std::vector<std::pair<std::string, const char*>>&)`.

- [ ] **Step 1: JKDesktopShell에 ListInstalled 추가**

`engine/src/desktop/JKDesktopShell.h` — `ConsoleAppInfo` 선언 바로 아래:
```cpp
    // 에이전트 관리자 (specs/2026-09-16-agent-manager §2.4): 설치 앱의
    // 이름/kind 열람. kind = "console"(consoleCmd 비지 않음) | "jkx" |
    // "builtin". cmd/지문은 run_console_app 승인 경로의 관심사 — 최소 노출.
    void ListInstalled(
        std::vector<std::pair<std::string, const char*>>& out) const;
```
헤더에 `#include <utility>` 없으면 추가(벡터 pair).

`engine/src/desktop/JKDesktopShell.cpp` — `ConsoleAppInfo` 구현 뒤:
```cpp
// 에이전트 관리자 (스펙 §2.4): 런처 스캔 결과의 이름/kind만 돌려준다.
void JKDesktopShell::ListInstalled(
        std::vector<std::pair<std::string, const char*>>& out) const {
    for (const LauncherIcon& icon : launcherIcons_) {
        const char* kind = !icon.consoleCmd.empty()
            ? "console"
            : (!icon.jkxPath.empty() ? "jkx" : "builtin");
        out.emplace_back(icon.appName, kind);
    }
}
```

- [ ] **Step 2: kPermMatrix + 도구 3종 구현 (JKWindowServer.cpp)**

`HandleAgentQuery` 정의 바로 앞에 파일 스코프 정적 테이블 (Task 2/3의 RMW가 재사용 — 행 추가는 여기만):
```cpp
// 권한 매트릭스의 행 — 게이트 소비처별 정직 표기 (스펙 §2.1). 서버는
// AgentToolAllowed를 3종(+신규 2종)에서만 검사하고 브로커 bool 맵이 MCP
// 경로만 걸러낸다. "none" 행의 파일값은 서버 경로에서 무력.
struct AgentPermRow { const char* tool; const char* gate; const char* deflt; };
static const AgentPermRow kPermMatrix[] = {
    {"close_window", "server", "deny"},
    {"trust_request", "server", "ask"},
    {"run_console_app", "server", "ask"},
    {"trust_revoke", "server", "ask"},
    {"permission_set", "server(fixed)", "ask"},
    {"read_log", "broker", "allow"},
    {"read_events", "broker", "allow"},
    {"terminal_exec", "broker", "allow"},
    {"list_windows", "none", "allow"},
    {"focus_window", "none", "allow"},
    {"launch_app", "none", "allow"},
    {"save_layout", "none", "allow"},
    {"restore_layout", "none", "allow"},
    {"publish_event", "none", "allow"},
    {"capture_window", "none", "allow"},
    {"capture_region", "none", "allow"},
    {"trigger_toggle", "none", "allow"},
    {"theme_set", "none", "allow"},
    {"open_notify", "none", "allow"},
    {"launch_chat", "none", "allow"},
    {"approve", "none", "allow"},
    {"file_open", "none", "allow"},
    {"file_dialog_params", "none", "allow"},
    {"agent_permissions", "none", "allow"},
    {"installed_list", "none", "allow"},
    {"read_receipts", "none", "allow"},
};
```

`HandleAgentQuery` 체인의 `events_list` 블록 뒤(else-if 연쇄 안, `launch_chat` 앞)에 3분기:
```cpp
    } else if (tool == "agent_permissions") {
        // 스펙 §2.1: permissions.json + 기본값 병합. gate 뱃지로 게이트
        // 소비처를 정직 표기 — "none" 행의 파일값은 서버 무력(브로커만).
        // file은 "" = 오버라이드 없음 (AgentJson이 null을 못 읽는다).
        char exePath[1024] = {};
        GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
        std::string dir = exePath;
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir = dir.substr(0, slash);
        const std::string permPath = dir + "\\permissions.json";
        jk::agent::AgentJson perm("{}");
        bool fileExists = false;
        if (std::FILE* f = std::fopen(permPath.c_str(), "rb")) {
            fileExists = true;
            char pbuf[4096] = {};
            const size_t pn = std::fread(pbuf, 1, sizeof(pbuf) - 1, f);
            std::fclose(f);
            perm = jk::agent::AgentJson(pbuf);
        }
        if (fileExists && !perm.ok()) {
            // 파일 없음(기본값)과 파싱 실패(수동 편집 실수)를 구분 —
            // trust_list의 docs/38 선례.
            reply = "{\"ok\":false,\"error\":\"permissions_unreadable\"}";
        } else {
            std::string out = "{\"ok\":true,\"perms\":[";
            bool first = true;
            for (const AgentPermRow& row : kPermMatrix) {
                std::string fileVal;
                const bool hasFile = perm.ok() &&
                    perm.GetStr(row.tool, fileVal) &&
                    (fileVal == "allow" || fileVal == "ask" ||
                     fileVal == "deny");
                std::string effective = row.deflt;
                if (std::string(row.gate) == "server(fixed)") {
                    effective = "ask";
                } else if (std::string(row.gate) == "server") {
                    if (hasFile) effective = fileVal;
                } else {
                    effective = "allow";   // broker/none — 서버 미게이트
                }
                if (!first) out += ",";
                first = false;
                out += "{\"tool\":\"" + std::string(row.tool) +
                       "\",\"gate\":\"" + row.gate + "\",\"file\":\"" +
                       (hasFile ? fileVal : std::string()) +
                       "\",\"effective\":\"" + effective +
                       "\",\"default\":\"" + row.deflt + "\"}";
            }
            reply = out + "]}";
        }
    } else if (tool == "installed_list") {
        // 스펙 §2.4: 셸 런처 스캔의 이름/kind. shell_ 미기동 = 빈 배열(정상).
        std::vector<std::pair<std::string, const char*>> rows;
        if (shell_) shell_->ListInstalled(rows);
        std::string out = "{\"ok\":true,\"installed\":[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ",";
            out += "{\"name\":\"" + JsonEsc(rows[i].first) +
                   "\",\"kind\":\"" + rows[i].second + "\"}";
        }
        reply = out + "]}";
    } else if (tool == "read_receipts") {
        // 스펙 §2.5: 브로커 receipts.jsonl 꼬리 — ts/tool/ok만 반환
        // (result 전문은 args에 경로/명령어가 실릴 수 있다).
        int limit = 50;
        req.GetObjInt("args", "limit", limit);
        if (limit <= 0) limit = 50;
        if (limit > 200) limit = 200;
        const std::string path = StateDir() + "\\receipts.jsonl";
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            reply = "{\"ok\":true,\"rows\":[]}";   // 브로커 미사용 = 정상
        } else {
            std::fseek(f, 0, SEEK_END);
            const long size = std::ftell(f);
            const long start = size > 262144 ? size - 262144 : 0;
            std::fseek(f, start, SEEK_SET);
            std::vector<char> buf(static_cast<size_t>(size - start) + 1);
            const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
            std::fclose(f);
            buf[n] = '\0';
            std::vector<std::string> lines;
            size_t pos = 0;
            while (pos < n) {
                const char* begin = buf.data() + pos;
                const char* nl = static_cast<const char*>(
                    std::memchr(begin, '\n', n - pos));
                const size_t len = nl ? (size_t)(nl - begin) : (n - pos);
                if (len > 0) lines.push_back(std::string(begin, len));
                pos += len + (nl ? 1 : 0);
            }
            std::string out = "{\"ok\":true,\"rows\":[";
            int used = 0;
            for (size_t i = lines.size(); i-- > 0 && used < limit;) {
                const std::string& line = lines[i];
                // JSONL 행의 result 중첩은 2레벨 리더의 관심사가 아니다 —
                // ts/tool은 행 파서(AgentJson) + ts는 raw 스캔(숫자 필드),
                // ok는 result 내 raw 스캔.
                jk::agent::AgentJson row(line.c_str());
                std::string toolName;
                if (!row.ok() || !row.GetStr("tool", toolName)) continue;
                long long ts = 0;
                {
                    const size_t tp = line.find("\"ts\":");
                    if (tp != std::string::npos) {
                        ts = std::atoll(line.c_str() + tp + 5);
                    }
                }
                const bool okFlag =
                    line.find("\"result\":") != std::string::npos &&
                    line.find("\"ok\":true") != std::string::npos;
                if (used) out += ",";
                // 직렬화 규약(파서 계약): ts = epoch 초(2레벨 GetArrInt 경유,
                // ms → 초 절단), ok = "0"/"1" 문자열(리더가 bool을 못 읽는다).
                out += "{\"ts\":" + std::to_string(ts / 1000) +
                       ",\"tool\":\"" + JsonEsc(toolName) +
                       "\",\"ok\":\"" + (okFlag ? "1" : "0") + "\"}";
                ++used;
            }
            reply = out + "]}";
        }
    }
```

- [ ] **Step 3: capture_window 주석 정정 (스펙 §2.1 부수 발견)**

`JKWindowServer.cpp:2087-2090` 근처 capture_window 도구의 "permissions.json can deny" 주석은 실제 게이트 체크가 없는 부정확한 주석 — 게이트 추가는 범위 밖이므로 **주석만** 실제로 고친다:
```cpp
        // Note: permissions.json does NOT gate this tool server-side (only
        // close_window/trust_request/run_console_app are checked via
        // AgentToolAllowed). The broker's LoadPermissions bool map filters
        // MCP-agent calls; agent_permissions reports this row as gate
        // "none". Kept ungated — see specs/2026-09-16-agent-manager §2.1.
```
(기존 주석 문장과 교체 — 코드 변경 없음.)

- [ ] **Step 4: 빌드**

```bash
cd I:/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin cmake --build . --target jkdesktop
```
Expected: 0 errors. (레슨 37: 빌드 출력 통으로 확인, jkdesktop.exe mtime > JKWindowServer.cpp mtime.)

- [ ] **Step 5: 라이브 체크 (tmp 스크립트, 커밋 안 함)**

Write 도구로 `engine/tools/probes/mgr_t1_read.ps1` (ASCII-only):
```powershell
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item (Join-Path (Split-Path $exe) "permissions.json") -ErrorAction SilentlyContinue
Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
$p = Invoke-Agentctl '{"tool":"agent_permissions","args":{}}'
if ($p -match '"close_window","gate\\?":"server","file\\?":"","effective\\?":"deny"' -and `
    $p -match '"permission_set","gate\\?":"server\(fixed\)","file\\?":"","effective\\?":"ask"') {
    Write-Host "t1-perms: PASS"
} else { Write-Host "t1-perms: FAIL $p" }
$i = Invoke-Agentctl '{"tool":"installed_list","args":{}}'
if ($i -match 'sampletodo' -and $i -match '"kind\\?":"console"' -and `
    $i -match '"minesweeper"' -and $i -match '"kind\\?":"jkx"') {
    Write-Host "t1-installed: PASS"
} else { Write-Host "t1-installed: FAIL $i" }
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
```
Run: `powershell -ExecutionPolicy Bypass -File I:\progwork\JKENGINE\engine\tools\probes\mgr_t1_read.ps1`
Expected: `t1-perms: PASS` + `t1-installed: PASS` (sampletodo/minesweeper는 셸 스캔이 이미 설치돼 있다 — 없으면 스킵 사유를 기록하고 FAIL로 두지 않는다).

- [ ] **Step 6: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/src/desktop/JKDesktopShell.h engine/src/desktop/JKDesktopShell.cpp engine/src/server/JKWindowServer.cpp
git commit -m "feat(agentmgr): read tools - agent_permissions/installed_list/read_receipts (spec 2.1/2.4/2.5)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 2: permission_set — 고정 Ask 게이트 + 파킹 + 승인 시 RMW 쓰기

**Files:**
- Modify: `engine/include/server/JKWindowServer.h:187-201` (PendingApproval 신규 필드)
- Modify: `engine/src/server/JKWindowServer.cpp` (AgentToolAllowed 고정 분기 + askCapable 확장, HandleAgentQuery 신설 분기, approve kind 분기, 파일 스코프 helper `WritePermissionsEntry`)

**Interfaces:**
- Consumes: Task 1의 `kPermMatrix`, 기존 `AgentToolAllowed`/`PendingApproval`/`approve`/만료 스캔(변경 불필요 — permission_set은 file_open이 아니므로 `approval_timeout` 기본 분기).
- Produces: 도구 `permission_set {tool, decision}` — 파킹 시 요청 이벤트 `{"topic":"agent.approval_request","request":N,"tool":"permission_set","kind":"permission_set","target_tool":T,"decision":D,"ts":...}`, 해소 시 요청자 reply `{"ok":true,"written":true}` 또는 `{"ok":false,"error":"write_failed"}`. PendingApproval 신규 필드 `permTool`/`permDecision` (Task 5 jkchat이 `target_tool`/`decision` 페이로드를 읽는다).

- [ ] **Step 1: PendingApproval 필드 추가**

`engine/include/server/JKWindowServer.h` — `PendingApproval` 구조체의 `fingerprint` 필드 뒤:
```cpp
        // 매니저 권한 (specs/2026-09-16-agent-manager §2.2): permission_set의
        // 대상 도구와 결정 — kind 전용 페이로드(name 재용용 금지).
        std::string permTool;      // permission_set: 대상 도구
        std::string permDecision;  // permission_set: "allow"|"ask"|"deny"
```

- [ ] **Step 2: AgentToolAllowed — permission_set 고정 Ask**

`engine/src/server/JKWindowServer.cpp:2745` 함수 첫 줄(exePath 계산 전):
```cpp
AgentDecision JKWindowServer::AgentToolAllowed(const std::string& tool) const {
    // permission_set은 파일 값을 무시하고 항상 Ask — 파일로 이 도구를
    // allow로 바꿔두면 이후 모든 권한 변경이 무승인이 되는 2단 우회 봉쇄
    // (스펙 §2.2 핵심 안전 결정).
    if (tool == "permission_set") return AgentDecision::Ask;
    char exePath[1024] = {};
    // ... 이하 기존 코드 그대로, 다만 askCapable과 defaultDecision에
    // trust_revoke를 추가한다 (Task 3 전제 — 이 태스크에서 같이 넣는다):
```
같은 함수의 (a) 주석의 ask 목록 갱신, (b):
```cpp
    const bool askCapable = (tool == "close_window" || tool == "trust_request" ||
                             tool == "run_console_app" || tool == "trust_revoke");
    auto defaultDecision = [&]() -> AgentDecision {
        if (tool == "close_window") return AgentDecision::Deny;
        if (tool == "trust_request") return AgentDecision::Ask;
        if (tool == "run_console_app") return AgentDecision::Ask;
        if (tool == "trust_revoke") return AgentDecision::Ask;
        return AgentDecision::Allow;
    };
```
함수 선두 주석("wired for close_window + trust_request; other tools...")도 새 목록으로 갱신: `close_window + trust_request + run_console_app + trust_revoke; permission_set is hardwired Ask regardless of the file`.

- [ ] **Step 3: WritePermissionsEntry helper (파일 스코프, kPermMatrix 뒤)**

```cpp
// permissions.json RMW (스펙 §2.2): 알려진 도구 키 전부 명시 기록 — 없던 키는
// 현재 기본값으로 채워 다음 편집자가 기본값을 온전히 본다. permission_set 행은
// 기록하지 않는다(파일값 무시 게이트). 반환: 빈 문자열 = 성공, 아니면
// write_failed. kPermMatrix는 gate "server" 행의 기본값에도 쓰인다.
static std::string WritePermissionsEntry(const std::string& permTool,
                                         const std::string& decision) {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    const std::string path = dir + "\\permissions.json";

    std::map<std::string, std::string> values;
    if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
        char buf[4096] = {};
        const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
        std::fclose(f);
        jk::agent::AgentJson json(buf);
        std::string v;
        if (json.ok()) {
            for (const AgentPermRow& r : kPermMatrix) {
                if (json.GetStr(r.tool, v) &&
                    (v == "allow" || v == "ask" || v == "deny")) {
                    values[r.tool] = v;
                }
            }
        }
    }
    values[permTool] = decision;
    std::string out = "{";
    bool first = true;
    for (const AgentPermRow& r : kPermMatrix) {
        if (std::string(r.gate) == "server(fixed)") continue;
        if (!first) out += ",";
        first = false;
        const auto it = values.find(r.tool);
        out += "\"" + std::string(r.tool) + "\":\"" +
               (it != values.end() ? it->second : std::string(r.deflt)) + "\"";
    }
    out += "}";
    std::FILE* w = std::fopen(path.c_str(), "wb");
    if (!w) return "write_failed";
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), w);
    std::fclose(w);
    return wrote == out.size() ? std::string() : std::string("write_failed");
}
```

- [ ] **Step 4: HandleAgentQuery에 permission_set 분기 (trust_request 블록 뒤)**

```cpp
    } else if (tool == "permission_set") {
        // 스펙 §2.2: 매트릭스 쓰기 — AgentToolAllowed("permission_set")은
        // 파일값 무시하고 항상 Ask(하드코딩, §2.2 2단 우회 봉쇄). 검증은
        // 파킹 전(trust_request의 bad_* 선례).
        std::string permTool, decision;
        req.GetObjStr("args", "tool", permTool);
        req.GetObjStr("args", "decision", decision);
        bool known = false;
        for (const AgentPermRow& r : kPermMatrix) {
            if (permTool == r.tool) { known = true; break; }
        }
        if (permTool.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_tool\"}";
        } else if (!known) {
            reply = "{\"ok\":false,\"error\":\"unknown_tool\"}";
        } else if (decision != "allow" && decision != "ask" &&
                   decision != "deny") {
            reply = "{\"ok\":false,\"error\":\"bad_decision\"}";
        } else {
            switch (AgentToolAllowed("permission_set")) {
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
                        reply = "{\"ok\":false,"
                                "\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "permission_set";
                    p.permTool = permTool;
                    p.permDecision = decision;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"permission_set\","
                                  "\"kind\":\"permission_set\","
                                  "\"target_tool\":\"%s\","
                                  "\"decision\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(permTool).c_str(),
                                  decision.c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;   // 해소 시 답신
                    break;
                }
                case AgentDecision::Allow:
                case AgentDecision::Deny:
                default:
                    // 도달하지 않는다(고정 Ask) — 방어선으로 deny 유지.
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
```

- [ ] **Step 5: approve 분기에 kind 페이로드**

`approve` 도구의 요청자 reply 계산(`{"ok":true}` 상수)을 kind별로 교체 — 기존:
```cpp
            for (auto& c : clients_) {
                if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
                    const std::string result = allow
                        ? "{\"ok\":true}"
                        : "{\"ok\":false,\"error\":\"denied_by_user\"}";
                    ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                        it->queryId, allow ? 1 : 0, result);
                    break;
                }
            }
```
를:
```cpp
            for (auto& c : clients_) {
                if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
                    std::string result;
                    if (!allow) {
                        result = "{\"ok\":false,\"error\":\"denied_by_user\"}";
                    } else if (it->kind == "permission_set") {
                        // 쓰기 실패는 요청자 reply에 표면화 (docs/52 선례).
                        const std::string err = WritePermissionsEntry(
                            it->permTool, it->permDecision);
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else {
                        result = "{\"ok\":true}";
                    }
                    const int flag = (result.find("\"ok\":true") !=
                                      std::string::npos)
                                         ? 1 : 0;
                    ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                        it->queryId, flag, result);
                    break;
                }
            }
```
(Task 3가 같은 루프에 trust_revoke 분기를 추가한다 — this task's change keeps else-if extendable.)

- [ ] **Step 6: 빌드 + 라이브 체크**

```bash
cd I:/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin cmake --build . --target jkdesktop
```
Write 도구로 `engine/tools/probes/mgr_t2_permset.ps1` (ASCII-only) — 파킹 E2E. 승인 해소는 `approve` 도구(agentctl)이고 이벤트 푸시는 리플레이되지 않으므로(레슨 28) **구독자(jktriggers)를 먼저 띄운다**:
```powershell
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$root = Split-Path $exe
$permFile = Join-Path $root "permissions.json"
if (Test-Path $permFile) { Write-Host "t2: ABORT - permissions.json exists"; exit 1 }
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 3
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) -WindowStyle Hidden
Start-Sleep -Seconds 3
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
# 파킹된 쿼리는 agentctl이 블록 — 별도 잡으로
$job = Start-Job -ScriptBlock {
    param($exe)
    $escaped = '{"tool":"permission_set","args":{"tool":"close_window","decision":"allow"}}' -replace '"', '\"'
    & $exe agentctl $escaped
} -ArgumentList $exe
Start-Sleep -Seconds 3
# 승인 (request 1 = 서버 기동 후 첫 파킹; nextApprovalId_=1)
$ap = Invoke-Agentctl '{"tool":"approve","args":{"request":1,"decision":"allow"}}'
Start-Sleep -Seconds 2
$reply = Receive-Job $job -Wait
if ($reply -match '"written\\?":true') { Write-Host "t2-reply: PASS" }
else { Write-Host "t2-reply: FAIL $reply" }
$file = Get-Content $permFile -Raw -ErrorAction SilentlyContinue
if ($file -match '"close_window"\\?:"allow"' -and $file -match '"trust_request"') {
    Write-Host "t2-file: PASS"
} else { Write-Host "t2-file: FAIL $file" }
# 결정 검증 파킹 없이: unknown_tool / bad_decision
$bad = Invoke-Agentctl '{"tool":"permission_set","args":{"tool":"nope","decision":"allow"}}'
if ($bad -match 'unknown_tool') { Write-Host "t2-unknown: PASS" } else { Write-Host "t2-unknown: FAIL $bad" }
$bad2 = Invoke-Agentctl '{"tool":"permission_set","args":{"tool":"close_window","decision":"maybe"}}'
if ($bad2 -match 'bad_decision') { Write-Host "t2-baddec: PASS" } else { Write-Host "t2-baddec: FAIL $bad2" }
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $permFile -ErrorAction SilentlyContinue
```
주의: request id는 서버 기동 후 첫 파킹이 1(nextApprovalId_=1) — approve 요청 본문은 `{"request":1,"decision":"allow"}` (인자 순서 무관, 키-값). 스크립트의 `request` 키 순서는 위 그대로 허용된다(서버는 GetObjInt("request")).
Run: `powershell -ExecutionPolicy Bypass -File I:\progwork\JKENGINE\engine\tools\probes\mgr_t2_permset.ps1`
Expected: `t2-reply: PASS` + `t2-file: PASS` + `t2-unknown: PASS` + `t2-baddec: PASS`.

- [ ] **Step 7: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/include/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp
git commit -m "feat(agentmgr): permission_set with fixed-ask gate + approval pipeline write (spec 2.2)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 3: trust_revoke — 파킹 + 승인 시 trust.json RMW(.bak 1회) + trust_list 전체 지문 필드

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (HandleAgentQuery — trust_request 블록의 지역 람다 `ValidFingerprint`를 파일 스코프로 추출; 신설 분기; helper `TrustRecordInfo`/`RevokeTrustRecord`; `trust_list`에 `"fp"` 필드; approve 루프에 trust_revoke 분기)

**Interfaces:**
- Consumes: Task 2의 approve kind 분기 구조(else-if 연쇄에 삽입), `PendingApproval.fingerprint`/`name` (기존 필드 재사용 — 신규 필드 없음), kPermMatrix의 `trust_revoke` 행.
- Produces: 도구 `trust_revoke {fingerprint}` — 즉답 `bad_fingerprint`/`not_found`, 파킹 이벤트 `{"topic":"agent.approval_request","request":N,"tool":"trust_revoke","kind":"trust_revoke","name":<display>,"fingerprint":<full>,"ts":...}`, 해소 reply `{"ok":true,"written":true,"restart_needed":true}`. `trust_list` 응답 행에 `"fp":"<full fingerprint>"` 추가 (Task 6 신뢰 탭이 이 필드로 해지 요청).

- [ ] **Step 1: ValidFingerprint 파일 스코프 추출**

`trust_request` 분기 안의 지역 람다(약 line 1763)를 잘라내고, `kPermMatrix` 정의 뒤로 이동:
```cpp
// 지문 형식: 정확히 "sha256:" + 64 소문자 hex (로더 형식 — docs/37).
static bool ValidFingerprint(const std::string& fp) {
    if (fp.size() != 7 + 64 || fp.compare(0, 7, "sha256:") != 0) return false;
    for (size_t i = 7; i < fp.size(); ++i) {
        const char c = fp[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
```
trust_request 분기의 람다 정의는 삭제하고 참조만 남긴다(이름 동일 — 컴파일 그대로 통과).

- [ ] **Step 2: TrustRecordInfo + RevokeTrustRecord helper (WritePermissionsEntry 뒤)**

```cpp
// trust.json 원본 텍스트에서 지문 레코드의 { ... } 경계를 찾는다. 레코드는
// 로더 쓰기 형식(name/source/fingerprint/ts — 중첩 객체 없음)이므로 중괄호
// 스캔이 안전하다. AgentJson 재직렬화는 ts(int64)를 잃는다(AgentJson에 int64
// 접근자 없음 — docs/38) — 그래서 원문 수술.
static bool TrustRecordText(const std::string& text,
                            const std::string& fingerprint,
                            std::string& recOut) {
    const std::string needle = "\"fingerprint\":\"" + fingerprint + "\"";
    const size_t hit = text.find(needle);
    if (hit == std::string::npos) return false;
    const size_t begin = text.rfind('{', hit);
    const size_t end = text.find('}', hit);
    if (begin == std::string::npos || end == std::string::npos) return false;
    recOut = text.substr(begin, end - begin + 1);
    return true;
}

// trust.json에서 해당 지문 레코드 제거 + .bak 1회 보존(북마크 선례 — 최초
// 덮어쓰기 시점 원본만). 반환: 빈 문자열 = 성공(제거 1건), 아니면 오류 문자열.
static std::string RevokeTrustRecord(const std::string& fingerprint) {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    CreateDirectoryA((dir + "\\state").c_str(), nullptr);
    const std::string path = dir + "\\state\\trust.json";

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "trust_store_unreadable";
    std::vector<char> buf(65536);
    const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    const std::string text(buf.data());

    std::string rec;
    if (!TrustRecordText(text, fingerprint, rec)) return "not_found";
    const size_t hit = text.find(rec);
    size_t begin = hit;
    size_t end = hit + rec.size() - 1;
    // 선행 쉼표 흡수 — "},{" 형태에서 앞 레코드의 쉼표를 남기지 않는다.
    size_t cutBegin = begin;
    if (cutBegin > 0 && text[cutBegin - 1] == ',') --cutBegin;
    else if (end + 1 < text.size() && text[end + 1] == ',') ++end;
    const std::string out = text.substr(0, cutBegin) +
                            text.substr(end + 1);

    const std::string bak = path + ".bak";
    if (std::FILE* b = std::fopen(bak.c_str(), "rb")) {
        std::fclose(b);   // .bak 이미 있음 — 1회 보존 규약
    } else if (std::FILE* b = std::fopen(bak.c_str(), "wb")) {
        std::fwrite(text.data(), 1, text.size(), b);
        std::fclose(b);
    }
    std::FILE* w = std::fopen(path.c_str(), "wb");
    if (!w) return "write_failed";
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), w);
    std::fclose(w);
    return wrote == out.size() ? std::string() : std::string("write_failed");
}
```

- [ ] **Step 3: trust_revoke 분기 (trust_request 블록 뒤)**

```cpp
    } else if (tool == "trust_revoke") {
        // 스펙 §2.3: 신뢰 해지 — 안전 방향이지만 무게이트는 신뢰 저장소
        // 전면 소각 DoS 통로. 기본 Ask, 파일로 allow/deny 변경 가능(해지는
        // 권한 부여가 아니라 2단 우회 위험이 없다). 파킹 전 검증+실측.
        std::string fingerprint;
        req.GetObjStr("args", "fingerprint", fingerprint);
        std::string rec, name;
        if (fingerprint.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_fingerprint\"}";
        } else if (!ValidFingerprint(fingerprint)) {
            reply = "{\"ok\":false,\"error\":\"bad_fingerprint\"}";
        } else {
            std::string trustText;
            char exePath[1024] = {};
            GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
            std::string dir = exePath;
            const size_t dslash = dir.find_last_of("\\/");
            if (dslash != std::string::npos) dir = dir.substr(0, dslash);
            if (std::FILE* f = std::fopen(
                    (dir + "\\state\\trust.json").c_str(), "rb")) {
                std::vector<char> tbuf(65536);
                const size_t tn = std::fread(tbuf.data(), 1, tbuf.size() - 1, f);
                std::fclose(f);
                tbuf[tn] = '\0';
                trustText = tbuf.data();
            }
            if (!TrustRecordText(trustText, fingerprint, rec)) {
                reply = "{\"ok\":false,\"error\":\"not_found\"}";
            } else {
                jk::agent::AgentJson(rec.c_str()).GetStr("name", name);
                switch (AgentToolAllowed("trust_revoke")) {
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
                            reply = "{\"ok\":false,"
                                    "\"error\":\"approval_unavailable\"}";
                            break;
                        }
                        PendingApproval p;
                        p.kind = "trust_revoke";
                        p.name = name.empty() ? "script" : name;
                        p.fingerprint = fingerprint;
                        p.requestId = nextApprovalId_++;
                        p.queryId = queryId;
                        p.requesterId = client.Id();
                        p.targetId = 0;
                        p.expiresAt = std::time(nullptr) + 60;
                        char buf[1024];
                        std::snprintf(buf, sizeof(buf),
                                      "{\"topic\":\"agent.approval_request\","
                                      "\"request\":%u,\"tool\":\"trust_revoke\","
                                      "\"kind\":\"trust_revoke\",\"name\":\"%s\","
                                      "\"fingerprint\":\"%s\",\"ts\":%lld}",
                                      p.requestId, JsonEsc(p.name).c_str(),
                                      fingerprint.c_str(),
                                      static_cast<long long>(
                                          std::time(nullptr)) * 1000);
                        pendingApprovals_.push_back(p);
                        PushAgentEventJson(buf);
                        replied = false;
                        break;
                    }
                    case AgentDecision::Allow:
                        // 파일이 allow로 명시한 설치 — 즉시 해지(사용자가
                        // 매트릭스에서 그렇게 정한 것).
                        {
                            const std::string err =
                                RevokeTrustRecord(fingerprint);
                            reply = err.empty()
                                ? "{\"ok\":true,\"written\":true,"
                                  "\"restart_needed\":true}"
                                : "{\"ok\":false,\"error\":\"" + err + "\"}";
                        }
                        break;
                    case AgentDecision::Deny:
                    default:
                        reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                        break;
                }
            }
        }
```

- [ ] **Step 4: approve 루프에 trust_revoke 분기**

Task 2에서 만든 kind별 result 계산에 추가:
```cpp
                    } else if (it->kind == "trust_revoke") {
                        const std::string err =
                            RevokeTrustRecord(it->fingerprint);
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true,"
                              "\"restart_needed\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else {
```

- [ ] **Step 5: trust_list에 전체 지문 필드**

`trust_list` 분기의 행 직렬화에 `"fp"` 추가 — 기존 truncated `fingerprint` 필드는 유지(표시용). 행 조립부에서:
```cpp
                // 에이전트 관리자 해지용 전체 지문 (스펙 §2.3 전제) — 15자
                // 절단 fingerprint는 표시용으로 유지.
                std::string fullFp;
                json.GetArrStr("records", i, "fingerprint", fullFp);
```
행 out에 `,"fp":"<fullFp>"` 삽입(기존 필드들과 함께 — 정확한 위치는 기존 코드의 행 직렬화 형식에 맞춘다).

- [ ] **Step 6: 빌드 + 라이브 체크**

```bash
cd I:/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin cmake --build . --target jkdesktop
```
Write 도구로 `engine/tools/probes/mgr_t3_revoke.ps1` (ASCII-only):
```powershell
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$root = Split-Path $exe
$state = Join-Path $root "state"
$trust = Join-Path $state "trust.json"
# 시딩 전 기존 trust.json 백업(복구용) — 테스트가 만든 파일이 아닐 수 있다
$hadTrust = Test-Path $trust
if ($hadTrust) { Copy-Item $trust (Join-Path $env:TEMP "trust_pre_t3.json") -Force }
$fp = "sha256:" + ("ab" * 32)   # 형식 유효 가짜 지문
$fake = "{`"records`":[{`"fingerprint`":`"$fp`",`"name`":`"t3probe`",`"source`":`"dev`",`"ts`":1700000000000}]}"
# 기존 레코드 보존 병합(간단화: 없으면 교체)
if (-not $hadTrust) { Set-Content -Path $trust -Value $fake -Encoding UTF8 }
else {
    $cur = Get-Content $trust -Raw
    $cur = $cur -replace '\]\s*}$', (",{`"fingerprint`":`"$fp`",`"name`":`"t3probe`",`"source`":`"dev`",`"ts`":1700000000000}]}")
    Set-Content -Path $trust -Value $cur -Encoding UTF8
}
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 3
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) -WindowStyle Hidden
Start-Sleep -Seconds 3
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
$bad = Invoke-Agentctl ('{"tool":"trust_revoke","args":{"fingerprint":"sha256:xyz"}}')
if ($bad -match 'bad_fingerprint') { Write-Host "t3-badfp: PASS" } else { Write-Host "t3-badfp: FAIL $bad" }
$nf = Invoke-Agentctl ('{"tool":"trust_revoke","args":{"fingerprint":"sha256:' + ("cd" * 32) + '"}}')
if ($nf -match 'not_found') { Write-Host "t3-notfound: PASS" } else { Write-Host "t3-notfound: FAIL $nf" }
# 해지 E2E
$job = Start-Job -ScriptBlock {
    param($exe, $fp)
    $body = '{"tool":"trust_revoke","args":{"fingerprint":"' + $fp + '"}}' -replace '"', '\"'
    & $exe agentctl $body
} -ArgumentList $exe, $fp
Start-Sleep -Seconds 3
$ap = Invoke-Agentctl '{"tool":"approve","args":{"request":1,"decision":"allow"}}'
Start-Sleep -Seconds 2
$reply = Receive-Job $job -Wait
if ($reply -match '"restart_needed\\?":true') { Write-Host "t3-reply: PASS" } else { Write-Host "t3-reply: FAIL $reply" }
$after = Get-Content $trust -Raw
if ($after -notmatch ($fp -replace ':', '\:')) { Write-Host "t3-removed: PASS" } else { Write-Host "t3-removed: FAIL" }
$bak = Join-Path $state "trust.json.bak"
if ((Test-Path $bak) -and ((Get-Content $bak -Raw) -match ($fp -replace ':', '\:'))) {
    Write-Host "t3-bak: PASS"
} else { Write-Host "t3-bak: FAIL" }
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
# 복원: t3가 만든 레코드 제거 (서버가 쓴 trust.json에서 fake 지문 행 제거)
if ($hadTrust -and (Test-Path (Join-Path $env:TEMP "trust_pre_t3.json"))) {
    Copy-Item (Join-Path $env:TEMP "trust_pre_t3.json") $trust -Force
} elseif (-not $hadTrust) {
    Remove-Item $trust, (Join-Path $state "trust.json.bak") -ErrorAction SilentlyContinue
}
```
Run: `powershell -ExecutionPolicy Bypass -File I:\progwork\JKENGINE\engine\tools\probes\mgr_t3_revoke.ps1`
Expected: `t3-badfp/t3-notfound/t3-reply/t3-removed/t3-bak` 전부 PASS.

- [ ] **Step 7: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/src/server/JKWindowServer.cpp
git commit -m "feat(agentmgr): trust_revoke with approval gate + trust.json RMW (.bak 1-keep) + full fp in trust_list (spec 2.3)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 4: 브로커 jkagentd 갱신 (TDD: selftest 먼저)

**Files:**
- Modify: `engine/tools/jkagentd/main.cpp` (`kToolsListJson`, `IsKnownTool`, `LoadPermissions`, `HandleLine` args 재빌더, `RunSelfTest`)

**Interfaces:**
- Consumes: 서버 도구 시그니처(Task 1-3) — MCP schema 미러.
- Produces: MCP 도구 5종 노출 + 브로커 기본 deny 2종(`permission_set`, `trust_revoke` — run_console_app 선례: "permissions.json에 ask를 넣는 것"이 승인 행위).

- [ ] **Step 1: 실패할 selftest 추가 (TDD)**

`RunSelfTest`의 `unknown_tool` 체크 뒤에 추가:
```cpp
    // 에이전트 관리자 도구 5종이 tools/list에 노출되는가 (스펙 §3).
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/list\"}",
                   isResp);
    if (!isResp || r.find("agent_permissions") == std::string::npos ||
        r.find("installed_list") == std::string::npos ||
        r.find("read_receipts") == std::string::npos ||
        r.find("permission_set") == std::string::npos ||
        r.find("trust_revoke") == std::string::npos) ++failures;
```
Run: `PATH=/c/msys64/ucrt64/bin cmake --build . --target jkagentd && ./jkagentd.exe --selftest`
Expected: FAIL (failures > 0 — kToolsListJson에 아직 없음).

- [ ] **Step 2: kToolsListJson + IsKnownTool + LoadPermissions**

`kToolsListJson`의 `trust_list` 항목 뒤에 5종 추가:
```cpp
"{\"name\":\"agent_permissions\",\"description\":\"Show the permission matrix: tool x allow/ask/deny with gate badge (server/broker/none)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"permission_set\",\"description\":\"Change a tool permission (allow/ask/deny) - always requires approval (fixed ask gate)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"tool\":{\"type\":\"string\"},\"decision\":{\"type\":\"string\",\"enum\":[\"allow\",\"ask\",\"deny\"]}},\"required\":[\"tool\",\"decision\"]}},"
"{\"name\":\"trust_revoke\",\"description\":\"Revoke a trusted script fingerprint (approval-gated)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"fingerprint\":{\"type\":\"string\",\"pattern\":\"sha256:[0-9a-f]{64}\"}},\"required\":[\"fingerprint\"]}},"
"{\"name\":\"installed_list\",\"description\":\"List installed apps (console + .jkx) with kind\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"read_receipts\",\"description\":\"Tail the broker receipt log (ts/tool/ok only)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"limit\":{\"type\":\"integer\"}}}},"
```
`IsKnownTool`의 kNames에 동일 5명 추가. `LoadPermissions`의 `kNames` 배열에도 5명 추가 + 기본값:
```cpp
    // 매니저 쓰기 2종 — 브로커 기본 deny (run_console_app 선례: MCP 에이전트가
    // 직접 권한/신뢰를 바꾸려면 permissions.json 편집이 선행 승인 행위다.
    // 서버 게이트(Ask 파이프라인)는 별도로 살아 있다 — 여기선 정직한 기본값).
    perms["permission_set"] = false;
    perms["trust_revoke"] = false;
```

- [ ] **Step 3: args 재빌더 (HandleLine tools/call)**

`theme_set` 분기 뒤:
```cpp
        } else if (tool == "permission_set") {
            // 매니저 (스펙 §2.2): 서버 args는 {tool, decision}.
            std::string pt, dec;
            if (req.GetDeepStr("params", "arguments", "tool", pt) &&
                req.GetDeepStr("params", "arguments", "decision", dec)) {
                argsJson = "{\"tool\":\"" + JsonEsc(pt) +
                           "\",\"decision\":\"" + JsonEsc(dec) + "\"}";
            }
        } else if (tool == "trust_revoke") {
            std::string fp;
            if (req.GetDeepStr("params", "arguments", "fingerprint", fp)) {
                argsJson = "{\"fingerprint\":\"" + JsonEsc(fp) + "\"}";
            }
        } else if (tool == "read_receipts") {
            int limit = 50;
            if (req.GetDeepInt("params", "arguments", "limit", limit)) {
                argsJson = "{\"limit\":" + std::to_string(limit) + "}";
            }
        }
```
(agent_permissions/installed_list는 인자 없음 — argsJson 기본 "{}" 통과.)

- [ ] **Step 4: selftest PASS 확인 + Commit**

Run: `PATH=/c/msys64/ucrt64/bin cmake --build . --target jkagentd && ./jkagentd.exe --selftest`
Expected: `selftest: 0 failures`.
```bash
cd I:/progwork/JKENGINE && git add engine/tools/jkagentd/main.cpp
git commit -m "feat(agentmgr): broker surface for the 5 new tools (deny defaults, args rebuild, selftest)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 5: jkchat 승인 스트립 2종 + 팔레트 /agentmgr

**Files:**
- Modify: `engine/tools/jkchat/main.cpp` (ShowPermissionApproval/ShowTrustRevokeApproval + HandleEvent 분기)
- Modify: `engine/src/apps/ClientPaletteApp.cpp` (슬래시 + help 텍스트)

**Interfaces:**
- Consumes: Task 2/3의 approval_request 페이로드 필드 (`target_tool`/`decision`, `name`/`fingerprint`), 기존 `approve` 공용 버튼(변경 불필요 — request id만 쓴다).
- Produces: 채팅 승인 스트립이 permission_set/trust_revoke 요청을 렌더링. 팔레트 `/agentmgr` 슬래시.

- [ ] **Step 1: jkchat ShowTrustApproval 뒤에 2함수 추가**

```cpp
// 매니저 권한 변경 (스펙 §2.2): 동일 스트립, 다른 문구. approve 도구는
// kind 불문 공용이라 서버 변경 없이 허용/거부가 해소한다.
static void ShowPermissionApproval(const std::string& targetTool,
                                   const std::string& decision,
                                   uint32_t request) {
    g_approvalRequest = request;
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE, L"[권한 변경] %s → %s 승인할까요?",
                 Utf8ToWide(targetTool).c_str(),
                 Utf8ToWide(decision).c_str());
    SetWindowTextW(g_hPrompt, buf);
    ShowWindow(g_hPrompt, SW_SHOWNORMAL);
    ShowWindow(g_hAllow, SW_SHOWNORMAL);
    ShowWindow(g_hDeny, SW_SHOWNORMAL);
}

// 매니저 신뢰 해지 (스펙 §2.3): 지문은 15자 절단 표시(ShowTrustApproval 선례).
static void ShowTrustRevokeApproval(const std::string& name,
                                    const std::string& fingerprint,
                                    uint32_t request) {
    g_approvalRequest = request;
    const std::string fp8 =
        fingerprint.size() > 15 ? fingerprint.substr(0, 15) : fingerprint;
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE, L"[신뢰 해지] %s (%s…) 승인할까요?",
                 Utf8ToWide(name).c_str(), Utf8ToWide(fp8).c_str());
    SetWindowTextW(g_hPrompt, buf);
    ShowWindow(g_hPrompt, SW_SHOWNORMAL);
    ShowWindow(g_hAllow, SW_SHOWNORMAL);
    ShowWindow(g_hDeny, SW_SHOWNORMAL);
}
```

- [ ] **Step 2: HandleEvent 분기**

`HandleEvent`의 `if (kind == "trust_request")` 뒤:
```cpp
        } else if (kind == "permission_set") {
            std::string targetTool, decision;
            e.GetStr("target_tool", targetTool);
            e.GetStr("decision", decision);
            ShowPermissionApproval(targetTool, decision,
                                   static_cast<uint32_t>(request));
            Log("[권한 변경] " + targetTool + " → " + decision);
        } else if (kind == "trust_revoke") {
            std::string name, fp;
            e.GetStr("name", name);
            e.GetStr("fingerprint", fp);
            ShowTrustRevokeApproval(name, fp, static_cast<uint32_t>(request));
            Log("[신뢰 해지] " + name);
        } else {
```

- [ ] **Step 3: 팔레트 /agentmgr**

`ClientPaletteApp.cpp`의 `/notify` 분기 뒤:
```cpp
    } else if (cmd == "agentmgr") {
        // 에이전트 관리자 앱 (specs/2026-09-16-agent-manager) — launch_app
        // 재사용. MVP는 토글 없음(재스폰 = 사용자 책임).
        SendTool("launch_app", "{\"app\":\"agentmgr\"}");
```
`/help` 출력에 ` /agentmgr` 추가(2행 꼬리에 이어붙임).

- [ ] **Step 4: 빌드 + Commit**

```bash
cd I:/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin cmake --build . --target jkchat --target jkapp_palette
```
Expected: 0 errors. (UI 렌더링 실측은 Task 8 프로브/사용자 눈확인 — 여기선 컴파일+링크만.)
```bash
cd I:/progwork/JKENGINE && git add engine/tools/jkchat/main.cpp engine/src/apps/ClientPaletteApp.cpp
git commit -m "feat(agentmgr): chat approval strips for permission_set/trust_revoke + palette /agentmgr (spec 3/1)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 6: jkapp_agentmgr — 앱 본체 4탭 + CMake + jkx + 아이콘

**Files:**
- Create: `engine/include/apps/ClientAgentMgrApp.h`
- Create: `engine/src/apps/ClientAgentMgrApp.cpp`
- Create: `engine/src/apps/JKAppModule_agentmgr.cpp`
- Create: `engine/assets/icons/launcher_agentmgr@{1x,2x}.png` (생성기 tmp/make_agentmgr_icon.ps1)
- Modify: `engine/CMakeLists.txt` (타겟 + JKX_ICON_APPS + jkx 팩 커스텀 커맨드)

**Interfaces:**
- Consumes: Task 1-3 도구 응답 스키마; `JKClientSurface::SendAgentQuery(id, json)`/`PollAgentReply(AgentReply&)` (팔레트 선례, engine/include/client/JKClientSurface.h:111/114); `jk::theme::ApplyImGuiTheme()`/`OnThemeChanged` (docs/52).
- Produces: 스폰 키 "agentmgr" (meta name) — Task 7 프로브와 팔레트 /agentmgr가 `launch_app {app:"agentmgr"}`로 스폰. meta = `{ "agentmgr", "Agent Manager", 560, 640 }`.

- [ ] **Step 1: ClientAgentMgrApp.h**

`engine/include/apps/ClientAgentMgrApp.h`:
```cpp
#ifndef CLIENTAGENTMGRAPP_H
#define CLIENTAGENTMGRAPP_H

// 에이전트 관리자 클라 (specs/2026-09-16-agent-manager): 4탭 ImGui 앱 —
// 권한 매트릭스/트리거/신뢰/설치+receipts. ClientNotifyApp 템플릿(docs/33),
// 이벤트 구독 없음 — 요청-응답 폴링(팔레트 선례: 클라 read 루프는 프레임 루프
// 결합이라 PollAgentReply로 큐를 드레인한다).

#include <client/JKClientApplication.h>
#include <cstdint>
#include <string>
#include <vector>

namespace jk {

struct MgrPermRow { std::string tool, gate, file, effective, deflt; };
// topics는 2레벨 리더가 못 읽어(파서 계약, Step 2 주의 문단) 열에서 뺐다 —
// 트리거 탭은 name/enabled만.
struct MgrTriggerRow { std::string name; int enabled = 1; };
struct MgrTrustRow { std::string name, source, fp, shortFp; };
struct MgrInstalledRow { std::string name, kind; bool running = false; };
struct MgrReceiptRow { long long ts = 0; std::string tool; bool ok = false; };

class ClientAgentMgrApp : public JKClientApplication {
public:
    ClientAgentMgrApp() = default;
    ~ClientAgentMgrApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;   // ImGui 팔레트 재적용 (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    enum class Query { Perms, Triggers, Trust, InstalledApps, RunningWindows,
                       Receipts, Toggle, PermissionSet, TrustRevoke };
    struct PendingQuery { Query kind; std::string arg; };

    void BuildUi(int w, int h);
    void BuildPermissionsTab();
    void BuildTriggersTab();
    void BuildTrustTab();
    void BuildInstalledTab();
    void PollReplies();
    void SendQuery(const char* tool, const std::string& args, Query kind,
                   const std::string& arg = {});
    void Refresh(int tab);              // 0=권한 1=트리거 2=신뢰 3=설치
    void ApplyReply(Query kind, const std::string& json);
    void MergeRunning();
    static std::string EscapeJson(const std::string& in);

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;   // Malgun Gothic (한글 폰트, notify 선례)
    int tab_ = 0;               // 활성 탭 — BuildUi의 BeginTabItem이 기록
    uint32_t nextQueryId_ = 1;
    std::string status_;        // 하단 상태줄 (마지막 reply/에러)
    std::string pendingPerm_;   // 승인 대기 표시 "tool → decision"
    std::string trustStoreError_;  // 신뢰 탭 에러 박스 (스펙 §5)

    std::vector<MgrPermRow> perms_;
    std::vector<MgrTriggerRow> triggers_;
    std::vector<MgrTrustRow> trust_;
    std::vector<MgrInstalledRow> installed_;
    std::vector<MgrReceiptRow> receipts_;
    std::vector<std::pair<uint32_t, std::string>> windows_;  // id, title
    std::vector<std::pair<uint32_t, PendingQuery>> pending_;
};

} // namespace jk
#endif // CLIENTAGENTMGRAPP_H
```

- [ ] **Step 2: ClientAgentMgrApp.cpp (전체)**

```cpp
// Agent manager client (specs/2026-09-16-agent-manager). ClientNotifyApp
// template (docs/33): 16ms timer frame gate, dark root, malgun font, theme
// re-apply hook. No event subscription — request/response only (palette
// idiom: PollAgentReply drains the window connection's reply queue).
#include <apps/ClientAgentMgrApp.h>

#include <agent/JKAgentJson.h>
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <JKWindow.h>
#include <SDL.h>

#include <cstdio>

namespace jk {
namespace {
// Root window paints the dark clear color (palette/notify idiom).
class MgrRoot : public JKWindow {
public:
    explicit MgrRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
        dc.FillRect(client);
    }
};

std::string EscapeJson(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const char ch : in) {
        if (ch == '"' || ch == '\\') { out += '\\'; out += ch; }
        else if (static_cast<unsigned char>(ch) < 0x20) {
            char num[8];
            std::snprintf(num, sizeof(num), "\\u%04x", ch);
            out += num;
        } else {
            out += ch;
        }
    }
    return out;
}
} // namespace

ClientAgentMgrApp::~ClientAgentMgrApp() = default;

void ClientAgentMgrApp::OnInit() {
    auto main = std::make_unique<MgrRoot>("Agent Manager");
    main->SetWindowRect(JKRect{ 0, 0, 560, 640 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16);   // ~60 Hz frame cadence (taskmgr clock)

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme();
    ImGui::GetIO().IniFilename = nullptr;
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f,
                                     nullptr,
                                     io.Fonts->GetGlyphRangesKorean())) {
        koreanFont_ = true;
    }
    Refresh(0);   // 첫 탭 데이터
}

void ClientAgentMgrApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

void ClientAgentMgrApp::OnThemeChanged() {
    if (imguiReady_) jk::theme::ApplyImGuiTheme();   // docs/52 hot-swap
}

bool ClientAgentMgrApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) frameDirty_ = true;
    return true;
}

void ClientAgentMgrApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientAgentMgrApp::SendQuery(const char* tool, const std::string& args,
                                  Query kind, const std::string& arg) {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) {
        status_ = koreanFont_ ? "[!] 서버에 연결되어 있지 않습니다"
                              : "[!] not connected";
        return;
    }
    const std::string json =
        "{\"tool\":\"" + std::string(tool) + "\",\"args\":" + args + "}";
    const uint32_t id = nextQueryId_++;
    if (!surface->SendAgentQuery(id, json)) {
        status_ = "[!] send failed";
        return;
    }
    pending_.emplace_back(id, PendingQuery{ kind, arg });
}

void ClientAgentMgrApp::Refresh(int tab) {
    switch (tab) {
        case 0: SendQuery("agent_permissions", "{}", Query::Perms); break;
        case 1: SendQuery("trigger_list", "{}", Query::Triggers); break;
        case 2: SendQuery("trust_list", "{}", Query::Trust); break;
        case 3:
            SendQuery("installed_list", "{}", Query::InstalledApps);
            SendQuery("list_windows", "{}", Query::RunningWindows);
            SendQuery("read_receipts", "{\"limit\":50}", Query::Receipts);
            break;
        default: break;
    }
}

void ClientAgentMgrApp::MergeRunning() {
    // 완화 매칭 (스펙 델타): 타이틀에 앱 이름이 포함되면 실행 중으로 표기.
    for (MgrInstalledRow& row : installed_) {
        row.running = false;
        for (const auto& w : windows_) {
            if (!w.second.empty() &&
                w.second.find(row.name) != std::string::npos) {
                row.running = true;
                break;
            }
        }
    }
}

void ClientAgentMgrApp::PollReplies() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface) return;
    jk::client::AgentReply reply;
    while (surface->PollAgentReply(reply)) {
        Query kind = Query::Perms;
        bool found = false;
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it->first == reply.queryId) {
                kind = it->second.kind;
                pending_.erase(it);
                found = true;
                break;
            }
        }
        if (found) ApplyReply(kind, reply.json);
    }
}

void ClientAgentMgrApp::ApplyReply(Query kind, const std::string& json) {
    jk::agent::AgentJson r(json);
    switch (kind) {
        case Query::Perms: {
            perms_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("perms", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrPermRow row;
                    r.GetArrStr("perms", i, "tool", row.tool);
                    r.GetArrStr("perms", i, "gate", row.gate);
                    r.GetArrStr("perms", i, "file", row.file);
                    r.GetArrStr("perms", i, "effective", row.effective);
                    r.GetArrStr("perms", i, "default", row.deflt);
                    perms_.push_back(row);
                }
            }
            break;
        }
        case Query::Triggers: {
            // topics 배열은 2레벨 리더의 관심사 밖 — name/enabled만 (파서 계약).
            triggers_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("triggers", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrTriggerRow row;
                    r.GetArrStr("triggers", i, "name", row.name);
                    r.GetArrInt("triggers", i, "enabled", row.enabled);
                    triggers_.push_back(row);
                }
            }
            break;
        }
        case Query::Trust: {
            trust_.clear();
            if (!r.ok()) {
                // trust_store_unreadable은 상태줄이 아니라 탭에 에러 박스
                status_ = "";
                trust_.clear();
                trustStoreError_ = json;
                break;
            }
            trustStoreError_.clear();
            int cnt = 0;
            if (r.GetArraySize("records", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrTrustRow row;
                    r.GetArrStr("records", i, "name", row.name);
                    r.GetArrStr("records", i, "source", row.source);
                    r.GetArrStr("records", i, "fingerprint", row.shortFp);
                    r.GetArrStr("records", i, "fp", row.fp);
                    trust_.push_back(row);
                }
            }
            break;
        }
        case Query::InstalledApps: {
            installed_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("installed", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrInstalledRow row;
                    r.GetArrStr("installed", i, "name", row.name);
                    r.GetArrStr("installed", i, "kind", row.kind);
                    installed_.push_back(row);
                }
            }
            MergeRunning();
            break;
        }
        case Query::RunningWindows: {
            windows_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("windows", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    int idInt = 0;   // GetArrInt는 int — uint32로 안전 승격
                    std::string title;
                    r.GetArrInt("windows", i, "id", idInt);
                    r.GetArrStr("windows", i, "title", title);
                    windows_.emplace_back(static_cast<uint32_t>(idInt),
                                          title);
                }
            }
            MergeRunning();
            break;
        }
        case Query::Receipts: {
            receipts_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("rows", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrReceiptRow row;
                    row.ts = 0;
                    int ts32 = 0;
                    if (r.GetArrInt("rows", i, "ts", ts32)) {
                        row.ts = ts32;   // 표시용 초 단위 절단 허용
                    }
                    r.GetArrStr("rows", i, "tool", row.tool);
                    std::string okRaw;
                    row.ok = r.GetArrStr("rows", i, "ok", okRaw) &&
                             okRaw == "1";
                    receipts_.push_back(row);
                }
            }
            break;
        }
        case Query::Toggle:
            if (r.ok()) Refresh(1);
            else status_ = "[!] " + json;
            break;
        case Query::PermissionSet:
        case Query::TrustRevoke:
            pendingPerm_.clear();
            if (r.ok()) {
                status_ = json.find("restart_needed") != std::string::npos
                    ? (koreanFont_ ? "완료 — jktriggers 재시작 후 적용"
                                   : "done (jktriggers restart needed)")
                    : (koreanFont_ ? "완료" : "done");
                Refresh(tab_);
            } else {
                status_ = "[!] " + json;
            }
            break;
    }
}
```
주의(파서 계약 — Task 1/3 직렬화와 이미 맞춰져 있다): (a) AgentJson은 2레벨
리더라 3레벨 접근자(`GetArrSize("triggers", i, "topics", tc)`)가 **없다** —
트리거 탭은 name/enabled만 표시한다(topics 열 포기, 스펙 델타).
(b) read_receipts rows는 Task 1이 `ok:"0"/"1"` 문자열 + ts(epoch 초)로 보낸다
— 위 Receipts 파싱(`okRaw == "1"`, ts32)이 그 규약을 전제한다.
(c) trust_list 행은 Task 3의 `"fp"`(전체) + 기존 `fingerprint`(15자) 필드
둘 다 읽는다.

RenderOverlay + BuildUi:
```cpp
void ClientAgentMgrApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer)) return;
        imguiReady_ = true;
    }
    PollReplies();

    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientAgentMgrApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    if (!ImGui::Begin("agentmgr", nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
        return;
    ImGui::End();

    // 서버 크롬이 상단 24pt를 먹는다(레슨 8) — 탭바는 y>=30부터.
    ImGui::SetNextWindowPos(ImVec2(0, 30));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h - 30));
    if (ImGui::Begin("##mgrtabs", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        const int prev = tab_;
        if (ImGui::BeginTabBar("mgrtabs")) {
            if (ImGui::BeginTabItem(koreanFont_ ? "권한" : "Perms")) {
                tab_ = 0;
                BuildPermissionsTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(koreanFont_ ? "트리거" : "Triggers")) {
                tab_ = 1;
                BuildTriggersTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(koreanFont_ ? "신뢰" : "Trust")) {
                tab_ = 2;
                BuildTrustTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(koreanFont_ ? "설치/로그" : "Apps")) {
                tab_ = 3;
                BuildInstalledTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (tab_ != prev) {   // 탭 진입 시 데이터 리프레시
            status_.clear();
            Refresh(tab_);
        }
        ImGui::Separator();
        if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    }
    ImGui::End();
}
```

탭 1 — 권한:
```cpp
void ClientAgentMgrApp::BuildPermissionsTab() {
    if (ImGui::Button(koreanFont_ ? "새로고침" : "Refresh")) Refresh(0);
    if (!pendingPerm_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.5f, 1.0f), "%s",
                           pendingPerm_.c_str());
    }
    if (ImGui::BeginTable(
            "perms", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(koreanFont_ ? "도구" : "Tool");
        ImGui::TableSetupColumn("gate");
        ImGui::TableSetupColumn(koreanFont_ ? "파일" : "File");
        ImGui::TableSetupColumn(koreanFont_ ? "기본값" : "Default");
        ImGui::TableSetupColumn(koreanFont_ ? "변경" : "Set");
        ImGui::TableHeadersRow();
        for (const MgrPermRow& row : perms_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.tool.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.gate.c_str());
            ImGui::TableSetColumnIndex(2);
            if (row.file.empty()) ImGui::TextDisabled("-");
            else ImGui::TextUnformatted(row.file.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%s", row.deflt.c_str());
            ImGui::TableSetColumnIndex(4);
            if (row.gate == std::string("server(fixed)")) {
                ImGui::TextDisabled(koreanFont_ ? "ask 고정" : "fixed ask");
            } else {
                static const char* kDec[3] = { "allow", "ask", "deny" };
                for (int d = 0; d < 3; ++d) {
                    if (row.effective == kDec[d]) continue;
                    ImGui::PushID((row.tool + kDec[d]).c_str());
                    if (ImGui::SmallButton(kDec[d])) {
                        pendingPerm_ = row.tool + " -> " + kDec[d] + " " +
                                      (koreanFont_ ? "(승인 대기)" : "(await)");
                        SendQuery("permission_set",
                                  std::string("{\"tool\":\"") +
                                      EscapeJson(row.tool) +
                                      "\",\"decision\":\"" + kDec[d] + "\"}",
                                  Query::PermissionSet, row.tool);
                    }
                    ImGui::PopID();
                    ImGui::SameLine();
                }
                ImGui::Dummy(ImVec2(0, 0));
            }
        }
        ImGui::EndTable();
    }
}
```
(gate "none"/"broker" 행에도 버튼을 준다 — 스펙 §2.2: 쓰기 허용, 매트릭스 표기가 정직하면 사용자가 판단. gate 뱃지가 그 안내.)

탭 2 — 트리거:
```cpp
void ClientAgentMgrApp::BuildTriggersTab() {
    if (ImGui::Button(koreanFont_ ? "새로고침" : "Refresh")) Refresh(1);
    if (triggers_.empty())
        ImGui::TextDisabled(koreanFont_ ? "(트리거 없음)" : "(no triggers)");
    if (ImGui::BeginTable("trigs", 2, ImGuiTableFlags_Borders |
                                         ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(koreanFont_ ? "이름" : "Name");
        ImGui::TableSetupColumn(koreanFont_ ? "상태" : "State");
        ImGui::TableHeadersRow();
        for (const MgrTriggerRow& row : triggers_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(("tg" + row.name).c_str());
            if (ImGui::SmallButton(row.enabled
                                       ? (koreanFont_ ? "끄기" : "off")
                                       : (koreanFont_ ? "켜기" : "on"))) {
                SendQuery("trigger_toggle",
                          std::string("{\"name\":\"") +
                              EscapeJson(row.name) + "\",\"on\":" +
                              (row.enabled ? "0" : "1") + "}",
                          Query::Toggle);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}
```

탭 3 — 신뢰:
```cpp
void ClientAgentMgrApp::BuildTrustTab() {
    if (ImGui::Button(koreanFont_ ? "새로고침" : "Refresh")) Refresh(2);
    if (!trustStoreError_.empty()) {
        // state 파일 corrupt/unreadable — 앱이 죽지 않고 에러 박스 (스펙 §5)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.4f, 1.0f));
        ImGui::TextWrapped("[!] %s", trustStoreError_.c_str());
        ImGui::PopStyleColor();
        return;
    }
    ImGui::TextDisabled(koreanFont_
        ? "해지는 jktriggers 재시작 후 적용된다 (로드 1회 규약)"
        : "revokes apply after jktriggers restart (load-once)");
    if (ImGui::BeginTable("trust", 4, ImGuiTableFlags_Borders |
                                          ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_ScrollY,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(koreanFont_ ? "이름" : "Name");
        ImGui::TableSetupColumn("source");
        ImGui::TableSetupColumn(koreanFont_ ? "지문" : "Fingerprint");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for (const MgrTrustRow& row : trust_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.source.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(row.shortFp.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(("rv" + row.shortFp).c_str());
            if (ImGui::SmallButton(koreanFont_ ? "해지" : "revoke") &&
                !row.fp.empty()) {
                SendQuery("trust_revoke",
                          std::string("{\"fingerprint\":\"") + row.fp +
                              "\"}",
                          Query::TrustRevoke);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}
```

탭 4 — 설치/receipts:
```cpp
void ClientAgentMgrApp::BuildInstalledTab() {
    if (ImGui::Button(koreanFont_ ? "새로고침" : "Refresh")) Refresh(3);
    if (ImGui::BeginTable("inst", 3, ImGuiTableFlags_Borders |
                                         ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(koreanFont_ ? "이름" : "Name");
        ImGui::TableSetupColumn("kind");
        ImGui::TableSetupColumn(koreanFont_ ? "실행 중" : "Running");
        ImGui::TableHeadersRow();
        for (const MgrInstalledRow& row : installed_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.kind.c_str());
            ImGui::TableSetColumnIndex(2);
            if (row.running) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                ImGui::TextUnformatted(koreanFont_ ? "예" : "yes");
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("-");
            }
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::TextUnformatted(koreanFont_ ? "브로커 수행 기록 (receipts)"
                                       : "Broker receipts");
    if (receipts_.empty()) {
        ImGui::TextDisabled(koreanFont_ ? "(기록 없음)" : "(no rows)");
    }
    if (ImGui::BeginTable("rcpts", 3, ImGuiTableFlags_Borders |
                                          ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_ScrollY,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ts");
        ImGui::TableSetupColumn("tool");
        ImGui::TableSetupColumn("ok");
        ImGui::TableHeadersRow();
        for (const MgrReceiptRow& row : receipts_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%lld", row.ts);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.tool.c_str());
            ImGui::TableSetColumnIndex(2);
            if (row.ok) ImGui::TextUnformatted("true");
            else {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(1.0f, 0.5f, 0.4f, 1.0f));
                ImGui::TextUnformatted("false");
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }
}
```

- [ ] **Step 3: JKAppModule_agentmgr.cpp**

`engine/src/apps/JKAppModule_agentmgr.cpp`:
```cpp
// Agent manager app module (specs/2026-09-16-agent-manager). All C++ —
// app object construction, Init and Run — happens inside this DLL; the host
// only sees the C ABI from JKAppModule.h.
#include <apps/JKAppModule.h>
#include <apps/ClientAgentMgrApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "agentmgr", "Agent Manager", 560, 640 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientAgentMgrApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}
```
(파서 계약은 Step 2 주의 문단 — Task 1/3 직렬화와 정합.)

- [ ] **Step 4: CMakeLists**

`jkapp_notify` 타겟 블록 뒤:
```cmake
# Agent manager module (specs/2026-09-16-agent-manager): 4-tab GUI over the
# permission matrix, triggers, trust store, installed apps + receipts. A
# launcher .jkx (palette /agentmgr + launch_app open it).
add_library(jkapp_agentmgr SHARED
    src/apps/JKAppModule_agentmgr.cpp
    src/apps/ClientAgentMgrApp.cpp
)
target_compile_definitions(jkapp_agentmgr PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_agentmgr PRIVATE jkclient imgui)
set_target_properties(jkapp_agentmgr PROPERTIES PREFIX "")
```
`JKX_ICON_APPS` set(...) 리스트에 `agentmgr` 추가 (줄 끝 `notify shot)` → `notify shot agentmgr)`).
`apps/notify.jkx` 커스텀 커맨드 블록 뒤에 팩 커맨드:
```cmake
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/agentmgr.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack agentmgr
    DEPENDS jkdesktop jkapp_agentmgr
        "${JKX_ICONS}/launcher_agentmgr@1x.png"
        "${JKX_ICONS}/launcher_agentmgr@2x.png"
    COMMENT "Repacking apps/agentmgr.jkx"
    VERBATIM)
```

- [ ] **Step 5: 아이콘 생성 (GDI+, tmp 스크립트 — 레슨 21 변수 사전계산)**

`tmp/make_agentmgr_icon.ps1` (ASCII-only):
```powershell
Add-Type -AssemblyName System.Drawing
function Make-Icon([int]$s, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap -ArgumentList $s, $s
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = "AntiAlias"
    $g.Clear([System.Drawing.Color]::Transparent)
    # 다크 슬레이트 타일 + 라운드 r14 (make_icons 선례)
    $r = [int](14 * $s / 64)
    $tile = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $tile.AddArc(0, 0, $d, $d, 180, 90)
    $tile.AddArc($s - $d, 0, $d, $d, 270, 90)
    $tile.AddArc($s - $d, $s - $d, $d, $d, 0, 90)
    $tile.AddArc(0, $s - $d, $d, $d, 90, 90)
    $tile.CloseFigure()
    $b = New-Object System.Drawing.Drawing2D.LinearGradientBrush `
        -ArgumentList (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $s, $s), `
        [System.Drawing.Color]::FromArgb(255, 43, 48, 60), `
        [System.Drawing.Color]::FromArgb(255, 34, 38, 47), 90
    $g.FillPath($b, $tile)
    # 글리프: 슬라이더 3줄(관리자) — 선 + 손금
    $pen = New-Object System.Drawing.Pen -ArgumentList [System.Drawing.Color]::FromArgb(255, 122, 162, 247), ([int]($s / 18))
    for ($i = 0; $i -lt 3; $i++) {
        $y = [int]($s * (0.30 + 0.20 * $i))
        $g.DrawLine($pen, [int]($s*0.2), $y, [int]($s*0.8), $y)
        $kx = [int]($s * (0.35 + 0.2 * $i))
        $g.FillEllipse([System.Drawing.Brushes]::White, $kx - [int]($s*0.07), $y - [int]($s*0.07), [int]($s*0.14), [int]($s*0.14))
    }
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}
Make-Icon 64  "I:\progwork\JKENGINE\engine\assets\icons\launcher_agentmgr@1x.png"
Make-Icon 128 "I:\progwork\JKENGINE\engine\assets\icons\launcher_agentmgr@2x.png"
Write-Host "icons written"
```
Run: `powershell -ExecutionPolicy Bypass -File I:\progwork\JKENGINE\tmp\make_agentmgr_icon.ps1`
Expected: `icons written` + PNG 2개 생성.

- [ ] **Step 6: 빌드 + 재팩 (레슨 57/18)**

```bash
cd I:/progwork/JKENGINE/engine/build
PATH=/c/msys64/ucrt64/bin cmake --build . --target jkapp_agentmgr
PATH=/c/msys64/ucrt64/bin cmake --build . --target jkx_packages
```
Expected: 0 errors; `apps/agentmgr.jkx` 생성; `ls -la apps/agentmgr.jkx jkapp_agentmgr.dll`로 mtime > 소스 확인.

- [ ] **Step 7: 스폰 실측 (tmp 체크)**

Write 도구로 `engine/tools/probes/mgr_t6_spawn.ps1` (ASCII-only):
```powershell
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3
$escaped = '{"tool":"launch_app","args":{"app":"agentmgr"}}' -replace '"', '\"'
& $exe agentctl $escaped | Out-Null
Start-Sleep -Seconds 3
$escaped2 = '{"tool":"list_windows","args":{}}' -replace '"', '\"'
$wins = & $exe agentctl $escaped2
if ($wins -match 'Agent Manager') { Write-Host "t6-spawn: PASS" } else { Write-Host "t6-spawn: FAIL $wins" }
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
```
Run + Expected: `t6-spawn: PASS`.

- [ ] **Step 8: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/include/apps/ClientAgentMgrApp.h engine/src/apps/ClientAgentMgrApp.cpp engine/src/apps/JKAppModule_agentmgr.cpp engine/CMakeLists.txt engine/assets/icons/launcher_agentmgr@1x.png engine/assets/icons/launcher_agentmgr@2x.png
git commit -m "feat(agentmgr): jkapp_agentmgr 4-tab GUI + jkx + icons (spec 1.1)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 7: probe_agentmgr.ps1 + 전체 회귀 + docs/53 + 스펙 §8 표기

**Files:**
- Create: `engine/tools/probes/probe_agentmgr.ps1` (커밋함 — 공식 회귀 프로브)
- Create: `docs/53_desktop_agent_mgr.md`
- Modify: `docs/superpowers/specs/2026-09-16-agent-manager-design.md` (§8 구현 표기 + 델타 백필)

**Interfaces:**
- Consumes: Tasks 1-6 전부; 기존 프로브 선례 `probe_agent_trust.ps1`/`probe_agent_triggerctl.ps1` (Invoke-Agentctl 패턴, subscriber-before-approve, Start-Job).
- Produces: docs/53 as-built + 회귀 GREEN 증명 (최종리뷰 입력).

- [ ] **Step 1: probe_agentmgr.ps1 (스펙 §6 8체크)**

`engine/tools/probes/probe_agentmgr.ps1` — ASCII-only (레슨 50). 스폰 전 가드: `permissions.json` 존재 시 abort(테스트가 만든 파일만 다룬다), `trust.json`/`receipts.jsonl`은 있으면 %TEMP% 백업 후 종료 시 복원. jktriggers 서브스크라이버 선행 기동(레슨 28 — 푸시 리플레이 없음). 스펙 §6 체크 8건 순서: spawn → shape → permission_set E2E → trust_revoke E2E → not_found/bad_fingerprint → installed_list → read_receipts(부재+시딩) → trigger 회귀. Write 도구로 생성:
```powershell
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
$root = Split-Path $exe
$permFile = Join-Path $root "permissions.json"
$state = Join-Path $root "state"
$trust = Join-Path $state "trust.json"
$rcpt = Join-Path $state "receipts.jsonl"
$script:fail = 0

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host "ok: $name" } else { Write-Host "FAIL: $name"; $script:fail++ }
}

# --- guard: permissions.json must not pre-exist (test owns its lifecycle)
if (Test-Path $permFile) { Write-Host "ABORT - permissions.json exists"; exit 1 }
$hadTrust = Test-Path $trust
$hadRcpt = Test-Path $rcpt
if ($hadTrust) { Copy-Item $trust (Join-Path $env:TEMP "trust_pre_mgr.json") -Force }
if ($hadRcpt) { Copy-Item $rcpt (Join-Path $env:TEMP "rcpt_pre_mgr.json") -Force }

# --- seed trust fake record (revoked later in check 4)
$fp = "sha256:" + ("ab" * 32)
$fakeRec = ",{`"fingerprint`":`"$fp`",`"name`":`"mgrprobe`",`"source`":`"dev`",`"ts`":1700000000000}"
if (-not $hadTrust) {
    New-Item -ItemType Directory -Force -Path $state | Out-Null
    Set-Content -Path $trust -Value ("{`"records`":[{`"fingerprint`":`"$fp`",`"name`":`"mgrprobe`",`"source`":`"dev`",`"ts`":1700000000000}]}") -Encoding UTF8
} else {
    $cur = Get-Content $trust -Raw
    $cur = $cur -replace '\]\s*}$', ($fakeRec + "]}")
    Set-Content -Path $trust -Value $cur -Encoding UTF8
}

Get-Process jkdesktop,jktriggers,jkapp_agentmgr -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 3
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) -WindowStyle Hidden
Start-Sleep -Seconds 3

# --- 1. spawn
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"agentmgr"}}' | Out-Null
Start-Sleep -Seconds 4
$wins = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
Check "1-spawn" ($wins -match 'Agent Manager')

# --- 2. agent_permissions shape (permissions.json absent)
$p = Invoke-Agentctl '{"tool":"agent_permissions","args":{}}'
Check "2-shape" ($p -match '"tool":"close_window","gate":"server","file":"","effective":"deny","default":"deny"' -and $p -match '"tool":"permission_set","gate":"server\(fixed\)","file":"","effective":"ask"')

# --- 3. permission_set E2E (parked query in a job; approve request 1)
$job = Start-Job -ScriptBlock {
    param($e)
    $x = '{"tool":"permission_set","args":{"tool":"close_window","decision":"allow"}}' -replace '"', '\"'
    & $e agentctl $x
} -ArgumentList $exe
Start-Sleep -Seconds 3
Invoke-Agentctl '{"tool":"approve","args":{"request":1,"decision":"allow"}}' | Out-Null
Start-Sleep -Seconds 2
$reply = Receive-Job $job -Wait
Check "3a-reply" ($reply -match '"written":true')
$fraw = Get-Content $permFile -Raw -ErrorAction SilentlyContinue
Check "3b-file" ($fraw -match '"close_window":"allow"')
$p2 = Invoke-Agentctl '{"tool":"agent_permissions","args":{}}'
Check "3c-filefield" ($p2 -match '"tool":"close_window","gate":"server","file":"allow"')

# --- 4. trust_revoke E2E (approve request 2)
$job2 = Start-Job -ScriptBlock {
    param($e, $f)
    $x = '{"tool":"trust_revoke","args":{"fingerprint":"' + $f + '"}}' -replace '"', '\"'
    & $e agentctl $x
} -ArgumentList $exe, $fp
Start-Sleep -Seconds 3
Invoke-Agentctl '{"tool":"approve","args":{"request":2,"decision":"allow"}}' | Out-Null
Start-Sleep -Seconds 2
$reply2 = Receive-Job $job2 -Wait
Check "4a-reply" ($reply2 -match '"restart_needed":true')
$tafter = Get-Content $trust -Raw -ErrorAction SilentlyContinue
Check "4b-removed" ($tafter -notmatch $fp)
Check "4c-bak" ((Test-Path ($trust + ".bak")) -and ((Get-Content ($trust + ".bak") -Raw) -match $fp))

# --- 5. trust_revoke immediate errors
$nf = Invoke-Agentctl ('{"tool":"trust_revoke","args":{"fingerprint":"sha256:' + ("cd" * 32) + '"}}')
Check "5a-notfound" ($nf -match 'not_found')
$bf = Invoke-Agentctl '{"tool":"trust_revoke","args":{"fingerprint":"sha256:xyz"}}'
Check "5b-badfp" ($bf -match 'bad_fingerprint')

# --- 6. installed_list
$il = Invoke-Agentctl '{"tool":"installed_list","args":{}}'
Check "6-installed" ($il -match '"name":"sampletodo","kind":"console"' -and $il -match '"name":"minesweeper","kind":"jkx"')

# --- 7. read_receipts: absent -> []; seed 2 rows -> reverse tail + limit cap
if ($hadRcpt) { Remove-Item $rcpt -Force }
$r0 = Invoke-Agentctl '{"tool":"read_receipts","args":{}}'
Check "7a-empty" ($r0 -match '"rows":\[\]')
Set-Content -Path $rcpt -Value ('{"ts":1700000001000,"tool":"first_tool","result":"{\"ok\":true}"}' + [char]10 + '{"ts":1700000002000,"tool":"second_tool","result":"{\"ok\":false,\"error\":\"x\"}"}') -Encoding UTF8
$r1 = Invoke-Agentctl '{"tool":"read_receipts","args":{}}'
$r2 = Invoke-Agentctl '{"tool":"read_receipts","args":{"limit":1}}'
Check "7b-reverse" ($r1 -match '"tool":"second_tool"' -and ($r1.IndexOf("second_tool") -lt $r1.IndexOf("first_tool")))
Check "7c-cap" ($r2 -match '"tool":"second_tool"' -and $r2 -notmatch 'first_tool')

# --- 8. trigger toggle regression
$tl = Invoke-Agentctl '{"tool":"trigger_list","args":{}}'
$tn = [regex]::Match($tl, '"name":"([^"]+)"').Groups[1].Value
$te = [int][regex]::Match($tl, '"name":"' + $tn + '","enabled":(\d)').Groups[1].Value
if ($tn -ne "") {
    Invoke-Agentctl ('{"tool":"trigger_toggle","args":{"name":"' + $tn + '","on":' + (1 - $te) + '}}') | Out-Null
    $tl2 = Invoke-Agentctl '{"tool":"trigger_list","args":{}}'
    $te2 = [int][regex]::Match($tl2, '"name":"' + $tn + '","enabled":(\d)').Groups[1].Value
    Check "8-toggle" ($te2 -eq (1 - $te))
    Invoke-Agentctl ('{"tool":"trigger_toggle","args":{"name":"' + $tn + '","on":' + $te + '}}') | Out-Null
} else { Check "8-toggle" $false }

# --- cleanup: kill, restore seeded files
Get-Process jkdesktop,jktriggers,jkapp_agentmgr -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $permFile -Force -ErrorAction SilentlyContinue
if ($hadTrust -and (Test-Path (Join-Path $env:TEMP "trust_pre_mgr.json"))) {
    Copy-Item (Join-Path $env:TEMP "trust_pre_mgr.json") $trust -Force
} elseif (-not $hadTrust) {
    Remove-Item $trust, ($trust + ".bak") -Force -ErrorAction SilentlyContinue
}
if ($hadRcpt -and (Test-Path (Join-Path $env:TEMP "rcpt_pre_mgr.json"))) {
    Copy-Item (Join-Path $env:TEMP "rcpt_pre_mgr.json") $rcpt -Force
} else {
    Remove-Item $rcpt -Force -ErrorAction SilentlyContinue
}
if ($script:fail -gt 0) { Write-Host "probe_agentmgr: $script:fail FAILURES"; exit 1 }
Write-Host "probe_agentmgr: ALL PASS"
```
전제 노트: (a) request id 1/2 = 서버 기동 후 첫/둘째 파킹(nextApprovalId_=1) — 체크 5의 즉답 요청은 파킹하지 않으므로 카운터를 오염하지 않는다. (b) 체크 3/4의 파킹 쿼리만 Start-Job(블로킹), 즉답 도구는 메인 폴링. (c) trigger_list 행 직렬화에 `"name":"...","enabled":N` 순서를 전제 — 서버 기존 형식 확인 후 정규식 조정. (d) sampletodo/minesweeper는 빌드 트리 기본 설치 — 누락 시 6은 FAIL로 두고 사유 기록.

Run: `powershell -ExecutionPolicy Bypass -File I:\progwork\JKENGINE\engine\tools\probes\probe_agentmgr.ps1`
Expected: `ok:` 15건(1, 2, 3a/3b/3c, 4a/4b/4c, 5a/5b, 6, 7a/7b/7c, 8), `probe_agentmgr: ALL PASS`, 종료 코드 0.

- [ ] **Step 2: 전체 회귀**

```bash
cd I:/progwork/JKENGINE/engine/build
PATH=/c/msys64/ucrt64/bin cmake --build . --target jkdesktop jkagentd
PATH=/c/msys64/ucrt64/bin cmake --build . --target jkx_packages
cd I:/progwork/JKENGINE
powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_agent_mcp.ps1
powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_agent_trust.ps1
powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_agent_triggerctl.ps1
powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_agent_chat.ps1
./engine/build/jkagentd.exe --selftest
./engine/build/jkdesktop.exe --test
```
Expected: mcp tools/list에 신규 5종 포함(카운트 갱신 반영), trust/triggerctl PASS, chat PASS(Task 5가 jkchat 코드를 손댔다 — 스펙 §6 회귀 명세), selftest 0, test 0. probe_agent_chat.ps1 파일명은 기존 probes 디렉토리에서 확인 후 사용(Glob `engine/tools/probes/probe_agent*.ps1`).

- [ ] **Step 3: docs/53_desktop_agent_mgr.md**

골격(왜를 기록 — docs are deliverables):
- §1 요약: 4탭 매니저 + 서버 도구 5종 + 브로커 갱신.
- §2 게이트 뱃지 모델: §2.1 조사 결론(서버 3도구만 게이트, capture_window 주석 부정확 → 주석 정정) — 이것이 이 문서의 핵심 "왜".
- §3 permission_set 안전 설계: 하드코딩 Ask(2단 우회 봉쇄) + 알려진 키 전재기록(RMW 복구 효과).
- §4 trust_revoke: DoS 통로 논의 → Ask 기본, raw text surgery 이유(AgentJson 재직렬화 ts 손실), .bak 신규 적용.
- §5 파서 계약 델타: read_receipts ok="0"/"1" 문자열 + ts 초 단위, trust_list "fp" 전체 지문, 트리거 topics 열 포기(2레벨 리더).
- §6 as-built 파일 목록 + 커밋 해시들.
- §7 프로브 결과 + 회귀 결과.
- §8 레슨(새로 발견된 것만).

- [ ] **Step 4: 스펙 §8 + 델타 백필**

`docs/superpowers/specs/2026-09-16-agent-manager-design.md` §8:
```
- 커밋: <Task별 커밋 해시 나열>
- docs: docs/53_desktop_agent_mgr.md
- 프로브: probe_agentmgr.ps1 (8/8) + 회귀 GREEN
```
§2.1/§2.5에 델타 명시(구현과 스펙 정합): file:"" 규약, rows.ok 문자열, ts 초 단위, trust_list "fp".

- [ ] **Step 5: Commit**

```bash
cd I:/progwork/JKENGINE
git add engine/tools/probes/probe_agentmgr.ps1 docs/53_desktop_agent_mgr.md docs/superpowers/specs/2026-09-16-agent-manager-design.md
git commit -m "test(agentmgr): probe 8-check + docs/53 as-built (spec 6)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

- [ ] **Step 6: 최종리뷰 (opus)**

기존 관례대로 최강 모델 리뷰 — diff 범위: Tasks 1-7 전체 커밋. APPROVE 시 세션 종료(메모리 갱신).

<!-- plan-continued -->