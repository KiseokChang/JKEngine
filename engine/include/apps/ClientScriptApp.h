#ifndef APPS_CLIENTSCRIPTAPP_H
#define APPS_CLIENTSCRIPTAPP_H

#include <JKApplicationHost.h>
// FileMtime (docs/60: 100ns mtime for the watch poll) lives in JKPlatform —
// implemented in JKPlatform_win32.cpp, the one windows.h-clean core TU. This
// header must not touch windows.h/fileapi.h: wingdi's TextOut macro poisons
// JKDC::TextOut users (main.cpp) and fileapi.h collides with wancode legacy
// typedefs (both measured 2026-09-20); the first 수기 GetFileAttributesExA
// attempt also segfaulted the workshop client (docs/60 §7).
#include <JKPlatform.h>
#include <JKEvent.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <agent/JKAgentJson.h>
#include <client/JKClientApplication.h>
#include <client/JKClientSurface.h>
#include <script/JKScriptHost.h>

#include <cstdio>
#include <map>
#include <vector>

#include <memory>
#include <string>

namespace jk {

// Common script app (docs/27 단계 1): one implementation over either base —
//   ScriptAppT<JKApplication>         single-process mode (its own SDL window)
//   ScriptAppT<JKClientApplication>   window-server client mode (jkapp_script)
// Both bases expose the same surface the script app needs: SetMainWindow/
// GetMainWindow, AddTimer/RemoveTimer, GetLogicalSize, and the OnInit/OnIdle/
// RouteMessage hooks. The script (app.js) is data; the host binds a
// chrome-less panel window that fills the main window's client area (the root
// keeps painting the title text; the window server overlays the chrome in
// client mode).
//
// Hot reload (dev-only, JK_SCRIPT_WATCH=1): polls app.js mtime in OnIdle and
// rebuilds the panel — the script's controls live in the panel and a fresh
// panel is the clean slate (JKWindow cannot remove individual children).
template <typename BaseApp>
class ScriptAppT : public BaseApp {
public:
    ScriptAppT() { host_ = std::make_unique<JKScriptHost>(); }
    ~ScriptAppT() override {
        if (watchTimer_) this->RemoveTimer(watchTimer_);
    }

    // Title for the root window and the app.js path. Set before Init().
    void SetScriptInfo(const std::string& title, const std::string& scriptPath) {
        scriptTitle_ = title;
        scriptPath_ = scriptPath;
    }

    // Workshop hot reload (docs/60 §2.2): MANI `watch=1` forces the mtime
    // poll on regardless of the JK_SCRIPT_WATCH env switch. Built-in SCRI
    // apps never call this — deployment behavior stays env-opt-in.
    void SetHotWatch(bool force) { watchForced_ = force; }

protected:
    void OnInit() override {
        // Hot reload is an opt-in dev switch (docs/27 단계 1) — deployment
        // paths never set JK_SCRIPT_WATCH. The mtime poll rides an internal
        // app timer (both base classes provide AddTimer) at the watch winId,
        // so the reload path is identical in single-process and client mode.
        // Workshop apps force it via MANI watch=1 (docs/60 §2.2).
        const char* env = std::getenv("JK_SCRIPT_WATCH");
        watch_ = watchForced_ || (env && env[0] == '1');

        auto main = std::make_unique<JKWindow>(scriptTitle_);
        int w = 0, h = 0;
        this->GetLogicalSize(w, h);
        if (w <= 0) w = 320;
        if (h <= 0) h = 240;
        main->SetWindowRect(JKRect{ 0, 0, w, h });

        this->SetMainWindow(std::move(main));
        StartScript();
        if (watch_) watchTimer_ = this->AddTimer(kWatchTimerWinId, 500, true);
    }

    // Script-start hook (의미 커서, docs/60 §5): every fresh script evaluation
    // (boot, hot reload, ReloadNow) ends here so a registration that depends
    // on what the new script just did (declareCursor / onAgentAct) re-runs.
    // Fires after EVERY Start attempt — a failed reload leaves the host's
    // declared cursor empty, and the hook must re-register WITHOUT it so a
    // dead script's cursor does not linger on the server manifest.
    virtual void OnScriptStarted() {}

