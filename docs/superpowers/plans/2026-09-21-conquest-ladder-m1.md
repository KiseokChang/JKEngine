# 앱 정복 사다리 M1 (send_input + minesweeper 정복) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** send_input 서버 도구(트랙 B 조작 수단)를 구현하고, minesweeper를 도구 표면만으로 5단계(launch/observe/drive/verify/recover) 완주하는 첫 정복 계약 프로브를 확립한다.

**Architecture:** send_input은 서버 도구 1종 — 논리 데스크톱 좌표를 대상 레이어 표면 px로 변환해 기존 InputEventPayload 와이어로 주입(신규 와이어 0). ask 기본 게이트는 run_console_app 선례의 inline-approval 파이프라인을 재사용하고, 승인 시점에 원 요청을 재실행한다(files_access 선례). 정복 프로브는 OS SendInput을 쓰지 않고 도구 호출만으로 5단계를 돌린다 — 이것이 자동화 씨앗 이식성의 전제다.

**Tech Stack:** C++17 (JKWindowServer), QuickJS AgentJson, PowerShell 5.1 probes, MinGW/ninja build.

**Spec:** docs/superpowers/specs/2026-09-21-conquest-ladder-design.md (§2 계약, §3.1 send_input, §3.2 프로브 원칙, §5 사다리)

## Global Constraints

- 작업 디렉터리: 워크트리 `I:\progwork\JKENGINE\.claude\worktrees\conquest-ladder` (브랜치 `worktree-conquest-ladder`). 빌드는 이 워크트리의 `engine/build`에서 — **docs/51 부록의 worktree 빌드 워크플로**를 먼저 따른다(cef `/.` 복사·별도 temp·runtime DLL). 빌드 명령: `cd engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target <T>`.
- **레슨 37**: 빌드 후 exe mtime > 최신 소스 mtime을 확인한다 — 스테일 바이너리로 프로브를 돌리면 "픽스 실패" 오판.
- **레슨 57**: 타겟 빌드는 형제 타겟을 링크하지 않는다 — jkdesktop/jkagentd 양쪽 수정 후 둘 다 빌드.
- 프로브: PowerShell 5.1, **ASCII 전용**(무BOM .ps1에 비ASCII는 cp949 파산 — docs/15), `> log 2>&1` 파일 리다이렉트, **2연통 원칙**(공식런 2회 연속 PASS).
- **레슨 42**: 클라 프로세스는 pid로만 kill(서버와 같은 이미지명 jkdesktop.exe).
- **서버 단일 인스턴스**(docs/59 §11): 프로브 시작 시 기존 jkdesktop 전면 Stop(shoot/trust 선례) — 같은 파이프 이름 인스턴스 갈림 방지.
- **permissions.json은 유저 런타임 파일**(빌드 디렉터리 = 라이브 서버와 동일): 프로브가 수정할 때는 **원본 바이트 백업 + finally 복원 + 콘솔 고지** 필수(docs/59 §16.1 레슨).
- 커밋 메시지는 저장소 관례(`feat(server): …` 한국어 + docs 링크), 각 Task 끝에서 커밋.

---

### Task 1: send_input 서버 도구 — Allow 경로 + 게이트 행 + 브로커 등록

**Files:**
- Modify: `engine/include/server/JKWindowServer.h` (SendInputOp 구조체 + 메서드 2종 선언)
- Modify: `engine/src/server/JKWindowServer.cpp` (kPermMatrix 행 + 도구 분기 + 실행기)
- Modify: `engine/tools/jkagentd/main.cpp` (IsKnownTool + LoadPermissions + MCP 카탈로그)
- Test: `engine/tools/probes/probe_send_input.ps1` (신설)

**Interfaces:**
- Consumes: `jk::agent::AgentJson`(GetObjRaw/GetStr/GetInt), `ipc::InputEventPayload`(JKWireProtocol.h:234), `SendInputEvent()`(JKWindowServer.cpp:1574), `FindClientById/FocusClient/AgentToolAllowed`
- Produces: `JKWindowServer::SendInputOp{op,target,x,y,dx,dy,key,mods,button,clicks,text,action}`, `static std::string JKWindowServer::BuildSendInputOp(const jk::agent::AgentJson& args, SendInputOp*)`(빈 문자열=성공, 아니면 error 키), `std::string JKWindowServer::ExecuteSendInputOp(const SendInputOp&)`(빈 문자열=성공) — Task 2의 승인 재실행이 이 시그니처를 그대로 소비한다. 도구 응답: 성공 `{"ok":true,"sent":true}`, 실패 `{"ok":false,"error":"<키>"}`(키: bad_op/bad_target/bad_key/bad_text/bad_action/window_not_found/bad_target(셸)/permission_denied).

