# 앱 도구 허브 (app tool hub) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 앱이 런치 시 자기 도구를 서버에 선언(연결 수명 묶임)하고, 에이전트가 `app_tool` 중계 도구로 앱 기능을 네이티브로 호출하는 MCP-hub 구조 — 첫 앱 vplayer 6도구.

**Architecture:** 앱→서버 `AgentToolRegister` IPC(연결 수명에 묶임), 서버 레지스트리+3단 게이트+`app_tool` 중계(args 원문 패스스루), 브로커(jkagentd)가 tools/list 시점에 서버 질의로 개별 MCP 도구 합성(B안 — 세션 후 런치 앱 미노출은 수용한 한계), 승인 파이프라인 재사용 ask + 대상 시각화 3단.

**Tech Stack:** C++17/Win32 named pipe IPC, SDL2 컴포지터(서버 드로잉), jkcore AgentJson, claude CLI MCP(.mcp.json → jkagentd), PS5.1 프로브.

**Spec:** `docs/superpowers/specs/2026-09-19-app-tool-hub-design.md` (플랜은 스펙과 함께 읽는다 — 결정 사유·선례는 전부 스펙에 있다)

**스테이징 (사용자 지시 "바로 효과를 볼 수 있는 부분부터 살 붙여가기"):**
- **스테이지 1 = Task 1–6**: 코어 경로(등록/카탈로그/중계/게이트) + vplayer 6도구. 완료 시점에 agentctl과 **폰 브리지(generic relay 무수정)**에서 즉시 사용 가능.
- **스테이지 2 = Task 7**: 브로커 동적 tools/list + 라우팅. "vplayer 30초로 시크해 줘" 말로 가능.
- **스테이지 3 = Task 8–11**: 승인 시각화 3단 + 회귀 + skill 샘플 + docs/58 as-built.

## Global Constraints

- 빌드: `cd engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target <T>`. **레슨 57**: 새로 링크된 타겟은 최소 1회 빌드. **레슨 18**: vplayer DLL 재빌드 후 반드시 `--target jkx_packages` 재팩(안 하면 구코드로 "픽스 실패" 오판).
- 빌드 검증: 빌드 출력을 grep으로 필터하지 않는다(레슨 37 — Permission denied 링크 실패가 가려진다); exe/dll mtime > 소스 mtime 확인.
- 엔진 테스트: `./jkdesktop test` (0 failures 기준). 브로커: `./jkagentd --selftest` (0 failures).
- **레슨 35**: `clientsMutex_` 보유 경로(HandleAgentQuery/ProcessClientMessage/CleanupDisconnectedClients)에서 호출되는 헬퍼는 **절대 락을 잡지 않는다** — 새 헬퍼 전부 락 프리.
- **레슨 33**: 서버는 클라 연결을 **id(connId)로** 저장한다 — 포인터 저장 금지(요청자 죽으면 댕글).
- 프로브: PS5.1, **ASCII-only 문자열**(레슨 50), `> log 2>&1` 파일 리다이렉트(grep 파이프 버퍼링 오판), agentctl JSON 인자는 공백 없는 페이로드 + 기존 프로브의 `ProcessStartInfo.Arguments` 원시 조립 관용구(레슨: PS 인용 재파싱 함정), 이벤트 구독 연결을 트리거 쿼리 **이전에** 설립(레슨 28).
- permissions.json 테스트: 파일 백업/복원은 프로브 규약(state 백업 대상에 permissions.json 포함 — settings/notes/files 허브 선례).
- 파이프 원시 클라이언트: `engine/tools/probes/mgr_t1_read.ps1`의 헬퍼 관용구를 복사 — **프레임 header+payload를 한 함수가 통째로 소비**(레슨: 이중 읽기 misalign 블록).
- 최종리뷰: 최강 모델(메모리 규약 "최종리뷰는 최강 모델") — 스테이지 1 종료 시점과 최종 시점에 리뷰 루틴.
- 문서는 산출물(메모리 규약): 각 스테이지 완료 시 결정+직관을 스펙/플랜에 기록.

**File Structure (전체):**
- Modify: `engine/include/ipc/JKWireProtocol.h` + `engine/src/ipc/JKWireProtocol.cpp` (메시지 3종)
- Modify: `engine/include/server/JKWindowServer.h` + `engine/src/server/JKWindowServer.cpp` (레지스트리/도구 2종/게이트/시각화)
- Modify: `engine/include/client/JKClientSurface.h` + `engine/src/client/JKClientSurface.cpp` (클라 수신 공통)
- Modify: `engine/include/client/JKClientApplication.h` + `.cpp` (코어 펌프 + 가상 훅)
- Modify: `engine/include/apps/ClientVPlayerApp.h` + `engine/src/apps/ClientVPlayerApp.cpp` (vplayer 6도구)
- Modify: `engine/tools/jkagentd/main.cpp` (동적 tools/list + 라우팅)
- Create: `engine/tools/probes/probe_app_tools.ps1`
- Modify: `engine/tools/jkchat/main.cpp` (승인 스트립 target 문구), jkbridge 무수정(검증만)
- Create: `engine/templates/console-app/SKILL.md` (콘솔 앱 claude skill 래퍼 샘플)
- Create: `docs/58_app_tool_hub.md` (as-built)

---

# 스테이지 1 — 코어 경로 (agentctl/폰 즉효)

### Task 1: 와이어 프로토콜 — 메시지 3종

**Files:**
- Modify: `engine/include/ipc/JKWireProtocol.h:14-53` (MsgType), `:174-188` (헤더 구조체)
- Modify: `engine/src/ipc/JKWireProtocol.cpp:46-90` (WriteAgentJson/ReadAgentJson)
- Test: 빌드 + `./jkdesktop test`

**Interfaces:**
- Consumes: 기존 `WriteAgentJson/ReadAgentJson(IWireTransport&, MsgType, uint32_t queryId, uint32_t ok, const std::string& json)` 스위치 구조.
- Produces: `MsgType::AgentToolRegister(22)/AgentToolCall(23)/AgentToolResult(24)`, `WriteAgentToolRegister/ReadAgentToolRegister(transport|msg, std::string& json)`, `WriteAgentToolCall/ReadAgentToolCall(transport|msg, uint32_t& reqId, std::string& json)`, `WriteAgentJson`가 `AgentToolResult`도 수용(AgentReplyHeader 레이아웃 재사용).

- [ ] **Step 1: MsgType 3종 + 헤더 2종 추가**

`JKWireProtocol.h` MsgType enum(21 WindowTitle 뒤):

```cpp
    // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §3): 앱이 런치 시 자기
    // 도구를 선언하고, 서버가 에이전트의 도구 호출을 중계한다.
    AgentToolRegister = 22, // C -> S: {jsonLen + JSON {app, tools:[{name, description, inputSchema}]}}
    AgentToolCall     = 23, // S -> C: {reqId + jsonLen + JSON {app, tool, args}}
    AgentToolResult   = 24  // C -> S: AgentReplyHeader와 동일 레이아웃 {reqId, ok, jsonLen}
```

AgentEventHeader 근처(174-188 블록)에:

```cpp
#pragma pack(push, 1)
// 앱 도구 허브 (스펙 §3): 등록은 AgentEvent와 같은 {jsonLen} 헤더, 중계는
// {reqId, jsonLen}. 결과는 AgentReplyHeader({queryId, ok, jsonLen})를
// 그대로 재사용 — queryId 자리가 reqId다.
struct AgentToolRegisterHeader {
    uint32_t jsonLen = 0;
};
struct AgentToolCallHeader {
    uint32_t reqId = 0;  // 서버 채번 — AgentToolResult가 에코 (0 = 등록 ack)
    uint32_t jsonLen = 0;
};
#pragma pack(pop)
```

- [ ] **Step 2: writer/reader 확장**

`JKWireProtocol.cpp` — `WriteAgentJson`의 else-if 체인에 `AgentToolResult` 케이스 추가(AgentReply와 동일 빌드 — queryId 자리에 reqId):

```cpp
    } else if (type == MsgType::AgentToolResult) {
        AgentReplyHeader h{queryId, ok, jsonLen};   // queryId = reqId
        payload.resize(sizeof(h) + jsonLen);
        std::memcpy(payload.data(), &h, sizeof(h));
    } else {
        return false;
    }
```

`ReadAgentJson`에도 `AgentToolResult`를 AgentReply와 동일하게 수용. 그리고 파일 말미 신규 함수 2쌍:

```cpp
bool WriteAgentToolRegister(IWireTransport& transport, const std::string& json) {
    if (json.size() > 0x7FFFFFFFull) return false;
    const uint32_t jsonLen = static_cast<uint32_t>(json.size());
    std::vector<uint8_t> payload(sizeof(AgentToolRegisterHeader) + jsonLen);
    AgentToolRegisterHeader h{jsonLen};
    std::memcpy(payload.data(), &h, sizeof(h));
    if (jsonLen) std::memcpy(payload.data() + sizeof(h), json.data(), jsonLen);
    return WriteMessage(transport, MsgType::AgentToolRegister, payload);
}
bool ReadAgentToolRegister(const Message& msg, std::string& json) {
    json.clear();
    if (msg.type != MsgType::AgentToolRegister) return false;
    if (msg.payload.size() < sizeof(AgentToolRegisterHeader)) return false;
    AgentToolRegisterHeader h; std::memcpy(&h, msg.payload.data(), sizeof(h));
    if (msg.payload.size() < sizeof(h) + h.jsonLen) return false;
    json.assign(reinterpret_cast<const char*>(msg.payload.data()) + sizeof(h), h.jsonLen);
    return true;
}
bool WriteAgentToolCall(IWireTransport& transport, uint32_t reqId, const std::string& json) {
    // AgentToolCallHeader{reqId, jsonLen} — 위와 같은 조립
}
bool ReadAgentToolCall(const Message& msg, uint32_t& reqId, std::string& json) {
    // AgentToolCallHeader 판독 — ReadAgentToolRegister와 동일 관용구
}
```

헤더의 함수 선언도 `JKWireProtocol.h` 말미에 추가.

- [ ] **Step 3: 빌드 + 회귀**

Run: `PATH=/c/msys64/ucrt64/bin cmake --build . --target jkcore jkclient jkserver jkagentd && ./jkdesktop test`
Expected: 빌드 성공, 0 failures.

- [ ] **Step 4: Commit**

```bash
git add engine/include/ipc/JKWireProtocol.h engine/src/ipc/JKWireProtocol.cpp
git commit -m "feat(ipc): AgentToolRegister/Call/Result — 앱 도구 허브 메시지 3종 (스펙 2026-09-19 §3)"
```

### Task 2: 서버 — 등록 수신 + 레지스트리 + 연결 정리

**Files:**
- Modify: `engine/include/server/JKWindowServer.h:189-224` 부근(멤버), 헤더 선언부
- Modify: `engine/src/server/JKWindowServer.cpp:1584` (ProcessClientMessage dispatch), `:4755` (CleanupDisconnectedClients), 파일 내 신규 함수들
- Test: 빌드 + `./jkdesktop test`

**Interfaces:**
- Consumes: Task 1의 `ReadAgentToolRegister`/`WriteAgentJson`/`WriteAgentToolResult`.
- Produces: `appToolManifests_`(connId→매니페스트), `HandleToolRegister(client, json)`, `HandleToolResult(client, msg)`, `AppToolAllowed(app, tool)`, `ReplyAppToolError(inflight, err)`, `PublishAppToolsChanged()` — Task 3/8/9가 이 이름을 그대로 쓴다.

- [ ] **Step 1: JKWindowServer.h — 구조체/멤버/선언**

`PendingApproval` 블록(189-224) 근처에 추가:

```cpp
    // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.1): 연결이 선언한 도구
    // 매니페스트 — 연결 수명에 묶임(CleanupDisconnectedClients에서 소멸).
    struct AppToolDef {
        std::string name;        // ^[a-z][a-z0-9_]{0,31}$
        std::string description; // ≤512B
        std::string inputSchema; // 원문 JSON ≤2KiB (MCP inputSchema 그대로)
    };
    struct AppToolManifest {
        uint32_t connId = 0;
        std::string app;         // ^[a-z][a-z0-9_]{0,15}$
        uint32_t windowId = 0;   // 창 클라만(= connId), 제어 연결 0
        std::string title;       // 카탈로그 표기용
        std::vector<AppToolDef> tools;
    };
    // 중계 대기 중인 도구 호출 — 앱의 AgentToolResult를 기다린다(10s 만료).
    struct InflightAppTool {
        uint32_t reqId = 0;
        uint32_t queryId = 0;         // 완료할 AgentQuery
        uint32_t requesterConnId = 0; // 레슨 33: id로 저장 — 포인터 금지
        uint32_t targetConnId = 0;
        uint32_t windowId = 0;
        time_t expiresAt = 0;
    };
    std::map<uint32_t, AppToolManifest> appToolManifests_;  // connId → 매니페스트
    std::map<uint32_t, InflightAppTool> inflightAppTools_;  // reqId → 중계
    uint32_t nextToolReqId_ = 1;
```

메서드 선언(HandleAgentQuery 선언 근처):

```cpp
    // 등록 수신(검증+저장+ack+agent.app_tools_changed)과 결과 상관관계 회수.
    // clientsMutex_ 보유 경로에서만 호출 — 헬퍼는 락을 잡지 않는다(레슨 35).
    void HandleToolRegister(JKClientConnection& client, const std::string& json);
    void HandleToolResult(JKClientConnection& client, const ipc::Message& msg);
    // app_tool 3단 키 게이트: app_tool.<app>.<tool> > app_tool.<app> >
    // app_tool. 키 부재 폴백 = allow (스펙 §0 결정 3).
    AgentDecision AppToolAllowed(const std::string& app, const std::string& tool) const;
    void ReplyAppToolError(const InflightAppTool& inf, const char* err);
    void PublishAppToolsChanged();
```

- [ ] **Step 2: 등록 수신 구현**

`JKWindowServer.cpp` — 파일 스코프 검증 헬퍼(HandleToolRegister 위):

```cpp
// 앱 도구 허브 (스펙 §4.1): 소문자+숫자+밑줄 토큰 검증. std::regex 대신
// 수기 — 기존 sha256/fingerprint 검증기 관용구.
static bool ValidAppToolToken(const std::string& s, size_t maxLen) {
    if (s.empty() || s.size() > maxLen) return false;
    if (s[0] < 'a' || s[0] > 'z') return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}
```

구현 본체:

```cpp
// 스펙 §4.1: 등록 검증 — bad_app/bad_name/too_many_tools/schema_too_large/
// namespace_conflict. 셸 특권 연결의 등록은 봉쇄(셸 도구 표면 오염 방지).
// ack는 AgentReply 재사용(queryId=0 상수 — 앱 쿼리 id는 1부터 시작하는
// 기존 관례와 충돌 없음; 앱은 미인지 queryId reply를 무시한다 — 정보성).
void JKWindowServer::HandleToolRegister(JKClientConnection& client,
                                        const std::string& json) {
    auto ack = [&](bool ok, const char* err) {
        std::string body = ok ? std::string("{\"ok\":true}")
            : std::string("{\"ok\":false,\"error\":\"") + err + "\"}";
        ipc::WriteAgentJson(client.Transport(), ipc::MsgType::AgentReply,
                            0, ok ? 1u : 0u, body);
    };
    if (client.IsShell()) { ack(false, "shell_denied"); return; }
    jk::agent::AgentJson req(json);
    std::string app;
    if (!req.ok() || !req.GetStr("app", app)) { ack(false, "bad_request"); return; }
    if (!ValidAppToolToken(app, 16)) { ack(false, "bad_app"); return; }
    int toolCount = 0;
    if (!req.GetArraySize("tools", toolCount)) { ack(false, "bad_request"); return; }
    if (toolCount < 0 || toolCount > 32) { ack(false, "too_many_tools"); return; }
    // namespace_conflict: 코어 도구명(kPermMatrix 전 행 = 서버 도구 전체
    // 목록)과 다른 연결이 이미 등록한 app를 금지. 같은 connId 재등록은
    // 언제나 upsert 허용.
    if (app != "app_tool") {
        for (const auto& row : kPermMatrix)
            if (app == row.tool) { ack(false, "namespace_conflict"); return; }
        for (const auto& kv : appToolManifests_)
            if (kv.second.app == app && kv.first != client.Id()) {
                ack(false, "namespace_conflict");
                return;
            }
    }
    AppToolManifest m;
    m.connId = client.Id();
    m.app = app;
    m.windowId = client.IsControlOnly() ? 0u : client.Id();
    m.title = client.Title();
    for (int i = 0; i < toolCount; ++i) {
        AppToolDef d;
        std::string schemaRaw;
        if (!req.GetArrStr("tools", i, "name", d.name) ||
            !ValidAppToolToken(d.name, 32)) { ack(false, "bad_name"); return; }
        req.GetArrStr("tools", i, "description", d.description);
        if (d.description.size() > 512) { ack(false, "schema_too_large"); return; }
        // inputSchema는 원문 JSON — AgentJson 배열 원소의 raw 접근이 필요하다.
        // JKAgentJson.h에 GetArrRaw(key, idx, field) 신설(§Task 2a)하거나,
        // 등록 JSON을 서버에서 직접 스캔하는 대신 아래 대안을 쓴다(Step 2a).
        ...
    }
    appToolManifests_[client.Id()] = m;   // upsert
    ack(true, "");
    PublishAppToolsChanged();
}
```