    void RouteMessage(const JKEvent& ev) override {
        if (ev.type == JKEventType::Timer) {
            if (ev.winId == kWatchTimerWinId) {
                OnWatchTick();
                return;
            }
            if (ev.winId >= JKScriptHost::ScriptTimerWinIdBase) {
                host_->DispatchTimerAt(ev.winId - JKScriptHost::ScriptTimerWinIdBase);
                return;
            }
        }
        BaseApp::RouteMessage(ev);
    }

protected:
    // Fresh script panel + host Attach/Start. Used by OnInit and by reload.
    void StartScript() {
        JKWindow* main = this->GetMainWindow();
        if (!main) return;

        // The script builds controls into a chrome-less panel that fills the
        // main window's client area. The root paints the title bar text; the
        // window server overlays close/move/resize chrome (docs/19 §7 — same
        // split the minesweeper client uses for its game window).
        const JKRect client = main->GetClientRect();
        auto panel = std::make_unique<JKWindow>();
        panel->SetAttrFlags(WA_CHROMELESS);
        panel->SetWindowRect(JKRect{ 0, 0, client.w, client.h });
        panel->SetDock(DOCK_FILL);
        panel_ = panel.get();
        main->AddControl(std::move(panel));

        // Script setInterval rides the application timer thread under a winId
        // claimed by JKScriptHost::ScriptTimerWinIdBase; RouteMessage steers
        // the resulting Timer events back to DispatchTimerAt.
        JKScriptTimerServices services;
        services.start = [this](uint32_t winId, uint32_t intervalMs) -> uint64_t {
            return this->AddTimer(winId, intervalMs, true);
        };
        services.stop = [this](uint64_t handle) { this->RemoveTimer(handle); };
        host_->SetTimerServices(std::move(services));

        host_->Attach(panel_);
        // 상태 복원 적재 (docs/67 단 1): 같은 경로의 직전 리로드가 보관한
        // 스냅샷이 있으면 JS 훅 원문을 pending으로 적재 — Start 후반
        // (onCreate 직후)의 onRestoreState가 먼저, 위젯 복원이 그 다음
        // (마지막에 이긴다). 실패 리로드는 스냅샷을 보존한다 — 소비(erase)는
        // 성공 Start 뒤에만.
        std::string widgetJson;
        if (auto it = stateByPath_.find(scriptPath_); it != stateByPath_.end()) {
            widgetJson = it->second.widgetJson;
            if (!it->second.jsState.empty())
                host_->SetPendingRestoreState(it->second.jsState);
        }
        if (!host_->Start(scriptPath_)) {
            std::printf("[script] start failed: %s\n",
                        host_->LastError().c_str());
            std::fflush(stdout);
            // docs/60 §2.3: a failed reload must not leave a silent empty
            // panel — one static label with the error text keeps the failure
            // visible until the next successful reload rebuilds the panel.
            const JKRect cr = main->GetClientRect();
            auto* label = new JKStatic(
                JKRect{ 8, 8, cr.w > 16 ? cr.w - 16 : cr.w, 24 });
            label->SetText("[script error] " + host_->LastError());
            panel_->AddControl(std::unique_ptr<JKStatic>(label));
        } else {
            // 복원 2단: 위젯(JKEdit 텍스트+입력모드) — onCreate+onRestoreState
            // 후에 와서 마지막에 이긴다(위젯 복원 우선, 라벨은 복원 금지).
            if (!widgetJson.empty()) host_->RestoreWidgetState(widgetJson);
            stateByPath_.erase(scriptPath_);  // 성공만 소비
        }
        lastMtime_ = FileMtime(scriptPath_);
        OnScriptStarted();
    }

    static long long FileMtime(const std::string& path) {
        // 100ns FILETIME, not _stat64::st_mtime: the stat mtime is 1-second
        // resolution, so two saves within the same second (rapid notepad
        // edits / probe back-to-back writes) alias to the same stamp and the
        // watcher never fires (probe_workshop c5, first live run).
        return JKPlatform::FileMtime100ns(path);
    }

