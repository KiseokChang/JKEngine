#include <apps/JangoUI.h>

#include <JKButton.h>
#include <JKFileDialog.h>
#include <JKListBox.h>
#include <JKMessageBox.h>
#include <JKStatic.h>
#include <JKEdit.h>
#include <JKWindow.h>
#include <JKDC.h>

#include <apps/Equip24App.h>
#include <apps/EquipApp.h>
#include <apps/InsaApp.h>

#include <SDL.h>
#include <cstdio>
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
// AboutPanel
// ---------------------------------------------------------------------------
AboutPanel::AboutPanel(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    SetBackColor(COL_LTBLUE_R, COL_LTBLUE_G, COL_LTBLUE_B);
}

void AboutPanel::OnPaintClient(JKDC& dc) {
    const JKRect rc = GetScreenClientRect();
    dc.SetColor(COL_LTBLUE_R, COL_LTBLUE_G, COL_LTBLUE_B, 255);
    dc.FillRect(rc);
    dc.SetColor(COL_BLUE_R, COL_BLUE_G, COL_BLUE_B, 255);
    dc.DrawRect(rc);
    dc.SetTextColor(0, 0, 0);
    dc.TextOutX(rc.Expand(4), "JANGO Launcher\nSDL2 port prototype\n\nPersonnel / 2.4G Equip / Equipment\nported from WINDBASE originals", ADJ_XYCENTER, false);
}


// ---------------------------------------------------------------------------
// PasswordDialog
// ---------------------------------------------------------------------------
PasswordDialog::PasswordDialog(DoneCallback onDone) : JKDialog("Password") {
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

void PasswordDialog::ClearPassword() {
    if (edit_) {
        edit_->SetText("");
    }
}


// ---------------------------------------------------------------------------
// BudaeDialog
// ---------------------------------------------------------------------------
BudaeDialog::BudaeDialog(DoneCallback onDone) : JKDialog("Change Unit") {
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

void BudaeDialog::SelectUnit(const std::string& current) {
    if (!list_) return;
    for (size_t i = 0; i < list_->GetCount(); ++i) {
        if (list_->GetString(i) == current) {
            list_->SetSelectedIndex(static_cast<int32_t>(i));
            return;
        }
    }
    list_->SetSelectedIndex(0);
}


// ---------------------------------------------------------------------------
// JangoUI
// ---------------------------------------------------------------------------
JangoUI::JangoUI(std::function<void()> onExit) : onExit_(std::move(onExit)) {
}

JangoUI::~JangoUI() = default;

void JangoUI::BuildMainWindow() {
    mainWindow_ = std::make_unique<JKWindow>("JANGO - Main Menu");
    mainWindow_->SetBackColor(COL_LTBLUE_R, COL_LTBLUE_G, COL_LTBLUE_B);
    mainWindow_->SetAttrFlags(WA_TITLEMOVEABLE);

    int32_t y = 50;
    const int32_t h = 105;

    AddMenuButton(ID_BTN_INSA,    "Personnel",    MakeRect(50, y, 300, y + 70)); y += h;
    AddMenuButton(ID_BTN_EQUIP24, "2.4G Equip",   MakeRect(50, y, 300, y + 70)); y += h;
    AddMenuButton(ID_BTN_EQUIP,   "Equipment",    MakeRect(50, y, 300, y + 70)); y += h;
    AddMenuButton(ID_BTN_FILE,    "File Select",  MakeRect(50, y, 300, y + 70)); y += h;
    AddMenuButton(ID_BTN_BUDAE,   "Change Unit",  MakeRect(50, y, 300, y + 70)); y += h;
    AddMenuButton(ID_BTN_EXIT,    "Exit",         MakeRect(50, y, 300, y + 70));

    // FHD(1920x1080) 클라이언트 영역(1916x1054)에서 우측 정보 영역을 채운다.
    auto about = std::make_unique<AboutPanel>(MakeRect(360, 50, 1896, 1030), 0);
    mainWindow_->AddControl(std::move(about));
}

std::unique_ptr<JKWindow> JangoUI::TakeMainWindow() {
    return std::move(mainWindow_);
}

void JangoUI::AddMenuButton(uint16_t id, const char* label, const JKRect& rect) {
    auto btn = std::make_unique<JKButton>(rect, id);
    btn->SetText(label);
    btn->SetBackColor(COL_LTGRAY_R, COL_LTGRAY_G, COL_LTGRAY_B);
    btn->SetTextColor(0, 0, 0);
    btn->SetDepth(2);
    btn->SetOnClick([this, id]() { OnMenuButton(id); });
    mainWindow_->AddControl(std::move(btn));
}

void JangoUI::CreateDialogs() {
    passwordDlg = std::make_unique<PasswordDialog>(
        [this](int r, const std::string& s) { OnPasswordDone(r, s); });

    budaeDlg = std::make_unique<BudaeDialog>(
        [this](int r, const std::string& s) { OnBudaeDone(r, s); });

    fileDialog = std::make_unique<JKFileDialog>("File Select");
    fileDialog->SetFilter("*.*");
    fileDialog->SetOnOk([this](const std::string& path) {
        ShowMessage("File Select", path);
    });
    fileDialog->SetOnCancel([]() {
        // no-op
    });
}

void JangoUI::ShowMessage(const std::string& title, const std::string& msg) {
    msgBox = std::make_unique<JKMessageBox>(title, msg, JKMessageBox::Buttons::Ok,
                                            [](int) { /* dismissed */ });
    msgBox->Show();
}

void JangoUI::OnMenuButton(uint16_t id) {
    switch (id) {
        case ID_BTN_INSA:
            if (passwordDlg) {
                passwordDlg->ClearPassword();
                passwordDlg->Show();
            }
            break;
        case ID_BTN_EQUIP24:
            if (!equip24Dlg) {
                equip24Dlg = std::make_unique<Equip24Dialog>(budaeName);
            }
            equip24Dlg->RefreshAll();
            equip24Dlg->Show();
            break;
        case ID_BTN_EQUIP:
            if (!equipDlg) {
                equipDlg = std::make_unique<EquipDialog>(budaeName);
            }
            equipDlg->RefreshAll();
            equipDlg->Show();
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
        case ID_BTN_EXIT:
            if (onExit_) {
                onExit_();
            }
            break;
        default:
            break;
    }
}

void JangoUI::OnPasswordDone(int result, const std::string& entered) {
    if (result != JKDialog::ResultOk) return;
    if (entered == StubPassword) {
        // 원본과 동일: 인사 관리는 비밀번호 확인 후 진입한다.
        if (!insaDlg) {
            insaDlg = std::make_unique<InsaDialog>(budaeName);
        }
        insaDlg->RefreshAll();
        insaDlg->Show();
    } else {
        ShowMessage("Error", "Incorrect password.");
    }
}

void JangoUI::OnBudaeDone(int result, const std::string& selected) {
    if (result == JKDialog::ResultOk && !selected.empty()) {
        budaeName = selected;
        ShowMessage("Change Unit", "Current unit: " + selected);
    }
}

} // namespace jk