#include <JKDialog.h>
#include <JKApplication.h>
#include <JKEvent.h>
#include <SDL.h>

namespace jk {

JKDialog::JKDialog(const std::string& title) : JKWindow(title) {
}

void JKDialog::Show() {
    Open();
    // SetModalWindow saves the previously focused control and then focuses the
    // first child of the modal; calling FocusFirstChild before this would
    // overwrite the control whose focus should be restored on close.
    if (g_jkAppHost) {
        g_jkAppHost->SetModalWindow(this);
    }
}

void JKDialog::Close(int result) {
    result_ = result;
    if (onClose_) {
        onClose_(result);
    }
    RequestClose();
    if (g_jkAppHost && g_jkAppHost->GetModalWindow() == this) {
        g_jkAppHost->SetModalWindow(nullptr);
    }
}

void JKDialog::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::KeyDown && ev.keyCode == SDLK_ESCAPE) {
        Close(ResultCancel);
        return;
    }

    JKWindow::RespondMessage(ev);

    // The title-bar close button sets closeRequested_ without resetting modal state.
    if (IsCloseRequested() && g_jkAppHost && g_jkAppHost->GetModalWindow() == this) {
        result_ = ResultCancel;
        if (onClose_) {
            onClose_(ResultCancel);
        }
        g_jkAppHost->SetModalWindow(nullptr);
    }
}

} // namespace jk