    // Hot reload tick (dev-only, JK_SCRIPT_WATCH=1). Two-phase: the closed
    // panel is destroyed by RemoveClosedChildren after the event drain, so the
    // rebuild waits for the next tick to start from a clean child list.
    void OnWatchTick() {
        if (reloadPending_) {
            reloadPending_ = false;
            // 이중 기동 가드 (docs/67 단 1 — 잠복 결함 봉합): 도구 경로
            // (ReloadNow)가 틱 사이에 이미 패널을 새로 지었으면 pending은
            // 유령이 된다 — 무조건 StartScript하던 2단이 만들던 패널 중복.
            if (panel_) return;
            StartScript();
            return;
        }
        long long mtime = FileMtime(scriptPath_);
        if (mtime == 0 || mtime == lastMtime_) return;
        lastMtime_ = mtime;
        std::printf("[script] app.js changed - hot reload\n");
        std::fflush(stdout);
        RequestReload();
    }

    // --- 통합 리로드 경로 (docs/67 단 1) -------------------------------------
    // 감시 핫 리로드·도구 동기 리로드·슬롯 전환이 같은 문(TeardownLiveScript)을
    // 지난다 — 상태 캡처의 누락 경로가 없다. 캡처는 Stop 전(onSaveState →
    // onExit 순서 계약), 복원은 Start 후 onCreate 직후(§ StartScript).
    void TeardownLiveScript() {
        ScriptSavedState saved;
        host_->CaptureWidgetState(saved.widgetJson);
        host_->DispatchSaveState(saved.jsState);
        if (!saved.widgetJson.empty() || !saved.jsState.empty())
            stateByPath_[scriptPath_] = std::move(saved);
        host_->Stop();
        if (g_jkAppHost) {
            g_jkAppHost->SetModalWindow(nullptr);
            g_jkAppHost->ReleaseCapture();
        }
        if (panel_) {
            panel_->RequestClose();
            panel_ = nullptr;
        }
    }

    // 감시 핫 리로드 1단: 패널만 닫고 다음 틱에서 재기동(이벤트 드레인 후
    // RemoveClosedChildren가 파산 청산 — 기존 2단 사다리 유지).
    void RequestReload() {
        TeardownLiveScript();
        reloadPending_ = true;
    }

    // 동기 리로드/슬롯 전환 (docs/60 §2.3 SyncReload의 후신): 즉시 재기동,
    // returning whether the fresh script booted (LastError() has the error
    // otherwise). Only valid on the frame thread — agent tool calls run
    // inside JKClientApplication::Run's sweep (app-tool-hub §8.2), the same
    // main/UI thread as every event handler, so QuickJS's main-thread-only
    // rule (docs/27 §3.2) holds. The sweep calls RemoveClosedChildren a few
    // lines after the tool poll; doing it inline here first is idempotent.
    bool ReloadNow() {
        TeardownLiveScript();
        reloadPending_ = false;
        if (JKWindow* main = this->GetMainWindow()) {
            main->RemoveClosedChildren();
        }
        StartScript();
        return host_->IsRunning();
    }

    std::string scriptTitle_;
    std::string scriptPath_;
    std::unique_ptr<JKScriptHost> host_;
    JKWindow* panel_ = nullptr;  // owned by the main window's child list

    // 상태 보존 리로드 (docs/67 단 1 — 사훈 1의 폴백 경로): 리로드 간 수송
    // 수단일 뿐 진실원이 아니다(파일화=리로드마다 쓰기+크래시 부패 부활+
    // state/scripts 잡음 — in-memory가 맞다). key = scriptPath_. 슬롯 전환은
    // 이 map이 무료로 상태를 보존한다(스위치 아웃→구 경로 키, 복귀→복원).
    struct ScriptSavedState {
        std::string widgetJson;  // {"v":1,"edits":[...]} (JKScriptHost 캡처)
        std::string jsState;     // onSaveState() 원문
    };
    std::map<std::string, ScriptSavedState> stateByPath_;

