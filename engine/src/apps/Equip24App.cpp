#include <apps/Equip24App.h>

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
constexpr uint8_t BLUE_R = 0, BLUE_G = 0, BLUE_B = 170;
constexpr uint8_t BROWN_R = 150, BROWN_G = 90, BROWN_B = 40;

const char* kDivisions[] = { "HQ", "1st Co", "2nd Co", "3rd Co" };
const size_t kDivisionCount = 4;

std::string IntToText(int v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", v);
    return buf;
}

} // namespace

// ---------------------------------------------------------------------------
// Equip24DataManager (원본 EQ24DMAN.CPP)
// ---------------------------------------------------------------------------
void Equip24DataManager::Load() {
    names.clear();
    kinds.clear();
    std::ifstream in(fileName);
    if (!in) {
        return;  // 첫 실행: 빈 데이터로 시작
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = apputil::Split(line);
        if (f[0] == "N" && f.size() >= 5) {
            Name24 n;
            n.division = f[1];
            n.attached = f[2];
            n.name = f[3];
            n.number = f[4];
            names.push_back(n);
        } else if (f[0] == "K" && f.size() >= 9) {
            Kind24 k;
            k.name = f[1];
            k.number = f[2];
            k.inUse = (f[3] == "1");
            k.a = apputil::ParseInt(f[4]);
            k.b = apputil::ParseInt(f[5]);
            k.c = apputil::ParseInt(f[6]);
            k.date = f[7];
            k.nameIndex = apputil::ParseInt(f[8], -1);
            kinds.push_back(k);
        }
    }
}

void Equip24DataManager::Save() const {
    std::ofstream out(fileName);
    for (const Name24& n : names) {
        out << "N|" << n.division << "|" << n.attached << "|"
            << n.name << "|" << n.number << "\n";
    }
    for (const Kind24& k : kinds) {
        out << "K|" << k.name << "|" << k.number << "|"
            << (k.inUse ? "1" : "0") << "|" << k.a << "|" << k.b << "|"
            << k.c << "|" << k.date << "|" << k.nameIndex << "\n";
    }
}

int Equip24DataManager::FindNameIndexByNumber(const std::string& number) const {
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i].number == number) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Equip24DataManager::AddRecord(const Name24& name, const Kind24& kind) {
    int nameIndex = FindNameIndexByNumber(name.number);
    if (nameIndex < 0) {
        names.push_back(name);
        nameIndex = static_cast<int>(names.size()) - 1;
    }
    Kind24 k = kind;
    k.nameIndex = nameIndex;
    kinds.push_back(k);
    return static_cast<int>(kinds.size()) - 1;
}

void Equip24DataManager::UpdateKind(size_t kindIndex, const Kind24& kind) {
    if (kindIndex >= kinds.size()) {
        return;
    }
    Kind24 k = kind;
    k.nameIndex = kinds[kindIndex].nameIndex;
    kinds[kindIndex] = k;
}

void Equip24DataManager::DeleteKind(size_t kindIndex) {
    if (kindIndex < kinds.size()) {
        kinds.erase(kinds.begin() + static_cast<long>(kindIndex));
    }
}

void Equip24DataManager::DeleteName(size_t nameIndex) {
    if (nameIndex >= names.size()) {
        return;
    }
    std::vector<Kind24> keep;
    for (const Kind24& k : kinds) {
        if (k.nameIndex != static_cast<int>(nameIndex)) {
            keep.push_back(k);
        }
    }
    kinds.swap(keep);
    names.erase(names.begin() + static_cast<long>(nameIndex));
    for (Kind24& k : kinds) {
        if (k.nameIndex > static_cast<int>(nameIndex)) {
            --k.nameIndex;
        }
    }
}

void Equip24DataManager::CountTotals(int& totalKinds, int& totalA,
                                     int& totalB, int& totalC) const {
    totalKinds = static_cast<int>(kinds.size());
    totalA = totalB = totalC = 0;
    for (const Kind24& k : kinds) {
        totalA += k.a;
        totalB += k.b;
        totalC += k.c;
    }
}

// ---------------------------------------------------------------------------
// 장비 입력 다이얼로그 (원본 EQ24IDLG.CPP Eqp24InputDialog 포팅)
// ---------------------------------------------------------------------------
namespace {

std::string IntToText(int v);

class Equip24InputDialog : public JKDialog {
public:
    using DoneFn = std::function<void(const Name24&, const Kind24&)>;

    Equip24InputDialog(bool modify, const Name24& nameBase, const Kind24& base,
                       DoneFn onDone)
        : JKDialog(modify ? "2.4G Equipment - Modify" : "2.4G Equipment - Add") {
        onDone_ = std::move(onDone);
        SetWindowRect(MakeRect(660, 220, 1260, 860));
        SetAttrFlags(WA_TITLEMOVEABLE);
        SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);
        modify_ = modify;
        baseName_ = nameBase.name;
        baseNumber_ = nameBase.number;

