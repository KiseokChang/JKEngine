# jkchat LLM 연동 계획 (claude CLI 서브프로세스 — ollama launch claude)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 채팅창의 슬래시가 아닌 입력(자연어)을 `ollama launch claude -p` 헤드리스 서브프로세스로 위임하고, 최종 결과 텍스트만 트랜스크립트에 표시한다 (MVP — 스트리밍은 후속).

**Architecture:** jkchat(Win32)은 NL 메시지를 받으면 **워커 스레드**로 자식 프로세스(`ollama launch claude --model <m> -- -p "<msg>" --output-format json [--resume <sid>]`)를 띄우고 stdout을 끝까지 읽어 `{result, session_id}`를 파싱, `PostMessage`로 UI에 돌린다. claude의 도구 호출은 `.mcp.json`의 jkagentd(MCP) 경로로 나가므로 권한 관문·receipts·ask 승인 파이프라인이 **무변경**으로 동작한다. LLM 실행 중에도 UI(400ms 펌프·승인 버튼)는 살아 있어야 하므로 UI 스레드 블로킹 금지.

**Tech Stack:** Win32(CreateProcessW + anonymous pipe + worker thread + PostMessage), jkcore(AgentJson), claude_wrapper 가이드(`I:\progwork\claude_wrapper\claude_cli_wrapper_guide.md` §1.2, §2)의 실측 플래그.

**Spec:** 스펙 §6.2 "자연어 = 파서→에이전트 위임"; 사용자 결정(2026-09-10): A안(claude CLI 서브프로세스), 엔진은 `ollama launch claude` (workbench `I:\progwork\claude_wrapper\workbench\config.json` 관례 재사용), MVP 출력 = 최종 결과만.

## Global Constraints

- **UI 스레드에서 claude 프로세스를 동기 대기하지 않는다** — 워커 스레드 + `PostMessage(WM_APP)`만이 결과를 UI로 전달한다.
- 설정은 `<exeDir>\state\chat.json` — 없으면 기본값(`engine:"ollama"`, `model:"kimi-k2.7-code:cloud"`, `skip_permissions:true`, `directory:"I:\\progwork\\JKENGINE"`).
- 프롬프트 인젝션 주의: `-p "<메시지>"`의 메시지는 cmd 인자로 전달 — 큰따옴표 이스케이프(`"` → `\"`)만 하고 나머지는 claude가 받는다.
- 응답 JSON은 UTF-8 — 기존 Utf8ToWide 헬퍼로 UI 반영.
- 세션 연속성: 응답의 `session_id`를 저장, 다음 NL 턴에 `--resume`. `/new`로 리셋.
- 승인 UX 불변: claude의 도구 호출이 ask 모드에 걸리면 기존 프롬프트로 해결 (워커 스레드 덕에 가능).
- 빌드: `cd engine && export PATH="/c/msys64/ucrt64/bin:$PATH" && cmake --build build --target jkchat`.
- 커밋: task-per-commit, `Co-Authored-By: Claude Code <noreply@anthropic.com>`, main push.

---

### Task 1: jkchat — chat.json 설정 + 엔진 커맨드 조립

**Files:**
- Modify: `engine/tools/jkchat/main.cpp`

**Interfaces:**
- Produces (Task 2 사용):
  - `struct ChatConfig { std::string engine; std::string model; bool skipPermissions; std::string directory; }`
  - `ChatConfig LoadChatConfig()` — `<exeDir>\state\chat.json` 읽기(AgentJson), 없으면 기본값
  - `std::wstring BuildEngineCmd(const ChatConfig& cfg, const std::string& prompt, const std::string& resumeSessionId)` — UTF-16 커맨드라인
- Consumes: jk::agent::AgentJson, Utf8ToWide/WideToUtf8 헬퍼

- [ ] **Step 1: 설정 구조체 + 로더**

```cpp
// state/chat.json — user tunables (workbench config.json convention).
struct ChatConfig {
    std::string engine = "ollama";     // "ollama" | "claude" | "stub"
    std::string model = "kimi-k2.7-code:cloud";
    bool skipPermissions = true;
    std::string directory = "I:\\progwork\\JKENGINE";  // .mcp.json lives here
};

static std::string ExeDir() {
    char path[1024] = {};
    GetModuleFileNameW(nullptr, /*needs wide*/ nullptr, 0); // replaced below
    return path;
}
```
(실제 구현: `GetModuleFileNameA` + 마지막 슬래시 절단 — 서버의 StateDir 패턴과 동일.)