- [ ] **Step 2a: AgentJson `GetArrRaw` 1종 추가**

배열 원소 안의 raw 값(스키마/args) 접근이 서버·브로커·프로브 모두에 필요하다. `engine/include/agent/JKAgentJson.h`에 `GetObjRaw`(선례: jkbridge GetRaw)의 배열 형제를 추가하고 `JKAgentJson.cpp`에서 구현 — GetObjRaw 구현의 인덱스 탐색부를 재사용한다:

```cpp
    // GetArrStr/GetArrInt의 raw 형제 — 배열 원소 안 필드를 원문 JSON으로
    // 돌려준다 (앱 도구 inputSchema/args 패스스루용).
    bool GetArrRaw(const char* key, int idx, const char* field, std::string& out) const;
```

- [ ] **Step 3: 결과 수신 + 정리 + 이벤트**

```cpp
// 스펙 §3/§4.2: 앱의 AgentToolResult를 reqId 상관관계로 원 요청자에 회송.
// 요청자 에코 상관관계(filedlg requesterConnId 선례) — 남의 reqId는 무시.
void JKWindowServer::HandleToolResult(JKClientConnection& client,
                                      const ipc::Message& msg) {
    uint32_t reqId = 0, ok = 0; std::string json;
    if (!ipc::ReadAgentJson(msg, reqId, ok, json)) return;
    if (json.size() > 16 * 1024) {   // 결과 상한 (스펙 §4.1)
        auto it = inflightAppTools_.find(reqId);
        if (it != inflightAppTools_.end()) {
            ReplyAppToolError(it->second, "result_too_large");
            inflightAppTools_.erase(it);
        }
        return;
    }
    auto it = inflightAppTools_.find(reqId);
    if (it == inflightAppTools_.end()) return;
    if (it->second.targetConnId != client.Id()) return;
    for (auto& c : clients_) {
        if (c && c->Id() == it->second.requesterConnId && !c->IsDisconnected()) {
            std::string reply = std::string("{\"ok\":true,\"windowId\":") +
                std::to_string(it->second.windowId) + "," +
                (ok ? "\"result\":" + json : "\"error\":" + json) + "}";
            ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                it->second.queryId, ok, reply);
            break;
        }
    }
    inflightAppTools_.erase(it);
}

void JKWindowServer::PublishAppToolsChanged() {
    PushAgentEventJson("{\"topic\":\"agent.app_tools_changed\"}");
}

void JKWindowServer::ReplyAppToolError(const InflightAppTool& inf,
                                       const char* err) {
    for (auto& c : clients_) {
        if (c && c->Id() == inf.requesterConnId && !c->IsDisconnected()) {
            std::string reply = std::string("{\"ok\":false,\"windowId\":") +
                std::to_string(inf.windowId) + ",\"error\":\"" + err + "\"}";
            ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                inf.queryId, 0, reply);
            return;
        }
    }
}
```

`CleanupDisconnectedClients`(4755) — `pendingFileDialog_` 회수 블록 뒤:

```cpp
                // 앱 도구 허브 (스펙 §4.1): 연결 수명에 묶인 매니페스트 소멸 +
                // 이 연결로 중계 중이던 호출 tool_gone 회수 + 카탈로그 변경
                // 이벤트. 별도 언레지스터 메시지 없음.
                bool toolsChanged = false;
                if (appToolManifests_.erase(client->Id())) toolsChanged = true;
                for (auto iit = inflightAppTools_.begin();
                     iit != inflightAppTools_.end();) {
                    if (iit->second.targetConnId == client->Id()) {
                        ReplyAppToolError(iit->second, "tool_gone");
                        iit = inflightAppTools_.erase(iit);
                    } else ++iit;
                }
```

(루프 밖 `if (toolsChanged) PublishAppToolsChanged();` — CleanupDisconnectedClients는 clientsMutex_ 보유 중, PublishAppToolsChanged는 락 프리.)

`ProcessClientMessage`(1584) dispatch에 2케이스:

```cpp
    } else if (msg.type == ipc::MsgType::AgentToolRegister) {
        std::string json;
        if (ipc::ReadAgentToolRegister(msg, json))
            HandleToolRegister(client, json);
    } else if (msg.type == ipc::MsgType::AgentToolResult) {
        HandleToolResult(client, msg);
    }
```

- [ ] **Step 4: events_list 카탈로그 1행**

`events_list` 정적 카탈로그(kTopics 등의 테이블)에 추가:

```cpp
    // 앱 도구 허브 (스펙 §4.1): 등록/소멸 시 publish. 레슨 ⑧ 신규 토픽은
    // 카탈로그 즉시 등록.
    {"agent.app_tools_changed", "server", "앱 도구 등록/소멸(연결 수명)", "topic"},
```

(테이블의 실제 컬럼 구조는 주변 행을 따른다 — fields 컬럼은 "topic".)

- [ ] **Step 5: 빌드 + 회귀**

Run: `PATH=/c/msys64/ucrt64/bin cmake --build . --target jkserver jkcore && ./jkdesktop test && ./jkagentd --selftest`
Expected: 0 failures.

- [ ] **Step 6: Commit**

```bash
git add engine/include/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp engine/include/agent/JKAgentJson.h engine/src/agent/JKAgentJson.cpp
git commit -m "feat(server): 앱 도구 등록/레지스트리/결과 회송 + agent.app_tools_changed (스펙 §4.1)"
```

### Task 3: 서버 — list_app_tools + app_tool 중계 + 3단 게이트 + 타임아웃

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp:1757` (kPermMatrix), `:2476` (HandleAgentQuery), `:4599` (게이트 헬퍼), 만료 스캔 위치(승인 만료 스캔 옆 — ProcessPendingMessages의 expiry loop)
- Test: 빌드 + `./jkdesktop test` (e2e 검증은 Task 6 프로브)

**Interfaces:**
- Consumes: Task 2의 `appToolManifests_`/`InflightAppTool`/`HandleToolResult`/`ReplyAppToolError`, Task 1의 `WriteAgentToolCall`.
- Produces: 서버 도구 `list_app_tools`(평면 tools[] 카탈로그)/`app_tool {app, tool, args, windowId?}`; `AppToolAllowed(app, tool)` 3단 게이트 — Task 6/7 프로브와 브로커가 소비.

- [ ] **Step 1: kPermMatrix 행 2종 + askCapable**

`kPermMatrix`(1757) 파일 허브 행 뒤:

```cpp
    // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.3/§4.4): app_tool의
    // 실질 게이트는 3단 키(AppToolAllowed) — 이 행은 전역 기본값/매트릭스
    // 표기용. list_app_tools는 카탈로그 조회(none 등급).
    {"list_app_tools", "none", "allow"},
    {"app_tool", "server", "allow"},
