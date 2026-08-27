#ifndef JKWINDOW_H
#define JKWINDOW_H

#include <JKControl.h>
#include <JKTypes.h>
#include <string>

namespace jk {

class JKWindow : public JKControl {
public:
    enum class WindowRegion { None, TitleBar, Border, Client };

    JKWindow();
    explicit JKWindow(const std::string& title);

    void SetTitle(const std::string& title);
    const std::string& GetTitle() const;

    void SetRect(const JKRect& rect) override;
    void SetWindowRect(const JKRect& rect);
    void MoveWindow(int32_t dx, int32_t dy);
    void MoveTo(int32_t x, int32_t y);
    void ResizeWindow(int32_t dx, int32_t dy);

    WindowRegion HitTestRegion(int32_t screenX, int32_t screenY) const;
    JKControl* HitTest(int32_t screenX, int32_t screenY) override;

    void SetFocusChild(JKControl* child);
    JKControl* GetFocusChild() { return focusChild_; }
    const JKControl* GetFocusChild() const { return focusChild_; }

    void FocusFirstChild();
    void FocusNextChild();
    void FocusPrevChild();

    void PaintWindow(JKDC& dc) override;
    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

protected:
    std::string title_;
    JKControl* focusChild_ = nullptr;

    bool dragging_ = false;
    JKPoint dragStartMouse_;
    JKRect dragStartRect_;

    bool resizing_ = false;
    JKPoint resizeStartMouse_;
    JKRect resizeStartRect_;

    JKRect GetCloseButtonRect() const;

    JKRect GetScreenClientRect() const override;
};

} // namespace jk

#endif // JKWINDOW_H
