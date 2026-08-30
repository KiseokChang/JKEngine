#include <apps/InsaApp.h>

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

// 원본 PERSNREC.H의 GetBudae/IsGanbu/IsHasa/IsSabyong 분류에 대응.
const char* kRanks[] = { "Officer", "NCO", "Enlisted" };
const size_t kRankCount = 3;

std::string IntToText(int v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", v);
    return buf;
}

std::string ToLower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// PersonManager (원본 PERSNMAN.CPP)
// ---------------------------------------------------------------------------
void PersonManager::Load() {
    persons.clear();
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
        if (f[0] == "P" && f.size() >= 8) {
            PersonRec p;
            p.name = f[1];
            p.rank = f[2];
            p.serial = f[3];
            p.unit = f[4];
            p.birth = f[5];
            p.enlist = f[6];
            p.specialty = f[7];
            persons.push_back(p);
        }
    }
}

void PersonManager::Save() const {
    std::ofstream out(fileName);
    for (const PersonRec& p : persons) {
        out << "P|" << p.name << "|" << p.rank << "|" << p.serial << "|"
            << p.unit << "|" << p.birth << "|" << p.enlist << "|"
            << p.specialty << "\n";
    }
}

int PersonManager::FindIndexByName(const std::string& name) const {
    const std::string key = ToLower(name);
    for (size_t i = 0; i < persons.size(); ++i) {
        if (ToLower(persons[i].name).find(key) != std::string::npos) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int PersonManager::FindIndexBySerial(const std::string& serial) const {
    for (size_t i = 0; i < persons.size(); ++i) {
        if (persons[i].serial == serial) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void PersonManager::AddRecord(const PersonRec& rec) {
    persons.push_back(rec);
}

void PersonManager::UpdateRecord(size_t index, const PersonRec& rec) {
    if (index < persons.size()) {
        persons[index] = rec;
    }
}

void PersonManager::DeleteRecord(size_t index) {
    if (index < persons.size()) {
        persons.erase(persons.begin() + static_cast<long>(index));
    }
}

void PersonManager::RankCounts(int& officers, int& ncos, int& enlisted) const {
    officers = ncos = enlisted = 0;
    for (const PersonRec& p : persons) {
        if (p.rank == "Officer") {
            ++officers;
        } else if (p.rank == "NCO") {
            ++ncos;
        } else {
            ++enlisted;
        }
    }
}

// ---------------------------------------------------------------------------
// 인원 입력/검색 다이얼로그 (원본 IN* 다이얼로그군 포팅)
// ---------------------------------------------------------------------------
namespace {

class InsaInputDialog : public JKDialog {
public:
    using DoneFn = std::function<void(const PersonRec&)>;

    InsaInputDialog(bool modify, const PersonRec& base, DoneFn onDone)
        : JKDialog(modify ? "Personnel - Modify" : "Personnel - Add") {
        onDone_ = std::move(onDone);
        modify_ = modify;
        baseSerial_ = base.serial;
        SetWindowRect(MakeRect(560, 160, 1360, 860));
        SetAttrFlags(WA_TITLEMOVEABLE);
        SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);

        auto addLabel = [&](int y, const char* text) {
            JKStatic* stc = new JKStatic(MakeRect(20, y, 130, y + 24), 0);
            stc->SetText(text);
            stc->SetAdjustFlag(ADJ_YCENTER);
            AddControl(std::unique_ptr<JKStatic>(stc));
        };
        auto addEdit = [&](int y, size_t maxLen, JKEdit** slot) {
            *slot = new JKEdit(MakeRect(140, y, 640, y + 26), 0, maxLen);
            AddControl(std::unique_ptr<JKEdit>(*slot));
        };

        addLabel(20, "Name");
        addEdit(20, 20, &editName_);
        editName_->SetText(base.name);

        addLabel(56, "Rank");
        comboRank_ = new JKComboBox(MakeRect(140, 56, 640, 82), 0);
        int preselect = 0;
        for (size_t i = 0; i < kRankCount; ++i) {
            comboRank_->AddString(kRanks[i]);
            if (kRanks[i] == base.rank) {
                preselect = static_cast<int>(i);
            }
        }
        comboRank_->SetSelectedIndex(preselect);
        AddControl(std::unique_ptr<JKComboBox>(comboRank_));

        addLabel(92, "Serial No");
        addEdit(92, 16, &editSerial_);
        editSerial_->SetText(base.serial);

        addLabel(128, "Unit");
        addEdit(128, 20, &editUnit_);
        editUnit_->SetText(base.unit.empty() ? std::string("HQ") : base.unit);

        addLabel(164, "Birth");
        addEdit(164, 10, &editBirth_);
        editBirth_->SetText(base.birth);

        addLabel(200, "Enlist");
        addEdit(200, 10, &editEnlist_);
        editEnlist_->SetText(base.enlist);

        addLabel(236, "Specialty");
        addEdit(236, 16, &editSpecialty_);
        editSpecialty_->SetText(base.specialty);

        if (modify) {
            // 군번 번호는 원본과 동일하게 고정된다(수정 시 OK에서 기존 값 유지).
        }

        JKButton* okBtn = new JKButton(MakeRect(200, 290, 330, 326), 0);
        okBtn->SetText("OK");
        okBtn->SetOnClick([this]() { OnOk(); });
        AddControl(std::unique_ptr<JKButton>(okBtn));

        JKButton* cancelBtn = new JKButton(MakeRect(360, 290, 490, 326), 0);
        cancelBtn->SetText("Cancel");
        cancelBtn->SetOnClick([this]() { Close(ResultCancel); });
        AddControl(std::unique_ptr<JKButton>(cancelBtn));
    }

private:
    void OnOk() {
        if (editName_->GetText().empty() || editSerial_->GetText().empty()) {
            return;  // 성명/군번 번호 필수
        }
        PersonRec p;
        p.name = editName_->GetText();
        p.rank = comboRank_->GetSelectedString();
        p.serial = modify_ ? baseSerial_ : editSerial_->GetText();
        p.unit = editUnit_->GetText();
        p.birth = editBirth_->GetText();
        p.enlist = editEnlist_->GetText();
        p.specialty = editSpecialty_->GetText();
        if (onDone_) {
            onDone_(p);
        }
        Close(ResultOk);
    }

    JKEdit* editName_ = nullptr;
    JKComboBox* comboRank_ = nullptr;
    JKEdit* editSerial_ = nullptr;
    JKEdit* editUnit_ = nullptr;
    JKEdit* editBirth_ = nullptr;
    JKEdit* editEnlist_ = nullptr;
    JKEdit* editSpecialty_ = nullptr;
    DoneFn onDone_;
    bool modify_ = false;
    std::string baseSerial_;
};

class InsaSearchDialog : public JKDialog {
public:
    using DoneFn = std::function<void(const std::string&)>;

    explicit InsaSearchDialog(DoneFn onDone)
        : JKDialog("Personnel - Search") {
        onDone_ = std::move(onDone);
        SetWindowRect(MakeRect(760, 420, 1160, 580));
        SetAttrFlags(WA_TITLEMOVEABLE);
        SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);

        JKStatic* stc = new JKStatic(MakeRect(10, 10, 90, 36), 0);
        stc->SetText("Name");
        stc->SetAdjustFlag(ADJ_YCENTER);
        AddControl(std::unique_ptr<JKStatic>(stc));

        editName_ = new JKEdit(MakeRect(100, 10, 380, 36), 0, 20);
        AddControl(std::unique_ptr<JKEdit>(editName_));

        JKButton* okBtn = new JKButton(MakeRect(60, 60, 180, 96), 0);
        okBtn->SetText("OK");
        okBtn->SetOnClick([this]() {
            if (onDone_) {
                onDone_(editName_->GetText());
            }
            Close(ResultOk);
        });
        AddControl(std::unique_ptr<JKButton>(okBtn));

        JKButton* cancelBtn = new JKButton(MakeRect(210, 60, 300, 96), 0);
        cancelBtn->SetText("Cancel");
        cancelBtn->SetOnClick([this]() { Close(ResultCancel); });
        AddControl(std::unique_ptr<JKButton>(cancelBtn));
    }

private:
    JKEdit* editName_ = nullptr;
    DoneFn onDone_;
};

} // namespace

// ---------------------------------------------------------------------------
// InsaDialog (원본 INSAWIN.CPP InsaWindow 포팅)
// ---------------------------------------------------------------------------
InsaDialog::InsaDialog(const std::string& budae)
    : JKDialog("Personnel Management - " + (budae.empty() ? std::string("HQ") : budae)
               + " - Copyright 2 C/A 1996 (SDL2 Port)"),
      budae_(budae.empty() ? std::string("HQ") : budae) {
    personMan_.Load();
    SetWindowRect(MakeRect(160, 80, 1760, 1000));
    SetAttrFlags(WA_TITLEMOVEABLE);
    SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);
    BuildControls();
    RefreshAll();
}

InsaDialog::~InsaDialog() {
    personMan_.Save();  // 원본 InsaWindow 종료 시 저장에 해당
}

void InsaDialog::BuildControls() {
    JKStatic* colHead = new JKStatic(MakeRect(10, 10, 1586, 34), 0);
    colHead->SetText("  Name            Rank      Serial        Unit            Birth       Enlist      Specialty");
    colHead->SetBackColor(BROWN_R, BROWN_G, BROWN_B);
    colHead->SetTextColor(255, 255, 255);
    colHead->SetAdjustFlag(ADJ_YCENTER);
    AddControl(std::unique_ptr<JKStatic>(colHead));

    listBox_ = new JKListBox(MakeRect(10, 40, 1586, 760), 0);
    listBox_->SetBackColor(255, 255, 255);
    listBox_->SetTextColor(0, 0, 0);
    AddControl(std::unique_ptr<JKListBox>(listBox_));

    auto addBtn = [&](int x1, int x2, const char* label, std::function<void()> fn) {
        JKButton* btn = new JKButton(MakeRect(x1, 775, x2, 815), 0);
        btn->SetText(label);
        btn->SetOnClick(std::move(fn));
        AddControl(std::unique_ptr<JKButton>(btn));
    };

    addBtn(10, 140, "Add", [this]() { ShowInputDialog(false); });
    addBtn(150, 280, "Modify", [this]() { ShowInputDialog(true); });
    addBtn(290, 420, "Delete", [this]() { ConfirmDelete(); });
    addBtn(430, 560, "Search", [this]() { ShowSearchDialog(); });
    addBtn(570, 700, "Totals", [this]() { ShowTotals(); });
    addBtn(710, 840, "Save", [this]() {
        personMan_.Save();
        ShowMessageModal("Personnel", "Saved to " + personMan_.fileName);
    });
    addBtn(1450, 1586, "Close",
           [this]() { personMan_.Save(); Close(ResultOk); });

    statusLine_ = new JKStatic(MakeRect(10, 830, 1586, 860), 0);
    statusLine_->SetAdjustFlag(ADJ_YCENTER);
    AddControl(std::unique_ptr<JKStatic>(statusLine_));
    UpdateStatusLine();
}

void InsaDialog::RebuildList() {
    rows_.clear();
    listBox_->Clear();
    for (size_t i = 0; i < personMan_.persons.size(); ++i) {
        const PersonRec& p = personMan_.persons[i];
        std::string row = apputil::PadRight(p.name, 16) +
                          apputil::PadRight(p.rank, 10) +
                          apputil::PadRight(p.serial, 14) +
                          apputil::PadRight(p.unit, 16) +
                          apputil::PadRight(p.birth, 12) +
                          apputil::PadRight(p.enlist, 12) + p.specialty;
        listBox_->AddString(row);
        rows_.push_back(i);
    }
    UpdateStatusLine();
}

void InsaDialog::RefreshAll() {
    RebuildList();
}

void InsaDialog::ShowInputDialog(bool modify) {
    PersonRec base;
    int idx = -1;
    if (modify) {
        int sel = listBox_->GetSelectedIndex();
        if (sel < 0 || static_cast<size_t>(sel) >= rows_.size()) {
            ShowMessageModal("Personnel", "Select a record first.");
            return;
        }
        idx = static_cast<int>(rows_[static_cast<size_t>(sel)]);
        base = personMan_.persons[static_cast<size_t>(idx)];
    }

    inputDlg_ = std::make_unique<InsaInputDialog>(
        modify, base, [this, idx](const PersonRec& p) {
            if (idx < 0) {
                if (personMan_.FindIndexBySerial(p.serial) >= 0) {
                    ShowMessageModal("Personnel",
                                     "Serial number already exists.");
                    return;
                }
                personMan_.AddRecord(p);
            } else {
                personMan_.UpdateRecord(static_cast<size_t>(idx), p);
            }
        });
    inputDlg_->SetOnClose([this](int) {
        personMan_.Save();
        if (g_currentJKApp) {
            g_currentJKApp->SetModalWindow(this);
        }
        RebuildList();
    });
    inputDlg_->Show();
}

void InsaDialog::ShowSearchDialog() {
    inputDlg_ = std::make_unique<InsaSearchDialog>(
        [this](const std::string& name) {
            if (name.empty()) {
                return;
            }
            const int idx = personMan_.FindIndexByName(name);
            if (idx < 0) {
                ShowMessageModal("Personnel", "Not found: " + name);
                return;
            }
            for (size_t row = 0; row < rows_.size(); ++row) {
                if (rows_[row] == static_cast<size_t>(idx)) {
                    listBox_->SetSelectedIndex(static_cast<int32_t>(row));
                    break;
                }
            }
        });
    inputDlg_->SetOnClose([this](int) {
        if (g_currentJKApp) {
            g_currentJKApp->SetModalWindow(this);
        }
    });
    inputDlg_->Show();
}

void InsaDialog::ShowTotals() {
    int officers = 0, ncos = 0, enlisted = 0;
    personMan_.RankCounts(officers, ncos, enlisted);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "Total: %d\nOfficer: %d   NCO: %d   Enlisted: %d",
                  static_cast<int>(personMan_.persons.size()), officers, ncos,
                  enlisted);
    ShowMessageModal("Personnel Totals", buf);
}

