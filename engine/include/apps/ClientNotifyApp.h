#ifndef CLIENTNOTIFYAPP_H
#define CLIENTNOTIFYAPP_H

// Notification center client (agent platform, docs/33): an ImGui window
// client (palette/taskmgr template) that subscribes to desktop agent events
// on its own window connection and keeps the single notification history
// (spec §7). MVP surface: in-window toast strip + unread-count title badge
// (MsgType::WindowTitle); a screen-corner toast needs the shell track later.

#include <client/JKClientApplication.h>
#include <cstdint>
#include <string>
#include <vector>

namespace jk {

// One notification in the center's history (docs/33). ts is epoch ms.
struct NotifyEntry {
    std::string topic;
    std::string title;
    std::string body;
    long long ts = 0;
    bool read = false;
};

class ClientNotifyApp : public JKClientApplication {
public:
    ClientNotifyApp() = default;
    ~ClientNotifyApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    void BuildUi(int w, int h);
    void DrainEvents();
    // Title badge (docs/33): "Notifications (N)" while N unread entries —
    // rides MsgType::WindowTitle so the taskbar button text follows.
    void UpdateBadge();
    void MarkAllRead();
    void ClearHistory();
    // Config + persistence (docs/33 §state): <exeDir>\state\notify.json
    // {"topics":[{"topic":"agent.notify"},...]} and notify_history.json
    // {"entries":[...]}. Missing files fall back to the default topic and
    // an empty history; a corrupt history starts empty.
    static std::string StatePath(const char* name);
    void LoadConfig();
    void LoadHistory();
    void SaveHistory();
    static std::string EscapeJson(const std::string& in);
    static uint64_t NowMs();

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;   // Malgun Gothic loaded (hangul-capable)

    std::vector<std::string> topics_;    // subscription filter (Task 3)
    std::vector<NotifyEntry> history_;   // newest last, capped at 200
    int unread_ = 0;

    // In-window toast strip (MVP — a screen-corner toast needs the shell
    // track, docs/33 §제한): the newest entry shows for 5 s, 2 s fade-out.
    NotifyEntry toastEntry_;
    uint64_t toastUntilMs_ = 0;
    std::string badge_;   // last title sent — UpdateBadge skips no-ops
};

} // namespace jk

#endif // CLIENTNOTIFYAPP_H