    // Hot reload state (dev-only). The mtime poll rides an internal app timer
    // at kWatchTimerWinId so the reload path is identical on both bases;
    // ScriptTimerWinIdBase (0x7F00) must stay above it.
    static constexpr uint32_t kWatchTimerWinId = 0x7E00;
    bool watch_ = false;
    bool watchForced_ = false;    // MANI watch=1 (docs/60 §2.2)
    bool reloadPending_ = false;  // panel closed; start on the next watch tick
    long long lastMtime_ = 0;
    uint64_t watchTimer_ = 0;
};

// Client-mode alias kept for the jkapp_script module (server mode).
using ClientScriptApp = ScriptAppT<JKClientApplication>;

// Workshop script app (docs/60 §2.3) — a client-mode ScriptAppT that exposes
// its script file as agent tools (app tool hub, docs/58): get_script returns
// the source, set_script writes it and reloads synchronously, so a script
// error returns to the agent immediately — the talk-to-fix closed loop. The
// MANI `scriptfile=`/`watch=1` interpretation happens in JKAppModule_script;
// this class only registers the tools and serves them.
class WorkshopScriptApp : public ScriptAppT<JKClientApplication> {
public:
    WorkshopScriptApp() {
        // 의미 커서 (docs/60 §5 백로그 소각): declareCursor가 봉합될 때마다
        // (최초 평가 포함 — 스크립트는 onCreate/전역 코드에서 선언한다) 등록을
        // 재송신한다. SendToolRegister의 내용 동일 dedupe가 재선언 홍수를 막는다.
        host_->SetCursorDeclChanged(
            [this](const std::string&) { SendToolRegister(); });
    }

    // App token for AgentToolRegister — must match the MANI name so the
    // broker's composed MCP names (<app>_<tool>) line up. Set before Init().
    void SetAgentAppName(const std::string& name) { agentAppName_ = name; }

protected:
    void OnInit() override {
        ScriptAppT<JKClientApplication>::OnInit();
        // The register itself rides OnScriptStarted (fired from StartScript
        // right after the script evaluated — a declareCursor in global code /
        // onCreate is already sealed by then, so the first register carries
        // the cursor). Timing (docs/58 §5.1): OnInit runs after the surface
        // Connect, so a register here never hits the "before-connect silent
        // false" path.
    }

    // Tool register (앱 도구 허브 docs/58 §5.1 + 의미 커서 docs/60 §5). Called
    // from OnScriptStarted (every script start — reloads included) and from
    // the declareCursor callback (runtime re-declaration). Dedupes by declared
    // cursor content: the callback fires during eval and the hook fires again
    // right after, so the same declaration must not hit the server twice
    // (재선언 홍수 방지 — 서버 upsert는 커서를 (0,0)으로 리셋한다).
    void SendToolRegister() {
        if (agentAppName_.empty() || scriptPath_.empty()) return;
        jk::client::JKClientSurface* surface = this->Surface();
        if (!surface) return;
        const std::string decl = host_->DeclaredCursorJson();
        if (!decl.empty() && decl == lastSentDeclJson_) return;
        using Decl = jk::client::JKClientSurface::AgentToolDecl;
        std::vector<Decl> tools = {
            {"get_script", "Read the workshop script source", "{}"},
            {"set_script",
             "Replace the workshop script and reload it synchronously — "
             "a script error comes back in the same response",
             "{\"type\":\"object\",\"properties\":{\"source\":{\"type\":"
             "\"string\"}},\"required\":[\"source\"]}"},
            {"api",
             "List the workshop script API: function signatures and "
             "constraints (charset, timers, events). Call this before "
             "writing a script",
             "{}"},
        };
        if (!decl.empty() && host_->HasGlobalFn("onAgentAct")) {
            // 앱 자체 act 도구 — 서버가 선언 kinds를 이 스키마의 enum에 병합하고
            // act 중계가 스크립트의 전역 onAgentAct(kind,row,col)로 간다
            // (스펙 2026-09-22-semantic-cursor §2의 앱 계약).
            tools.push_back(
                {"act",
                 "Semantic act on the declared cursor cell (kind enum from the "
                 "script's declareCursor). Runs the script's onAgentAct.",
                 "{\"type\":\"object\",\"properties\":{\"kind\":{\"type\":"
                 "\"string\"},\"row\":{\"type\":\"integer\"},\"col\":"
                 "{\"type\":\"integer\"}},\"required\":[\"kind\",\"row\","
                 "\"col\"]}"});
        }
        if (!decl.empty() && host_->HasGlobalFn("onSnapshot")) {
            // 앱 자체 snapshot 도구 (docs/60 §13 후속) — 커서 read 중계의
            // 조립 원문(스펙 §3). NIT-7 즉답 대신 스크립트 직렬화로 응답.
            tools.push_back(
                {"snapshot",
                 "Board snapshot serialization for the semantic cursor read "
                 "(the script's onSnapshot). Read composes it with the cursor "
                 "header.",
                 "{\"type\":\"object\",\"properties\":{}}"});
        }
        // cursorJson: 봉합 원문(빈 문자열 = 미선언 — 재등록 시 커서 해제).
        surface->SendAgentToolRegister(agentAppName_, tools, false, decl);
        lastSentDeclJson_ = decl;
    }

