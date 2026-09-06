#ifndef JANGOUI_H
#define JANGOUI_H

// Shared JANGO launcher UI, used by both the single-process JangoApp
// (src/apps/JangoApp.cpp) and the server-mode client module
// (src/apps/ClientJangoApp.cpp). The host (JKApplication or
// JKClientApplication) owns the SDL side; this class only builds and drives
// the JKWindow control tree, so the UI code exists exactly once.

#include <JKControl.h>
#include <JKDialog.h>
#include <JKTypes.h>
#include <functional>
#include <memory>
#include <string>

namespace jk {

class JKEdit;
class JKFileDialog;
class JKListBox;
class JKMessageBox;
class JKWindow;
class Equip24Dialog;
class EquipDialog;
class InsaDialog;

// ---------------------------------------------------------------------------
// AboutPanel: fills the right-hand information area of the main menu.
// ---------------------------------------------------------------------------
class AboutPanel : public JKControl {
public:
    AboutPanel(const JKRect& rect, uint16_t controlId);

    void OnPaintClient(JKDC& dc) override;
};

// ---------------------------------------------------------------------------
// PasswordDialog: modal dialog with a single edit and OK/Cancel.
// ---------------------------------------------------------------------------
class PasswordDialog : public JKDialog {
public:
    using DoneCallback = std::function<void(int result, const std::string& text)>;

    explicit PasswordDialog(DoneCallback onDone);

    void ClearPassword();

private:
    JKEdit* edit_ = nullptr;
};

// ---------------------------------------------------------------------------
// BudaeDialog: modal dialog that selects a unit (budae).
// ---------------------------------------------------------------------------
class BudaeDialog : public JKDialog {
public:
    using DoneCallback = std::function<void(int result, const std::string& selected)>;

    explicit BudaeDialog(DoneCallback onDone);

    void SelectUnit(const std::string& current);

private:
    JKListBox* list_ = nullptr;
};

// ---------------------------------------------------------------------------
// JANGO launcher UI state + menu handling.
// ---------------------------------------------------------------------------
class JangoUI {
public:
    // onExit is invoked by the Exit button: the single-process shell pushes
    // SDL_QUIT; the client shell stops its run loop (server disconnect).
    explicit JangoUI(std::function<void()> onExit);
    ~JangoUI();

    void BuildMainWindow();
    // Shells hand the window to their host via SetMainWindow(std::move(...)).
    std::unique_ptr<JKWindow> TakeMainWindow();
    void CreateDialogs();

private:
    void AddMenuButton(uint16_t id, const char* label, const JKRect& rect);
    void ShowMessage(const std::string& title, const std::string& msg);
    void OnMenuButton(uint16_t id);
    void OnPasswordDone(int result, const std::string& entered);
    void OnBudaeDone(int result, const std::string& selected);

    std::function<void()> onExit_;
    std::unique_ptr<JKWindow> mainWindow_;
    std::unique_ptr<PasswordDialog> passwordDlg;
    std::unique_ptr<BudaeDialog> budaeDlg;
    std::unique_ptr<JKFileDialog> fileDialog;
    std::unique_ptr<JKMessageBox> msgBox;

    // 포팅된 원본 JANGO 앱들 (재사용을 위해 닫힌 후에도 유지)
    std::unique_ptr<Equip24Dialog> equip24Dlg;
    std::unique_ptr<EquipDialog> equipDlg;
    std::unique_ptr<InsaDialog> insaDlg;

    std::string budaeName = "HQ";
};

} // namespace jk

#endif // JANGOUI_H