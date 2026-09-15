#ifndef APPS_CLIENTFILEDIALOGAPP_H
#define APPS_CLIENTFILEDIALOGAPP_H

#include <client/JKClientApplication.h>
#include <client/JKClientSurface.h>
#include <chrono>
#include <string>
#include <vector>

namespace jk {

// File open dialog (file dialog design §1b, Task 2): the resolver of the
// server's parked `file_open` query. Spawned by the server as a client process
// (jkapp_filedlg.dll) with `--filedlg <json>` argv — but per design D3 the
// spawn args are a transport convenience only: the contract is the
// `file_dialog_params` query fired right after connect (1-shot, first come
// first served on the server slot) and the single `file_open_result` sent on
// the way out (open, cancel, or server chrome close — guarded to fire exactly
// once). UI mirrors the legacy JKFileDialog contract: dirs-first listing,
// MatchFilter semantics, Enter=OK / Esc=cancel, double-click folder descends /
// double-click file opens.
class ClientFileDialogApp : public JKClientApplication {
public:
    ~ClientFileDialogApp() override;

    // Last-chance result send for the server chrome Quit path: the Quit event
    // stops the run loop before any UI hook can fire, so the module calls this
    // between Run() returning and destruction while the pipe is still open.
    // No-op when the result already went out (open/cancel paths).
    void FlushPendingResult();

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;  // ImGui palette re-apply (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    struct Entry {
        std::string name;
        bool isDir = false;
    };

    void BuildUi(int w, int h);
    // Re-enumerate currentDir_ into entries_ (dirs first, MatchFilter on
    // files). On failure fills error_ — the list area shows the error overlay.
    void RefreshList();
    // Validate + switch to `path`, then RefreshList. Invalid/non-directory
    // paths keep the current directory and raise the error overlay.
    void NavigateTo(const std::string& path);
    void NavigateUp();
    // 열기/Enter: a directory name descends, anything else resolves to the
    // full path and ends the dialog with ok=true (legacy OnOk contract).
    void OnOk();
    // End the dialog: send file_open_result exactly once (guard flag), then
    // stop the run loop — Close() follows from the destructor.
    void Finish(bool ok, const std::string& path);
    void SendResult(bool ok, const std::string& path);
    // Send the file_dialog_params query once the connection is up (retrying
    // per frame until it lands — the spawn/connect race must not strand the
    // dialog on defaults).
    void RequestParams();
    // Poll queued AgentReplies; the params reply applies filter/start/title
    // and stores requesterConnId_ for the result echo (sender correlation —
    // the server resolves file_open_result only when it matches the slot).
    void PumpReplies();
    void SetFileName(const std::string& name);
    void SyncDirBuffer();
    void SyncFilterBuffer();
    // Split filterAll_ into combo items (dedup + a 모든 파일 fallback whose
    // LABEL is Korean but whose pattern is "*.*" — a label must never leak
    // into MatchFilter).
    void BuildFilterChoices();
    static std::string EscapeJson(const std::string& in);

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool focusFileName_ = true;  // grab keyboard focus on open (dialog idiom)
    std::chrono::steady_clock::time_point lastFrame_;

    // Filesystem state (ported from JKFileDialog — dialog-local, no jkcore
    // promotion: the module owns its core copy by design).
    struct FilterChoice {
        std::string label;    // combo display text
        std::string pattern;  // what MatchFilter runs against
    };
    std::string currentDir_;
    std::string filterAll_;      // full filter list from the params reply
    std::string filterActive_;   // the pattern MatchFilter runs against
    std::vector<FilterChoice> filterChoices_;  // combo items (split + 전체)
    std::vector<Entry> entries_;
    std::string error_;          // non-empty → error overlay in the list area
    int selectedIdx_ = -1;

    char dirBuf_[1024] = {};      // current-dir edit box
    char fileBuf_[1024] = {};     // file-name edit box
    char filterBuf_[256] = {};    // combo preview (filterActive_ mirror)

    // Agent channel state. The dialog is one query deep on the params leg;
    // file_open_result is fire-and-forget (the parked requester gets the
    // reply, the dialog just exits).
    uint32_t nextQueryId_ = 1;
    uint32_t paramsQueryId_ = 0;  // 0 = reply consumed (or never sent)
    bool paramsRequested_ = false;
    bool resultSent_ = false;     // file_open_result exactly-once guard
    // Sender correlation (final-review MAJOR-1): file_dialog_params delivers
    // the requester's connection id; SendResult echoes it so the server can
    // verify the result comes from the dialog that owns the CURRENT slot —
    // an orphan dialog (slot expired/reclaimed while still open) must resolve
    // nothing. 0 also happens for a manual `--client filedlg` run with no
    // parked slot — its result is a harmless parked:false no-op either way.
    uint32_t requesterConnId_ = 0;
};

} // namespace jk
#endif // APPS_CLIENTFILEDIALOGAPP_H