```

`AgentToolAllowed`의 askCapable 스위치(4619)에 `tool == "app_tool"` 추가(파일값 "ask"가 Allow로 열화하지 않게 — docs/54 §11 선례. 실질 게이트는 AppToolAllowed지만 값 일관성을 위해 묶는다).

- [ ] **Step 2: 3단 게이트 헬퍼**

`AgentToolAllowed`(4599) 아래:

```cpp
// 스펙 §4.3: app_tool 3단 키 해석 — app_tool.<app>.<tool> > app_tool.<app>
// > app_tool. 파일 부재/키 부재 폴백 = allow. permissions.json 핫리드는
// AgentToolAllowed 기존 계약(호출마다 읽음) 유지.
AgentDecision JKWindowServer::AppToolAllowed(const std::string& app,
                                             const std::string& tool) const {
    std::string k0 = "app_tool." + app + "." + tool;
    std::string k1 = "app_tool." + app;
    const char* keys[3] = {k0.c_str(), k1.c_str(), "app_tool"};
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    std::FILE* f = std::fopen((dir + "\\permissions.json").c_str(), "rb");
    if (!f) return AgentDecision::Allow;
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson perm(buf);
    if (!perm.ok()) return AgentDecision::Allow;
    for (const char* k : keys) {
        std::string v;
        if (perm.GetStr(k, v)) {
            if (v == "allow") return AgentDecision::Allow;
            if (v == "ask")   return AgentDecision::Ask;
            if (v == "deny")  return AgentDecision::Deny;
        }
    }
    return AgentDecision::Allow;
}
```

- [ ] **Step 3: HandleAgentQuery 분기 2종**

`HandleAgentQuery`(2476) — 새 분기(`window_fullscreen`과 `close_window` 사이 어디든). **핵심: app_tool은 allow든 ask든 응답이 지연된다 — 기존 파킹 경로의 `replied` 플래그 기계를 그대로 쓴다.**

```cpp
    } else if (tool == "list_app_tools") {
        // 스펙 §4.4: 평면 행 카탈로그 (레슨 39 — 중첩 배열 리더 부재).
        std::string out = "{\"ok\":true,\"tools\":[";
        bool first = true;
        for (auto& kv : appToolManifests_) {
            const AppToolManifest& m = kv.second;
            for (const auto& t : m.tools) {
                if (!first) out += ",";
                first = false;
                out += "{\"app\":\"" + JsonEsc(m.app) + "\",\"name\":\"" +
                       JsonEsc(t.name) + "\",\"description\":\"" +
                       JsonEsc(t.description) + "\",\"inputSchema\":" +
                       (t.inputSchema.empty() ? "{}" : t.inputSchema) +
                       ",\"windowId\":" + std::to_string(m.windowId) +
                       ",\"title\":\"" + JsonEsc(m.title) + "\",\"connId\":" +
                       std::to_string(m.connId) + "}";
            }
        }
        reply = out + "]}";
    } else if (tool == "app_tool") {
        // 스펙 §4.4: 중계. args 원문 패스스루(서버 검증 안 함 — 앱 계약).
        // allow/ask 모두 응답 지연(app_tool allow 중계 = AgentToolResult 대기).
        std::string app, toolName, argsRaw;
        int windowIdArg = 0;
        const bool hasWindowId = req.GetObjInt("args", "windowId", windowIdArg);
        req.GetObjStr("args", "app", app);
        req.GetObjStr("args", "tool", toolName);
        req.GetObjRaw("args", "args", argsRaw);
        if (argsRaw.size() > 8 * 1024) {
            reply = "{\"ok\":false,\"error\":\"args_too_large\"}";
        } else if (app.empty() || toolName.empty()) {
            reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
        } else {
            // 후보 수집: 등록된 (app, tool) 조합 역매칭 + 연결 생존 확인
            JKClientConnection* target = nullptr;
            uint32_t targetWindowId = 0, targetTitleIdx = 0;
            std::vector<const AppToolManifest*> cands;
            for (auto& kv : appToolManifests_)
                for (const auto& t : kv.second.tools)
                    if (kv.second.app == app && t.name == toolName) {
                        // 창 클라는 windowId 지정 변별, 제어 연결은 단독 후보만
                        cands.push_back(&kv.second);
                        break;
                    }
            // windowId 필터
            if (hasWindowId && windowIdArg > 0) {
                std::vector<const AppToolManifest*> filtered;
                for (auto* m : cands)
                    if (m->windowId == static_cast<uint32_t>(windowIdArg))
                        filtered.push_back(m);
                cands = filtered;
            }
            if (cands.empty()) {
                reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
            } else if (cands.size() > 1) {
                // 스펙 §4.2: 묵시적 추측 라우팅 금지 — 후보 제시(자기교정)
                std::string list = "[";
                for (size_t i = 0; i < cands.size(); ++i) {
                    if (i) list += ",";
                    list += "{\"windowId\":" + std::to_string(cands[i]->windowId) +
                            ",\"title\":\"" + JsonEsc(cands[i]->title) + "\"}";
                }
                reply = "{\"ok\":false,\"error\":\"ambiguous\",\"candidates\":" +
                        list + "]}";
            } else {
                const AppToolManifest* m = cands.front();
                JKClientConnection* conn = nullptr;
                for (auto& c : clients_)
                    if (c && c->Id() == m->connId && !c->IsDisconnected())
                        { conn = c.get(); break; }
                if (!conn) {
                    reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
                } else {
                    switch (AppToolAllowed(app, toolName)) {
                        case AgentDecision::Deny:
                            reply = "{\"ok\":false,\"error\":\"denied\"}";
                            break;
                        case AgentDecision::Ask: {
                            // 스펙 §4.3: 기존 승인 파이프라인 재사용 —
                            // close_window ask 패턴 그대로(구독자 체크 +
                            // PendingApproval push + agent.approval_request
                            // 방송 + replied=false). kind="app_tool",
                            // name=app+"."+tool, targetId=windowId.
                            // (§5 2단: 이벤트에 target 필드 추가 — Task 8.)
                            // TODO 없음 — 이 블록은 close_window ask 분기
                            // (2560-2610)를 복제해 위 값만 채운다.
                            break;
                        }
                        case AgentDecision::Allow: {
                            const uint32_t reqId = nextToolReqId_++;
                            InflightAppTool inf;
                            inf.reqId = reqId;
                            inf.queryId = queryId;
                            inf.requesterConnId = client.Id();
                            inf.targetConnId = conn->Id();
                            inf.windowId = m->windowId;
                            inf.expiresAt = std::time(nullptr) + 10;
                            inflightAppTools_[reqId] = inf;
                            std::string callJson = "{\"app\":\"" + JsonEsc(app) +
                                "\",\"tool\":\"" + JsonEsc(toolName) +
                                "\",\"args\":" +
                                (argsRaw.empty() ? "{}" : argsRaw) + "}";
                            ipc::WriteAgentToolCall(conn->Transport(), reqId, callJson);
                            replied = false;   // 응답은 HandleToolResult가
                            break;
                        }
                    }
                }
            }
        }
    }
```

(위 스케치의 `targetTitleIdx` 등 쓰지 않는 변수는 구현 시 제거. Ask 블록은 기존 close_window ask 경로의 기계를 복제한다 — PendingApproval에 `kind="app_tool"`, `name=app+"."+tool`, `targetId=m->windowId`, `queryId`, `requesterId=client.Id()` 채우고 broadcast. 승인 resolve 측(approve 도구)은 kind에 관계없이 queryId로 reply하므로 **추가 분기 불필요** — resolve 코드는 queryId/ok만 다룬다. 캡처/파일 허브처럼 재실행형이 아니라 파킹-응답형이다.)

- [ ] **Step 4: 중계 만료 스캔**

승인 만료 스캔(approval_timeout을 돌리는 ProcessPendingMessages의 expiry loop) 바로 뒤:

```cpp
        // 앱 도구 중계 타임아웃 (스펙 §9): 10s — expiresAt은 allow 중계
        // 시점에 설정되므로 승인 파킹 대기 중엔 오발하지 않는다.
        const time_t toolNow = std::time(nullptr);
        for (auto it = inflightAppTools_.begin();
             it != inflightAppTools_.end();) {
            if (toolNow >= it->second.expiresAt) {
                ReplyAppToolError(it->second, "tool_timeout");
                it = inflightAppTools_.erase(it);
            } else ++it;
        }
```

- [ ] **Step 5: 빌드 + 회귀**

Run: `PATH=/c/msys64/ucrt64/bin cmake --build . --target jkserver && ./jkdesktop test`
Expected: 0 failures.

- [ ] **Step 6: Commit**

```bash
git add engine/src/server/JKWindowServer.cpp
git commit -m "feat(server): list_app_tools 카탈로그 + app_tool 중계(3단 게이트, 10s 타임아웃) (스펙 §4.3-4.4)"
```

### Task 4: 클라 공통 — JKClientSurface 큐 + JKClientApplication 훅

**Files:**
- Modify: `engine/include/client/JKClientSurface.h:106-166` + `engine/src/client/JKClientSurface.cpp` (ReadLoop dispatch — 파일 내 MsgType 분기 위치)
- Modify: `engine/include/client/JKClientApplication.h:123` (OnAgentEvent 훅 아래) + `engine/src/client/JKClientApplication.cpp` (Run 펌프 위치 — settings-hub §2.3 단일 펌프 선례)
- Test: 빌드 + `./jkdesktop test`

**Interfaces:**
- Consumes: Task 1 writer/reader, Task 2 등록 ack(AgentReply, queryId=0 — 클라는 무시).
- Produces: `JKClientSurface::SendAgentToolRegister(app, tools)/PollToolCall(out)/SendAgentToolResult(reqId, ok, json)` + `JKClientApplication::OnAgentToolCall(tool, argsJson, resultJson)→bool` 가상 훅(기본 구현 unsupported 에러) — Task 5(vplayer)가 소비.

- [ ] **Step 1: JKClientSurface — 선언/멤버**

`JKClientSurface.h` (SendAgentQuery 선언 근처):

```cpp
    // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §8.2): 런치 시 자기 도구를
    // 선언하고, 서버 중계 호출(AgentToolCall)을 프레임 루프에서 폴링한다.
    struct AgentToolDecl {
        std::string name, description, inputSchema;  // inputSchema = 원문 JSON
    };
    struct AgentToolCallMsg {
        uint32_t reqId = 0;
        std::string app, tool, args;
    };
    bool SendAgentToolRegister(const std::string& app,
                               const std::vector<AgentToolDecl>& tools);
    bool PollToolCall(AgentToolCallMsg& out);
    bool SendAgentToolResult(uint32_t reqId, bool ok, const std::string& resultJson);
