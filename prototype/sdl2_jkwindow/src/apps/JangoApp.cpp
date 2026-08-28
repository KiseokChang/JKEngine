#include <apps/JangoApp.h>

#include <JKApplication.h>
#include <JKButton.h>
#include <JKDialog.h>
#include <JKFileDialog.h>
#include <JKListBox.h>
#include <JKMessageBox.h>
#include <JKStatic.h>
#include <JKEdit.h>
#include <JKWindow.h>
#include <JKDC.h>
#include <JKTypes.h>

#include <SDL.h>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

namespace jk {

namespace {

constexpr uint16_t ID_BTN_INSA      = 101;
constexpr uint16_t ID_BTN_EQUIP24   = 102;
constexpr uint16_t ID_BTN_EQUIP     = 103;
constexpr uint16_t ID_BTN_FILE      = 104;
constexpr uint16_t ID_BTN_BUDAE     = 105;
constexpr uint16_t ID_BTN_EXIT      = 106;

constexpr uint16_t ID_EDIT_PASSWORD = 200;
constexpr uint16_t ID_LIST_BUDAE    = 201;

constexpr uint8_t COL_LTBLUE_R = 173;
constexpr uint8_t COL_LTBLUE_G = 216;
constexpr uint8_t COL_LTBLUE_B = 230;
constexpr uint8_t COL_LTGRAY_R = 192;
constexpr uint8_t COL_LTGRAY_G = 192;
constexpr uint8_t COL_LTGRAY_B = 192;
constexpr uint8_t COL_BLUE_R   = 0;
constexpr uint8_t COL_BLUE_G   = 0;
constexpr uint8_t COL_BLUE_B   = 170;

const char* StubPassword = "1234";

} // anonymous namespace


// ---------------------------------------------------------------------------
// AboutPanel: fills the right-hand information area of the main menu.
// ---------------------------------------------------------------------------
class AboutPanel : public JKControl {
public:
    AboutPanel(const JKRect& rect, uint16_t controlId) {
        SetRect(rect);
        SetControlId(controlId);
        SetBackColor(COL_LTBLUE_R, COL_LTBLUE_G, COL_LTBLUE_B);
    }

    void OnPaintClient(JKDC& dc) override {
        const JKRect rc = GetScreenClientRect();
        dc.SetColor(COL_LTBLUE_R, COL_LTBLUE_G, COL_LTBLUE_B, 255);
        dc.FillRect(rc);
        dc.SetColor(COL_BLUE_R, COL_BLUE_G, COL_BLUE_B, 255);
        dc.DrawRect(rc);
        dc.SetTextColor(0, 0, 0);
        dc.TextOutX(rc.Expand(4), "JANGO Launcher\nSDL2 port prototype\n\nInsa / Equip24 / Equip stubs", ADJ_XYCENTER, false);
    }
};

// ---------------------------------------------------------------------------
// PasswordDialog: modal dialog with a single edit and OK/Cancel.
// ---------------------------------------------------------------------------
class PasswordDialog : public JKDialog {
public:
    using DoneCallback = std::function<void(int result, const std::string& text)>;

    explicit PasswordDialog(DoneCallback onDone) : JKDialog("Password") {
        SetWindowRect(MakeRect(220, 170, 420, 300));
        SetAttrFlags(WA_TITLEMOVEABLE);

        auto label = std::make_unique<JKStatic>(MakeRect(10, 10, 190, 30), 0);
        label->SetText("Enter password:");
        AddControl(std::move(label));

        edit_ = new JKEdit(MakeRect(10, 45, 190, 70), ID_EDIT_PASSWORD, 32, false);
        AddControl(std::unique_ptr<JKEdit>(edit_));

        auto ok = std::make_unique<JKButton>(MakeRect(20, 80, 90, 110), 0);
        ok->SetText("OK");
        ok->SetOnClick([this, onDone]() {
            onDone(ResultOk, edit_->GetText());
            Close(ResultOk);
        });
        AddControl(std::move(ok));

        auto cancel = std::make_unique<JKButton>(MakeRect(110, 80, 180, 110), 0);
        cancel->SetText("Cancel");
        cancel->SetOnClick([this, onDone]() {
            onDone(ResultCancel, std::string());
            Close(ResultCancel);
        });
        AddControl(std::move(cancel));
    }