void InsaDialog::ConfirmDelete() {
    int sel = listBox_->GetSelectedIndex();
    if (sel < 0 || static_cast<size_t>(sel) >= rows_.size()) {
        ShowMessageModal("Personnel", "Select a record first.");
        return;
    }
    ShowMessageModal("Personnel", "Delete selected person?",
                     JKMessageBox::Buttons::YesNo, [this](int result) {
                         if (result == JKMessageBox::ResultYes) {
                             int row = listBox_->GetSelectedIndex();
                             if (row >= 0 &&
                                 static_cast<size_t>(row) < rows_.size()) {
                                 personMan_.DeleteRecord(
                                     rows_[static_cast<size_t>(row)]);
                                 personMan_.Save();
                             }
                         }
                         RebuildList();
                     });
}

void InsaDialog::UpdateStatusLine() {
    if (!statusLine_) {
        return;
    }
    statusLine_->SetText("Persons: " +
                         IntToText(static_cast<int>(personMan_.persons.size())) +
                         "   File: " + personMan_.fileName + "   Unit: " + budae_);
}

void InsaDialog::ShowMessageModal(const std::string& title, const std::string& msg,
                                  JKMessageBox::Buttons buttons,
                                  const std::function<void(int)>& onResult) {
    apputil::ShowModalMessage(this, msgBox_, title, msg, buttons, onResult);
}

} // namespace jk