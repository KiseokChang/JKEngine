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
#include <JKComboBox.h>
#include <JKEvent.h>
#include <JKHangulUtil.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <agent/JKAgentJson.h>
#include <client/JKClientApplication.h>
#include <client/JKClientSurface.h>
#include <script/JKScriptHost.h>
#include <script/JKWorkshopStore.h>

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
        // pending은 "빈 원문도 항상 적재" — 이전 리로드의 잔존 pending이
        // 새 Start에 유입되지 않게(불변: pending 1건 ↔ Start 1회).
        if (auto it = stateByPath_.find(scriptPath_); it != stateByPath_.end())
            widgetJson = it->second.widgetJson;
        host_->SetPendingRestoreState(
            stateByPath_.count(scriptPath_)
                ? stateByPath_[scriptPath_].jsState
                : std::string());
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
        // 마지막 슬롯 영속 (docs/67 단 1): .current_<appName>이 유효한 슬롯을
        // 가리키면 그 파일로 부팅(파일=진실원), 무효·부재·파일 부재면 MANI
        // 스템(myapp) 유지. ScriptAppT::OnInit가 StartScript하므로 경로 교체는
        // 그 전에.
        std::string slot;
        const std::string dir = jk::workshop::DirOf(scriptPath_);
        if (jk::workshop::ReadCurrentSlotFile(dir, agentAppName_, slot) &&
            jk::workshop::IsValidSlotName(slot)) {
            std::string probe;  // 존재 확인 겸용 — 읽은 내용은 버린다
            if (ReadTextFile(dir + "\\" + slot + ".js", probe))
                scriptPath_ = dir + "\\" + slot + ".js";
        }
        ScriptAppT<JKClientApplication>::OnInit();
        // The register itself rides OnScriptStarted (fired from StartScript
        // right after the script evaluated — a declareCursor in global code /
        // onCreate is already sealed by then, so the first register carries
        // the cursor). Timing (docs/58 §5.1): OnInit runs after the surface
        // Connect, so a register here never hits the "before-connect silent
        // false" path.

        // 슬롯 스트립 (docs/67 단 2 — 캡션 임베딩): 스트립은 타이틀 바 안으로
        // 올라갔다. 패널 DOCK_FILL 마진은 0 — 클라이언트 영역 28px을 회수한다.
        // 컨테이너 컨트롤 금지는 유지: 팝업은 여전히 메인 창 직접 자식이어야
        // PushClipRect 클립을 피한다.
        JKWindow* main = this->GetMainWindow();
        if (!main) return;
        if (panel_) panel_->SetMargins(0, 0, 0, 0);
        main->PerformLayout(main->GetClientRect());
        // 콤보만 패스스루 선언 — 라벨·타이틀 텍스트 위 더블클릭은 최대화 토글을
        // 유지한다(네이티브 관습). surface 좌표 변환은 JKWindow 몫(상수 미러).
        main->SetFrameStripRect(kStripComboRect);
        if (jk::client::JKClientSurface* surface = this->Surface()) {
            surface->SendTitlePassthrough(main->GetFrameStripSurfaceRect());
        }
        BuildStrip();
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
            {"get_script",
             "Read the workshop script source (slot optional — default is "
             "the current slot)",
             "{\"type\":\"object\",\"properties\":{\"slot\":{\"type\":"
             "\"string\"}}}"},
            {"set_script",
             "Replace the workshop script and reload it synchronously — "
             "a script error comes back in the same response. slot optional: "
             "writes another slot and auto-switches; the pre-write source is "
             "snapshotted to the version ribbon (.history/<slot>/NNNN.js). "
             "live:1 = patch the running context instead of a full reload "
             "(widget/timer state survives) — same slot only; use it for "
             "small edits of callback bodies",
             "{\"type\":\"object\",\"properties\":{\"source\":{\"type\":"
             "\"string\"},\"slot\":{\"type\":\"string\"},\"live\":{\"type\":"
             "\"number\"}},\"required\":[\"source\"]}"},
            {"api",
             "List the workshop script API: function signatures and "
             "constraints (charset, timers, events). Call this before "
             "writing a script",
             "{}"},
            {"list_slots",
             "List workshop slots (script files). Each slot is its own "
             "workspace; widget/JS state is preserved per slot",
             "{\"type\":\"object\",\"properties\":{}}"},
            {"use_slot",
             "Switch to another slot and reload it (state preserved)",
             "{\"type\":\"object\",\"properties\":{\"slot\":{\"type\":"
             "\"string\"}},\"required\":[\"slot\"]}"},
            {"script_history",
             "Version ribbon: list saved generations (NNNN.js snapshots) of a "
             "slot, oldest first",
             "{\"type\":\"object\",\"properties\":{\"slot\":{\"type\":"
             "\"string\"}}}"},
            {"restore_script",
             "Restore a slot to a saved generation. The pre-restore content "
             "becomes a new generation, so a restore is itself undoable",
             "{\"type\":\"object\",\"properties\":{\"slot\":{\"type\":"
             "\"string\"},\"gen\":{\"type\":\"integer\"}},\"required\":"
             "[\"gen\"]}"},
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
    // 슬롯 스트립 갱신도 여기 — 모든 Start(부트·리로드·슬롯 전환)가 지나는
    // 문이므로 도구·전환 어느 경로로 슬롯이 바뀌어도 콤보가 따라온다.
    void OnScriptStarted() override {
        SendToolRegister();
        RefreshStrip();
        // 라이브 결함 봉합 (2026-09-26 사용자 보고 — "슬롯 선택이 먹통"):
        // StartScript의 새 패널(DOCK_FILL)이 AddControl로 맨 뒤(=z-order 최상,
        // HitTest 역순)에 추가되어 리로드마다 스트립을 페인트·클릭 양쪽에서
        // 가렸다. 모든 Start가 지나는 이 문에서 스트립을 다시 맨 위로.
        if (JKWindow* main = this->GetMainWindow()) {
            if (slotLabel_) main->MoveChildToTop(slotLabel_);
            if (slotCombo_) main->MoveChildToTop(slotCombo_);
        }
    }

    // --- 슬롯 스트립 (docs/67 단 1 — 네이티브 헤더 스트립) -------------------

    void BuildStrip() {
        JKWindow* main = this->GetMainWindow();
        if (!main) return;
        // 캡션 임베딩 (docs/67 단 2): 부모-클라이언트 y는 음수 — 화면상
        // surface y 3..25(콤보)·6..24(라벨)로 타이틀 바에 얹힌다. rect는
        // ANCHOR_NONE 기본이라 리사이즈 재배치에도 그대로다(JKControl.cpp).
        auto label = std::make_unique<JKStatic>(JKRect{ 8, -18, 38, 18 }, 0);
        label->SetText(jk::Utf8ToKssm("슬롯:"));
        slotLabel_ = label.get();
        main->AddControl(std::move(label));
        auto combo = std::make_unique<JKComboBox>(kStripComboRect, 0);
        slotCombo_ = combo.get();
        slotCombo_->SetOnSelectionChanged(
            [this](int32_t idx) { OnStripSelect(idx); });
        RefreshStrip();
        main->AddControl(std::move(combo));
    }

    // 스트립 갱신: ListSlots 스템 목록 + 현재 선택. 직접 SetSelectedIndex는
    // 콜백을 발화하지 않으므로(§ JKComboBox 주석) 전환 루프가 없다.
    void RefreshStrip() {
        if (!slotCombo_) return;
        const std::string current = CurrentSlotName();
        std::vector<std::string> slots;
        jk::workshop::ListSlots(jk::workshop::DirOf(scriptPath_), slots);
        slotCombo_->Clear();
        int32_t sel = -1;
        for (size_t i = 0; i < slots.size(); ++i) {
            slotCombo_->AddString(jk::Utf8ToKssm(slots[i].c_str()));
            if (slots[i] == current) sel = static_cast<int32_t>(i);
        }
        slotCombo_->SetSelectedIndex(sel);
    }

    // 콤보 선택 = 슬롯 전환. 상태는 stateByPath_가 보존한다(SwitchToSlot).
    // 콤보 목록은 스토어 목록이 진실원 — 파일이 사라진 선택은 조용히 무시.
    void OnStripSelect(int32_t idx) {
        if (!slotCombo_ || idx < 0) return;
        const std::string slot =
            jk::KssmToUtf8(slotCombo_->GetSelectedString().c_str());
        if (slot.empty() || slot == CurrentSlotName()) return;
        std::string probe;
        if (!ReadTextFile(jk::workshop::DirOf(scriptPath_) + "\\" + slot +
                              ".js",
                          probe)) {
            RefreshStrip();
            return;
        }
        SwitchToSlot(slot);  // ReloadNow + RefreshStrip 포함
    }

    JKComboBox* slotCombo_ = nullptr;
    JKStatic* slotLabel_ = nullptr;
    // 캡션 임베딩 (docs/67 단 2): 부모-클라이언트 y는 음수 — 화면상 surface
    // y 3..25(콤보)·6..24(라벨)로 타이틀 바에 얹힌다. 단일 모드/클라 모드 공유
    // 진실원 — 좌표 조정은 이 1곳만.
    static constexpr JKRect kStripComboRect{ 50, -21, 160, 22 };

    bool OnAgentToolCall(const std::string& tool, const std::string& argsJson,
                         std::string& out) override {
        if (tool == "api") {
            out = kApiCatalog;
            return true;
        }
        if (tool == "get_script") {
            jk::agent::AgentJson args(argsJson);
            std::string slotArg;
            if (args.ok()) (void)args.GetStr("slot", slotArg);
            std::string slot, path;
            if (!ResolveSlot(slotArg, slot, path)) {
                out = R"({"error":"bad_slot","rule":"[A-Za-z0-9_-]{1,32}"})";
                return false;
            }
            std::string source;
            if (!ReadTextFile(path, source)) {
                out = "{\"error\":\"read_failed\",\"slot\":\"" +
                      JsonEsc(slot) + "\"}";
                return false;
            }
            out = "{\"ok\":true,\"slot\":\"" + JsonEsc(slot) +
                  "\",\"source\":\"" + JsonEsc(source) + "\"}";
            return true;
        }
        if (tool == "set_script") {
            jk::agent::AgentJson args(argsJson);
            std::string source, slotArg;
            if (!args.ok() || !args.GetStr("source", source)) {
                out = "{\"error\":\"bad_args\",\"need\":\"source:string\"}";
                return false;
            }
            (void)args.GetStr("slot", slotArg);  // 옵션 — 있으면 자동 전환
            int live = 0;
            (void)args.GetInt("live", live);
            if (source.size() > kMaxScriptBytes) {
                out = "{\"error\":\"too_large\",\"cap\":" +
                      std::to_string(kMaxScriptBytes) + "}";
                return false;
            }
            std::string slot, path;
            if (!ResolveSlot(slotArg, slot, path)) {
                out = R"({"error":"bad_slot","rule":"[A-Za-z0-9_-]{1,32}"})";
                return false;
            }
            // 라이브 패치 (스펙 §4): 같은 슬롯+호스트 러닝 조건에서만. 그 외는
            // live 요청 무시하고 기존 경로(응답에 live:false 표기). 게이트 실패
            // = 스냅샷·기록 모두 스킵(파일·컨텍스트 무손상 — 원칙 1).
            const bool liveCapable =
                live != 0 && scriptPath_ == path && host_->IsRunning();
            if (liveCapable) {
                std::string gateErr;
                if (!host_->CompileGate(source, &gateErr)) {
                    out = "{\"ok\":false,\"live\":true,\"slot\":\"" +
                          JsonEsc(slot) + "\",\"error\":\"" +
                          JsonEsc(gateErr) +
                          "\",\"hint\":\"fix the syntax and retry live:1, or "
                          "drop live for a full reload\"}";
                    return false;
                }
            }
            // 버전 리본 (docs/67 단 1): 도구 매개 덮어쓰기 직전 원문 스냅샷.
            // 대상 부재(신규 슬롯 첫 쓰기)는 스냅샷 없음(gen 0) — 스냅샷 실패는
            // 진실원 불접촉(원칙 1: 부분 실패가 진실원을 오염시키지 않는다).
            std::string prev;
            int gen = 0;
            if (ReadTextFile(path, prev)) {
                gen = jk::workshop::AppendSnapshot(
                    jk::workshop::DirOf(scriptPath_), slot, prev);
                if (gen == 0) {
                    out = "{\"error\":\"snapshot_failed\"}";
                    return false;
                }
            }
            if (!WriteTextFile(path, source)) {
                out = "{\"error\":\"write_failed\"}";
                return false;
            }
            // 같은 슬롯: live 요청이면 라이브 재평가, 아니면 동기 리로드(폐곡선).
            // 라이브 성공엔 리본 gen을 그대로 실는다(스냅샷·기록은 게이트 통과
            // 후에도 했다 — 이후 낙하해도 파일=진실원 회복).
            bool reloadOk = true;
            std::string liveErr;
            if (liveCapable) {
                std::string patchErr;
                if (host_->PatchEval(source, &patchErr)) {
                    // 감시 억제 (e2e 실측 — probe_workshop_livepatch c1):
                    // 도구의 자기 쓰기를 lastMtime_에 반영하지 않으면 500ms
                    // 감시 틱이 그 쓰기를 수기 편집으로 오판해 풀 리로드
                    // 낙하(타이머·클로저 증발 — 라이브 패치의 생존 계약을
                    // 무력화). 풀 경로는 StartScript가 갱신하므로 조용했던
                    // 것 — 라이브 경로도 같은 불변식을 따른다.
                    lastMtime_ = FileMtime(scriptPath_);
                    std::printf("[script] live patch: gen %d\n", gen);
                    std::fflush(stdout);
                    out = "{\"ok\":true,\"live\":true,\"slot\":\"" +
                          JsonEsc(slot) + "\",\"gen\":" + std::to_string(gen) +
                          "}";
                    return true;
                }
                // 런타임 예외 → 자동 풀 리로드 낙하(스펙 §4 흐름 4). 파일엔
                // 이미 새 원문이 기록됐다(파일=진실원 회복 경로). 폐곡선:
                // 에러를 응답에 실어 돌려보낸다.
                liveErr = patchErr;
                reloadOk = ReloadNow();
            } else {
                reloadOk = (scriptPath_ == path) ? ReloadNow()
                                                 : SwitchToSlot(slot);
            }
            if (!reloadOk) {
                // 기존 경로와 동일한 에러 응답(live 필드만 추가 — additive).
                out = "{\"ok\":false,\"live\":" +
                      std::string(liveCapable ? "true" : "false") +
                      ",\"slot\":\"" + JsonEsc(slot) +
                      "\",\"gen\":" + std::to_string(gen) + ",\"error\":\"" +
                      JsonEsc(host_->LastError()) +
                      "\",\"hint\":\"call the api tool for the function list\"}";
                return false;
            }
            // 라이브 재평가 실패 후 낙하 성공 — 앱은 살아 있고(위젯 스냅샷+
            // onSaveState 수송) 에러는 폐곡선으로 노출된다(스펙 §4 흐름 4).
            if (liveCapable) {
                out = "{\"ok\":true,\"live\":false,\"slot\":\"" +
                      JsonEsc(slot) + "\",\"gen\":" + std::to_string(gen) +
                      ",\"error\":\"" + JsonEsc(liveErr) +
                      "\",\"note\":\"recovered by full reload\"}";
                return true;
            }
            out = "{\"ok\":true,\"live\":false,\"slot\":\"" + JsonEsc(slot) +
                  "\",\"gen\":" + std::to_string(gen) + "}";
            return true;
        }
        if (tool == "list_slots") {
            std::vector<std::string> slots;
            jk::workshop::ListSlots(jk::workshop::DirOf(scriptPath_), slots);
            std::string body;
            for (const auto& s : slots) {
                if (!body.empty()) body += ",";
                body += "\"" + JsonEsc(s) + "\"";
            }
            out = "{\"ok\":true,\"current\":\"" + JsonEsc(CurrentSlotName()) +
                  "\",\"slots\":[" + body + "]}";
            return true;
        }
        if (tool == "use_slot") {
            jk::agent::AgentJson args(argsJson);
            std::string slotArg;
            if (!args.ok() || !args.GetStr("slot", slotArg)) {
                out = "{\"error\":\"bad_args\",\"need\":\"slot:string\"}";
                return false;
            }
            if (!jk::workshop::IsValidSlotName(slotArg)) {
                out = R"({"error":"bad_slot","rule":"[A-Za-z0-9_-]{1,32}"})";
                return false;
            }
            if (slotArg == CurrentSlotName()) {
                out = "{\"ok\":true,\"slot\":\"" + JsonEsc(slotArg) + "\"}";
                return true;
            }
            std::string probe;
            if (!ReadTextFile(jk::workshop::DirOf(scriptPath_) + "\\" +
                                  slotArg + ".js",
                              probe)) {
                out = "{\"error\":\"no_such_slot\",\"slot\":\"" +
                      JsonEsc(slotArg) + "\"}";
                return false;
            }
            if (!SwitchToSlot(slotArg)) {
                // 전환·기동은 성립했고 새 슬롯의 실패 라벨이 패널에 표시된다 —
                // 오류를 폐곡선으로 회신(조용한 눌먹기 금지).
                out = "{\"ok\":false,\"slot\":\"" + JsonEsc(slotArg) +
                      "\",\"error\":\"" + JsonEsc(host_->LastError()) +
                      "\",\"hint\":\"call the api tool for the function list\"}";
                return false;
            }
            out = "{\"ok\":true,\"slot\":\"" + JsonEsc(slotArg) + "\"}";
            return true;
        }
        if (tool == "script_history") {
            jk::agent::AgentJson args(argsJson);
            std::string slotArg;
            if (args.ok()) (void)args.GetStr("slot", slotArg);
            std::string slot, path;
            if (!ResolveSlot(slotArg, slot, path)) {
                out = R"({"error":"bad_slot","rule":"[A-Za-z0-9_-]{1,32}"})";
                return false;
            }
            std::vector<jk::workshop::HistoryEntry> gens;
            if (!jk::workshop::ListHistory(jk::workshop::DirOf(scriptPath_),
                                           slot, gens)) {
                gens.clear();  // 세대 없음(첫 스냅샷 전) — 빈 목록이 정답
            }
            std::string body;
            for (const auto& g : gens) {
                if (!body.empty()) body += ",";
                body += "{\"gen\":" + std::to_string(g.gen) +
                        ",\"bytes\":" + std::to_string(g.bytes) +
                        ",\"mtime\":" + std::to_string(g.mtimeSecs) + "}";
            }
            out = "{\"ok\":true,\"slot\":\"" + JsonEsc(slot) +
                  "\",\"gens\":[" + body + "]}";
            return true;
        }
        if (tool == "restore_script") {
            jk::agent::AgentJson args(argsJson);
            int gen = 0;
            std::string slotArg;
            if (!args.ok() || !args.GetInt("gen", gen)) {
                out = "{\"error\":\"bad_args\",\"need\":\"gen:int\"}";
                return false;
            }
            (void)args.GetStr("slot", slotArg);
            std::string slot, path;
            if (!ResolveSlot(slotArg, slot, path)) {
                out = R"({"error":"bad_slot","rule":"[A-Za-z0-9_-]{1,32}"})";
                return false;
            }
            const std::string dir = jk::workshop::DirOf(scriptPath_);
            std::string source;
            if (!jk::workshop::ReadGen(dir, slot, gen, source)) {
                out = "{\"error\":\"no_such_gen\",\"slot\":\"" +
                      JsonEsc(slot) + "\",\"gen\":" + std::to_string(gen) +
                      "}";
                return false;
            }
            // 복원 자체도 undoable — 직전 원문이 새 세대로 스냅샷된다.
            int snapshotGen = 0;
            std::string prev;
            if (ReadTextFile(path, prev)) {
                snapshotGen = jk::workshop::AppendSnapshot(dir, slot, prev);
                if (snapshotGen == 0) {
                    out = "{\"error\":\"snapshot_failed\"}";
                    return false;
                }
            }
            if (!WriteTextFile(path, source)) {
                out = "{\"error\":\"write_failed\"}";
                return false;
            }
            const bool reloadOk =
                (scriptPath_ == path) ? ReloadNow() : SwitchToSlot(slot);
            if (!reloadOk) {
                out = "{\"ok\":false,\"slot\":\"" + JsonEsc(slot) +
                      "\",\"gen\":" + std::to_string(gen) +
                      ",\"snapshotGen\":" + std::to_string(snapshotGen) +
                      ",\"error\":\"" + JsonEsc(host_->LastError()) + "\"}";
                return false;
            }
            out = "{\"ok\":true,\"slot\":\"" + JsonEsc(slot) +
                  "\",\"gen\":" + std::to_string(gen) + ",\"snapshotGen\":" +
                  std::to_string(snapshotGen) + "}";
            return true;
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

    // 현재 슬롯명 = scriptPath_ 스템(myapp 등) — 응답·영속의 표시 이름.
    std::string CurrentSlotName() const {
        const std::string dir = jk::workshop::DirOf(scriptPath_);
        std::string stem = scriptPath_;
        if (dir != ".") stem = scriptPath_.substr(dir.size() + 1);
        const size_t dot = stem.find_last_of('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        return stem;
    }

    // 슬롯 인자 해석 (docs/67 단 1): 있으면 [A-Za-z0-9_-]{1,32} 검증 후
    // <dirOf(scriptPath_)>\<slot>.js, 없으면 현재. 검증 실패 = bad_slot.
    bool ResolveSlot(const std::string& slotArg, std::string& slotOut,
                     std::string& pathOut) const {
        if (slotArg.empty()) {
            slotOut = CurrentSlotName();
            pathOut = scriptPath_;
            return true;
        }
        if (!jk::workshop::IsValidSlotName(slotArg)) return false;
        slotOut = slotArg;
        pathOut = jk::workshop::DirOf(scriptPath_) + "\\" + slotArg + ".js";
        return true;
    }

    // 슬롯 전환 (docs/67 단 1): 상태는 stateByPath_가 무료로 보존한다
    // (스위치 아웃 → 구 경로 키 보관, 복귀 → 복원). 사전 조건: 슬롯 파일이
    // 존재한다(호출자 검증). 영속은 전환마다 재기록.
    bool SwitchToSlot(const std::string& slot) {
        const std::string dir = jk::workshop::DirOf(scriptPath_);
        const std::string target = dir + "\\" + slot + ".js";
        if (scriptPath_ == target) return true;  // 이미 현재
        TeardownLiveScript();  // 구 경로 키로 상태 보존 — 경로 교체 전에
        if (!agentAppName_.empty())
            jk::workshop::WriteCurrentSlotFile(dir, agentAppName_, slot);
        scriptPath_ = target;
        const bool ok = ReloadNow();
        RefreshStrip();  // 실패 시에도 콤보가 실제 상태를 따르게
        return ok;
    }

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
        "\"state\":\"리로드는 상태를 보존한다 — 편집창(텍스트·한/영 모드)은 자동 복원; onSaveState()를 정의하면 임의 JS 상태를 직렬화해 보존하고 onRestoreState(saved)로 복귀한다(정의 없으면 편집창만). 라벨·캔버스는 스크립트 소유 파생 출력이라 새 스크립트가 다시 그린다\","
        "\"patch\":\"작은 수정(콜백 몸통 교체)은 도구 set_script live:1 — 컨텍스트가 살아 있고 위젯·타이머·글로벌 상태·의미 커서 선언이 보존된다. 패치 안전 형태: top-level은 function 정의만 (top-level const/let 재선언은 에러 → 풀 리로드 낙하); 위젯·타이머 생성은 onCreate에서 — 패치 평가 중 생성은 bad_patch 에러. 타이머 간격·위젯 구조 변경은 live 불가 → live:0 풀 리로드\","
        "\"slots\":\"여러 슬롯(<scriptsDir>/<slot>.js) 지원 — 도구 list_slots/use_slot/script_history/restore_script; set_script의 slot 인자=해당 슬롯에 쓰고 자동 전환. 도구로 덮어쓰면 직전 원문이 .history/<slot>/NNNN.js로 자동 스냅샷(20세대 캡); 메모장 수기 편집은 리본을 우회한다\","
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