    // 의미 커서 훅 (docs/60 §5): every script start re-registers with whatever
    // the fresh script declared (a failed start declares nothing — the hook
    // then registers WITHOUT the cursor, clearing a dead script's declaration).
    void OnScriptStarted() override { SendToolRegister(); }

    bool OnAgentToolCall(const std::string& tool, const std::string& argsJson,
                         std::string& out) override {
        if (tool == "api") {
            out = kApiCatalog;
            return true;
        }
        if (tool == "get_script") {
            std::string source;
            if (!ReadTextFile(scriptPath_, source)) {
                out = "{\"error\":\"read_failed\"}";
                return false;
            }
            out = "{\"ok\":true,\"source\":\"" + JsonEsc(source) + "\"}";
            return true;
        }
        if (tool == "set_script") {
            jk::agent::AgentJson args(argsJson);
            std::string source;
            if (!args.ok() || !args.GetStr("source", source)) {
                out = "{\"error\":\"bad_args\",\"need\":\"source:string\"}";
                return false;
            }
            if (source.size() > kMaxScriptBytes) {
                out = "{\"error\":\"too_large\",\"cap\":" +
                      std::to_string(kMaxScriptBytes) + "}";
                return false;
            }
            if (!WriteTextFile(scriptPath_, source)) {
                out = "{\"error\":\"write_failed\"}";
                return false;
            }
            if (ReloadNow()) {
                out = "{\"ok\":true}";
                return true;
            }
            // The closed loop (docs/60 §2.3): the failing script's error text
            // travels back inside the tool response so the agent fixes itself.
            // The api hint turns "createListBox is not defined" style failures
            // into a one-turn fix (docs/60 §8 — 폰 세션 실측: 추측 3턴 소모).
            out = "{\"ok\":false,\"error\":\"" + JsonEsc(host_->LastError()) +
                  "\",\"hint\":\"call the api tool for the function list\"}";
            return false;
        }
        if (tool == "act") {
            // 의미 커서 act 중계 (스펙 2026-09-22-semantic-cursor §2, docs/60
            // §5): 서버가 kind/row/col을 사전 검증한 뒤 원문 패스스루한다.
            // 결과는 DispatchAgentAct 계약 — onAgentAct 반환(문자열=원문,
            // 객체=JSON.stringify, 없음={"ok":true}).
            jk::agent::AgentJson args(argsJson);
            std::string kind;
            int row = 0, col = 0;
            if (!args.ok() || !args.GetStr("kind", kind) ||
                !args.GetInt("row", row) || !args.GetInt("col", col)) {
                out = "{\"error\":\"bad_args\",\"need\":\"kind,row,col\"}";
                return false;
            }
            bool actOk = false;
            if (!host_->DispatchAgentAct(kind, row, col, actOk, out)) {
                // 호스트 정지(리로드 경합 등) — 도구 실패로 회신.
                out = "{\"error\":\"host_stopped\"}";
                return false;
            }
            return actOk;
        }
        if (tool == "snapshot") {
            // 커서 read의 snapshot 중계 (docs/60 §13 후속): 결과 원문이
            // ComposeCursorRead의 "snapshot" 필드에 그대로 실린다(ok=true).
            bool snapOk = false;
            if (!host_->DispatchAgentSnapshot(snapOk, out)) {
                out = "{\"error\":\"host_stopped\"}";
                return false;
            }
            return snapOk;
        }
        // Unreachable through the server (reverse matching answers
        // unknown_app_tool first) — defensive, same as vplayer.
        out = "{\"error\":\"unknown_tool\",\"tool\":\"" + tool + "\"}";
        return false;
    }

private:
    static constexpr size_t kMaxScriptBytes = 256 * 1024;  // docs/60 §2.3

