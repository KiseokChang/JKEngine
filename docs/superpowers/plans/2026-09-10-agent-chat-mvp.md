# Desktop Agent 채팅창 MVP 구현 계획 (슬래시 + 승인 UX)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 순수 Win32 별도 프로세스 `jkchat.exe` — JKWindow/SDL 체계 밖의 내부 에이전트 채팅창. 슬래시 결정적 커맨드 + 에이전트 조작 요청(close_window)의 인라인 승인 프롬프트(Allow/Deny).

**Architecture:** 채팅창은 control-only 클라이언트(M1 `JKAgentClient` 재사용, 비동기 쿼리 추가)로 서버에 붙는다. 승인 보류 상태는 **서버**가 갖는다: permissions.json의 `ask` 모드에서 close_window 요청이 오면 서버는 응답을 보류하고 `agent.approval_request` 이벤트를 방송, 채팅창의 `approve` 도구 호출로 완료한다. 요청자(브로커/팔레트)는 결과 응답을 기다린다 — 브로커 변경은 ask 패스스루뿐.

**Tech Stack:** Win32(W API, UTF-8↔UTF-16 변환 — 한글 렌더링 가능), jkcore(JKAgentClient/JKAgentJson), 파이프 IPC, PowerShell probes.

**Spec:** `docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md` §5(승인: "인라인 승인 UX"), §9(undo/승인/receipt 3중 방어); 사용자 제약(M1 계획 말미): "내부 에이전트 채팅창은 JKWindow/SDL 체계 밖" — STT 확장 여지. 사용자 결정(AskUserQuestion): 두뇌 = 슬래시+승인UX(LLM은 다음 계획), 진입점 = 팔레트 /chat.

## Global Constraints

