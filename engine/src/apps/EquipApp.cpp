#include <apps/EquipApp.h>

#include <JKApplication.h>
#include <JKButton.h>
#include <JKComboBox.h>
#include <JKEdit.h>
#include <JKListBox.h>
#include <JKMessageBox.h>
#include <JKStatic.h>
#include <JKTypes.h>
#include <JKWindow.h>

#include <apps/AppUtil.h>

#include <cstdio>
#include <fstream>

namespace jk {
namespace {

constexpr uint8_t LTGRAY_R = 192, LTGRAY_G = 192, LTGRAY_B = 192;
constexpr uint8_t BROWN_R = 150, BROWN_G = 90, BROWN_B = 40;

// 원본 EQUIPWIN.H의 Kind[16] 탄약 종류.
const char* kAmmoKinds[] = {
    "HE", "WP", "ICM", "DP-ICM", "ILL", "HEAT", "RAP", "RA-FAS",
    "5.56MM", "7.62MM", "40MM", "60MM", "81MM", "105MM", "ATM", "ETC",
};
const size_t kAmmoKindCount = 16;

// 원본 EQUIPWIN.H의 Name[5] 부대 명칭 (본부/1/2/3중대).
const char* kUnitNames[] = { "HQ", "1st Co", "2nd Co", "3rd Co" };
const size_t kUnitCount = 4;

std::string IntToText(int v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", v);
    return buf;
}

} // namespace

// ---------------------------------------------------------------------------
// BombManager (원본 BOMBMAN.CPP)
// ---------------------------------------------------------------------------
void BombManager::Load() {
    stocks.clear();
    std::ifstream in(fileName);
    if (!in) {
        return;  // 첫 실행: 빈 데이터
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = apputil::Split(line);
        if (f[0] == "B" && f.size() >= 6) {
            BombStock s;
            s.text = f[1];
            for (size_t i = 0; i < kUnitCount && i + 2 < f.size(); ++i) {
                s.counts[i] = apputil::ParseInt(f[i + 2]);
            }
            stocks.push_back(s);
        }
    }
}

void BombManager::Save() const {
    std::ofstream out(fileName);
    for (const BombStock& s : stocks) {
        out << "B|" << s.text;
        for (int i = 0; i < 4; ++i) {
            out << "|" << s.counts[i];
        }
        out << "\n";
    }
}

int BombManager::FindIndexByText(const std::string& text) const {
    for (size_t i = 0; i < stocks.size(); ++i) {
        if (stocks[i].text == text) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void BombManager::AddRecord(const BombStock& rec) {
    stocks.push_back(rec);
}

void BombManager::UpdateRecord(size_t index, const BombStock& rec) {
    if (index < stocks.size()) {
        stocks[index] = rec;
    }
}

void BombManager::DeleteRecord(size_t index) {
    if (index < stocks.size()) {
        stocks.erase(stocks.begin() + static_cast<long>(index));
    }
}

void BombManager::UnitTotals(int out[4]) const {
    for (int i = 0; i < 4; ++i) {
        out[i] = 0;
    }
    for (const BombStock& s : stocks) {
        for (int i = 0; i < 4; ++i) {
            out[i] += s.counts[i];
        }
    }
}

// ---------------------------------------------------------------------------
// 재고 입력 다이얼로그 (원본 EQUIPWIN의 입력 윈도우 포팅)
// ---------------------------------------------------------------------------
namespace {

class EquipInputDialog : public JKDialog {
public:
    using DoneFn = std::function<void(const BombStock&)>;

    EquipInputDialog(bool modify, const BombStock& base, DoneFn onDone)
        : JKDialog(modify ? "Equipment - Modify Stock" : "Equipment - Add Stock") {
        onDone_ = std::move(onDone);
        modify_ = modify;
        baseText_ = base.text;
        SetWindowRect(MakeRect(660, 300, 1260, 780));
        SetAttrFlags(WA_TITLEMOVEABLE);
        SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);

        auto addLabel = [&](int y, const char* text) {
            JKStatic* stc = new JKStatic(MakeRect(20, y, 120, y + 24), 0);
            stc->SetText(text);
            stc->SetAdjustFlag(ADJ_YCENTER);
            AddControl(std::unique_ptr<JKStatic>(stc));
        };

        addLabel(20, "Kind");
        comboKind_ = new JKComboBox(MakeRect(130, 20, 440, 46), 0);
        int preselect = 0;
        for (size_t i = 0; i < kAmmoKindCount; ++i) {
            comboKind_->AddString(kAmmoKinds[i]);
            if (kAmmoKinds[i] == base.text) {
                preselect = static_cast<int>(i);
            }
        }
        comboKind_->SetSelectedIndex(preselect);
        AddControl(std::unique_ptr<JKComboBox>(comboKind_));

        for (size_t u = 0; u < kUnitCount; ++u) {
            const int y = 56 + static_cast<int>(u) * 36;
            addLabel(y, kUnitNames[u]);
            editCount_[u] = new JKEdit(MakeRect(130, y, 440, y + 26), 0, 6);
            editCount_[u]->SetText(IntToText(base.counts[u]));
            AddControl(std::unique_ptr<JKEdit>(editCount_[u]));
        }

        if (modify) {
            // 종류는 유지(프로토타입: 읽기전용 미지원 → OK 시 기존 종류로 되돌림)
        }

        JKButton* okBtn = new JKButton(MakeRect(130, 210, 260, 246), 0);
        okBtn->SetText("OK");
        okBtn->SetOnClick([this]() { OnOk(); });
        AddControl(std::unique_ptr<JKButton>(okBtn));

        JKButton* cancelBtn = new JKButton(MakeRect(300, 210, 440, 246), 0);
        cancelBtn->SetText("Cancel");
        cancelBtn->SetOnClick([this]() { Close(ResultCancel); });
        AddControl(std::unique_ptr<JKButton>(cancelBtn));
    }

private:
    void OnOk() {
        BombStock s;
        s.text = modify_ ? baseText_ : comboKind_->GetSelectedString();
        if (s.text.empty()) {
            return;
        }
        for (size_t u = 0; u < kUnitCount; ++u) {
            s.counts[u] = apputil::ParseInt(editCount_[u]->GetText());
        }
        if (onDone_) {
            onDone_(s);
        }
        Close(ResultOk);
    }

    JKComboBox* comboKind_ = nullptr;
    JKEdit* editCount_[4] = { nullptr, nullptr, nullptr, nullptr };
    DoneFn onDone_;
    bool modify_ = false;
    std::string baseText_;
};

} // namespace

// ---------------------------------------------------------------------------
// EquipDialog (원본 EQUIPWIN.CPP EquipWindow 포팅)
// ---------------------------------------------------------------------------
EquipDialog::EquipDialog(const std::string& budae)
    : JKDialog("Equipment - " + (budae.empty() ? std::string("HQ") : budae)
               + " - Copyright 2 C/A 1996 (SDL2 Port)"),
      budae_(budae.empty() ? std::string("HQ") : budae) {
    bombMan_.Load();
    SetWindowRect(MakeRect(240, 120, 1680, 960));
    SetAttrFlags(WA_TITLEMOVEABLE);
    SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);
    BuildControls();
    RefreshAll();
}

EquipDialog::~EquipDialog() {
    bombMan_.Save();  // 원본 EquipWindow 종료 시 SaveBomb에 해당
}

void EquipDialog::BuildControls() {
    JKStatic* colHead = new JKStatic(MakeRect(10, 10, 1426, 34), 0);
    colHead->SetText("  Kind             HQ        1st Co    2nd Co    3rd Co");
    colHead->SetBackColor(BROWN_R, BROWN_G, BROWN_B);
    colHead->SetTextColor(255, 255, 255);
    colHead->SetAdjustFlag(ADJ_YCENTER);
    AddControl(std::unique_ptr<JKStatic>(colHead));

    listBox_ = new JKListBox(MakeRect(10, 40, 1426, 700), 0);
    listBox_->SetBackColor(255, 255, 255);
    listBox_->SetTextColor(0, 0, 0);
    AddControl(std::unique_ptr<JKListBox>(listBox_));

    auto addBtn = [&](int x1, int x2, const char* label, std::function<void()> fn) {
        JKButton* btn = new JKButton(MakeRect(x1, 715, x2, 755), 0);
        btn->SetText(label);
        btn->SetOnClick(std::move(fn));
        AddControl(std::unique_ptr<JKButton>(btn));
    };

    addBtn(10, 140, "Add", [this]() { ShowInputDialog(false); });
    addBtn(150, 280, "Modify", [this]() { ShowInputDialog(true); });
    addBtn(290, 420, "Delete", [this]() { ConfirmDelete(); });
    addBtn(430, 560, "Save", [this]() {
        bombMan_.Save();
        ShowMessageModal("Equipment", "Saved to " + bombMan_.fileName);
    });
    addBtn(570, 700, "Total", [this]() { ShowTotals(); });
    addBtn(1290, 1426, "Close", [this]() { bombMan_.Save(); Close(ResultOk); });

    statusLine_ = new JKStatic(MakeRect(10, 765, 1426, 795), 0);
    statusLine_->SetAdjustFlag(ADJ_YCENTER);
    AddControl(std::unique_ptr<JKStatic>(statusLine_));
    UpdateStatusLine();
}

void EquipDialog::RefreshAll() {
    listBox_->Clear();
    for (const BombStock& s : bombMan_.stocks) {
        std::string row = apputil::PadRight(s.text, 16) +
                          apputil::PadRight(IntToText(s.counts[0]), 10) +
                          apputil::PadRight(IntToText(s.counts[1]), 10) +
                          apputil::PadRight(IntToText(s.counts[2]), 10) +
                          IntToText(s.counts[3]);
        listBox_->AddString(row);
    }
    UpdateStatusLine();
}

void EquipDialog::ShowInputDialog(bool modify) {
    BombStock base;
    int idx = -1;
    if (modify) {
        int sel = listBox_->GetSelectedIndex();
        if (sel < 0 || static_cast<size_t>(sel) >= bombMan_.stocks.size()) {
            ShowMessageModal("Equipment", "Select a record first.");
            return;
        }
        idx = sel;
        base = bombMan_.stocks[static_cast<size_t>(idx)];
    } else {
        // 추가 시 사용하지 않는 종류를 기본 선택한다.
        for (size_t i = 0; i < kAmmoKindCount; ++i) {
            if (bombMan_.FindIndexByText(kAmmoKinds[i]) < 0) {
                base.text = kAmmoKinds[i];
                break;
            }
        }
    }

    inputDlg_ = std::make_unique<EquipInputDialog>(
        modify, base, [this, idx](const BombStock& s) {
            if (idx < 0) {
                bombMan_.AddRecord(s);
            } else {
                bombMan_.UpdateRecord(static_cast<size_t>(idx), s);
            }
        });
    inputDlg_->SetOnClose([this](int) {
        bombMan_.Save();
        if (g_currentJKApp) {
            g_currentJKApp->SetModalWindow(this);
        }
        RefreshAll();
    });
    inputDlg_->Show();
}

void EquipDialog::ShowTotals() {
    int totals[4] = { 0, 0, 0, 0 };
    bombMan_.UnitTotals(totals);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "Kinds: %d\nHQ: %d   1st Co: %d   2nd Co: %d   3rd Co: %d",
                  static_cast<int>(bombMan_.stocks.size()), totals[0], totals[1],
                  totals[2], totals[3]);
    ShowMessageModal("Equipment Totals", buf);
}

