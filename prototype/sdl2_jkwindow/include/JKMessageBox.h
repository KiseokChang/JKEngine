#ifndef JKMESSAGEBOX_H
#define JKMESSAGEBOX_H

#include <JKWindow.h>
#include <functional>
#include <string>

namespace jk {

class JKMessageBox : public JKWindow {
public:
    enum class Buttons { Ok, OkCancel, YesNo, YesNoCancel };

    static constexpr int ResultOk = 1;
    static constexpr int ResultCancel = 2;
    static constexpr int ResultYes = 3;
    static constexpr int ResultNo = 4;

    JKMessageBox(const std::string& title, const std::string& message,
                 Buttons buttons = Buttons::Ok,
                 std::function<void(int)> onResult = nullptr);

    void Show();

    void SetOnResult(std::function<void(int)> cb) { onResult_ = std::move(cb); }

    void RespondMessage(const JKEvent& ev) override;

protected:
    void OnInitControls();

private:
    std::string message_;
    Buttons buttons_;
    std::function<void(int)> onResult_;

    void Close(int result);
    int CancelResult() const;
};

} // namespace jk

#endif // JKMESSAGEBOX_H
