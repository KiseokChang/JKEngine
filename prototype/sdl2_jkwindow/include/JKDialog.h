#ifndef JKDIALOG_H
#define JKDIALOG_H

#include <JKWindow.h>
#include <functional>

namespace jk {

// Lightweight modal dialog base for porting legacy JKDialog behavior.
// Legacy code used a synchronous Run() returning an ID (ID_OK / ID_CANCEL).
// The SDL2 prototype is callback-driven, so dialogs are shown with Show()
// and report their result through SetOnClose() or GetResult().
class JKDialog : public JKWindow {
public:
    static constexpr int ResultOk = 1;
    static constexpr int ResultCancel = 2;
    static constexpr int ResultYes = 3;
    static constexpr int ResultNo = 4;

    explicit JKDialog(const std::string& title = "");

    void Show();
    void Close(int result = ResultCancel);
    void RespondMessage(const JKEvent& ev) override;

    int GetResult() const { return result_; }
    void SetOnClose(std::function<void(int)> cb) { onClose_ = std::move(cb); }

protected:
    int result_ = ResultCancel;
    std::function<void(int)> onClose_;
};

} // namespace jk

#endif // JKDIALOG_H
