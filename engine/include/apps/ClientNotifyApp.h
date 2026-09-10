#ifndef CLIENTNOTIFYAPP_H
#define CLIENTNOTIFYAPP_H

// Notification center client (agent platform, docs/33): an ImGui window
// client (palette/taskmgr template) that subscribes to desktop agent events
// on its own window connection and keeps the single notification history
// (spec §7). MVP surface: in-window toast strip + unread-count title badge
// (MsgType::WindowTitle); a screen-corner toast needs the shell track later.

#include <client/JKClientApplication.h>
#include <string>
#include <vector>

namespace jk {

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

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;   // Malgun Gothic loaded (hangul-capable)
    bool scrollDirty_ = false;

    std::vector<std::string> preview_;   // Task 2 skeleton feed (bounded)
};

} // namespace jk

#endif // CLIENTNOTIFYAPP_H