- [ ] **Step 1: 워크트리 빌드 환경 준비 + 기준 빌드**

docs/51 부록 절차로 워크트리 `engine/build` configure+풀빌드(jkdesktop+jkagentd). 기존 빌드가 있으면 ninja 증분. 완료 후 `ls -la engine/build/jkdesktop.exe`로 산출물 확인.

- [ ] **Step 2: 실패할 프로브 작성** — `engine/tools/probes/probe_send_input.ps1`:

```powershell
# send_input tool probe (spec 2026-09-21-conquest-ladder Task 1).
# Conventions: probe_agent_maximize.ps1 (server lifecycle + MCP pipe +
# escaped tool-text regex), lesson 42 (pid-only client kill), ASCII-only
# PS5.1, "> log 2>&1" redirect. The probe edits permissions.json -> backup,
# restore in finally, console notice (docs/59 s16.1 lesson).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\.claude\worktrees\conquest-ladder\engine\build"
$exe   = "$build\jkdesktop.exe"
$agnt  = "$build\jkagentd.exe"
$perm  = "$build\permissions.json"
$script:fail = 0
function Check([string]$name, [bool]$cond) {
    if ($cond) { Write-Output "PASS $name" } else { Write-Output "FAIL $name"; $script:fail++ }
}
function Stop-ProbeProcs {
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
}
function Invoke-Mcp([string]$line) { return (($line | & $agnt) -join "`n") }

