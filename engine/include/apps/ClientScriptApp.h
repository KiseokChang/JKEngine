#ifndef APPS_CLIENTSCRIPTAPP_H
#define APPS_CLIENTSCRIPTAPP_H

#include <JKApplicationHost.h>
#include <JKEvent.h>
#include <JKWindow.h>
#include <client/JKClientApplication.h>
#include <script/JKScriptHost.h>

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

protected:
    void OnInit() override {
        // Hot reload is an opt-in dev switch (docs/27 단계 1) — deployment
        // paths never set JK_SCRIPT_WATCH. The mtime poll rides an internal
        // app timer (both base classes provide AddTimer) at the watch winId,
        // so the reload path is identical in single-process and client mode.
        const char* env = std::getenv("JK_SCRIPT_WATCH");
        watch_ = (env && env[0] == '1');

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

private:
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
        }
        lastMtime_ = FileMtime(scriptPath_);
    }

    static long long FileMtime(const std::string& path) {
#ifdef _WIN32
        struct _stat64 st = {};
        if (_stat64(path.c_str(), &st) != 0) return 0;
        return static_cast<long long>(st.st_mtime);
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

    std::string scriptTitle_;
    std::string scriptPath_;
    std::unique_ptr<JKScriptHost> host_;
    JKWindow* panel_ = nullptr;  // owned by the main window's child list

    // Hot reload state (dev-only). The mtime poll rides an internal app timer
    // at kWatchTimerWinId so the reload path is identical on both bases;
    // ScriptTimerWinIdBase (0x7F00) must stay above it.
    static constexpr uint32_t kWatchTimerWinId = 0x7E00;
    bool watch_ = false;
    bool reloadPending_ = false;  // panel closed; start on the next watch tick
    long long lastMtime_ = 0;
    uint64_t watchTimer_ = 0;
};

// Client-mode alias kept for the jkapp_script module (server mode).
using ClientScriptApp = ScriptAppT<JKClientApplication>;

} // namespace jk

#endif // APPS_CLIENTSCRIPTAPP_H