        auto addLabel = [&](int y, const char* text) {
            JKStatic* stc = new JKStatic(MakeRect(20, y, 120, y + 24), 0);
            stc->SetText(text);
            stc->SetAdjustFlag(ADJ_YCENTER);
            AddControl(std::unique_ptr<JKStatic>(stc));
        };

        addLabel(20, "Name");
        editName_ = new JKEdit(MakeRect(130, 20, 440, 46), 0, 18);
        editName_->SetText(nameBase.name);
        AddControl(std::unique_ptr<JKEdit>(editName_));

        addLabel(56, "Number");
        editNumber_ = new JKEdit(MakeRect(130, 56, 440, 82), 0, 18);
        editNumber_->SetText(nameBase.number);
        AddControl(std::unique_ptr<JKEdit>(editNumber_));

        addLabel(92, "Division");
        comboDivision_ = new JKComboBox(MakeRect(130, 92, 440, 118), 0);
        for (size_t i = 0; i < kDivisionCount; ++i) {
            comboDivision_->AddString(kDivisions[i]);
            if (kDivisions[i] == nameBase.division) {
                comboDivision_->SetSelectedIndex(static_cast<int>(i));
            }
        }
        if (comboDivision_->GetSelectedIndex() < 0) {
            comboDivision_->SetSelectedIndex(0);
        }
        AddControl(std::unique_ptr<JKComboBox>(comboDivision_));

        addLabel(128, "Status");
        comboInga_ = new JKComboBox(MakeRect(130, 128, 440, 154), 0);
        comboInga_->AddString("Use");
        comboInga_->AddString("Idle");
        comboInga_->SetSelectedIndex(base.inUse ? 0 : 1);
        AddControl(std::unique_ptr<JKComboBox>(comboInga_));

        addLabel(164, "Count A");
        editA_ = new JKEdit(MakeRect(130, 164, 440, 190), 0, 6);
        editA_->SetText(IntToText(base.a));
        AddControl(std::unique_ptr<JKEdit>(editA_));

        addLabel(200, "Count B");
        editB_ = new JKEdit(MakeRect(130, 200, 440, 226), 0, 6);
        editB_->SetText(IntToText(base.b));
        AddControl(std::unique_ptr<JKEdit>(editB_));

        addLabel(236, "Count C");
        editC_ = new JKEdit(MakeRect(130, 236, 440, 262), 0, 6);
        editC_->SetText(IntToText(base.c));
        AddControl(std::unique_ptr<JKEdit>(editC_));

        addLabel(272, "Date");
        editDate_ = new JKEdit(MakeRect(130, 272, 440, 298), 0, 10);
        editDate_->SetText(base.date.empty() ? apputil::TodayString() : base.date);
        AddControl(std::unique_ptr<JKEdit>(editDate_));

        if (modify) {
            // 원본에서는 수정 시 명칭/번호가 고정된다. (프로토타입은
            // 컨트롤 읽기전용 미지원으로 값 유지, 변경 시 Kind24에만 반영)
        }

        JKButton* okBtn = new JKButton(MakeRect(130, 330, 260, 366), 0);
        okBtn->SetText("OK");
        okBtn->SetOnClick([this]() { OnOk(); });
        AddControl(std::unique_ptr<JKButton>(okBtn));

        JKButton* cancelBtn = new JKButton(MakeRect(300, 330, 440, 366), 0);
        cancelBtn->SetText("Cancel");
        cancelBtn->SetOnClick([this]() { Close(ResultCancel); });
        AddControl(std::unique_ptr<JKButton>(cancelBtn));
    }

private:
    void OnOk() {
        const std::string& name = editName_->GetText();
        const std::string& number = editNumber_->GetText();
        if (name.empty() || number.empty()) {
            return;  // 필수 입력 누락: 다이얼로그를 닫지 않는다.
        }
        Name24 n;
        n.division = comboDivision_->GetSelectedString();
        n.name = name;
        n.number = number;

        Kind24 k;
        k.name = name;
        k.number = number;
        k.inUse = (comboInga_->GetSelectedIndex() == 0);
        k.a = apputil::ParseInt(editA_->GetText());
        k.b = apputil::ParseInt(editB_->GetText());
        k.c = apputil::ParseInt(editC_->GetText());
        k.date = editDate_->GetText();

        if (modify_) {
            n.name = baseName_;
            n.number = baseNumber_;
            k.name = baseName_;
            k.number = baseNumber_;
        }

        if (onDone_) {
            onDone_(n, k);
        }
        Close(ResultOk);
    }