Stop-ProbeProcs
Start-Sleep -Seconds 1
# permissions: send_input must be ALLOW for Task 1 (default is ask = parked).
# RMW: set the key on the existing file, restore original bytes in finally.
Write-Output "NOTICE: editing $perm (backup+restore)"
Copy-Item $perm "$perm.probe_bak" -Force
try {
    $json = Get-Content $perm -Raw | ConvertFrom-Json
    $json | Add-Member -NotePropertyName send_input -NotePropertyValue "allow" -Force
    $json | ConvertTo-Json -Depth 5 | Set-Content $perm -Encoding ASCII
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -WindowStyle Hidden
    Start-Sleep -Seconds 4

    Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"minesweeper"}}}' | Out-Null
    $win = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $r = Invoke-Mcp '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
        $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"([^"\\]*)\\",\\"pid\\":(\d+),' +
                             '\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),\\"h\\":(\d+)')
        if ($m.Success) {
            $win = [pscustomobject]@{ id=[int]$m.Groups[1].Value; pid=[int]$m.Groups[3].Value
                x=[int]$m.Groups[4].Value; y=[int]$m.Groups[5].Value
                w=[int]$m.Groups[6].Value; h=[int]$m.Groups[7].Value }
            break
        }
    }
    Check "launch-window" ($null -ne $win)
    if ($null -ne $win) {
        function Capture-Hash([int]$wid) {
            $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"capture_window","arguments":{"id":' + $wid + '}}}')
            # path extraction: reuse probe_agent_shot.ps1's escaped-path regex
            $pm = [regex]::Match($r, 'path\\":\\"([^"\\]*(?:\\.[^"\\]*)*)\\"')
            if (-not $pm.Success) { return "" }
            $p = $pm.Groups[1].Value -replace '\\\\', '\'
            if (-not (Test-Path $p)) { return "" }
            return (Get-FileHash $p -Algorithm SHA256).Hash
        }
        $h0 = Capture-Hash $win.id
        Check "capture-before" ($h0 -ne "")

        $cx = $win.x + [int]($win.w / 2)
        $cy = $win.y + [int]($win.h / 2)
        $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"click","x":' + $cx + ',"y":' + $cy + '}}}')
        Check "click-ok" ($r -match 'sent\\":true')
        Start-Sleep -Milliseconds 700
        $h1 = Capture-Hash $win.id
        Check "click-changes-pixels" ($h1 -ne "" -and $h1 -ne $h0)

        $r = Invoke-Mcp '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"send_input","arguments":{"id":9999,"op":"click","x":10,"y":10}}}'
        Check "bad-id" ($r -match 'window_not_found')
        $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"nosuch"}}}')
        Check "bad-op" ($r -match 'bad_op')
        $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"key","key":1073741883}}}')
        Check "key-ok" ($r -match 'sent\\":true')   # SDLK_F2 smoke: delivery only
        $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"wheel","dy":1}}}')
        Check "wheel-ok" ($r -match 'sent\\":true')
    }
} finally {
    Stop-ProbeProcs
    Copy-Item "$perm.probe_bak" $perm -Force
    Remove-Item "$perm.probe_bak" -Force
    Write-Output "NOTICE: permissions.json restored"
}
if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "ALL PASS"
exit 0
```

- [ ] **Step 3: 프로브 1런 → FAIL 확인** (send_input unknown_tool/bad_request — 도구 미구현). Run: `cd engine/tools/probes && pwsh -NoProfile -File probe_send_input.ps1 > probe_send_input.log 2>&1` (PS5.1: `powershell`). Expected: FAIL unknown tool 또는 launch 단계 이전 실패. **로그를 파일로 남긴다**(레슨: 파이프 버퍼링 행걸 오판).

- [ ] **Step 4: 헤더 구현** — `JKWindowServer.h` 클래스 private 섹션(PendingApproval 근처):

```cpp
        // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): send_input의
        // 실행 원본 — 도구 경로와 승인 재실행 경로(approve)가 같은 구조를
        // 소비한다(files_access의 파킹-재실행 선례).
        struct SendInputOp {
            std::string op;              // "click"|"key"|"type"|"wheel"
            uint32_t target = 0;         // window id (셸 제외)
            int32_t x = 0, y = 0;        // click: 논리 데스크톱 좌표
            int32_t dx = 0, dy = 0;      // wheel: 델타
            uint32_t key = 0;            // key: SDL keycode
            uint32_t mods = 0;           // key: SDL mod
            uint32_t button = 1;         // click: 마우스 버튼
            uint32_t clicks = 1;         // click: 클릭 수
            std::string text;            // type: UTF-8 (63B 단위 분할 발송)
            std::string action = "tap";  // key: "tap"|"down"|"up"
        };
        static std::string BuildSendInputOp(
            const jk::agent::AgentJson& args, SendInputOp* out);
        std::string ExecuteSendInputOp(const SendInputOp& op);
```

(파일 상단에 `#include "agent/JKAgentJson.h"`가 이미 있거나 추가 — 실행자가 grep로 확인.)

- [ ] **Step 5: 서버 구현** — `JKWindowServer.cpp`:

(a) kPermMatrix에 행 추가(`{"app_tool", "server", "allow"},` 뒤):

```cpp
    // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): 트랙 B 조작
    // 수단 — 대상 창 합성 입력. ask 기본(2026-09-21 사용자 승인): 매 호출이
    // 승인 파킹으로 들어간다. 자동화 편의는 permissions.json에서 allow로.
    {"send_input", "server", "ask"},
```

(b) `ExecuteSendInputOp` — HandleEvent 실시간 경로(1482-1494) 변환식을 send_input용으로 소거한 형태:

```cpp
// 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): send_input 실행기.
// 도구 경로와 승인 재실행 경로가 공유한다. 좌표는 논리 데스크톱 좌표 — 실시간
// 경로의 표면 변환식은 ((mx/outputScale) - client->X()) / layerScale (mx는
// 물리 px)인데 send_input은 논리 좌표를 받으므로 물리 전곱이 소거돼
// (논리 - client->X()) / layerScale이 된다. 셸은 대상에서 제외 — 크롬 닫기는
// close_window가 담당한다.
std::string JKWindowServer::ExecuteSendInputOp(const SendInputOp& op) {
    JKClientConnection* client = FindClientById(op.target);
    if (!client || client->IsDisconnected()) return "window_not_found";
    if (client->IsShell()) return "bad_target";
    ipc::InputEventPayload p{};
    p.surfaceId = op.target;
    if (op.op == "click") {
        float sx = 1.0f, sy = 1.0f;
        if (compositor_) {
            if (auto* layer = compositor_->FindLayerById(op.target)) {
                sx = layer->ScaleX();
                sy = layer->ScaleY();
            }
        }
        p.x = static_cast<int>(std::llround((op.x - client->X()) / sx));
        p.y = static_cast<int>(std::llround((op.y - client->Y()) / sy));
        p.keyCode = op.button;
        p.detail = op.clicks;
        p.option = 0;
        p.type = ipc::InputEventType::MouseDown;
        SendInputEvent(*client, p);
        p.type = ipc::InputEventType::MouseUp;
        SendInputEvent(*client, p);
        return "";
    }
    if (op.op == "key") {
        if (op.key == 0) return "bad_key";
        p.keyCode = op.key;
        p.option = op.mods;
        if (op.action != "up") {
            p.type = ipc::InputEventType::KeyDown;
            p.detail = 0;
            SendInputEvent(*client, p);
        }
        if (op.action != "down") {
            p.type = ipc::InputEventType::KeyUp;
            SendInputEvent(*client, p);
        }
        return "";
    }
    if (op.op == "type") {
        if (op.text.empty()) return "bad_text";
        // Char 페이로드는 63B — UTF-8 후속 바이트(0x80-0xBF)를 넘지 않게 분할.
        size_t off = 0;
        while (off < op.text.size()) {
            size_t len = std::min<size_t>(63, op.text.size() - off);
            while (len > 0 && (op.text[off + len] & 0xC0) == 0x80) --len;
            p.type = ipc::InputEventType::Char;
            std::memcpy(p.text, op.text.c_str() + off, len);
            p.text[len] = '\0';
            SendInputEvent(*client, p);
            off += len;
        }
        return "";
    }
    if (op.op == "wheel") {
        p.type = ipc::InputEventType::MouseWheel;
        p.dx = op.dx;
        p.dy = op.dy;
        SendInputEvent(*client, p);
        return "";
    }
    return "bad_op";
}

// 도구 인자(args 오브젝트) → SendInputOp. 빈 문자열=성공, 아니면 error 키.
std::string JKWindowServer::BuildSendInputOp(
    const jk::agent::AgentJson& args, SendInputOp* out) {
    std::string op, text, action;
    int id = 0, x = 0, y = 0, dx = 0, dy = 0, key = 0, mods = 0;
    int button = 1, clicks = 1;
    args.GetStr("op", op);
    args.GetInt("id", id);
    args.GetInt("x", x);
    args.GetInt("y", y);
    args.GetInt("dx", dx);
    args.GetInt("dy", dy);
    args.GetInt("key", key);
    args.GetInt("mods", mods);
    args.GetInt("button", button);
    args.GetInt("clicks", clicks);
    args.GetStr("text", text);
    args.GetStr("action", action);
    if (op != "click" && op != "key" && op != "type" && op != "wheel")
        return "bad_op";
    if (id <= 0) return "bad_target";
    if (action.empty()) action = "tap";
    if (action != "tap" && action != "down" && action != "up") return "bad_action";
    SendInputOp& o = *out;
    o.op = op;
    o.target = static_cast<uint32_t>(id);
    o.x = x;
    o.y = y;
    o.dx = dx;
    o.dy = dy;
    o.key = static_cast<uint32_t>(key);
    o.mods = static_cast<uint32_t>(mods);
    o.button = static_cast<uint32_t>(button > 0 ? button : 1);
    o.clicks = static_cast<uint32_t>(clicks > 0 ? clicks : 1);
    o.text = text;
    o.action = action;
    return "";
}
```

(c) 도구 분기 — `} else if (tool == "run_console_app") {` **앞**에 삽입:

```cpp
    } else if (tool == "send_input") {
        // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): 트랙 B 조작
        // 수단 — 대상 창에 합성 입력. ask 기본 게이트(run_console_app와 같은
        // inline-approval 파이프라인, 승인 시점 원 요청 재실행 = files_access
        // 선례). 원문 args는 승인 재실행을 위해 파킹에 함께 저장한다.
        std::string rawArgs;
        req.GetObjRaw("args", rawArgs);
        jk::agent::AgentJson args(rawArgs);
        SendInputOp op;
        const std::string buildErr =
            args.ok() ? BuildSendInputOp(args, &op) : "bad_args";
        if (!buildErr.empty()) {
            reply = "{\"ok\":false,\"error\":\"" + buildErr + "\"}";
        } else {
            switch (AgentToolAllowed("send_input")) {
                case AgentDecision::Allow: {
                    const std::string ex = ExecuteSendInputOp(op);
                    reply = ex.empty() ? "{\"ok\":true,\"sent\":true}"
                                       : "{\"ok\":false,\"error\":\"" + ex + "\"}";
                    break;
                }
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
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "send_input";
                    p.name = op.op;           // 승인 스트립 표시용 조작명
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = op.target;
                    p.sendArgs = rawArgs;     // 승인 시점 원 요청 재실행 원문
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[512];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"send_input\","
                                  "\"name\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(op.op).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // 승인 해소 시 응답
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
```

- [ ] **Step 6: 브로커 등록** — `engine/tools/jkagentd/main.cpp` 3곳:
  (a) `IsKnownTool` kNames에 `"send_input"` 추가(app_tool 뒤).
  (b) `LoadPermissions` kNames에 `"send_input"` 추가 — **기본 true(통과)**: ask는 서버 파이프라인으로 통과가 정의(주석 122-127)이고 서버 kPermMatrix가 ask를 강제한다.
  (c) MCP 도구 카탈로그 JSON 배열 끝(`app_tool` 항목 뒤)에 추가:

```
{"name":"send_input","description":"Send synthetic input to a window: click/key/type/wheel. Coordinates are logical desktop points; the server converts to the target surface. Default gate is ask (approval strip)","inputSchema":{"type":"object","properties":{"id":{"type":"integer"},"op":{"type":"string","enum":["click","key","type","wheel"]},"x":{"type":"integer"},"y":{"type":"integer"},"key":{"type":"integer"},"mods":{"type":"integer"},"button":{"type":"integer"},"clicks":{"type":"integer"},"dx":{"type":"integer"},"dy":{"type":"integer"},"text":{"type":"string"},"action":{"type":"string","enum":["tap","down","up"]}},"required":["id","op"]}}
```

- [ ] **Step 7: 빌드** — `cmake --build . --target jkdesktop` **and** `--target jkagentd`. mtime 확인(레슨 37). `jkagentd --selftest` 실행 → 0 failures(카탈로그 어설트가 있으면 갱신).

- [ ] **Step 8: 프로브 공식런 ×2** — 2연속 ALL PASS. 첫 런에서 click-changes-pixels가 FAIL이면: 캡처 PNG 2장을 눈으로 대조하고, 좌표 변환 검증(클릭 전 list_windows 좌표와 minesweeper 보드 기하 320x380 대조) 후 원인 규명 — 추측성 보정 금지(레슨 17).

- [ ] **Step 9: Commit**

```bash
git add engine/include/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp engine/tools/jkagentd/main.cpp engine/tools/probes/probe_send_input.ps1
git commit -m "feat(server): send_input 도구 — 정복 사다리 트랙 B 조작 수단 (스펙 2026-09-21-conquest-ladder §3.1)"
```

---

### Task 2: send_input ask 파킹 + 승인 시점 재실행

**Files:**
- Modify: `engine/include/server/JKWindowServer.h` (PendingApproval에 sendArgs 필드)
- Modify: `engine/src/server/JKWindowServer.cpp` (approve resolver에 send_input 재실행 분기)
- Test: `engine/tools/probes/probe_send_input_ask.ps1` (신설)

**Interfaces:**
- Consumes: Task 1의 `BuildSendInputOp/ExecuteSendInputOp`, approve resolver의 files_access 재실행 분기(JKWindowServer.cpp:4864 선례), `AgentToolAllowed("send_input")`
- Produces: `PendingApproval.sendArgs`(std::string — args 오브젝트 원문 JSON). 승인 resolve 시: 게이트 재검사(Deny면 permission_denied) + 원 요청 재실행. 응답 = 재실행 결과(성공 `{"ok":true,"sent":true}`, 거부 `{"ok":false,"error":"denied_by_user"}`).

- [ ] **Step 1: PendingApproval 필드 추가** — `JKWindowServer.h` struct PendingApproval의 filesLimit 뒤:

```cpp
        // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): send_input의
        // 재실행 원본 — 파킹된 args 원문을 승인 시점에 다시 파싱해 실행한다
        // (files_access의 승인 시점 재검증+재실행 선례).
        std::string sendArgs;      // send_input: args 오브젝트 원문 JSON
```

- [ ] **Step 2: approve resolver 분기** — `} else if (it->kind == "app_tool") {` **앞**에 삽입:

```cpp
                    } else if (it->kind == "send_input") {
                        // 승인 시점 게이트 재검사(파킹 대기 중 permissions.json이
                        // 바뀌면 최신 게이트 강제 — files_access/run_console_app
                        // 선례) + 원 요청 재실행.
                        if (AgentToolAllowed("send_input") ==
                            AgentDecision::Deny) {
                            result =
                                "{\"ok\":false,\"error\":\"permission_denied\"}";
                        } else {
                            jk::agent::AgentJson args(it->sendArgs);
                            SendInputOp op;
                            const std::string buildErr =
                                args.ok() ? BuildSendInputOp(args, &op)
                                          : "bad_args";
                            const std::string ex =
                                buildErr.empty() ? ExecuteSendInputOp(op)
                                                 : buildErr;
                            result = ex.empty()
                                ? "{\"ok\":true,\"sent\":true}"
                                : "{\"ok\":false,\"error\":\"" + ex + "\"}";
                        }
```

**주의(레슨 35)**: approve resolver는 HandleAgentQuery 핫패스(clientsMutex_ 보유 전제)다. `ExecuteSendInputOp`가 이 컨텍스트에서 호출하는 FindClientById/SendInputEvent는 이미 같은 전제의 핫패스 관용구다. 구현 시 `FindClientById`와 `FocusClient`가 내부에서 락을 다시 잡는지 반드시 grep로 확인하고, 잡는다면 Unsafe 코어 분리 관용구(주석 1948 선례)로 해결한다. (본 플랜의 Task 1 실행기는 FocusClient를 부르지 않게 설계돼 있다 — 클릭이 포커스를 안 가져가도 다음 key/type 주입은 대상 id로 직행하므로 동작상 문제가 없다.)

- [ ] **Step 3: 실패 프로브 작성** — `engine/tools/probes/probe_send_input_ask.ps1`. 구성(코드는 probe_send_input.ps1을 복제하고 아래를 바꾼다 — 이 파일에 없는 헬퍼는 복사해 간다, 실행자는 임의 축약 금지):
  1. permissions RMW를 `"send_input":"ask"`로.
  2. 서버 기동 + minesweeper 스폰 + list_windows로 id/기하 취득(동일).
  3. **승인 요청 id 획득은 probe_approve_self.ps1(7/7 선례)의 요청 id 획득 관례를 그대로 재사용**한다 — 그 파일의 헬퍼/패턴을 읽고 동일 방식으로 approval_request의 request 필드를 취득한다. (이 프로브는 MCP/agentctl 경로를 쓴다 — 승인자는 요청자와 **다른 연결**이어야 하고, jkagentd 원샷 호출마다 연결이 새로 열리므로 "백그라운드 잡 = 요청자, 새 원샷 = 승인자" 구조가 자기 승인 봉쇄를 자연히 회피한다.)
  4. allow 시나리오: 백그라운드 잡으로 parked send_input 클릭 발사 → approval_request 이벤트에서 request id 취득 → 새 원샷 `{"request":N,"decision":"allow"}` → 잡의 최종 응답이 `"ok\":true,\"sent\":true` + **클릭이 실제 착지**(capture hash 변화) 2중 단언.
  5. deny 시나리오: 같은 절차로 decision `"deny"` → 잡 응답 `"error\":\"denied_by_user"` + 해시 불변 단언.
  6. finally: permissions 복원 + 프로세스 정리(동일).

- [ ] **Step 4: 프로브 공식런 ×2** — 2연속 ALL PASS(60s 승인 만료 안에 resolve — 잡과 승인 사이 수면 초과 금지).

- [ ] **Step 5: Commit**

```bash
git add engine/include/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp engine/tools/probes/probe_send_input_ask.ps1
git commit -m "feat(server): send_input ask 파킹 + 승인 시점 원 요청 재실행 (스펙 §3.1, files_access 선례)"
```

---

### Task 3: 정복 계약 프로브 — minesweeper (템플릿 확립)

