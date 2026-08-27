#ifndef JKFILEDIALOG_H
#define JKFILEDIALOG_H

#include <JKWindow.h>
#include <JKListBox.h>
#include <JKEdit.h>
#include <functional>
#include <string>

namespace jk {

class JKFileDialog : public JKWindow {
public:
    JKFileDialog();
    explicit JKFileDialog(const std::string& title);

    void SetFilter(const std::string& filter) { filter_ = filter; }
    void SetInitialDir(const std::string& dir) { currentDir_ = dir; }
    const std::string& GetFileName() const { return fileName_; }

    void Show();

    void SetOnOk(std::function<void(const std::string&)> cb) { onOk_ = std::move(cb); }
    void SetOnCancel(std::function<void()> cb) { onCancel_ = std::move(cb); }

    void RespondMessage(const JKEvent& ev) override;

protected:
    void OnInitControls();
    void RefreshList();
    void OnOk();
    void OnCancel();

private:
    std::string currentDir_;
    std::string filter_;
    std::string fileName_;
    std::function<void(const std::string&)> onOk_;
    std::function<void()> onCancel_;

    JKListBox* listBox_ = nullptr;
    JKEdit* fileEdit_ = nullptr;
};

} // namespace jk

#endif // JKFILEDIALOG_H