```

멤버(pendingAgentEvents_ 블록 아래, 같은 bounded-deque 관용구):

```cpp
    // 앱 도구 허브: ReadLoop가 적재, 메인 스레드가 PollToolCall로 소비.
    std::deque<AgentToolCallMsg> pendingToolCalls_;
    std::mutex agentToolMutex_;
```

- [ ] **Step 2: 구현**

`JKClientSurface.cpp` — 파일 스코프 `JsonEsc` 헬퍼가 없으면 기존 서버의 것(JKWindowServer.cpp JsonEsc)을 복사해 로컬 정적 함수로 둔다(이스케이프 `"` `\` 제어문자).

```cpp
bool JKClientSurface::SendAgentToolRegister(
        const std::string& app, const std::vector<AgentToolDecl>& tools) {
    std::string json = "{\"app\":\"" + JsonEsc(app) + "\",\"tools\":[";
    bool first = true;
    for (const auto& t : tools) {
        json += first ? "{" : ",{";
        first = false;
        json += "\"name\":\"" + JsonEsc(t.name) + "\",\"description\":\"" +
                JsonEsc(t.description) + "\",\"inputSchema\":" +
                (t.inputSchema.empty() ? "{}" : t.inputSchema) + "}";
    }
    json += "]}";
    return ipc::WriteAgentToolRegister(*transport_, json);
}

bool JKClientSurface::PollToolCall(AgentToolCallMsg& out) {
    std::lock_guard<std::mutex> lk(agentToolMutex_);
    if (pendingToolCalls_.empty()) return false;
    out = pendingToolCalls_.front();
    pendingToolCalls_.pop_front();
    return true;
}

bool JKClientSurface::SendAgentToolResult(uint32_t reqId, bool ok,
                                          const std::string& resultJson) {
    return ipc::WriteAgentJson(*transport_, ipc::MsgType::AgentToolResult,
                               reqId, ok ? 1u : 0u, resultJson);
}
```

`ReadLoop`의 MsgType 분기에(AgentQuery/AgentReply/AgentEvent 처리 위치 옆):

```cpp
        } else if (msg.type == ipc::MsgType::AgentToolCall) {
            uint32_t reqId = 0; std::string json;
            if (ipc::ReadAgentToolCall(msg, reqId, json)) {
                jk::agent::AgentJson body(json);
                JKClientSurface::AgentToolCallMsg tc;
                tc.reqId = reqId;
                body.GetStr("app", tc.app);
                body.GetStr("tool", tc.tool);
                body.GetRaw("args", tc.args);   // 원문 유지
                std::lock_guard<std::mutex> lk(agentToolMutex_);
                if (pendingToolCalls_.size() < 64)   // bounded (inputEvents_ 관용구)
                    pendingToolCalls_.push_back(std::move(tc));
            }
        }
```

(등록 ack(AgentReply, queryId=0)는 ReadLoop의 기존 AgentReply 큐에 들어가고 PollAgentReply 소비처가 reqId 0을 무시하면 그대로 폐기된다 — 기존 소비처(vplayer PumpAgentReplies 등)는 id 매칭이므로 이미 무시. **확인 항목: PumpAgentReplies류 소비처가 reqId=0 reply를 버리는지 확인** — id 매칭 실패=drop이 기본 동작.)

- [ ] **Step 3: JKClientApplication — 코어 펌프 + 가상 훅**

헤더(OnAgentEvent 선언 옆):

```cpp
    // 앱 도구 허브 (스펙 §8.2): 코어 펌프가 PollToolCall로 모아 이 훅으로
    // 전달한다(settings-hub §2.3 단일 펌프 이관 선례). resultJson에 결과
    // 객체(실패 시 {"error":...})를 담고 ok를 돌려주면 코어가
    // SendAgentToolResult까지 처리한다 — 앱은 reqId를 만지지 않는다.
    virtual bool OnAgentToolCall(const std::string& tool,
                                 const std::string& argsJson,
                                 std::string& resultJson);
```

기본 구현:

```cpp
bool JKClientApplication::OnAgentToolCall(const std::string& tool,
                                          const std::string& argsJson,
                                          std::string& resultJson) {
    resultJson = "{\"error\":\"unsupported_tool\",\"tool\":\"" + tool + "\"}";
    return false;
}
```

코어 펌프 — `JKClientApplication::Run`에서 DrainAgentEvents→OnAgentEvent 펌프(settings-hub §2.3에서 이관된 단일 위치)와 같은 스위프에:

```cpp
        // 앱 도구 허브 (스펙 §8.2): 프레임 펌프 결합 폴링 — DrvAgentEvents와
        // 동일 관용구. 결과 전송까지 코어가 처리(앱은 훅만 오버라이드).
        JKClientSurface::AgentToolCallMsg tc;
        while (surface_ && surface_->PollToolCall(tc)) {
            std::string resultJson;
            const bool ok = OnAgentToolCall(tc.tool, tc.args, resultJson);
            surface_->SendAgentToolResult(tc.reqId, ok, resultJson);
        }
```

- [ ] **Step 4: 빌드 + 회귀**

Run: `PATH=/c/msys64/ucrt64/bin cmake --build . --target jkclient jkserver && ./jkdesktop test`
Expected: 0 failures.

- [ ] **Step 5: Commit**

```bash
git add engine/include/client/JKClientSurface.h engine/src/client/JKClientSurface.cpp engine/include/client/JKClientApplication.h engine/src/client/JKClientApplication.cpp
git commit -m "feat(client): 앱 도구 등록/폴링/결과 공통 경로 + OnAgentToolCall 훅 (스펙 §8.2)"
```

### Task 5: vplayer — 6도구 등록 + 핸들러

**Files:**
- Modify: `engine/include/apps/ClientVPlayerApp.h` (훅 선언 1줄)
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` (OnInit 등록 + 핸들러)
- Test: 빌드(jkapp_vplayer + jkx 재팩) + vpt 회귀 + Task 6 프로브

**Interfaces:**
- Consumes: Task 4의 `SendAgentToolRegister/OnAgentToolCall`; PlayerCore 기존 API — `SnapNow()→Snap{opened, opening, openFailed, paused, ended, pos, dur, vol, error}`, `Seek(double)`(정밀), `SetPaused(bool)`, `SetVolume(float 0..1)`, `SetAvDelay(float ±1)`; 앱 내부 `OpenPath(const char*)`.
- Produces: 앱 네임스페이스 `vplayer`에 도구 6종(open/play_pause/seek/set_volume/set_av_delay/get_status).

- [ ] **Step 1: 등록 — OnInit**

`ClientVPlayerApp::OnInit`에서(기존 등록 코드 뒤 — SendAgentEventSubscribe 호출 위치 옆이 안전):

```cpp
    // 앱 도구 허브 (스펙 §8.1): 자기 도구 선언 — 연결 수명 동안 서버가 보관.
    // 스키마는 MCP inputSchema 그대로(브로커가 tools/list에 실는다).
    if (surface_) {
        using Decl = JKClientSurface::AgentToolDecl;
        std::vector<Decl> tools = {
            {"open", "Open a media file (async; poll get_status)",
             "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}"},
            {"play_pause", "Toggle play/pause", "{}"},
            {"seek", "Seek to absolute seconds",
             "{\"type\":\"object\",\"properties\":{\"seconds\":{\"type\":\"integer\"}},\"required\":[\"seconds\"]}"},
            {"set_volume", "Set volume percent 0..100",
             "{\"type\":\"object\",\"properties\":{\"percent\":{\"type\":\"integer\"}},\"required\":[\"percent\"]}"},
            {"set_av_delay", "Set A/V offset in seconds (-1..1)",
             "{\"type\":\"object\",\"properties\":{\"seconds\":{\"type\":\"integer\"}},\"required\":[\"seconds\"]}"},
            {"get_status", "Playback status snapshot (opened/paused/pos/dur/volume/error)", "{}"},
        };
        surface_->SendAgentToolRegister("vplayer", tools);
    }
```

- [ ] **Step 2: 핸들러 오버라이드**

헤더 protected 섹션:

```cpp
    bool OnAgentToolCall(const std::string& tool, const std::string& argsJson,
                         std::string& resultJson) override;