    JKEdit* editName_ = nullptr;
    JKEdit* editNumber_ = nullptr;
    JKComboBox* comboDivision_ = nullptr;
    JKComboBox* comboInga_ = nullptr;
    JKEdit* editA_ = nullptr;
    JKEdit* editB_ = nullptr;
    JKEdit* editC_ = nullptr;
    JKEdit* editDate_ = nullptr;
    DoneFn onDone_;
    bool modify_ = false;
    std::string baseName_;
    std::string baseNumber_;
};

} // namespace

// ---------------------------------------------------------------------------
// Equip24Dialog (원본 EQP24WIN.CPP Equip24Window 포팅)
// ---------------------------------------------------------------------------
Equip24Dialog::Equip24Dialog(const std::string& budae)
    : JKDialog("2.4G Equipment - " + (budae.empty() ? std::string("HQ") : budae)
               + " - Copyright 2 C/A 1996 (SDL2 Port)"),
      budae_(budae.empty() ? std::string("HQ") : budae) {
    dataMan_.Load();
    SetWindowRect(MakeRect(60, 40, 1860, 1040));
    SetAttrFlags(WA_TITLEMOVEABLE);
    SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);
    BuildControls();
    RebuildNameList();
    RebuildKindList();
}

Equip24Dialog::~Equip24Dialog() {
    dataMan_.Save();  // 원본 ~Equip24Window의 WriteConfig에 해당
}

void Equip24Dialog::BuildControls() {
    JKStatic* nameHead = new JKStatic(MakeRect(10, 10, 300, 34), 0);
    nameHead->SetText("  Equipment Register");
    nameHead->SetBackColor(BROWN_R, BROWN_G, BROWN_B);
    nameHead->SetTextColor(255, 255, 255);
    nameHead->SetAdjustFlag(ADJ_YCENTER);
    AddControl(std::unique_ptr<JKStatic>(nameHead));

    JKStatic* colHead = new JKStatic(MakeRect(310, 10, 1780, 34), 0);
    colHead->SetText("  Name              Number        Use    A     B     C     Date");
    colHead->SetBackColor(BROWN_R, BROWN_G, BROWN_B);
    colHead->SetTextColor(255, 255, 255);
    colHead->SetAdjustFlag(ADJ_YCENTER);
    AddControl(std::unique_ptr<JKStatic>(colHead));

    nameList_ = new JKListBox(MakeRect(10, 40, 300, 880), 0);
    nameList_->SetBackColor(255, 255, 255);
    nameList_->SetTextColor(0, 0, 0);
    nameList_->SetOnSelect([this](int32_t) { RebuildKindList(); });
    AddControl(std::unique_ptr<JKListBox>(nameList_));

    kindList_ = new JKListBox(MakeRect(310, 40, 1780, 880), 0);
    kindList_->SetBackColor(255, 255, 255);
    kindList_->SetTextColor(0, 0, 0);
    AddControl(std::unique_ptr<JKListBox>(kindList_));

    auto addBtn = [&](int x1, int y1, int x2, int y2, const char* label,
                      std::function<void()> fn) {
        JKButton* btn = new JKButton(MakeRect(x1, y1, x2, y2), 0);
        btn->SetText(label);
        btn->SetOnClick(std::move(fn));
        AddControl(std::unique_ptr<JKButton>(btn));
    };

    addBtn(310, 895, 440, 935, "Add", [this]() { ShowInputDialog(false); });
    addBtn(450, 895, 580, 935, "Modify", [this]() { ShowInputDialog(true); });
    addBtn(590, 895, 720, 935, "Delete", [this]() { ConfirmDelete(); });
    addBtn(730, 895, 860, 935, "Totals", [this]() { ShowTotals(); });
    addBtn(870, 895, 1000, 935, "Save", [this]() {
        dataMan_.Save();
        ShowMessageModal("2.4G Equipment", "Saved to " + dataMan_.fileName);
    });
    addBtn(1650, 895, 1780, 935, "Close",
           [this]() { dataMan_.Save(); Close(ResultOk); });

    statusLine_ = new JKStatic(MakeRect(10, 945, 1780, 970), 0);
    statusLine_->SetAdjustFlag(ADJ_YCENTER);
    AddControl(std::unique_ptr<JKStatic>(statusLine_));
    UpdateStatusLine();
}