    void ClearPassword() {
        if (edit_) {
            edit_->SetText("");
        }
    }

private:
    JKEdit* edit_ = nullptr;
};


// ---------------------------------------------------------------------------
// BudaeDialog: modal dialog that selects a unit (budae).
// ---------------------------------------------------------------------------
class BudaeDialog : public JKDialog {
public:
    using DoneCallback = std::function<void(int result, const std::string& selected)>;

    explicit BudaeDialog(DoneCallback onDone) : JKDialog("Change Unit") {
        SetWindowRect(MakeRect(220, 170, 420, 300));
        SetAttrFlags(WA_TITLEMOVEABLE);

        auto label = std::make_unique<JKStatic>(MakeRect(10, 10, 190, 30), 0);
        label->SetText("Select unit:");
        AddControl(std::move(label));

        list_ = new JKListBox(MakeRect(10, 45, 190, 100), ID_LIST_BUDAE);
        list_->AddString("HQ");
        list_->AddString("A Battalion");
        list_->AddString("B Battalion");
        list_->AddString("C Battalion");
        list_->SetSelectedIndex(0);
        AddControl(std::unique_ptr<JKListBox>(list_));

        auto ok = std::make_unique<JKButton>(MakeRect(20, 120, 90, 150), 0);
        ok->SetText("OK");
        ok->SetOnClick([this, onDone]() {
            int32_t sel = list_->GetSelectedIndex();
            std::string value = (sel >= 0) ? list_->GetString(static_cast<size_t>(sel)) : "";
            onDone(ResultOk, value);
            Close(ResultOk);
        });
        AddControl(std::move(ok));

        auto cancel = std::make_unique<JKButton>(MakeRect(110, 120, 180, 150), 0);
        cancel->SetText("Cancel");
        cancel->SetOnClick([this, onDone]() {
            onDone(ResultCancel, std::string());
            Close(ResultCancel);
        });
        AddControl(std::move(cancel));
    }

    void SelectUnit(const std::string& current) {
        if (!list_) return;
        for (size_t i = 0; i < list_->GetCount(); ++i) {
            if (list_->GetString(i) == current) {
                list_->SetSelectedIndex(static_cast<int32_t>(i));
                return;
            }
        }
        list_->SetSelectedIndex(0);
    }

private:
    JKListBox* list_ = nullptr;
};


// ---------------------------------------------------------------------------
// JangoApp implementation (PIMPL to keep the public header small).
// ---------------------------------------------------------------------------
class JangoApp::Impl {
public:
    JKApplication* app = nullptr;
    std::unique_ptr<JKWindow> mainWindow;
    std::unique_ptr<PasswordDialog> passwordDlg;
    std::unique_ptr<BudaeDialog> budaeDlg;
    std::unique_ptr<JKFileDialog> fileDialog;
    std::unique_ptr<JKMessageBox> msgBox;

    std::string budaeName = "HQ";