```

cpp — 파일 스코프 JsonEsc 헬퍼(서버의 것 복사, std::string 이스케이프) + 구현:

```cpp
// 앱 도구 허브 (스펙 §8.1): 전부 UI/프레임 스레드 — 기존 UI→PlayerCore
// 세터 경로와 동일 스레드라 새 락 없음. 인자 검증은 앱이 한다(서버는
// 패스스루 계약).
bool ClientVPlayerApp::OnAgentToolCall(const std::string& tool,
                                       const std::string& argsJson,
                                       std::string& out) {
    jk::agent::AgentJson args(argsJson);
    if (tool == "get_status") {
        const PlayerCore::Snap st =
            player_ ? player_->SnapNow() : PlayerCore::Snap{};
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "{\"opened\":%s,\"opening\":%s,\"openFailed\":%s,\"paused\":%s,"
            "\"ended\":%s,\"pos\":%.3f,\"dur\":%.3f,\"volume\":%.2f,"
            "\"error\":\"%s\"}",
            st.opened ? "true" : "false", st.opening ? "true" : "false",
            st.openFailed ? "true" : "false", st.paused ? "true" : "false",
            st.ended ? "true" : "false", st.pos, st.dur, st.vol,
            JsonEsc(st.error).c_str());
        out = buf;
        return true;
    }
    if (!player_) { out = "{\"error\":\"no_player\"}"; return false; }
    if (tool == "play_pause") {
        const PlayerCore::Snap st = player_->SnapNow();
        player_->SetPaused(!st.paused);
        out = std::string("{\"paused\":") + (!st.paused ? "true" : "false") + "}";
        return true;
    }
    if (tool == "seek") {
        int sec = 0;
        if (!args.ok() || !args.GetInt("seconds", sec) || sec < 0) {
            out = "{\"error\":\"bad_args\",\"need\":\"seconds:int>=0\"}";
            return false;
        }
        player_->Seek(static_cast<double>(sec));
        out = "{\"ok\":true}";
        return true;
    }
    if (tool == "set_volume") {
        int p = 0;
        if (!args.ok() || !args.GetInt("percent", p) || p < 0 || p > 100) {
            out = "{\"error\":\"bad_args\",\"need\":\"percent:int 0..100\"}";
            return false;
        }
        player_->SetVolume(static_cast<float>(p) / 100.0f);
        out = "{\"ok\":true}";
        return true;
    }
    if (tool == "set_av_delay") {
        int s = 0;
        if (!args.ok() || !args.GetInt("seconds", s) || s < -1 || s > 1) {
            out = "{\"error\":\"bad_args\",\"need\":\"seconds:int -1..1\"}";
            return false;
        }
        player_->SetAvDelay(static_cast<float>(s));
        out = "{\"ok\":true}";
        return true;
    }
    if (tool == "open") {
        std::string path;
        if (!args.ok() || !args.GetStr("path", path) || path.empty()) {
            out = "{\"error\":\"bad_args\",\"need\":\"path\"}";
            return false;
        }
        OpenPath(path.c_str());
        out = "{\"accepted\":true}";   // 비동기 — 진행은 get_status
        return true;
    }
    out = "{\"error\":\"unknown_tool\"}";
    return false;
}
```

- [ ] **Step 3: 빌드 + 재팩 + vpt 회귀**

Run: `PATH=/c/msys64/ucrt64/bin cmake --build . --target jkapp_vplayer jkx_packages && ./jkdesktop test && engine/tools/probes/probe_vpt4.ps1 && engine/tools/probes/probe_vpt9.ps1`
Expected: 0 failures, vpt4/vpt9 PASS (도구 등록이 재생/시크에 영향 없음).

- [ ] **Step 4: Commit**

```bash
git add engine/include/apps/ClientVPlayerApp.h engine/src/apps/ClientVPlayerApp.cpp
git commit -m "feat(vplayer): 앱 도구 6종 등록+핸들러 — open/play_pause/seek/volume/av_delay/status (스펙 §8.1)"
```

### Task 6: probe_app_tools.ps1 (스테이지 1) + 회귀

**Files:**
- Create: `engine/tools/probes/probe_app_tools.ps1`
- Test: 프로브 ×2 연속 + 기존 회귀

**Interfaces:**
- Consumes: `agentctl`(기존 `ProcessStartInfo.Arguments` 원시 조립 관용구 — probes/mgr_t1_read.ps1 등), 원시 파이프 클라(mgr_t1_read.ps1의 프레임 읽기 헬퍼 — **header+payload를 한 함수가 통째로 소비**).
- Produces: 스테이지 1 회귀 게이트.

- [ ] **Step 1: 프로브 골격**

PS5.1, ASCII-only. 구성(기존 프로브에서 복사): ①서버 기동/정리(vpt 프로브 관용구) ②`agentctl` 헬퍼 ③원시 파이프 클라 헬퍼(mgr_t1_read.ps1 복사 — 컨트롤 연결용) ④permissions.json 백업/복원(파킹 테스트용).

체크 목록(스테이지 1 스코프):

1. **카탈로그**: vplayer 스폰(launch_app jkx) → `list_app_tools`에 app=vplayer 도구 6종(name/description/windowId>0) — ×2 반복 아닌, 프로브 전체 ×2.
2. **seek e2e**: `get_status` pos0 → `app_tool {app:"vplayer",tool:"seek",args:{seconds:1}}` → `get_status` pos < 2.0 (테스트 클립 = vpt 프로브가 쓰는 것 재사용; 없으면 ffmpeg로 tmp 생성). 단, 클립 미오픈 상태면 `open` 도구로 먼저 오픈+get_status polling(≤10s)으로 opened 대기.
3. **오류 표면**: `app_tool {app:"nope",tool:"x"}` → unknown_app_tool; `app_tool seek`에 seconds 누락 → 앱의 bad_args 패스스루(`"error":"bad_args"`).
4. **자동 정리**: vplayer 창 닫기(close_window — permissions 백업/복원 주의) → `list_app_tools` 비움 + `agent.app_tools_changed` 이벤트 도착(구독 먼저 — 레슨 28).
5. **등록 검증**: 원시 파이프 클라로 (a) 잘못된 도구명(`Bad-Name`) 등록 → ack `bad_name`, (b) app="list_windows"(코어 도구명 충돌) → ack `namespace_conflict`, (c) 도구 33개 → `too_many_tools`.
6. **게이트 deny**: permissions.json에 `{"app_tool.vplayer.seek":"deny"}` → app_tool seek → denied; **복원**.
7. **게이트 ask + 파킹**: permissions.json에 `{"app_tool.vplayer.play_pause":"ask"}` + 구독자(원시 파이프 클라가 AgentEventSubscribe) → app_tool play_pause → agent.approval_request 도착(kind=app_tool, name=vplayer.play_pause) → `approve` 허용 → 원 쿼리에 결과 도착 + 실제 paused 상태 반전(get_status로 실측) → **복원**.
8. **tool_timeout**: 원시 파이프 클라가 도구 등록만 하고 AgentToolCall을 무시 → `app_tool` → ≤15s 내 tool_timeout 에러(만료 스캔 실측) + 그 연결 닫기 → tool_gone 아닌 정리 확인.
9. **폰 브리지 무수정 증명**: probe_jkbridge 기존 회귀만 재실행(generic relay가 app_tool을 패스스루 — 코드 변경 0).

- [ ] **Step 2: 공식런 ×2**

Run: `powershell -File engine/tools/probes/probe_app_tools.ps1 > probe1.log 2>&1` ×2 연속
Expected: ALL PASS ×2.

- [ ] **Step 3: 회귀**

Run: `probe_agent_chat` (승인 파이프라인 회귀 — kind 확장의 하위호환), `probe_jkbridge`, `jkagentd --selftest`, `./jkdesktop test`
Expected: 전부 PASS/0.

- [ ] **Step 4: Commit**

```bash
git add engine/tools/probes/probe_app_tools.ps1
git commit -m "test(probe): probe_app_tools — 앱 도구 허브 스테이지 1 체크 9종 ×2 ALL PASS"
```

---

# 스테이지 2 — 브로커 동적 tools/list + 라우팅

### Task 7: jkagentd — 동적 합성 + 이름 라우팅 + 브로커 게이트

**Files:**
- Modify: `engine/tools/jkagentd/main.cpp:48-94` (kToolsListJson 분해), `:359-370` (tools/list), `:362-470` (tools/call), `:113-136` (LoadPermissions)
- Test: `jkagentd --selftest` + probe_app_tools 스테이지 2 체크 추가

**Interfaces:**
- Consumes: 서버 도구 `list_app_tools`(평면)/`app_tool`, JKAgentClient `QueryRaw`(기존 SendQuery/PollReply 관용구).
- Produces: MCP 동적 도구명 `<app>_<tool>`(예: vplayer_seek) — claude가 직접 호출.

- [ ] **Step 1: 정적 목록 분해 + 동적 합성**

`kToolsListJson`을 `kCoreToolsListJson`으로 개명하고 **끝에 신규 코어 도구 2행 추가**(list_app_tools, app_tool — 스키마는 서버 도구와 동일). tools/list 분기(359):

```cpp
    if (method == "tools/list") {
        return result(ComposeToolsListJson());
    }