void Equip24Dialog::RebuildNameList() {
    int prevSel = nameList_->GetSelectedIndex();
    std::string prev =
        (prevSel > 0) ? nameList_->GetString(static_cast<size_t>(prevSel)) : std::string();
    nameList_->Clear();
    nameList_->AddString("<All>");
    for (const Name24& n : dataMan_.names) {
        nameList_->AddString(n.name);
    }
    int restore = 0;
    if (!prev.empty()) {
        for (size_t i = 0; i < dataMan_.names.size(); ++i) {
            if (dataMan_.names[i].name == prev) {
                restore = static_cast<int>(i) + 1;
                break;
            }
        }
    }
    nameList_->SetSelectedIndex(restore);
}

void Equip24Dialog::RebuildKindList() {
    kindRows_.clear();
    kindList_->Clear();
    const int sel = nameList_->GetSelectedIndex();
    const bool all = (sel <= 0);  // 0 == <All>
    for (size_t i = 0; i < dataMan_.kinds.size(); ++i) {
        const Kind24& k = dataMan_.kinds[i];
        if (!all && k.nameIndex != sel - 1) {
            continue;
        }
        std::string row = apputil::PadRight(k.name, 18) +
                          apputil::PadRight(k.number, 14) +
                          apputil::PadRight(k.inUse ? "Use" : "Idle", 6) +
                          apputil::PadRight(IntToText(k.a), 6) +
                          apputil::PadRight(IntToText(k.b), 6) +
                          apputil::PadRight(IntToText(k.c), 6) + k.date;
        kindList_->AddString(row);
        kindRows_.push_back(i);
    }
    UpdateStatusLine();
}

void Equip24Dialog::RefreshAll() {
    RebuildNameList();
    RebuildKindList();
}

void Equip24Dialog::ShowInputDialog(bool modify) {
    Kind24 base;
    Name24 nameBase;
    int kindIdx = -1;
    if (modify) {
        int sel = kindList_->GetSelectedIndex();
        if (sel < 0 || static_cast<size_t>(sel) >= kindRows_.size()) {
            ShowMessageModal("2.4G Equipment", "Select a record first.");
            return;
        }
        kindIdx = static_cast<int>(kindRows_[static_cast<size_t>(sel)]);
        base = dataMan_.kinds[static_cast<size_t>(kindIdx)];
        if (base.nameIndex >= 0 &&
            base.nameIndex < static_cast<int>(dataMan_.names.size())) {
            nameBase = dataMan_.names[static_cast<size_t>(base.nameIndex)];
        }
        if (nameBase.name.empty()) {
            nameBase.name = base.name;
            nameBase.number = base.number;
        }
    }

    inputDlg_ = std::make_unique<Equip24InputDialog>(
        modify, nameBase, base,
        [this, kindIdx](const Name24& n, const Kind24& k) {
            if (kindIdx < 0) {
                dataMan_.AddRecord(n, k);
            } else {
                dataMan_.UpdateKind(static_cast<size_t>(kindIdx), k);
            }
        });
    inputDlg_->SetOnClose([this](int) {
        dataMan_.Save();
        if (g_currentJKApp) {
            g_currentJKApp->SetModalWindow(this);
        }
        RefreshAll();
    });
    inputDlg_->Show();
}

void Equip24Dialog::ShowTotals() {
    int n = 0, a = 0, b = 0, c = 0;
    dataMan_.CountTotals(n, a, b, c);
    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "Records: %d\nTotal A: %d   Total B: %d   Total C: %d",
                  n, a, b, c);
    ShowMessageModal("2.4G Equipment Totals", buf);
}

void Equip24Dialog::ConfirmDelete() {
    int sel = kindList_->GetSelectedIndex();
    if (sel < 0 || static_cast<size_t>(sel) >= kindRows_.size()) {
        ShowMessageModal("2.4G Equipment", "Select a record first.");
        return;
    }
    ShowMessageModal("2.4G Equipment", "Delete selected record?",
                     JKMessageBox::Buttons::YesNo, [this](int result) {
                         if (result == JKMessageBox::ResultYes) {
                             int row = kindList_->GetSelectedIndex();
                             if (row >= 0 &&
                                 static_cast<size_t>(row) < kindRows_.size()) {
                                 dataMan_.DeleteKind(
                                     kindRows_[static_cast<size_t>(row)]);
                                 dataMan_.Save();
                             }
                         }
                         RefreshAll();
                     });
}

void Equip24Dialog::UpdateStatusLine() {
    if (!statusLine_) {
        return;
    }
    statusLine_->SetText("Records: " +
                         IntToText(static_cast<int>(dataMan_.kinds.size())) +
                         "   File: " + dataMan_.fileName + "   Unit: " + budae_);
}

void Equip24Dialog::ShowMessageModal(const std::string& title, const std::string& msg,
                                     JKMessageBox::Buttons buttons,
                                     const std::function<void(int)>& onResult) {
    apputil::ShowModalMessage(this, msgBox_, title, msg, buttons, onResult);
}

} // namespace jk