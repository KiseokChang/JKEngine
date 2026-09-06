#ifndef CLIENTTASKBARAPP_H
#define CLIENTTASKBARAPP_H

// Desktop shell client (docs/28): registers as the server's shell via
// ShellRegister and renders the taskbar — one button per app window,
// click to focus (and later, minimize-toggle). The server owns NO taskbar
// UI: it only pushes WindowList snapshots; everything below is jkwindow.

#include <client/JKClientApplication.h>
#include <JKButton.h>
#include <JKWindow.h>
#include <functional>
#include <string>
#include <vector>

namespace jk {

// One taskbar button bound to a server window (surfaceId). Paints a title
// hash-color chip + title with active / minimized / normal variants.
class TaskbarButton : public JKButton {
public:
    TaskbarButton() = default;

    // Rebind in place from a WindowList snapshot (pool reuse — no control
    // teardown, so no flicker and no remove-control API needed).
    void Bind(uint32_t surfaceId, const std::string& title,
              bool active, bool minimized);

    void SetOnActivate(std::function<void(uint32_t)> cb) { onActivate_ = std::move(cb); }

    void OnClick() override;
    void OnPaintClient(JKDC& dc) override;

private:
    uint32_t surfaceId_ = 0;
    bool active_ = false;
    bool minimized_ = false;
    std::function<void(uint32_t)> onActivate_;
};

class ClientTaskbarApp : public JKClientApplication {
public:
    ClientTaskbarApp() = default;
    ~ClientTaskbarApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    // Frame-less main window with the dark bar background (JKWindow paints a
    // hardcoded light-grey client background otherwise).
    class TaskbarWindow : public JKWindow {
    public:
        explicit TaskbarWindow(const std::string& title) : JKWindow(title) {}
    protected:
        void OnPaintClient(JKDC& dc) override;
    };

    void RefreshWindowList();
    void Relayout();

    // Latest snapshot from GetWindowList (copy of the surface's pending list).
    std::vector<jk::client::ShellWindowInfo> windows_;
    // Fixed pool: WindowListPayload holds at most 32 entries, so buttons are
    // pre-created once and rebound in place (hidden past the window count).
    std::vector<TaskbarButton*> buttons_;
};

} // namespace jk

#endif // CLIENTTASKBARAPP_H