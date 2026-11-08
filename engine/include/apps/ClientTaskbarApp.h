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
    // 네비 셀 모드(스펙 §4): 칩 없이 라벨 중앙 정렬 — 좁은 44px 셀에서
    // 창 버튼 레이아웃(칩+텍스트)의 클립으로 라벨이 안 보이는 실측 수형.
    void SetNavMode(bool v) { navMode_ = v; }

    // Rebind in place from a WindowList snapshot (pool reuse — no control
    // teardown, so no flicker and no remove-control API needed).
    void Bind(uint32_t surfaceId, const std::string& title,
              bool active, bool minimized);

    // Click behavior: focus the window; re-clicking the ACTIVE button asks
    // for a minimize toggle instead (docs/28 단계 4).
    void SetOnClickWindow(std::function<void(uint32_t, bool wasActive)> cb) {
        onClickWindow_ = std::move(cb);
    }

    void OnClick() override;
    void OnPaintClient(JKDC& dc) override;

private:
    uint32_t surfaceId_ = 0;
    bool active_ = false;
    bool minimized_ = false;
    bool navMode_ = false;
    std::function<void(uint32_t, bool)> onClickWindow_;
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
    // 네비 셀(스펙 2026-11-08-jkdesktop-navigation-design §4 — DeX 하단바
    // 관례 좌측: 뒤·홈·최근). 창 버튼 pool과 별개 고정 셀 — Relayout이
    // 창 버튼을 네비 스트립 우측으로 미끼친다.
    TaskbarButton* navBack_ = nullptr;
    TaskbarButton* navHome_ = nullptr;
    TaskbarButton* navRecents_ = nullptr;
    // [셸 2.0 S5 — 정리 셀] 시계 셀 = 자식 KSSM 미렌더 실측 제거(S4 위젯 대체).
    TaskbarButton* cleanupCell_ = nullptr;
};

} // namespace jk

#endif // CLIENTTASKBARAPP_H