```cpp
static ChatConfig LoadChatConfig() {
    ChatConfig cfg;
    char path[1024] = {};
    GetModuleFileNameA(nullptr, path, sizeof(path));
    std::string dir = path;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    std::FILE* f = std::fopen((dir + "\\state\\chat.json").c_str(), "rb");
    if (!f) return cfg;
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson c(buf);
    std::string v;
    if (c.ok()) {
        if (c.GetStr("engine", v)) cfg.engine = v;
        if (c.GetStr("model", v)) cfg.model = v;
        if (c.GetStr("directory", v)) cfg.directory = v;
        int skip = -1;
        if (c.GetInt("skip_permissions", skip)) cfg.skipPermissions = (skip != 0);
    }
    return cfg;
}
```
(chat.json의 skip_permissions는 int 0/1로 문서화.)

- [ ] **Step 2: 커맨드 조립**

```cpp
// claude_wrapper guide §2.2: ollama launch claude --model <m> -- [claude args]
static std::wstring BuildEngineCmd(const ChatConfig& cfg,
                                   const std::string& prompt,
                                   const std::string& resumeSessionId) {
    std::string p = prompt;
    // -p argument escaping: only quotes (the rest reaches claude verbatim).
    std::string esc;
    for (char ch : p) {
        if (ch == '"') esc += "\\\"";
        else esc += ch;
    }
    std::string claudeArgs = "-p \"" + esc + "\" --output-format json";
    if (cfg.skipPermissions) claudeArgs += " --dangerously-skip-permissions";
    if (!resumeSessionId.empty()) {
        claudeArgs += " --resume \"" + resumeSessionId + "\"";
    }
    if (!cfg.directory.empty()) {
        claudeArgs += " --directory \"" + cfg.directory + "\"";
    }

    std::string cmd;
    if (cfg.engine == "stub") {
        // No-network machinery test: emits a valid reply JSON.
        cmd = "cmd.exe /c echo {\"result\":\"stub ok\",\"session_id\":\"stub-1\"}";
    } else if (cfg.engine == "claude") {
        cmd = "claude " + claudeArgs;
    } else {  // "ollama" (default)
        cmd = "ollama launch claude --model \"" + cfg.model + "\" -- " + claudeArgs;
    }
    return Utf8ToWide(cmd);
}
```

- [ ] **Step 3: 빌드**

Run: `cmake --build build --target jkchat` → 0 errors.

- [ ] **Step 4: Commit**

```bash
git add engine/tools/jkchat/main.cpp
git commit -m "feat(chat): chat.json config + engine command assembly (chat LLM MVP)"
```

---

### Task 2: 워커 스레드 + 파이프 읽기 + UI 배선

**Files:**
- Modify: `engine/tools/jkchat/main.cpp`

**Interfaces:**
- Consumes: Task 1의 `BuildEngineCmd`, `LoadChatConfig`.
- Produces:
  - NL 입력(슬래시 아님) → LLM 턴 시작(비동기), 완료 시 트랜스크립트에 결과 표시
  - `session_id` 보관 + `/new` 리셋
  - UI 배선: WM_APP+1(완료) 처리

- [ ] **Step 1: 워커 스레드**

```cpp
struct LlmTurnResult {
    bool ok = false;
    std::string result;     // "result" field of the reply JSON
    std::string sessionId;  // "session_id" field ("" on parse failure)
};

static const UINT WM_APP_LLM_DONE = WM_APP + 1;
static std::string g_sessionId;   // claude session continuity (--resume)
static std::atomic<int> g_llmBusy{0};

static DWORD WINAPI LlmThread(LPVOID param) {
    // param = heap-allocated prompt (freed here)
    std::wstring* prompt = static_cast<std::wstring*>(param);
    const ChatConfig cfg = LoadChatConfig();
    const std::wstring cmd = BuildEngineCmd(cfg, WideToUtf8(*prompt),
                                            g_sessionId);
    delete prompt;

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE readEnd = nullptr, writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0)) { g_llmBusy = 0; return 0; }
    // Our read end must NOT be inherited by the child.
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    std::wstring full = L"cmd.exe /c " + cmd;
    std::vector<wchar_t> mutableCmd(full.begin(), full.end());
    mutableCmd.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writeEnd;
    si.hStdError = writeEnd;
    PROCESS_INFORMATION pi{};
    const BOOL spawned = CreateProcessW(nullptr, mutableCmd.data(), nullptr,
                                        nullptr, TRUE, CREATE_NO_WINDOW,
                                        nullptr, nullptr, &si, &pi);
    CloseHandle(writeEnd);  // child holds its end now
    LlmTurnResult* out = new LlmTurnResult;
    if (!spawned) {
        out->result = "engine spawn failed";
        PostMessageW(g_hMain, WM_APP_LLM_DONE, 0,
                     reinterpret_cast<LPARAM>(out));
        CloseHandle(readEnd);
        g_llmBusy = 0;
        return 0;
    }
    // Read stdout to EOF.
    std::string stdoutBuf;
    char chunk[4096];
    DWORD got = 0;
    while (ReadFile(readEnd, chunk, sizeof(chunk), &got, nullptr) && got > 0) {
        stdoutBuf.append(chunk, got);
    }
    CloseHandle(readEnd);
    // Turn timeout: kill a hung engine after 10 minutes (bridge convention).
    if (WaitForSingleObject(pi.hProcess, 600000) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    jk::agent::AgentJson reply(stdoutBuf);
    out->ok = reply.ok();
    reply.GetStr("result", out->result);
    reply.GetStr("session_id", out->sessionId);
    PostMessageW(g_hMain, WM_APP_LLM_DONE, 0, reinterpret_cast<LPARAM>(out));
    g_llmBusy = 0;
    return 0;
}

static void StartLlmTurn(const std::string& prompt) {
    if (g_llmBusy.exchange(1) == 1) {
        Log(L"[!] LLM이 이미 실행 중입니다 — /new 로 세션 리셋 가능");
        return;
    }
    Log(L"… LLM 실행 중 (claude 헤드리스)");
    CloseHandle(CreateThread(nullptr, 0, LlmThread,
                             new std::wstring(Utf8ToWide(prompt)), 0, nullptr));
}
```
(`g_hMain` 전역 HWND 추가 필요 — WM_CREATE에서 저장.)