    void BuildMainWindow() {
        mainWindow = std::make_unique<JKWindow>("JANGO - Main Menu");
        mainWindow->SetBackColor(COL_LTBLUE_R, COL_LTBLUE_G, COL_LTBLUE_B);
        mainWindow->SetAttrFlags(WA_TITLEMOVEABLE);

        int32_t y = 50;
        const int32_t h = 105;

        AddMenuButton(ID_BTN_INSA,    "Personnel",    MakeRect(50, y, 300, y + 70)); y += h;
        AddMenuButton(ID_BTN_EQUIP24, "2.4G Equip",   MakeRect(50, y, 300, y + 70)); y += h;
        AddMenuButton(ID_BTN_EQUIP,   "Equipment",    MakeRect(50, y, 300, y + 70)); y += h;
        AddMenuButton(ID_BTN_FILE,    "File Select",  MakeRect(50, y, 300, y + 70)); y += h;
        AddMenuButton(ID_BTN_BUDAE,   "Change Unit",  MakeRect(50, y, 300, y + 70)); y += h;
        AddMenuButton(ID_BTN_EXIT,    "Exit",         MakeRect(50, y, 300, y + 70));

        auto about = std::make_unique<AboutPanel>(MakeRect(360, 50, 974, 649), 0);
        mainWindow->AddControl(std::move(about));
    }

    void AddMenuButton(uint16_t id, const char* label, const JKRect& rect) {
        auto btn = std::make_unique<JKButton>(rect, id);
        btn->SetText(label);
        btn->SetBackColor(COL_LTGRAY_R, COL_LTGRAY_G, COL_LTGRAY_B);
        btn->SetTextColor(0, 0, 0);
        btn->SetDepth(2);
        btn->SetOnClick([this, id]() { OnMenuButton(id); });
        mainWindow->AddControl(std::move(btn));
    }

    void ShowMessage(const std::string& title, const std::string& msg) {
        msgBox = std::make_unique<JKMessageBox>(title, msg, JKMessageBox::Buttons::Ok,
                                               [](int) { /* dismissed */ });
        msgBox->Show();
    }

    void OnMenuButton(uint16_t id) {
        switch (id) {
            case ID_BTN_INSA:
                if (passwordDlg) {
                    passwordDlg->ClearPassword();
                    passwordDlg->Show();
                }
                break;
            case ID_BTN_EQUIP24:
                ShowMessage("2.4G Equipment", "Equip24Window port stub.");
                break;
            case ID_BTN_EQUIP:
                ShowMessage("Equipment", "EquipWindow port stub.");
                break;
            case ID_BTN_FILE:
                if (fileDialog) {
                    fileDialog->Show();
                }
                break;
            case ID_BTN_BUDAE:
                if (budaeDlg) {
                    budaeDlg->SelectUnit(budaeName);
                    budaeDlg->Show();
                }
                break;
            case ID_BTN_EXIT: {
                SDL_Event quit;
                quit.type = SDL_QUIT;
                SDL_PushEvent(&quit);
                break;
            }
            default:
                break;
        }
    }

    void OnPasswordDone(int result, const std::string& entered) {
        if (result != JKDialog::ResultOk) return;
        if (entered == StubPassword) {
            ShowMessage("Personnel", "InsaWindow port stub.");
        } else {
            ShowMessage("Error", "Incorrect password.");
        }
    }

    void OnBudaeDone(int result, const std::string& selected) {
        if (result == JKDialog::ResultOk && !selected.empty()) {
            budaeName = selected;
            ShowMessage("Change Unit", "Current unit: " + selected);
        }
    }
};

JangoApp::JangoApp() : impl_(std::make_unique<Impl>()) {
    impl_->app = this;
}

JangoApp::~JangoApp() = default;

void JangoApp::OnInit() {
    impl_->BuildMainWindow();
    SetMainWindow(std::move(impl_->mainWindow));

    impl_->passwordDlg = std::make_unique<PasswordDialog>(
        [this](int r, const std::string& s) { impl_->OnPasswordDone(r, s); });

    impl_->budaeDlg = std::make_unique<BudaeDialog>(
        [this](int r, const std::string& s) { impl_->OnBudaeDone(r, s); });

    impl_->fileDialog = std::make_unique<JKFileDialog>("File Select");
    impl_->fileDialog->SetFilter("*.*");
    impl_->fileDialog->SetOnOk([this](const std::string& path) {
        impl_->ShowMessage("File Select", path);
    });
    impl_->fileDialog->SetOnCancel([]() {
        // no-op
    });
}

} // namespace jk