    // Workshop API digest (docs/60 §8): the phone LLM guessed at bindings
    // (createListBox 헛다리 — 2026-09-20 폰 세션 실측) because the contract
    // lived only in scripts/jk.d.ts, which the agent never sees. This digest
    // rides the `api` tool. Same maintenance rule as jk.d.ts: binding changes
    // MUST update both (additive-only policy makes drift rare). Full contract
    // remains scripts/jk.d.ts — this is the LLM-facing digest.
    static constexpr const char* kApiCatalog =
        "{"
        "\"contract\":\"engine/scripts/jk.d.ts (full reference; additive only)\","
        "\"charset\":\"위젯 텍스트는 ASCII+한글+기호(■□●◆ 등) 안전 — 이모지는 ??로 렌더됨(CP949 인코딩 불가, UTF-16 서러게이트당 ? 1개)\","
        "\"events\":\"전역 함수 onClick(id)를 정의하면 모든 클릭이 id와 함께 전달된다; 캔버스용 onMouse(type,x,y,canvasId,button)/onWheel(dy,x,y)/onKey(key,down)도 전역 함수로 정의하면 캔버스 입력이 전달된다 — 정의 없으면 무시. onMouse의 type은 down/up/move이고 button은 SDL 버튼 번호(1=왼쪽, 2=중간, 3=오른쪽, move는 0) — 좌/우 구분은 button으로 한다(2026-09-24 v5.1). onAgentAct(kind,row,col)를 정의하면 의미 커서 act 호출이 전달된다(declareCursor 필수; 문자열/객체 반환은 act 도구 결과 JSON). onSnapshot()를 정의하면 read 도구의 snapshot 직렬화를 제공한다(객체 반환=JSON.stringify)\","
        "\"layout\":\"좌표는 패널 클라이언트 픽셀; 창이 리사이즈되어도 위젯은 재배치되지 않는다\","
        "\"functions\":["
        "{\"sig\":\"log(text)\",\"desc\":\"콘솔 로그\"},"
        "{\"sig\":\"messageBox(title, text)\",\"desc\":\"모달 메시지 박스(비동기, JS 비차단)\"},"
        "{\"sig\":\"createButton(rect, text)\",\"desc\":\"버튼, id 반환\"},"
        "{\"sig\":\"createLabel(rect, text)\",\"desc\":\"정적 라벨\"},"
        "{\"sig\":\"createEdit(rect, text)\",\"desc\":\"한 줄 입력창\"},"
        "{\"sig\":\"setText(id, text)\",\"desc\":\"위젯 텍스트 변경\"},"
        "{\"sig\":\"getText(id)\",\"desc\":\"위젯 텍스트 읽기\"},"
        "{\"sig\":\"setInterval(fn, ms)\",\"desc\":\"반복 타이머(자동 낙하/시계 등) — id 반환\"},"
        "{\"sig\":\"clearInterval(id)\",\"desc\":\"타이머 해제\"},"
        "{\"sig\":\"findControl(title)\",\"desc\":\"제목으로 위젯 탐색\"},"
        "{\"sig\":\"click(id)\",\"desc\":\"프로그래매틱 클릭\"},"
        "{\"sig\":\"injectMouse(x, y)\",\"desc\":\"마우스 이벤트 주입(테스트)\"},"
        "{\"sig\":\"injectKey(key)\",\"desc\":\"키 이벤트 주입(테스트)\"},"
        "{\"sig\":\"assert(cond, msg)\",\"desc\":\"셀프테스트 단정\"},"
        "{\"sig\":\"assertEq(a, b, msg)\",\"desc\":\"셀프테스트 동일 단정\"},"
        "{\"sig\":\"createDialog(rect, title, fn)\",\"desc\":\"모달 다이얼로그 생성\"},"
        "{\"sig\":\"dialogAddLabel(dialog, rect, text)\",\"desc\":\"다이얼로그 라벨\"},"
        "{\"sig\":\"dialogAddEdit(dialog, rect, text)\",\"desc\":\"다이얼로그 입력창\"},"
        "{\"sig\":\"dialogAddButton(dialog, rect, text)\",\"desc\":\"다이얼로그 버튼\"},"
        "{\"sig\":\"dialogShow(dialog)\",\"desc\":\"다이얼로그 표시\"},"
        "{\"sig\":\"dialogClose(dialog, result)\",\"desc\":\"다이얼로그 닫기\"},"
        "{\"sig\":\"readConfig(key)\",\"desc\":\"스크립트 옆 설정 파일 읽기\"},"
        "{\"sig\":\"createCanvas(rect)\",\"desc\":\"그리기 캔버스 (포커스 가능), id 반환 — 게임·토이용\"},"
        "{\"sig\":\"canvasClear(id, color?)\",\"desc\":\"캔버스 전체 지우기+바탕색 — 애니메이션은 프레임마다 호출 (옵 상한 4096)\"},"
        "{\"sig\":\"canvasRect(id, x,y,w,h, color, filled?)\",\"desc\":\"사각형 — filled 생략 시 외곽선\"},"
        "{\"sig\":\"canvasPixel(id, x,y, color)\",\"desc\":\"픽셀 1개\"},"
        "{\"sig\":\"canvasLine(id, x1,y1,x2,y2, color)\",\"desc\":\"선\"},"
        "{\"sig\":\"canvasCircle(id, x,y,r, color, filled?)\",\"desc\":\"원 — filled는 스캔라인 근사\"},"
        "{\"sig\":\"canvasText(id, x,y, text, color)\",\"desc\":\"텍스트 (한글 안전)\"},"
        "{\"sig\":\"declareCursor(decl)\",\"desc\":\"의미 커서 선언 — {origin:{x,y}, cellW, cellH, rows, cols, kinds:[..]} (origin은 패널 픽셀 좌상단). 서버가 move/read 플랫폼 도구를 합성하고 act(kind,row,col)를 onAgentAct로 중계; 재호출=재선언(커서 리셋)\"}"
        "],"
        "\"note\":\"createListBox/createCheckbox 같은 목록·체크 위젯은 아직 없다 — 목록은 라벨+버튼 조합으로 구성; 색은 0xRRGGBB 숫자 또는 '#rrggbb' 문자열; 캔버스 좌표는 캔버스 로컬 픽셀\""
        "}";

    static std::string JsonEsc(const std::string& s) {
        std::string r;
        r.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"': r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04X", c);
                        r += buf;
                    } else {
                        r += c;
                    }
            }
        }
        return r;
    }

    static bool ReadTextFile(const std::string& path, std::string& out) {
        std::FILE* f = nullptr;
        fopen_s(&f, path.c_str(), "rb");
        if (!f) return false;
        char buf[4096];
        size_t n = 0;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
        std::fclose(f);
        return true;
    }

    static bool WriteTextFile(const std::string& path, const std::string& data) {
        std::FILE* f = nullptr;
        fopen_s(&f, path.c_str(), "wb");
        if (!f) return false;
        const size_t w = std::fwrite(data.data(), 1, data.size(), f);
        std::fclose(f);
        return w == data.size();
    }

    std::string agentAppName_;
    std::string lastSentDeclJson_;  // SendToolRegister dedupe (재선언 홍수 방지)
};

} // namespace jk

#endif // APPS_CLIENTSCRIPTAPP_H