- **채팅창은 JKWindow/JKControls/ImGui를 절대 사용하지 않는다** — 순수 Win32(GDI 컨트롤). 별도 프로세스(exe)로, 서버에서 CreateProcess로 스폰.
- **JKAgentClient 재사용** — 새 채널 코드를 만들지 않고 M1 클래스에 `SendQuery`/`PollReply`(비동기)만 추가. 기존 `QueryRaw`의 블로킹 계약 유지.
- `HandleAgentQuery`는 clientsMutex_ 보유 전제 — 승인 보류/완료도 같은 잠금 하에서만. `FindClientById` 등 내부 락킹 헬퍼 호출 금지.
- 서버 상태는 `<exeDir>\state\` + permissions.json(exe 옆) 관례. permissions 3값: `allow`/`ask`/`deny`(무기록=close deny, 타 tool allow).
- jkagentd의 권한 게이트는 `ask`를 통과시킨다(서버가 승인 절차 수행).
- 빌드: `cd engine && export PATH="/c/msys64/ucrt64/bin:$PATH" && cmake --build build --target <t>`; 회귀: `jkdesktop test` / `jkagentd --selftest` / probe 3종.
- PowerShell 프로브 관례 + **lesson 25/26**: 함수 인자 문자열 결합은 변수에 먼저 만들 것; 네이티브 exe 인자의 큰따옴표는 `-replace '"','\"'`.
- 커밋: task-per-commit, `Co-Authored-By: Claude Code <noreply@anthropic.com>`, main push.
- 승인 타임아웃 60초 고정(MVP); 만료 = 응답 `approval_timeout` + resolved 이벤트.

---

### Task 1: 서버 — ask 모드 + 승인 파이프라인

**Files:**
- Modify: `engine/include/server/JKWindowServer.h` (AgentDecision enum, PendingApproval, PushAgentEventJson, SpawnProcess)
- Modify: `engine/src/server/JKWindowServer.cpp` (AgentToolAllowed enum화, close_window ask 경로, approve 도구, 만료 검사, PushAgentEventJson)

**Interfaces:**
- Consumes: M2a의 AgentToolAllowed/permissions.json, PushAgentEvent.
- Produces:
  - `enum class AgentDecision { Allow, Ask, Deny }` + `AgentDecision AgentToolAllowed(const std::string&) const`
  - `void PushAgentEventJson(const std::string& json)` — 호출자 clientsMutex_ 보유. (PushAgentEvent가 내부에서 재사용)
  - 와이어: `{"tool":"approve","args":{"request":N,"decision":"allow"|"deny"}}`; 이벤트 `agent.approval_request`(request/tool/target_title/target_id/ts)와 `agent.approval_resolved`(request/decision)
  - Task 5가 쓰는 `void SpawnProcess(const char* exeName, const std::string& args)` (SpawnClient의 코어 추출)

- [ ] **Step 1: 헤더 — enum/보류 구조/메서드**

```cpp
// jk::server namespace, before JKWindowServer class:
enum class AgentDecision { Allow, Ask, Deny };
```
클래스 private에:
```cpp
// Pending approval (M2 chat): an "ask"-gated AgentQuery parked until the
// chat window (any subscriber) resolves it with the approve tool.
struct PendingApproval {
    uint32_t requestId = 0;
    uint32_t queryId = 0;        // AgentQuery to complete on resolution
    uint32_t requesterId = 0;    // requesting connection's id (pointer-safe)
    uint32_t targetId = 0;
    time_t expiresAt = 0;
};
std::vector<PendingApproval> pendingApprovals_;
uint32_t nextApprovalId_ = 1;
// Push a fully-formed agent event JSON (topic included) to subscribers.
// Caller holds clientsMutex_.
void PushAgentEventJson(const std::string& json);
```
(<ctime> include 확인 — PushAgentEvent가 이미 std::time 사용.)

- [ ] **Step 2: AgentToolAllowed enum화**

```cpp
AgentDecision JKWindowServer::AgentToolAllowed(const std::string& tool) const {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    const std::string path = dir + "\\permissions.json";
    const bool defaultAllowed = (tool != "close_window");
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return defaultAllowed ? AgentDecision::Allow : AgentDecision::Deny;
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson perm(buf);
    std::string value;
    if (!perm.ok() || !perm.GetStr(tool.c_str(), value)) {
        return defaultAllowed ? AgentDecision::Allow : AgentDecision::Deny;
    }
    if (value == "allow") return AgentDecision::Allow;
    // "ask" = inline approval (chat window). The approval pipeline is wired
    // for close_window only; other tools degrade to allow.
    if (value == "ask") return (tool == "close_window") ? AgentDecision::Ask
                                                        : AgentDecision::Allow;
    return value == "deny" ? AgentDecision::Deny
                           : (defaultAllowed ? AgentDecision::Allow
                                             : AgentDecision::Deny);
}
```
기존 close_window 분기의 `AgentToolAllowed("close_window") == 허용` 비교를 enum에 맞게 갱신.

- [ ] **Step 3: close_window ask 경로 (비동기)**

HandleAgentQuery의 마지막 무조건 응답을 게이트로 바꾼다:
```cpp
bool replied = true;
... // ask 경로에서 replied = false
if (replied) {
    ipc::WriteAgentJson(client.Transport(), ipc::MsgType::AgentReply,
                        queryId, 1, reply);
}
```
close_window 분기 재구성:
```cpp
} else if (tool == "close_window") {
    int id = 0;
    JKClientConnection* target = nullptr;
    if (req.GetObjInt("args", "id", id)) {
        for (auto& c : clients_) {
            if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
        }
    }
    if (!target || target->IsControlOnly()) {
        reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
    } else {
        switch (AgentToolAllowed("close_window")) {
            case AgentDecision::Allow: {
                ipc::WriteMessage(target->Transport(), ipc::MsgType::Close,
                                  std::vector<uint8_t>{});
                reply = "{\"ok\":true}";
                break;
            }
            case AgentDecision::Ask: {
                bool subscriber = false;
                for (auto& c : clients_) {
                    if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
                        subscriber = true; break;
                    }
                }
                if (!subscriber) {
                    reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                    break;
                }
                PendingApproval p;
                p.requestId = nextApprovalId_++;
                p.queryId = queryId;
                p.requesterId = client.Id();
                p.targetId = target->Id();
                p.expiresAt = std::time(nullptr) + 60;
                char buf[640];
                std::snprintf(buf, sizeof(buf),
                    "{\"topic\":\"agent.approval_request\",\"request\":%u,"
                    "\"tool\":\"close_window\",\"target_id\":%u,"
                    "\"title\":\"%s\",\"ts\":%lld}",
                    p.requestId, p.targetId, JsonEsc(target->Title()).c_str(),
                    static_cast<long long>(std::time(nullptr)) * 1000);
                pendingApprovals_.push_back(p);
                PushAgentEventJson(buf);
                replied = false;   // answered when the approval resolves
                break;
            }
            case AgentDecision::Deny:
            default:
                reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                break;
        }
    }
}
```

- [ ] **Step 4: approve 도구 + PushAgentEventJson + 만료**

```cpp
} else if (tool == "approve") {
    int request = 0;
    std::string decision;
    req.GetObjInt("args", "request", request);
    req.GetObjStr("args", "decision", decision);
    const bool allow = (decision == "allow");
    bool resolved = false;
    for (auto it = pendingApprovals_.begin(); it != pendingApprovals_.end(); ++it) {
        if (it->requestId != static_cast<uint32_t>(request)) continue;
        resolved = true;
        if (allow) {
            for (auto& c : clients_) {
                if (c && c->Id() == it->targetId && !c->IsDisconnected()) {
                    ipc::WriteMessage(c->Transport(), ipc::MsgType::Close,
                                      std::vector<uint8_t>{});
                    break;
                }
            }
        }
        // Complete the parked agent query (the requester kept blocking).
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
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "{\"topic\":\"agent.approval_resolved\",\"request\":%u,"
            "\"decision\":\"%s\"}", it->requestId,
            allow ? "allow" : "deny");
        PushAgentEventJson(buf);
        pendingApprovals_.erase(it);
        reply = allow ? "{\"ok\":true,\"approved\":true}"
                      : "{\"ok\":true,\"approved\":false}";
        break;
    }
    if (!resolved) reply = "{\"ok\":false,\"error\":\"unknown_request\"}";
}
```
PushAgentEventJson (PushAgentEvent이 마지막 루프만 위임하도록 리팩터):
```cpp
void JKWindowServer::PushAgentEventJson(const std::string& json) {
    for (auto& c : clients_) {
        if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
            ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentEvent,
                                0, 1, json);
        }
    }
}
```
만료 검사 — ProcessPendingMessages 마지막(같은 clientsMutex_ 잠금 하)에:
```cpp
const time_t now = std::time(nullptr);
for (auto it = pendingApprovals_.begin(); it != pendingApprovals_.end();) {
    if (now < it->expiresAt) { ++it; continue; }
    for (auto& c : clients_) {
        if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
            ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                it->queryId, 0,
                                "{\"ok\":false,\"error\":\"approval_timeout\"}");
            break;
        }
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"topic\":\"agent.approval_resolved\",\"request\":%u,"
        "\"decision\":\"timeout\"}", it->requestId);
    PushAgentEventJson(buf);
    it = pendingApprovals_.erase(it);
}
```

- [ ] **Step 5: 빌드 + 회귀**

Run: `cmake --build build --target jkdesktop jkagentd` → 0 errors.
Run: `./build/jkdesktop.exe test` → 0; `probe_agent_e2e.ps1` → 7/7 (allow 모드 불변), `probe_agent_palette.ps1` → 4/4.

- [ ] **Step 6: Commit**

```bash
git add engine/include/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp
git commit -m "feat(server): approval pipeline for ask-gated close_window (chat MVP)"
```

---

### Task 2: jkagentd — ask 패스스루

**Files:**
- Modify: `engine/tools/jkagentd/main.cpp` (LoadPermissions 게이트)

**Interfaces:**
- Consumes/Produces: permissions.json 값 `allow`/`ask`/`deny` — ask는 브로커를 통과시켜 서버(승인 파이프라인)로.

- [ ] **Step 1: 게이트 조건 변경**

현재(도구별 allow 판정)을 찾아:
```cpp
allowed = (value == "allow" || value == "ask");
// "ask" is not a deny — the server runs the inline-approval pipeline and
// the broker's query simply blocks until the user (or a timeout) resolves.
```

- [ ] **Step 2: 빌드 + 회귀**

Run: `cmake --build build --target jkagentd`; `jkagentd --selftest` → 0; `probe_agent_mcp.ps1` → 5/5 (deny 기본 유지 확인).

- [ ] **Step 3: Commit**

```bash
git add engine/tools/jkagentd/main.cpp
git commit -m "feat(agentd): permissions 'ask' passes through to the server gate (chat MVP)"
```

---

### Task 3: JKAgentClient — 비동기 쿼리 (SendQuery/PollReply)

**Files:**
- Modify: `engine/include/agent/JKAgentClient.h` / `engine/src/agent/JKAgentClient.cpp`

**Interfaces:**
- Produces (Task 4 사용):
  - `struct AgentReplyMsg { uint32_t queryId; std::string json; }` (jk::agent)
  - `uint32_t SendQuery(const std::string& tool, const std::string& argsJson)` — 즉시 반환, queryId(0=실패)
  - `bool PollReply(uint32_t queryId, std::string& jsonOut)` — 큐에서 해당 응답 회수
- 기존 `QueryRaw` 계약 불변 (pendingReplies_를 id 포함 구조로 바꾸되 내부만).

- [ ] **Step 1: 헤더 + 구현**

```cpp
struct AgentReplyMsg {
    uint32_t queryId = 0;
    std::string json;
};
// Non-blocking query (chat MVP): send and return at once — the reply
// arrives during later pumps (ping round-trips) and is picked up with
// PollReply. Used when the tool result may be gated behind an inline
// approval, which must not freeze the chat UI.
uint32_t SendQuery(const std::string& tool, const std::string& argsJson);
bool PollReply(uint32_t queryId, std::string& jsonOut);
```
구현:
```cpp
uint32_t JKAgentClient::SendQuery(const std::string& tool,
                                  const std::string& argsJson) {
    return SendRaw("{\"tool\":\"" + tool + "\",\"args\":" + argsJson + "}");
}