```

```cpp
// 스펙 §6: 코어 정적부 + 앱 도구 동적부. 동적부는 tools/list 시점에 서버
// list_app_tools 질의 — 앱 도구명은 <app>_<tool>(MCP 도구명 규약: 점 부재),
// description 앞에 [<app>] 접두, inputSchema는 앱 선언 그대로. 서버 질의
// 실패 시 정적부만 반환(폴백 — 앱 도구만 잠깐 안 보이는 수준).
std::string ComposeToolsListJson() {
    std::string core = kCoreToolsListJson;   // {"tools":[ ... ,]} 완결 형태
    std::string dyn;
    if (EnsureConnected()) {
        std::string reply;
        if (g_agent.QueryRaw("{\"tool\":\"list_app_tools\",\"args\":{}}",
                             /*timeoutMs=*/2000, reply) && !reply.empty()) {
            jk::agent::AgentJson r(reply);
            int n = 0;
            if (r.ok() && r.GetArraySize("tools", n)) {
                std::string items;
                std::set<std::string> seen;   // 중복 인스턴스: 이름당 1개
                for (int i = 0; i < n; ++i) {
                    std::string app, name, desc, schema;
                    if (!r.GetArrStr("tools", i, "app", app) ||
                        !r.GetArrStr("tools", i, "name", name)) continue;
                    std::string mcpName = app + "_" + name;
                    if (!seen.insert(mcpName).second) continue;
                    r.GetArrStr("tools", i, "description", desc);
                    r.GetArrRaw("tools", i, "inputSchema", schema);
                    if (dyn.empty()) dyn = ",";
                    dyn += std::string("{\"name\":\"") + JsonEsc(mcpName) +
                           "\",\"description\":\"[" + JsonEsc(app) + "] " +
                           JsonEsc(desc) + "\",\"inputSchema\":" +
                           (schema.empty() ? "{}" : schema) + "}";
                }
            }
        }
    }
    if (dyn.empty()) return core;
    // core 끝 "]}"}를 열어 동적부 삽입 — 정적 문자열의 정확한 꼬리("]}")
    // 를 절단 후 재조립. kCoreToolsListJson은 {"tools":[...]}} 형태 유지.
    const size_t tail = core.rfind("]}");
    return core.substr(0, tail) + dyn + "]}";
}
```

(필요하면 QueryRaw의 실제 시그니처에 맞춘다 — JKAgentClient의 SendQuery+PollReply 블록 쿼리 관용구. 서버 부재 시 EnsureConnected 실패로 정적부만.)

- [ ] **Step 2: tools/call 라우팅**

`IsKnownTool` 실패 분기(367) 바로 뒤:

```cpp
        // 동적 앱 도구 라우팅 (스펙 §6): 접두 추측 아님 — 등록된 (app, tool)
        // 조합 역매칭으로 파싱한다(app/도구명에 _ 포함 가능).
        if (!IsKnownTool(tool)) {
            std::string argsRaw, app, toolName;
            req.GetObjRaw("params", "arguments", argsRaw);
            if (ResolveAppTool(tool, app, toolName)) {
                // 브로커 3단 게이트 (스펙 §4.3) — MCP 경로 선차단.
                if (BrokerAppToolAllowed(app, toolName) == false)
                    return result("{\"ok\":false,\"error\":\"denied\"}");
                std::string fwd = "{\"tool\":\"app_tool\",\"args\":{\"app\":\"" +
                    JsonEsc(app) + "\",\"tool\":\"" + JsonEsc(toolName) +
                    "\",\"args\":" + (argsRaw.empty() ? "{}" : argsRaw) + "}}";
                return result(ForwardServerQuery(fwd));  // 기존 전송 경로 재사용
            }
            return result("{\"ok\":false,\"error\":\"unknown_tool\"}");
        }
```

```cpp
// 등록된 (app, tool) 역매칭 — 서버에 실시간 질의(앱이 방금 종료해도
// 정확). mcpName == app + "_" + tool 전 행 검사.
bool ResolveAppTool(const std::string& mcpName, std::string& app, std::string& tool) {
    if (!EnsureConnected()) return false;
    std::string reply;
    if (!g_agent.QueryRaw("{\"tool\":\"list_app_tools\",\"args\":{}}",
                          2000, reply) || reply.empty()) return false;
    jk::agent::AgentJson r(reply);
    int n = 0;
    if (!r.ok() || !r.GetArraySize("tools", n)) return false;
    for (int i = 0; i < n; ++i) {
        std::string a, t;
        if (r.GetArrStr("tools", i, "app", a) &&
            r.GetArrStr("tools", i, "name", t) && a + "_" + t == mcpName) {
            app = a; tool = t;
            return true;
        }
    }
    return false;
}

// 브로커 3단 게이트 — 서버 AppToolAllowed와 동일 순서(스펙 §4.3).
// 기본 allow 명시(브로커 키 누락=deny 레슨 재발 방지).
bool BrokerAppToolAllowed(const std::string& app, const std::string& tool) {
    // LoadPermissions의 파일 핫리드 관용구 재사용 —
    // app_tool.<app>.<tool> > app_tool.<app> > app_tool 순 판정,
    // 어떤 키도 없으면 true.
}
```

`IsKnownTool`/`LoadPermissions`의 kNames 배열에 `list_app_tools`, `app_tool` 추가(기본값 true 명시).

- [ ] **Step 3: 프로브 스테이지 2 체크 추가**

probe_app_tools.ps1에:
10. **MCP tools/list**: jkagentd stdio로 tools/list(probe_agent_mcp.ps1의 MCP stdio 관용구) → 응답에 `vplayer_seek` 존재 + description에 `[vplayer]` 접두.
11. **MCP tools/call**: `vplayer_seek {seconds:2}` → ok + get_status로 pos 반영.
12. **폴백**: 서버 미기동 상태에서 jkagentd tools/list → 코어 도구만 반환(정적부 폴백 — 크래시/행블록 없음).

- [ ] **Step 4: 빌드 + 공식런**

Run: `PATH=/c/msys64/ucrt64/bin cmake --build . --target jkagentd && ./jkagentd --selftest && probe_app_tools ×2`
Expected: selftest 0 failures, 프로브 ALL PASS ×2.

- [ ] **Step 5: Commit**

```bash
git add engine/tools/jkagentd/main.cpp engine/tools/probes/probe_app_tools.ps1
git commit -m "feat(broker): 동적 tools/list 합성 + <app>_<tool> 라우팅 + 3단 브로커 게이트 (스펙 §6)"
```

---

# 스테이지 3 — 승인 시각화 3단 + 마무리

### Task 8: 승인 대상 창 하이라이트 (서버 드로잉)

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (DrawCloseOverlay 드로잉 패스 옆)
- Test: 빌드 + 픽셀 실측 프로브 체크

**Interfaces:**
- Consumes: `pendingApprovals_`(targetId) — 승인 파이프라인 일반화(close_window 등 기존 ask 도구도 동시 혜택).
- Produces: 파킹된 승인의 대상 창 위 호박색 링+배너 — resolve 시 자동 해제.

- [ ] **Step 1: 드로잉 패스 위치 파악**

`DrawCloseOverlay` 위치(Grep)와 렌더 패스 호출 순서를 확인한다 — 하이라이트는 close 오버레이와 **같은 드로잉 단계**에 넣는다(레이어 위, 크롬 아님).

- [ ] **Step 2: 하이라이트 드로잉**

```cpp
// 승인 대상 시각화 (스펙 2026-09-19-app-tool-hub §5 1단): 파킹된 승인의
// 대상 창 위에 호박색 링 + 상단 배너 "에이전트 승인 대기: <name>".
// 승인 파이프라인 자체의 기능 — close_window/run_console_app 등 targetId를
// 갖는 모든 ask 도구에 적용. resolve(허용/거부/타임아웃) 시 pendingApprovals_
// 에서 사라지므로 자동 해제. 셸/캡처 오버레이는 close 게이트와 동일 면제.
void JKWindowServer::DrawApprovalHighlights(SDL_Renderer* r) {
    std::set<uint32_t> targets;
    for (const auto& p : pendingApprovals_)
        if (p.targetId) targets.insert(p.targetId);
    for (uint32_t id : targets) {
        JKCompositorLayer* layer =
            compositor_ ? compositor_->FindLayerById(id) : nullptr;
        if (!layer) continue;
        // 면제: shell(kCaptureOverlayTitle 스폰 + IsShell 동일 가드 —
        // close 게이트의 면제 목록과 동일 조건을 쓴다)
        if (client->IsShell() || title == kCaptureOverlayTitle) continue;
        const SDL_FRect rc = layer rect...;
        // 링: 두꺼운 테두리 — 3중 사각형 스트로크, 색 호박 (230,140,40,255)
        // 배너: 상단 22pt 밴드 채움 + 크롬 타이틀과 동일 글리프 경로로
        // "에이전트 승인 대기: " + p.name (Utf8ToKssm 경유)
    }
}
```

(정확한 레이어 rect/텍스트 경로는 DrawCloseOverlay/크롬 타이틀 드로잉에서 그대로 가져온다 — 새 글리프 엔진 없음.)

- [ ] **Step 3: 픽셀 실측**

프로브: permissions.json `{"app_tool.vplayer.play_pause":"ask"}` + 원시 클라 구독자 → app_tool 파킹 → 서버 화면 readback(캡처 프로브 관용구 — CopyFromScreen)으로 vplayer 창 테두리 밴드 픽셀에 호박색 존재 확인 → approve → 하이라이트 소실 확인.

- [ ] **Step 4: Commit**

```bash
git add engine/src/server/JKWindowServer.cpp
git commit -m "feat(server): 승인 대상 창 하이라이트 — 파킹된 ask 승인의 시각 최상위 레이어 (스펙 §5 1단)"
```

### Task 9: 승인 이벤트 target 필드 + 썸네일 캡처

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (app_tool 파킹 지점 — Task 3에서 만든 ask 블록)
- Modify: `engine/tools/jkchat/main.cpp` (approval_request 파싱 — target 문구)
- Test: 프로브 체크

**Interfaces:**
- Consumes: capture_window의 레이어→PNG 경로(기존 capture_window 도구 구현부 — 필요하면 `CaptureLayerToPng(connId, path)` 헬퍼로 추출해 재사용).
- Produces: `agent.approval_request` 이벤트의 app_tool 페이로드에 `target:{app, tool, windowId, title}` + `thumb` 경로.

- [ ] **Step 1: 캡처 헬퍼 추출 + 파킹 시 호출**

capture_window 도구 본문의 레이어 readback→stb_image_write 부분을 `bool CaptureLayerToPng(uint32_t connId, const std::string& path)` 헬퍼로 추출(기존 도구 분기는 헬퍼 호출로 교체 — 동작 불변). app_tool 파킹 블록에서:

```cpp
            // 스펙 §5 3단: 대상 창 "조작 전" 썸네일 — 캡처 실패는 비치명.
            std::string thumb;
            {
                char tbuf[128];
                std::snprintf(tbuf, sizeof(tbuf),
                    "state\\screenshots\\approval_%lld_%u.png",
                    (long long)std::time(nullptr), p.requestId);
                if (CaptureLayerToPng(m->windowId, tbuf)) thumb = tbuf;
            }