void EquipDialog::ConfirmDelete() {
    int sel = listBox_->GetSelectedIndex();
    if (sel < 0 || static_cast<size_t>(sel) >= bombMan_.stocks.size()) {
        ShowMessageModal("Equipment", "Select a record first.");
        return;
    }
    ShowMessageModal("Equipment", "Delete selected record?",
                     JKMessageBox::Buttons::YesNo, [this](int result) {
                         if (result == JKMessageBox::ResultYes) {
                             int row = listBox_->GetSelectedIndex();
                             if (row >= 0) {
                                 bombMan_.DeleteRecord(static_cast<size_t>(row));
                                 bombMan_.Save();
                             }
                         }
                         RefreshAll();
                     });
}

void EquipDialog::UpdateStatusLine() {
    if (!statusLine_) {
        return;
    }
    statusLine_->SetText("Kinds: " +
                         IntToText(static_cast<int>(bombMan_.stocks.size())) +
                         "   File: " + bombMan_.fileName + "   Unit: " + budae_);
}

void EquipDialog::ShowMessageModal(const std::string& title, const std::string& msg,
                                   JKMessageBox::Buttons buttons,
                                   const std::function<void(int)>& onResult) {
    apputil::ShowModalMessage(this, msgBox_, title, msg, buttons, onResult);
}

} // namespace jk