uint32_t JKAgentClient::SendRaw(const std::string& requestJson) {
    if (!transport_ || !transport_->IsConnected()) return 0;
    const uint32_t id = nextQueryId_++;
    if (!ipc::WriteAgentJson(*transport_, ipc::MsgType::AgentQuery,
                             id, 0, requestJson)) {
        return 0;
    }
    return id;
}

bool JKAgentClient::PollReply(uint32_t queryId, std::string& jsonOut) {
    for (auto it = pendingReplies_.begin(); it != pendingReplies_.end(); ++it) {
        if (it->queryId == queryId) {
            jsonOut = std::move(it->json);
            pendingReplies_.erase(it);
            return true;
        }
    }
    return false;
}
```
private 멤버를 `std::deque<AgentReplyMsg> pendingReplies_;`로 교체하고 기존 PumpMessages/QueryRaw의 대기 로직을 id 매칭으로 수정(기존 구조 그대로, 요소 타입만 변경). `SendRaw`도 헤더에 선언.

- [ ] **Step 2: 빌드 + 회귀**

Run: `cmake --build build --target jkdesktop jkagentd`; `jkdesktop test` → 0; `probe_agent_mcp.ps1` → 5/5 (agentctl/브로커 블로킹 경로 회귀).

- [ ] **Step 3: Commit**

```bash
git add engine/include/agent/JKAgentClient.h engine/src/agent/JKAgentClient.cpp
git commit -m "feat(agent): non-blocking SendQuery/PollReply on JKAgentClient (chat MVP)"
```

---

### Task 4: jkchat.exe — Win32 채팅창 + 승인 UI

**Files:**
- Create: `engine/tools/jkchat/main.cpp`
- Modify: `engine/CMakeLists.txt` (jkagentd 타깃 근처에 jkchat 실행 파일)

**Interfaces:**
- Consumes: Task 1의 approve 도구/이벤트, Task 3의 SendQuery/PollReply, M1의 JKAgentClient(Connect/SubscribeEvents/Query/PollEvents).
- Produces: `jkchat.exe` (GUI 서브시스템, 타이틀 "Agent Chat") — Task 5의 launch_chat이 스폰.

UI(순수 Win32, 한글 가능):
- 트랜스크립트: multiline 읽기전용 EDIT + WS_VSCROLL
- 입력: EDIT + "보내기" 버튼 (Enter는 입력 EDIT 서브클래싱 VK_RETURN)
- 승인 영역: static 프롬프트 + [허용][거부] 버튼 — 요청 대기 중에만 표시
- 이벤트(승인 요청/해결, window.created/destroyed/focused)는 트랜스크립트에 시스템 줄로

- [ ] **Step 1: CMake 타깃**

jkagentd 타깃 옆:
```cmake
# Agent chat window (chat MVP): plain Win32 UI outside the JKWindow/SDL
# system (user constraint — keeps the door open for STT later).
add_executable(jkchat WIN32 tools/jkchat/main.cpp)
target_link_libraries(jkchat PRIVATE jkcore)
```
(WIN32 = WinMain 진입, 콘솔 없음.)

- [ ] **Step 2: main.cpp 골격**

핵심 구조 (전체 코드는 구현 시 작성 — 주요 결정만 고정):
- `JKAgentClient g_agent;` 전역, WM_CREATE에서 Connect + SubscribeEvents(true) + SetTimer(hwnd, 1, 400ms)
- UTF-8↔UTF-16 변환 헬퍼 (MultiByteToWideChar/WideCharToMultiByte, CP_UTF8)
- 컨트롤 id: IDC_INPUT=101, IDC_SEND=102, IDC_LOG=103, IDC_ALLOW=104, IDC_DENY=105, IDC_PROMPT=106
- WM_TIMER: `g_agent.Query("ping","{}",r)` 펌프 → PollEvents로 이벤트 처리 → PollReply로 미결 응답 표시
- 슬래시 커맨드: palette와 동일 집합 (/help /list /launch /close /save /restore /undo) — /close는 **SendQuery로 비동기**(ask 게이트 대기 중에도 UI 생존), 나머지도 SendQuery+PollReply로 통일
- NL 입력: "자연어 위임은 다음 단계(LLM 연동) — 지금은 슬래시 커맨드를 쓰세요." 안내
- 승인: `agent.approval_request` 이벤트 → requestId 저장 + 프롬프트/버튼 표시 → [허용]/[거부] → `{"tool":"approve","args":{"request":N,"decision":"allow"|"deny"}}` → `agent.approval_resolved` 이벤트로 프롬프트 정리 + 트랜스크립트 기록
- 타이머 펌프 중 블로킹 쿼리(ping)는 수 ms — UI 정지 체감 없음 (M1 RunAgentEvents와 같은 패턴)

- [ ] **Step 3: 빌드 + 스모크**

Run: `cmake --build build --target jkchat jkdesktop`.
Run: 서버 + minesweeper + `./jkchat.exe` → 창 등장, `/list` 결과 표시, `/launch tetris` → 창 생성 + window.created 시스템 라인 표시.

- [ ] **Step 4: Commit**

```bash
git add engine/CMakeLists.txt engine/tools/jkchat/main.cpp
git commit -m "feat(chat): Win32 agent chat window with slash commands (chat MVP)"
```

---

### Task 5: 승인 E2E — permissions "ask"로 닫기 승인 흐름

**Files:**
- Modify: `engine/tools/jkchat/main.cpp` (Task 4에서 승인 UI까지 포함해 구현 — 본 태스크는 검증)

**Interfaces:**
- Consumes: Task 1/4 전부.
- Produces: 검증된 승인 흐름 (probe_agent_chat.ps1이 Task 6에서 자동화).

- [ ] **Step 1: 수동 시나리오**

`permissions.json` = `{"close_window":"ask"}`; 서버 + minesweeper + jkchat:
1. 채팅에서 `/close <id>` → 프롬프트 "…를 닫을까요?" 표시
2. [허용] → 창 소멸 + 트랜스크립트 `{"ok":true}`
3. 다른 앱 /close → [거부] → `denied_by_user`, 창 생존
4. 승인 없이 방치 → 60초 후 `approval_timeout`
5. 팔레트(Alt+Space)에서 `/close <id>` → 채팅창에 프롬프트 표시 (얼굴이 다른 승인 위임) → [허용] 동작

- [ ] **Step 2: 회귀**

`probe_agent_e2e.ps1` (allow 모드) → 7/7; 기본 deny → permission_denied (probe_agent_mcp의 deny 체크).

- [ ] **Step 3: Commit** (Task 4와 함께 스쿼시 없이 — 수정분만)

```bash
git add engine/tools/jkchat/main.cpp
git commit -m "fix(chat): approval prompt resolution paths verified (chat MVP)"
```

---

### Task 6: launch_chat 도구 + 팔레트 /chat + 프로브 + 문서

**Files:**
- Modify: `engine/include/server/JKWindowServer.h` / `engine/src/server/JKWindowServer.cpp` (SpawnProcess 추출 + launch_chat 도구)
- Modify: `engine/src/apps/ClientPaletteApp.cpp` (/chat)
- Create: `engine/tools/probes/probe_agent_chat.ps1`
- Create: `docs/31_desktop_agent_chat.md`
- Modify: 스펙 헤더 상태줄, `docs/30_desktop_agent_m2a.md` §6

**Interfaces:**
- Consumes: Task 4의 jkchat.exe.
- Produces: `{"tool":"launch_chat"}` 도구 (MCP/agentctl/팔레트 모두 사용 가능).

- [ ] **Step 1: SpawnProcess 추출 + launch_chat**

SpawnClient의 CreateProcess 코어를 `void SpawnProcess(const char* exeName, const std::string& args)`로 추출(SpawnClient은 래퍼), launch_chat 도구:
```cpp
} else if (tool == "launch_chat") {
    SpawnProcess("jkchat.exe", "");
    reply = "{\"ok\":true}";
}
```
(500ms 스로틀은 lastSpawnTimes_["chat"] 재사용 — SpawnProcess 내부.)

- [ ] **Step 2: 팔레트 /chat**

ClientPaletteApp Submit에:
```cpp
} else if (cmd == "chat") {
    SendTool("launch_chat", "{}");
}
```
/help 목록에 `/chat` 추가.

- [ ] **Step 3: probe_agent_chat.ps1**

```powershell
# Chat MVP probe: ask-gated close → approval via the approve tool → resolved.
# The chat window itself is UI (eyeball item); the probe drives the wire.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# ask mode IS the approval act for this probe
$permFile = Join-Path (Split-Path $exe) "permissions.json"
'{"close_window":"ask"}' | Set-Content -Path $permFile -Encoding ASCII

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4
Start-Process -FilePath $exe -ArgumentList "--client","palette" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 2
# the chat window joins as the approval surface
Start-Process -FilePath (Join-Path (Split-Path $exe) "jkchat.exe") `
    -WorkingDirectory (Split-Path $exe)
Start-Sleep -Seconds 2

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    $out = & $exe agentctl $escaped
    return ($out -join "`n")
}