**Files:**
- Test: `engine/tools/probes/probe_conquest_minesweeper.ps1` (신설 — 이것이 곧 정복 템플릿)

**Interfaces:**
- Consumes: launch_app/list_windows/capture_window/send_input(전부 Task 1-2 산출), agent 이벤트(window.created/app.crashed — 읽기 관례는 probe_agent_events.ps1 재사용)
- Produces: 정복 프로브 골격(5단계 사이클) — docs/62가 이 파일을 템플릿으로 문서화. 다음 앱(tetris)은 이 파일을 복제해 시나리오만 바꾼다.

**핵심 원칙(스펙 §3.2): drive/verify는 도구 표면만 — OS SendInput 직접 호출 금지.**

- [ ] **Step 1: 프로브 작성** — 5단계 전 사이클. 골격:

```powershell
# Conquest contract probe: minesweeper (spec 2026-09-21-conquest-ladder s2).
# The FIRST conquest ladder rung - this file IS the conquest template:
# launch -> observe -> drive -> verify -> recover, all via the agent tool
# surface ONLY (send_input/app_tool, never OS SendInput - spec s3.2).
# Conventions: probe_send_input.ps1 (lifecycle/MCP/capture hash), lesson 28
# (subscribe-first event jobs), lesson 42 (pid-only kill).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\.claude\worktrees\conquest-ladder\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
$perm = "$build\permissions.json"
$script:fail = 0
function Check([string]$name, [bool]$cond) { ... }   # probe_send_input.ps1 복제
function Stop-ProbeProcs { ... }                      # 동일
function Invoke-Mcp([string]$line) { ... }            # 동일
function Get-MineWindow { ... }                       # probe_agent_maximize.ps1 정규식 복제
function Capture-Hash([int]$wid) { ... }              # probe_send_input.ps1 복제

# Stage helpers - the reusable conquest skeleton:
function Invoke-ConquestCycle {
    # (1) launch: launch_app + window 등장 폴링(하드 게이트)
    # (2) observe: list_windows(id/title/pid/기하) + capture_window(hash 비공백)
    # (3) drive: send_input click (보드 중앙, 논리 좌표)
    # (4) verify: capture hash 변화 + list_windows에 창 생존
    # 반환: 성공 $true/$false. 각 단계에서 Check "<stage>-<name>" 호출.
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
Copy-Item $perm "$perm.probe_bak" -Force
try {
    $json = Get-Content $perm -Raw | ConvertFrom-Json
    $json | Add-Member -NotePropertyName send_input -NotePropertyValue "allow" -Force
    $json | ConvertTo-Json -Depth 5 | Set-Content $perm -Encoding ASCII

    # event jobs: window.created (launch) + app.crashed (recover) - subscribe
    # BEFORE the triggering call (lesson 28). Reading convention: copy
    # probe_agent_events.ps1's event-reading helper verbatim; these are soft
    # checks (the hard gates are list_windows polling + capture hashes).
    # soft check: window.created event observed after launch
    Check "launch-window-created-event" ($createdEvent -ne $null)

    $ok = Invoke-ConquestCycle          # 1st cycle
    Check "cycle1" $ok

    # (5) recover: kill the client by PID -> app.crashed -> relaunch ->
    #     full cycle again. soft check: app.crashed event observed.
    Stop-Process -Id $win.pid -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    Check "recover-app-crashed-event" ($crashedEvent -ne $null)
    $ok = Invoke-ConquestCycle          # 2nd cycle after recovery
    Check "cycle2-after-recover" $ok
} finally {
    Stop-ProbeProcs
    Copy-Item "$perm.probe_bak" $perm -Force
    Remove-Item "$perm.probe_bak" -Force
    Write-Output "NOTICE: permissions.json restored"
}
if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "CONQUEST PASS"
exit 0
```

구현 세부: `Invoke-ConquestCycle`은 위 골격의 주석 스펙을 그대로 코드로 — launch는 `launch_app {"app":"minesweeper"}` 후 list_windows 폴링(20×500ms), drive는 `send_input {"op":"click","x":x+w/2,"y":y+h/2}`, verify는 `capture_window` 해시 2회 비교(전/후) + list_windows 재확인. 이벤트 소프트 체크는 probe_agent_events.ps1의 이벤트 읽기 헬퍼를 그대로 복사해 쓴다(실행자가 임의 재구현하지 않는다). 스폰 간 500ms 스로틀(docs/28)이 있으므로 2차 사이클 launch 전 Start-Sleep 1.

