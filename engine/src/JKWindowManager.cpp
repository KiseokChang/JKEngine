#include <JKWindowManager.h>
#include <algorithm>

namespace jk {

JKWindowManager::JKWindowManager() = default;

JKWindowManager::~JKWindowManager() = default;

void JKWindowManager::SetMainWindow(JKWindow* main) {
    mainWindow_ = main;
    floatingWindows_.clear();
    modalWindow_ = nullptr;
    inputWindow_ = nullptr;
    captureControl_ = nullptr;
    modalPrevFocus_ = nullptr;
}

void JKWindowManager::AddWindow(JKWindow* window) {
    if (!window || window == mainWindow_ || window == modalWindow_) return;
    if (std::find(floatingWindows_.begin(), floatingWindows_.end(), window)
        == floatingWindows_.end()) {
        floatingWindows_.push_back(window);
    }
}

void JKWindowManager::RemoveWindow(JKWindow* window) {
    auto it = std::find(floatingWindows_.begin(), floatingWindows_.end(), window);
    if (it != floatingWindows_.end()) {
        floatingWindows_.erase(it);
    }
}

void JKWindowManager::BringToFront(JKWindow* window) {
    if (!window) return;
    auto it = std::find(floatingWindows_.begin(), floatingWindows_.end(), window);
    if (it != floatingWindows_.end()) {
        floatingWindows_.erase(it);
        floatingWindows_.push_back(window);
    }
}

void JKWindowManager::ActivateWindow(JKWindow* window) {
    if (!window) {
        SetInputWindow(mainWindow_);
        return;
    }
    BringToFront(window);
    SetInputWindow(window);
}

void JKWindowManager::ForEachWindow(const std::function<void(JKWindow*)>& fn) const {
    for (JKWindow* w : floatingWindows_) {
        fn(w);
    }
}

void JKWindowManager::SetModalWindow(JKWindow* window) {
    if (modalWindow_ == window) return;

    ReleaseCapture();

    if (window && !modalWindow_) {
        // Save the control that had focus before the modal takes over.
        JKWindow* prevWindow = inputWindow_ ? inputWindow_ : mainWindow_;
        modalPrevFocus_ = prevWindow ? prevWindow->GetFocusChild() : nullptr;
    }

    modalWindow_ = window;

    if (modalWindow_) {
        inputWindow_ = modalWindow_;
        modalWindow_->FocusFirstChild();
    } else {
        inputWindow_ = mainWindow_;
        // Restore focus to the previously focused control if it still exists.
        if (modalPrevFocus_) {
            if (FindControlById(modalPrevFocus_->GetWinId()) == modalPrevFocus_) {
                modalPrevFocus_->SetFocus();
            } else {
                modalPrevFocus_ = nullptr;
            }
        }
        if (!modalPrevFocus_ && inputWindow_) {
            inputWindow_->FocusFirstChild();
        }
        modalPrevFocus_ = nullptr;
    }
}

void JKWindowManager::SetCapture(JKControl* control) {
    captureControl_ = control;
}

void JKWindowManager::ReleaseCapture() {
    captureControl_ = nullptr;
}

void JKWindowManager::SetInputWindow(JKWindow* window) {
    inputWindow_ = window;
}

JKWindow* JKWindowManager::GetKeyboardTargetWindow() const {
    if (modalWindow_) return modalWindow_;
    if (inputWindow_) return inputWindow_;
    return mainWindow_;
}

JKWindow* JKWindowManager::GetMouseTargetWindow() const {
    if (modalWindow_) return modalWindow_;
    return mainWindow_;
}

JKControl* JKWindowManager::FindControlById(uint32_t winId) {
    if (mainWindow_) {
        JKControl* c = mainWindow_->FindControlById(winId);
        if (c) return c;
    }
    if (modalWindow_) {
        return modalWindow_->FindControlById(winId);
    }
    return nullptr;
}

JKControl* JKWindowManager::FindControlByControlId(uint16_t controlId) {
    if (mainWindow_) {
        JKControl* c = mainWindow_->FindControlByControlId(controlId);
        if (c) return c;
    }
    if (modalWindow_) {
        return modalWindow_->FindControlByControlId(controlId);
    }
    return nullptr;
}

JKWindow* JKWindowManager::FindWindowById(uint32_t winId) {
    JKControl* c = FindControlById(winId);
    if (!c) return nullptr;
    return dynamic_cast<JKWindow*>(c);
}

static bool IsControlInSubtree(JKControl* control, JKWindow* root) {
    if (!control || !root) return false;
    if (control == root) return true;
    for (JKControl* p = control->GetParent(); p != nullptr; p = p->GetParent()) {
        if (p == root) return true;
    }
    return false;
}

void JKWindowManager::NotifyWindowClosing(JKWindow* window) {
    if (!window) return;

    // A modal window closing implicitly clears the modal state.
    if (modalWindow_ == window) {
        modalWindow_ = nullptr;
        // Intentionally keep modalPrevFocus_ so the next SetModalWindow(nullptr)
        // path can restore focus; if the closing window itself owned the focus
        // it will be cleared below.
    }

    if (inputWindow_ == window) {
        inputWindow_ = mainWindow_;
    }

    if (captureControl_ && IsControlInSubtree(captureControl_, window)) {
        captureControl_ = nullptr;
    }

    if (modalPrevFocus_ && IsControlInSubtree(modalPrevFocus_, window)) {
        modalPrevFocus_ = nullptr;
    }

    RemoveWindow(window);
}

} // namespace jk