```

- [ ] **Step 2: approval_request 페이로드에 target/thumb**

app_tool 파킹의 agent.approval_request 방송 JSON에:

```json
{"kind":"app_tool","name":"vplayer.seek","request":N,
 "target":{"app":"vplayer","tool":"seek","windowId":7,"title":"..."},
 "thumb":"state\\screenshots\\approval_..._....png"}
```

- [ ] **Step 3: jkchat 승인 스트립 문구**

jkchat HandleEvent의 approval_request 분기: kind가 app_tool이면 target 필드를 읽어 `"[<app> 창 #<windowId>] <tool> 실행할까요?"` 문구 사용(target 부재 시 기존 name 문구 유지 — 하위호환).

- [ ] **Step 4: Commit**

```bash
git add engine/src/server/JKWindowServer.cpp engine/tools/jkchat/main.cpp
git commit -m "feat(approval): app_tool 승인 스트립 target 표기 + 대상 창 조작 전 썸네일 캡처 (스펙 §5 2-3단)"
```

### Task 10: 프로브 스테이지 3 + 전체 회귀

**Files:**
- Modify: `engine/tools/probes/probe_app_tools.ps1`

- [ ] **Step 1: 체크 추가**

13. 하이라이트 픽셀 실측(Task 8 Step 3의 캡처 게이트).
14. approval_request 이벤트의 target/thumb 필드 + 썸네일 PNG 매직 8바이트 확인.
15. 승인 스트립 문구(jkchat 트랜스크립트 — probe_agent_chat 관용구)에 "[vplayer 창 #7]" 표기.

- [ ] **Step 2: 전체 회귀 공식런**

Run: probe_app_tools ×2 + probe_agent_chat + probe_agent_trust + probe_jkbridge + probe_vpt4/vpt9/vpt5/vpt11 + probe_settings + probe_notes + probe_files + jkagentd --selftest + ./jkdesktop test
Expected: 전부 PASS.

- [ ] **Step 3: Commit**

```bash
git add engine/tools/probes/probe_app_tools.ps1
git commit -m "test(probe): 앱 도구 허브 스테이지 3 체크 + 전체 회귀 GREEN"
```

### Task 11: 콘솔 앱 skill 래퍼 샘플 + docs/58 as-built

**Files:**
- Create: `engine/templates/console-app/SKILL.md`
- Create: `docs/58_app_tool_hub.md`

- [ ] **Step 1: 콘솔 앱 claude skill 래퍼 샘플**

`engine/templates/console-app/SKILL.md` — sampletodo 콘솔 앱 사용 계약을 담는 문서형 래퍼(사용자 확정: "내 skill은 claude skill을 말하는 것"). 배치: chat LLM worker cwd(`state\chat.json` `directory`)의 `.claude\skills\<app>\SKILL.md` 또는 유저 `~\.claude\skills\` — docs/58에 배치 절차 기록. 내용 구조:

```markdown
---
name: console-app-sampletodo
description: sampletodo 콘솔 앱을 에이전트로 구동/조작하는 방법 (jkdesktop P4 SDK)
---
# sampletodo 콘솔 앱

구동: run_console_app {"name":"sampletodo"} — 승인 ask 게이트(permissions.json).
출력 확인: terminal.output 이벤트 + read_log / terminal_exec.
종료: 프로세스 종료는 앱 자체 명령, 강제 종료는 하지 않는다.
```

(실제 명령/출력은 manifest.json과 샘플 README에서 발췌해 채운다 — 템플릿 README가 이미 `agent '<json>'` 원 요청 형식을 문서화한다.)

- [ ] **Step 2: docs/58 as-built 작성**

`docs/58_app_tool_hub.md` — 실제 구현된 최종 상태(설계→차이점, 레슨, 프로브 결과, 스펙 표기 갱신). 커밋과 함께 스펙의 "as-built는 docs/58" 표기 갱신.

- [ ] **Step 3: Commit**

```bash
git add engine/templates/console-app/SKILL.md docs/58_app_tool_hub.md
git commit -m "docs(58): 앱 도구 허브 as-built + 콘솔 앱 claude skill 래퍼 샘플"
```

---

## Self-Review 결과 (작성 시점)

1. **스펙 커버리지**: §3(Task 1) · §4.1(T2) · §4.2(T3 후보/ambiguity) · §4.3(T3) · §4.4(T3) · §5(T8/T9) · §6(T7) · §7(T11) · §8.1(T5) · §8.2(T4) · §9(각 오류 표면 T2/T3 분기) · §10(T6/T10) — 전 섹션 매핑 완료. §0 결정은 Task 배치로 반영.
2. 자리표시자: Task 3 ask 블록과 Task 8의 "TODO 없음" 주석은 **복제 대상 기존 기계를 명시**했으므로 자리표시자가 아니다(구현자가 참조할 정확한 위치 지정). Task 1의 WriteAgentToolCall/ReadAgentToolCall 본문은 "위와 같은 관용구"로 위임 — 구현자가 같은 파일의 WriteAgentToolRegister 완성본을 보고 동일 패턴 적용(수미상관, 위험 낮음).
3. 타입 일관성: `AppToolDef/AppToolManifest/InflightAppTool`(T2) ↔ T3/T8/T9 사용 일치 · `AgentToolDecl/AgentToolCallMsg`(T4) ↔ T5 일치 · `GetArrRaw`(T2a) ↔ T7 사용 일치.