- [ ] **Step 2: 프로브 공식런 ×2** — 2연속 `CONQUEST PASS`. recover 단계에서 app.crashed가 안 오면: 서버 SpawnProcess의 크래시 분류 조건(코드 ∉ {STILL_ACTIVE, 0})을 grep로 확인하고 Stop-Process의 종료 코드가 분류에 걸리는지 실측(Force kill = 비정상 종료여야 app.crashed) — 이벤트 미관측이면 소프트 체크를 진단 로그로 남기고 하드 게이트(cycle2)만으로 PASS 판정, 이유를 docs/62에 기록.

- [ ] **Step 3: Commit**

```bash
git add engine/tools/probes/probe_conquest_minesweeper.ps1
git commit -m "feat(probe): 정복 계약 프로브 minesweeper — 5단계 템플릿 확립 (스펙 §2/§3.2)"
```

---

### Task 4: docs/62 as-built + 회귀 스윕 + LLM 실전 체크리스트

**Files:**
- Create: `docs/62_conquest_ladder.md`
- Modify: `docs/README.md` (인덱스 — 파일이 있다면; 실행자가 확인)
- Modify: `docs/superpowers/specs/2026-09-21-conquest-ladder-design.md` (상태를 as-built로 갱신)

- [ ] **Step 1: docs/62 작성** — 구성: §1 왜/스펙 링크, §2 send_input as-built(인자표/게이트/승인 재실행), §3 정복 프로브 템플릿 설명(probe_conquest_minesweeper.ps1이 템플릿 — 복제해 시나리오만 교체), §4 검증 결과(프로브 2연통 수치), §5 사다리 현황표(minesweeper 정복 완료, 나머지 대기), §6 **LLM 실전 체크리스트**(아래 문구), §7 레슨(실측 것만).

§6 LLM 실전 체크리스트(minesweeper):
```
1. 폰/채팅에서: "지뢰찾기 켜 줘" → launch_app (MCP)
2. "한 칸 열어 줘" → send_input click (승인 스트립에서 허용)
3. 결과 확인: 창 상태 응답 or 스크린샷
```

- [ ] **Step 2: 회귀 스윕** — probe_app_tools ×2 + `jkdesktop test` 0 + probe_agent_e2e ×1(minesweeper 스폰 경로 무손상). 전부 GREEN.

- [ ] **Step 3: 스펙 상태 갱신** — 스펙 헤더의 `상태:`를 "M1 구현 완료(2026-09-21), minesweeper 정복 — LLM 실전 체크리스트는 docs/62 §6"로.

- [ ] **Step 4: Commit + push**

```bash
git add docs/62_conquest_ladder.md docs/README.md docs/superpowers/specs/2026-09-21-conquest-ladder-design.md
git commit -m "docs: 정복 사다리 M1 as-built — send_input+minesweeper 정복 (docs/62)"
git push -u origin worktree-conquest-ladder
```

---

## Self-Review 결과

- **스펙 커버**: §2 계약→Task 3(프로브 5단계), §3.1 send_input→Task 1+2, §3.2 원칙→Task 3 골격 주석, §5 첫 목표 minesweeper→Task 3, §6 기록→Task 4, §8 ask 확정→kPermMatrix 행. §3.3 LLM 체크리스트→Task 4 §6. 스펙 §2의 (b) LLM 실전 세션은 사용자 수행 항목이라 docs/62 §6에 절차로 기록(구현 범위 밖).
- **플레이스홀더**: 없음 — "복제" 지시는 전부 구체 원본 파일 지정(probe_send_input.ps1의 헬퍼, probe_agent_events.ps1의 이벤트 헬퍼, probe_approve_self.ps1의 request-id 관례)이며, 복제 대상은 실존 검증된 선례다.
- **타입 일치**: `SendInputOp`/`BuildSendInputOp`/`ExecuteSendInputOp`/`PendingApproval.sendArgs` — Task 1 정의 → Task 2 소비, 시그니처 동일. 도구 응답 형식(`sent":true`) 프로브 단언과 일치.