$launch = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}'
Start-Sleep -Seconds 2
$list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$id = $null
if ($list -match '"id\\?":(\d+),"title":"Minesweeper"') { $id = $Matches[1] }

# close under ask mode: BLOCKS until approved — run as a job.
$closeLine = '{"tool":"close_window","args":{"id":' + $id + '}}'
$job = Start-Job -ScriptBlock {
    param($e, $line)
    $escaped = $line -replace '"', '\"'
    (& $e agentctl $escaped) -join "`n"
} -ArgumentList $exe, $closeLine

# capture the approval request id from the event stream
$events = & $exe agent-events 5
$events = ($events -join "`n")
$reqId = $null
if ($events -match '"request\\?":(\d+)') { $reqId = $Matches[1] }

$approveLine = '{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}'
$approve = Invoke-Agentctl $approveLine
$closeOut = Receive-Job -Job $job -Wait | Out-String
Start-Sleep -Seconds 2
$list2 = Invoke-Agentctl '{"tool":"list_windows","args":{}}'

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $permFile -ErrorAction SilentlyContinue

$ok = $true
if ($approve -match 'approved\\?":true') { Write-Host "approve: PASS" }
else { $ok = $false; Write-Host "approve: FAIL $approve" }
if ($closeOut -match 'ok\\?":true') { Write-Host "close-resolved: PASS" }
else { $ok = $false; Write-Host "close-resolved: FAIL $closeOut" }
if ($id -and $list2 -notmatch ('"id\\?":' + $id + '\b')) { Write-Host "gone: PASS" }
else { $ok = $false; Write-Host "gone: FAIL" }
if ($ok) { Write-Host "PASS: agent chat"; exit 0 } else { Write-Host "FAIL: agent chat"; exit 1 }
```
(job 변수명 $closeOut 일치 주의 — 작성 시 통일.)

- [ ] **Step 4: 전체 회귀**

`jkdesktop test` 0 / `jkagentd --selftest` 0 / `probe_agent_mcp` 5/5 / `probe_agent_e2e` 7/7 / `probe_agent_palette` 4/4 / `probe_agent_chat` PASS.

- [ ] **Step 5: docs/31 + 상태 갱신 + Commit + push**

docs/31: 아키텍처(체팅창=control-only+Win32, 승인 파이프라인), permissions 3값 모델, 사용법(/chat → 채팅창, 슬래시, 승인 버튼), 제한(NL 위임 미구현 — LLM 연동 다음 계획, 타임아웃 60초 고정), STT 확장 여지(별도 프로세스), M2.5 연결.
스펙 헤더: "2단계: 커맨드 바(M2a) + 채팅창(승인 UX) 구현 완료; 트리거 스크립트(M2b) 미착수" 갱신.
docs/29·30 상호 링크.

```bash
git add -A engine/tools/probes/probe_agent_chat.ps1 docs/31_desktop_agent_chat.md docs/30_desktop_agent_m2a.md docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md engine/src/apps/ClientPaletteApp.cpp engine/src/server/JKWindowServer.* engine/CMakeLists.txt
git commit -m "feat(agent): launch_chat tool + palette /chat + chat probe + docs/31"
git push
```

## 완료 조건

1. 팔레트 `/chat` → 채팅창 스폰 (Win32, JKWindow/SDL 밖).
2. 채팅창에서 슬래시 커맨드 동작 (palette와 같은 도구 집합).
3. permissions.json `{"close_window":"ask"}` → close 요청이 채팅창 인라인 프롬프트로 승인/거부/타임아웃 3경로 모두 동작.
4. 브로커(MCP) 경로의 close 요청도 ask 시 채팅창 승인 대기 — 브로커 응답이 승인 후 도착.
5. 기존 회귀 전부 통과.