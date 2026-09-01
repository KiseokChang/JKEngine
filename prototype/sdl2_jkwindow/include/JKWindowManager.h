#ifndef JKWINDOWMANAGER_H
#define JKWINDOWMANAGER_H

#include <JKWindow.h>
#include <functional>
#include <vector>
#include <cstdint>

namespace jk {

// Window manager owns the top-level window list, z-order, focus routing,
// mouse capture, and modal state. In the current single-process prototype it
// operates on the JKWindow tree owned by JKApplication; in later phases it will
// become the compositor-side window list.
class JKWindowManager {
public:
    JKWindowManager();
    ~JKWindowManager();

    void SetMainWindow(JKWindow* main);
    JKWindow* GetMainWindow() const { return mainWindow_; }

    // Register/unregister a top-level (floating) window. The main window and
    // modal window do not need to be registered here.
    void AddWindow(JKWindow* window);
    void RemoveWindow(JKWindow* window);

    // Move a window to the top of the z-order. In the single-process backend
    // this updates the floating list; the caller is responsible for reordering
    // the underlying control tree if needed.
    void BringToFront(JKWindow* window);

    // Activate a window: bring it to the front and make it the input target.
    void ActivateWindow(JKWindow* window);

    // Iterate registered top-level windows from back to front.
    void ForEachWindow(const std::function<void(JKWindow*)>& fn) const;

    // Modal dialog support.
    void SetModalWindow(JKWindow* window);
    JKWindow* GetModalWindow() const { return modalWindow_; }

    // Mouse capture.
    void SetCapture(JKControl* control);
    void ReleaseCapture();
    JKControl* GetCapture() const { return captureControl_; }

    // Keyboard input target.
    void SetInputWindow(JKWindow* window);
    JKWindow* GetInputWindow() const { return inputWindow_; }

    // Convenience: return the window that should receive keyboard focus events.
    // Priority: modal > input window > main window.
    JKWindow* GetKeyboardTargetWindow() const;

    // Convenience: return the window used for mouse hit-testing.
    // Priority: modal > main window.
    JKWindow* GetMouseTargetWindow() const;

    // Control/window lookup across the whole window tree.
    JKControl* FindControlById(uint32_t winId);
    JKControl* FindControlByControlId(uint16_t controlId);
    JKWindow*  FindWindowById(uint32_t winId);

    // Called by a top-level window that is about to be closed. Clears any
    // WM state (focus, capture, modal previous focus) that points into the
    // closing subtree.
    void NotifyWindowClosing(JKWindow* window);

private:
    JKWindow* mainWindow_ = nullptr;
    std::vector<JKWindow*> floatingWindows_;

    JKWindow* modalWindow_ = nullptr;
    JKWindow* inputWindow_ = nullptr;
    JKControl* captureControl_ = nullptr;

    // Focus to restore when the modal dialog closes.
    JKControl* modalPrevFocus_ = nullptr;

    void ClearPointersInsideWindow(JKWindow* window);
};

} // namespace jk

#endif // JKWINDOWMANAGER_H