- [ ] **Step 2: UI 배선**

Submit()의 non-slash 분기 교체:
```cpp
if (line[0] != '/') {
    StartLlmTurn(line);   // natural language → claude headless
    return;
}
```
WndProc에:
```cpp
case WM_APP_LLM_DONE: {
    LlmTurnResult* r = reinterpret_cast<LlmTurnResult*>(l);
    if (!r->ok || r->result.empty()) {
        Log(L"[!] LLM 응답 파싱 실패 — 엔진/모델 설정(state\\chat.json) 확인");
    } else {
        Log(Utf8ToWide(r->result));
    }
    if (!r->sessionId.empty()) g_sessionId = r->sessionId;
    delete r;
    return 0;
}
```
/help에 `/new` 추가, Submit에:
```cpp
} else if (cmd == "new") {
    g_sessionId.clear();
    Log(L"새 LLM 세션");
}
```

- [ ] **Step 3: 빌드**

Run: `cmake --build build --target jkchat` → 0 errors.

- [ ] **Step 4: Commit**

```bash
git add engine/tools/jkchat/main.cpp
git commit -m "feat(chat): LLM worker thread — claude headless turn with session resume (chat LLM MVP)"
```

---

### Task 3: probe_agent_chat_llm.ps1 (스텁 엔진 — 네트워크 없이 기계부 검증)

**Files:**
- Create: `engine/tools/probes/probe_agent_chat_llm.ps1`

**Interfaces:**
- Consumes: stub 엔진, UI 조작 = WM_SETTEXT(입력)+BM_CLICK(보내기), 트랜스크립트 판정 = GetWindowTextW(IDC_LOG).

- [ ] **Step 1: 프로브**

