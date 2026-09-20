#ifndef APPS_CLIENTSCRIPTAPP_H
#define APPS_CLIENTSCRIPTAPP_H

#include <JKApplicationHost.h>
#ifdef _WIN32
// FileMtime's GetFileAttributesExA (docs/60: 100ns mtime for the watch poll).
// 수기 선언 (관례): windows.h/fileapi.h 둘 다 이 헤더 체인에선 안전하지 않다 —
// windows.h의 wingdi TextOut 매크로가 main.cpp의 JKDC::TextOut 사용부를
// 오염하고, fileapi.h의 CreateDirectoryA 등은 wancode 레거시 선언과 충돌한다
// (둘 다 2026-09-20 빌드 실측). windows.h가 먼저 온 TU에서는 SDK 선언을 그대로 쓴다.
struct JkFileAttrData {
    unsigned long attributes;       // dwFileAttributes (@0)
    unsigned long long createTime;  // ftCreationTime (@8)
    unsigned long long lastWriteTime;
    unsigned long long fileSize;
};
static_assert(sizeof(JkFileAttrData) == 32, "WIN32_FILE_ATTRIBUTE_DATA layout");
#ifndef _WINBASE_  // windows.h가 선행 인클루드됐으면 SDK 선언이 이미 있다
extern "C" __declspec(dllimport) int __stdcall GetFileAttributesExA(
    const char* lpFileName, int fInfoLevelId, void* lpFileInformation);
constexpr int kGetFileExInfoStandard = 0;  // winbase.h
#else
constexpr auto kGetFileExInfoStandard = ::GetFileExInfoStandard;
#endif
#endif
#include <JKEvent.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <agent/JKAgentJson.h>
#include <client/JKClientApplication.h>
#include <client/JKClientSurface.h>
#include <script/JKScriptHost.h>

#include <cstdio>
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
        }
        lastMtime_ = FileMtime(scriptPath_);
    }

    static long long FileMtime(const std::string& path) {
#ifdef _WIN32
        // 100ns FILETIME, not _stat64::st_mtime: the stat mtime is 1-second
        // resolution, so two saves within the same second (rapid notepad
        // edits / probe back-to-back writes) alias to the same stamp and the
        // watcher never fires (probe_workshop c5, first live run).
        JkFileAttrData fa = {};
        if (!GetFileAttributesExA(path.c_str(), kGetFileExInfoStandard, &fa)) {
            return 0;
        }
        return static_cast<long long>(fa.lastWriteTime);
#else
        struct stat st = {};
        if (::stat(path.c_str(), &st) != 0) return 0;
        return static_cast<long long>(st.st_mtime);
#endif
    }

    // Hot reload tick (dev-only, JK_SCRIPT_WATCH=1). Two-phase: the closed
    // panel is destroyed by RemoveClosedChildren after the event drain, so the
    // rebuild waits for the next tick to start from a clean child list.
    void OnWatchTick() {
        if (reloadPending_) {
            reloadPending_ = false;
            StartScript();
            return;
        }
        long long mtime = FileMtime(scriptPath_);
        if (mtime == 0 || mtime == lastMtime_) return;
        lastMtime_ = mtime;
        std::printf("[script] app.js changed - hot reload\n");
        std::fflush(stdout);
        host_->Stop();
        if (g_jkAppHost) {
            g_jkAppHost->SetModalWindow(nullptr);
            g_jkAppHost->ReleaseCapture();
        }
        if (panel_) {
            panel_->RequestClose();
            panel_ = nullptr;
        }
        reloadPending_ = true;
    }

    // Synchronous reload (docs/60 §2.3): Stop + panel teardown + immediate
    // rebuild, returning whether the fresh script booted (LastError() has the
    // error otherwise). Only valid on the frame thread — agent tool calls run
    // inside JKClientApplication::Run's sweep (app-tool-hub §8.2), the same
    // main/UI thread as every event handler, so QuickJS's main-thread-only
    // rule (docs/27 §3.2) holds. The sweep calls RemoveClosedChildren a few
    // lines after the tool poll; doing it inline here first is idempotent.
    bool SyncReload() {
        host_->Stop();
        if (g_jkAppHost) {
            g_jkAppHost->SetModalWindow(nullptr);
            g_jkAppHost->ReleaseCapture();
        }
        if (panel_) {
            panel_->RequestClose();
            panel_ = nullptr;
        }
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
    // App token for AgentToolRegister — must match the MANI name so the
    // broker's composed MCP names (<app>_<tool>) line up. Set before Init().
    void SetAgentAppName(const std::string& name) { agentAppName_ = name; }

protected:
    void OnInit() override {
        ScriptAppT<JKClientApplication>::OnInit();
        // Registration timing (docs/58 §5.1): OnInit runs after the surface
        // Connect, so a register here never hits the "before-connect silent
        // false" path. No reconnect path exists — one registration suffices.
        if (agentAppName_.empty() || scriptPath_.empty()) return;
        if (jk::client::JKClientSurface* surface = this->Surface()) {
            using Decl = jk::client::JKClientSurface::AgentToolDecl;
            std::vector<Decl> tools = {
                {"get_script", "Read the workshop script source", "{}"},
                {"set_script",
                 "Replace the workshop script and reload it synchronously — "
                 "a script error comes back in the same response",
                 "{\"type\":\"object\",\"properties\":{\"source\":{\"type\":"
                 "\"string\"}},\"required\":[\"source\"]}"},
            };
            surface->SendAgentToolRegister(agentAppName_, tools);
        }
    }

    bool OnAgentToolCall(const std::string& tool, const std::string& argsJson,
                         std::string& out) override {
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
            if (SyncReload()) {
                out = "{\"ok\":true}";
                return true;
            }
            // The closed loop (docs/60 §2.3): the failing script's error text
            // travels back inside the tool response so the agent fixes itself.
            out = "{\"ok\":false,\"error\":\"" + JsonEsc(host_->LastError()) +
                  "\"}";
            return false;
        }
        // Unreachable through the server (reverse matching answers
        // unknown_app_tool first) — defensive, same as vplayer.
        out = "{\"error\":\"unknown_tool\",\"tool\":\"" + tool + "\"}";
        return false;
    }

private:
    static constexpr size_t kMaxScriptBytes = 256 * 1024;  // docs/60 §2.3

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
};

} // namespace jk

#endif // APPS_CLIENTSCRIPTAPP_H