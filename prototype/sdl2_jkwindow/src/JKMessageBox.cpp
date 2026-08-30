#include <JKMessageBox.h>
#include <JKApplication.h>
#include <JKButton.h>
#include <JKStatic.h>
#include <JKEvent.h>
#include <SDL.h>
#include <algorithm>

namespace jk {

JKMessageBox::JKMessageBox(const std::string& title, const std::string& message,
                           Buttons buttons, std::function<void(int)> onResult)
    : JKWindow(title), message_(message), buttons_(buttons),
      onResult_(std::move(onResult)) {
    SetWindowRect(JKRect{ 160, 120, 320, 140 });
    SetAttrFlags(WA_TITLEMOVEABLE);
    OnInitControls();
}

void JKMessageBox::OnInitControls() {
    const JKRect client = GetClientRect();

    auto msg = std::make_unique<JKStatic>(JKRect{ 10, 10, client.w - 20, client.h - 60 }, 0);
    msg->SetText(message_);
    msg->SetAdjustFlag(ADJ_XYCENTER | ADJ_LEFT);
    msg->SetBackColor(240, 240, 240);
    msg->SetTextColor(0, 0, 0);
    AddControl(std::move(msg));

    auto addButton = [this](const std::string& label, int result, const JKRect& rect) {
        auto btn = std::make_unique<JKButton>(rect, 0);
        btn->SetText(label);
        btn->SetOnClick([this, result]() { Close(result); });
        AddControl(std::move(btn));
    };

    const int32_t btnW = 70;
    const int32_t btnH = 24;
    const int32_t y = client.h - 40;

    switch (buttons_) {
        case Buttons::Ok: {
            int32_t x = (client.w - btnW) / 2;
            addButton("OK", ResultOk, JKRect{ x, y, btnW, btnH });
            break;
        }
        case Buttons::OkCancel: {
            int32_t x1 = client.w / 2 - btnW - 10;
            int32_t x2 = client.w / 2 + 10;
            addButton("OK", ResultOk, JKRect{ x1, y, btnW, btnH });
            addButton("Cancel", ResultCancel, JKRect{ x2, y, btnW, btnH });
            break;
        }
        case Buttons::YesNo: {
            int32_t x1 = client.w / 2 - btnW - 10;
            int32_t x2 = client.w / 2 + 10;
            addButton("Yes", ResultYes, JKRect{ x1, y, btnW, btnH });
            addButton("No", ResultNo, JKRect{ x2, y, btnW, btnH });
            break;
        }
        case Buttons::YesNoCancel: {
            int32_t x1 = client.w / 2 - btnW - btnW / 2 - 20;
            int32_t x2 = client.w / 2 - btnW / 2;
            int32_t x3 = client.w / 2 + btnW / 2 + 20;
            addButton("Yes", ResultYes, JKRect{ x1, y, btnW, btnH });
            addButton("No", ResultNo, JKRect{ x2, y, btnW, btnH });
            addButton("Cancel", ResultCancel, JKRect{ x3, y, btnW, btnH });
            break;
        }
    }
}

void JKMessageBox::Show() {
    Open();
    // SetModalWindow saves the previously focused control and then focuses the
    // first child of the modal; calling FocusFirstChild before this would
    // overwrite the control whose focus should be restored on close.
    if (g_currentJKApp) {
        g_currentJKApp->SetModalWindow(this);
    }
}

int JKMessageBox::CancelResult() const {
    switch (buttons_) {
        case Buttons::Ok:        return ResultOk;
        case Buttons::OkCancel:
        case Buttons::YesNoCancel: return ResultCancel;
        case Buttons::YesNo:     return ResultNo;
    }
    return ResultCancel;
}

int JKMessageBox::DefaultResult() const {
    switch (buttons_) {
        case Buttons::Ok:          return ResultOk;
        case Buttons::OkCancel:    return ResultOk;
        case Buttons::YesNo:       return ResultYes;
        case Buttons::YesNoCancel: return ResultYes;
    }
    return ResultOk;
}

void JKMessageBox::Close(int result) {
    // Close the modal and restore focus before invoking the result callback,
    // so the callback can safely start a new game or open another dialog.
    RequestClose();
    if (g_currentJKApp && g_currentJKApp->GetModalWindow() == this) {
        g_currentJKApp->SetModalWindow(nullptr);
    }
    if (onResult_) onResult_(result);
}

void JKMessageBox::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::KeyDown) {
        if (ev.keyCode == SDLK_ESCAPE) {
            Close(CancelResult());
            return;
        }
        if (ev.keyCode == SDLK_RETURN || ev.keyCode == SDLK_KP_ENTER) {
            Close(DefaultResult());
            return;
        }
    }
    JKWindow::RespondMessage(ev);
    if (IsCloseRequested() && g_currentJKApp &&
        g_currentJKApp->GetModalWindow() == this) {
        g_currentJKApp->SetModalWindow(nullptr);
    }
}

} // namespace jk