```powershell
# Chat LLM MVP probe (stub engine — no network): NL message -> engine turn
# -> result appears in the transcript; /new clears the session.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$chat = "I:\progwork\JKENGINE\engine\build\jkchat.exe"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# stub engine config
$stateDir = "I:\progwork\JKENGINE\engine\build\state"
New-Item -ItemType Directory -Force -Path $stateDir | Out-Null
'{"engine":"stub","skip_permissions":1}' | Set-Content `
    -Path (Join-Path $stateDir "chat.json") -Encoding ASCII

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4
Start-Process -FilePath $chat -WorkingDirectory (Split-Path $exe)
Start-Sleep -Seconds 3

Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class W3 {
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern int GetWindowTextW(IntPtr h, [MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern int GetClassNameW(IntPtr h, [MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder s, int n);
}
"@
# find the chat window + its EDIT controls by class
$p = Get-Process jkchat | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$hMain = $p.MainWindowHandle
$hInput = [IntPtr]::Zero; $hLog = [IntPtr]::Zero
$cb = [W3+EnumProc]{ param($h, $l)
    $cn = New-Object System.Text.StringBuilder 64
    [W3]::GetClassNameW($h, $cn, 64) | Out-Null
    $sb = New-Object System.Text.StringBuilder 512
    [W3]::GetWindowTextW($h, $sb, 512) | Out-Null
    if ($cn.ToString() -eq 'Edit') {
        if ($sb.Length -gt 0) { $script:hLog = $h }   # transcript has text
        else { $script:hInput = $h }                   # input starts empty
    }
    return $true
}
[W3]::EnumChildWindows($hMain, $cb, [IntPtr]::Zero) | Out-Null
if ($hInput -eq [IntPtr]::Zero -or $hLog -eq [IntPtr]::Zero) {
    Write-Host "controls: FAIL"; exit 1
}

# send an NL message
[W3]::SendMessage($hInput, 0x000C, [IntPtr]::Zero, [MarshalAs] ... )  # WM_SETTEXT
```
(WM_SETTEXT의 LPARAM은 유니코드 문자열 마샬링 — 실제 구현은 `[System.Runtime.InteropServices.Marshal]::StringToHGlobalUni` 사용 후 해제, 또는 Add-Type에 `[MarshalAs(UnmanagedType.LPWStr)]` 시그니처 정의.)

```powershell
# click 보내기, wait for the stub engine, read the transcript
$send = ...  # EnumChildWindows에서 Button '보내기' 찾기
[W3]::SendMessage($send, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Seconds 5
$sb = New-Object System.Text.StringBuilder 8192
[W3]::GetWindowTextW($hLog, $sb, 8192) | Out-Null
$transcript = $sb.ToString()
Get-Process jkdesktop,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item (Join-Path $stateDir "chat.json") -ErrorAction SilentlyContinue

$ok = $true
if ($transcript -match 'stub ok') { Write-Host "stub-turn: PASS" }
else { $ok = $false; Write-Host "stub-turn: FAIL" }
if ($transcript -match '… LLM') { Write-Host "busy-line: PASS" }
else { $ok = $false; Write-Host "busy-line: FAIL" }
if ($ok) { Write-Host "PASS: agent chat llm"; exit 0 } else { Write-Host "FAIL: agent chat llm"; exit 1 }
```
(구현 시 컨트롤 탐색은 클래스 Edit 2개 중 텍스트 유무로 구분 + Button 텍스트 매칭 '보내기'.)

- [ ] **Step 2: 실행**

Run: `powershell -File tools/probes/probe_agent_chat_llm.ps1` → PASS.

- [ ] **Step 3: Commit**

```bash
git add engine/tools/probes/probe_agent_chat_llm.ps1
git commit -m "test(chat): stub-engine probe for LLM turn machinery (chat LLM MVP)"
```

---

### Task 4: 실엔진 수동 스모크 + 문서

**Files:**
- Modify: `docs/31_desktop_agent_chat.md` (§2 사용법 + 새 §LLM 연동), 스펙 상태줄

- [ ] **Step 1: 실엔진 수동 시나리오** (사용자 확인 항목 — 계획서에는 체크리스트로 남긴다)

1. `state\chat.json` 없이 기동 → 기본값(ollama/kimi-k2.7-code:cloud) 사용
2. 채팅창에 "열려 있는 창을 목록으로 보여줘" → 결과가 트랜스크립트에 (claude가 list_windows MCP 호출)
3. 후속: "그 목록에서 지뢰찾기는 몇 번 id야?" → `--resume`로 맥락 유지 확인
4. `permissions.json {"close_window":"ask"}` 상태에서 "지뢰찾기 닫아줘" → **채팅창 승인 프롬프트가 claude의 도구 호출에 뜨는지** 확인 → 허용 → LLM이 결과 보고
5. `/new` 후 후속 질문이 맥락 없이 시작되는지

- [ ] **Step 2: docs 갱신**

docs/31에 "LLM 연동" 섹션: 구조도, chat.json 필드, 세션/`/new`, 제한(최종 결과만, 타임아웃 10분, `--dangerously-skip-permissions` 기본 — chat.json으로 비활성, 헤드리스 자동 거부 특성), claude_wrapper 가이드 참조.
스펙 상태줄: "채팅창 LLM 위임(claude CLI 서브프로세스) 구현 완료" 추가.

- [ ] **Step 3: Commit + push**

```bash
git add docs/31_desktop_agent_chat.md docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md
git commit -m "docs(chat): LLM delegation usage + limits (chat LLM MVP)"
git push
```

## 완료 조건

1. 자연어 입력 → claude 헤드리스 턴 → 최종 결과 트랜스크립트 표시 (UI 논블로킹).
2. 세션 연속성: 응답 `session_id`로 `--resume`, `/new` 리셋.
3. 스텁 프로브 통과 (네트워크 없이 기계부 회귀 가능).
4. claude의 도구 호출이 기존 권한 관문 + ask 승인 프롬프트를 그대로 통과.