#include <JKDialog.h>
#include <JKApplication.h>
#include <JKEvent.h>
#include <SDL.h>

namespace jk {

JKDialog::JKDialog(const std::string& title) : JKWindow(title) {
}

void JKDialog::Show() {
    Open();
    FocusFirstChild();
    if (g_currentJKApp) {
        g_currentJKApp->SetModalWindow(this);
    }
}

void JKDialog::Close(int result) {
    result_ = result;
    if (onClose_) {
        onClose_(result);
    }
    RequestClose();
    if (g_currentJKApp && g_currentJKApp->GetModalWindow() == this) {
        g_currentJKApp->SetModalWindow(nullptr);
    }
}

void JKDialog::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::KeyDown && ev.keyCode == SDLK_ESCAPE) {
        Close(ResultCancel);
        return;
    }

    JKWindow::RespondMessage(ev);

    // The title-bar close button sets closeRequested_ without resetting modal state.
    if (IsCloseRequested() && g_currentJKApp && g_currentJKApp->GetModalWindow() == this) {
        result_ = ResultCancel;
        if (onClose_) {
            onClose_(ResultCancel);
        }
        g_currentJKApp->SetModalWindow(nullptr);
    }
}

} // namespace jk
