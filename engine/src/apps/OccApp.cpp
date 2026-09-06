#include <apps/OccApp.h>

#include <JKButton.h>
#include <JKComboBox.h>
#include <JKDialog.h>
#include <JKEdit.h>
#include <JKEvent.h>
#include <JKListBox.h>
#include <JKMessageBox.h>
#include <JKMenu.h>
#include <JOClock.h>
#include <JKStatic.h>
#include <JKTypes.h>
#include <JKWindow.h>
#include <JKDC.h>

#include <apps/AppUtil.h>

#include <cstdio>
#include <fstream>

namespace jk {
namespace {

constexpr uint8_t LTGRAY_R = 192, LTGRAY_G = 192, LTGRAY_B = 192;
constexpr uint8_t BROWN_R = 150, BROWN_G = 90, BROWN_B = 40;
constexpr uint8_t BLUE_R = 0, BLUE_G = 0, BLUE_B = 170;

// 목표 유형 (원본 METABANK 엔티티 타입의 축소판)
const char* kTargetTypes[] = { "Armor", "Infantry", "Artillery", "Air" };
const size_t kTargetTypeCount = 4;

// Phase 2: unit / fire order vocabulary (simplified from original 2CAOCC).
const char* kUnitTypes[] = { "Howitzer", "Rocket", "Air" };
const size_t kUnitTypeCount = 3;
const char* kUnitStatus[] = { "Standby", "Firing", "Moving" };
const size_t kUnitStatusCount = 3;
const char* kFireTypes[] = { "Immediate", "Planned" };
const size_t kFireTypeCount = 2;
} // namespace

// 지도 스케치 패널: 원본 REAL2PIX/CORDVIEW의 1단계 대체물.
// 세계 좌표(0..2000)를 패널 크기에 선형 스케일해서 격자와 목표를 그린다.
// 유형별 전술 심볼(장갑: 사각+타원, 보병: 사각+X, 포병: 사각+점, 항공: 다이아몬드),
// 격자에 세계 좌표 라벨, 하단 범례, 선택 목표 십자선 표시를 지원하며
// 심볼 클릭 시 onPickTarget 콜백으로 목록 선택과 연동된다.
class OccMapPanel : public JKControl {
public:
    OccMapPanel(const JKRect& rect, const std::vector<OccTarget>& targets,
                const std::vector<OccUnit>& units)
        : JKControl(), targets_(targets), units_(units) {
        SetRect(rect);
        SetControlId(0);
    }

    int32_t GetSelectedIndex() const { return selectedIndex_; }
    void SetSelectedIndex(int32_t idx) { selectedIndex_ = idx; }
    void SetOnPickTarget(std::function<void(int32_t)> cb) {
        onPick_ = std::move(cb);
    }

    int32_t GetSelectedUnitIndex() const { return selectedUnitIndex_; }
    void SetSelectedUnitIndex(int32_t idx) { selectedUnitIndex_ = idx; }
    void SetOnPickUnit(std::function<void(int32_t)> cb) {
        onPickUnit_ = std::move(cb);
    }

    void OnPaintClient(JKDC& dc) override {
        const JKRect rc = GetScreenClientRect();
        dc.SetColor(20, 60, 25, 255);
        dc.FillRect(rc);
        DrawGrid(dc, rc);
        for (size_t i = 0; i < targets_.size(); ++i) {
            DrawTarget(dc, rc, i);
        }
        for (size_t i = 0; i < units_.size(); ++i) {
            DrawUnit(dc, rc, i);
        }
        DrawLegend(dc, rc);
        dc.SetColor(0, 0, 0, 255);
        dc.DrawRect(rc);
    }

    void RespondMessage(const JKEvent& ev) override {
        if (ev.type == JKEventType::MouseDown) {
            const JKRect rc = GetScreenClientRect();
            for (int32_t i = static_cast<int32_t>(targets_.size()) - 1; i >= 0; --i) {
                const JKPoint p =
                    WorldToScreen(rc, targets_[static_cast<size_t>(i)]);
                if (ev.x >= p.x - 12 && ev.x <= p.x + 12 &&
                    ev.y >= p.y - 10 && ev.y <= p.y + 14) {
                    selectedIndex_ = i;
                    if (onPick_) {
                        onPick_(i);
                    }
                    return;
                }
            }
            for (int32_t i = static_cast<int32_t>(units_.size()) - 1; i >= 0; --i) {
                const JKPoint pu =
                    WorldToScreenU(rc, units_[static_cast<size_t>(i)]);
                if (ev.x >= pu.x - 10 && ev.x <= pu.x + 10 &&
                    ev.y >= pu.y - 10 && ev.y <= pu.y + 14) {
                    selectedUnitIndex_ = i;
                    if (onPickUnit_) {
                        onPickUnit_(i);
                    }
                    return;
                }
            }
        }
        JKControl::RespondMessage(ev);
    }

private:
    static JKPoint WorldToScreen(const JKRect& rc, const OccTarget& t) {
        return JKPoint{ rc.x + t.x * rc.w / 2000, rc.y + t.y * rc.h / 2000 };
    }

    static size_t TypeIndexOf(const std::string& type) {
        for (size_t i = 0; i < kTargetTypeCount; ++i) {
            if (type == kTargetTypes[i]) {
                return i;
            }
        }
        return 0;
    }

    void DrawGrid(JKDC& dc, const JKRect& rc) const {
        dc.SetColor(45, 95, 50, 255);
        for (int w = 250; w < 2000; w += 250) {
            const int gx = rc.x + w * rc.w / 2000;
            const int gy = rc.y + w * rc.h / 2000;
            dc.DrawLine(gx, rc.y, gx, rc.y + rc.h);
            dc.DrawLine(rc.x, gy, rc.x + rc.w, gy);
        }
        dc.SetTextColor(120, 180, 120);
        char buf[16];
        for (int w = 0; w < 2000; w += 500) {
            std::snprintf(buf, sizeof(buf), "%d", w);
            dc.TextOut(JKPoint{ rc.x + w * rc.w / 2000 + 2, rc.y + 2 }, buf);
            dc.TextOut(JKPoint{ rc.x + 2, rc.y + w * rc.h / 2000 + 2 }, buf);
        }
    }

    void DrawTypeGlyph(JKDC& dc, int px, int py, size_t ti) const {
        if (ti == 3) { // Air: 다이아몬드
            const std::vector<JKPoint> dia = {
                JKPoint{ px, py - 8 }, JKPoint{ px + 9, py },
                JKPoint{ px, py + 8 }, JKPoint{ px - 9, py }
            };
            dc.SetColor(140, 210, 255, 255);
            dc.FillPolygon(dia);
            dc.SetColor(255, 255, 255, 255);
            dc.DrawPolygon(dia);
            return;
        }
        const JKRect box{ px - 10, py - 7, 20, 14 };
        dc.SetColor(255, 255, 255, 255);
        dc.DrawRect(box);
        if (ti == 0) { // Armor: 타원
            dc.Ellipse(JKPoint{ px, py }, 6, 3);
        } else if (ti == 1) { // Infantry: X
            dc.DrawLine(box.x, box.y, box.x + box.w, box.y + box.h);
            dc.DrawLine(box.x, box.y + box.h, box.x + box.w, box.y);
        } else { // Artillery: 속이 찬 점
            dc.SetColor(255, 90, 60, 255);
            dc.FillRect(JKRect{ px - 2, py - 2, 5, 5 });
        }
    }

    void DrawTarget(JKDC& dc, const JKRect& rc, size_t i) const {
        const OccTarget& t = targets_[i];
        const JKPoint p = WorldToScreen(rc, t);
        if (static_cast<int32_t>(i) == selectedIndex_) {
            dc.SetColor(255, 70, 70, 255);
            dc.DrawLine(rc.x, p.y, rc.x + rc.w, p.y);
            dc.DrawLine(p.x, rc.y, p.x, rc.y + rc.h);
            dc.SetColor(255, 220, 60, 255);
            dc.DrawRect(JKRect{ p.x - 13, p.y - 11, 26, 22 });
        }
        DrawTypeGlyph(dc, p.x, p.y, TypeIndexOf(t.type));
        dc.SetTextColor(255, 230, 120);
        dc.TextOut(JKPoint{ p.x + 14, p.y - 7 }, t.name.c_str());
    }

    static JKPoint WorldToScreenU(const JKRect& rc, const OccUnit& u) {
        return JKPoint{ rc.x + u.x * rc.w / 2000, rc.y + u.y * rc.h / 2000 };
    }

    void DrawUnitGlyph(JKDC& dc, int px, int py) const {
        // Friendly unit: filled blue 12-gon (integer circle approximation).
        static const int32_t kDx[12] = { 8, 7, 4, 0, -4, -7, -8, -7, -4, 0, 4, 7 };
        static const int32_t kDy[12] = { 0, 4, 7, 8, 7, 4, 0, -4, -7, -8, -7, -4 };
        std::vector<JKPoint> poly;
        for (int a = 0; a < 12; ++a) {
            poly.push_back(JKPoint{ px + kDx[a], py + kDy[a] });
        }
        dc.SetColor(60, 130, 255, 255);
        dc.FillPolygon(poly);
        dc.SetColor(220, 235, 255, 255);
        dc.Circle(JKPoint{ px, py }, 8);
    }

    void DrawUnit(JKDC& dc, const JKRect& rc, size_t i) const {
        const OccUnit& u = units_[i];
        const JKPoint p = WorldToScreenU(rc, u);
        if (static_cast<int32_t>(i) == selectedUnitIndex_) {
            dc.SetColor(120, 200, 255, 255);
            dc.DrawRect(JKRect{ p.x - 13, p.y - 11, 26, 22 });
        }
        DrawUnitGlyph(dc, p.x, p.y);
        dc.SetTextColor(200, 225, 255);
        dc.TextOut(JKPoint{ p.x + 14, p.y - 7 }, u.name.c_str());
    }

    void DrawLegend(JKDC& dc, const JKRect& rc) const {
        const int y = rc.y + rc.h - 18;
        dc.SetColor(0, 45, 12, 255);
        dc.FillRect(JKRect{ rc.x + 4, y - 5, 820, 21 });
        int x = rc.x + 14;
        for (size_t i = 0; i < kTargetTypeCount; ++i) {
            DrawTypeGlyph(dc, x + 8, y + 3, i);
            dc.SetTextColor(225, 230, 210);
            dc.TextOut(JKPoint{ x + 24, y }, kTargetTypes[i]);
            x += 160;
        }
        DrawUnitGlyph(dc, x + 8, y + 3);
        dc.SetTextColor(225, 230, 210);
        dc.TextOut(JKPoint{ x + 24, y }, "Unit");
    }

    const std::vector<OccTarget>& targets_;
    const std::vector<OccUnit>& units_;
    int32_t selectedIndex_ = -1;
    int32_t selectedUnitIndex_ = -1;
    std::function<void(int32_t)> onPick_;
    std::function<void(int32_t)> onPickUnit_;
};

// ---------------------------------------------------------------------------
// OccDataManager (원본 POSDTMAN.CPP)
// ---------------------------------------------------------------------------
void OccDataManager::Load() {
    targets.clear();
    std::ifstream in(fileName);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = apputil::Split(line);
        if (f[0] == "T" && f.size() >= 5) {
            OccTarget t;
            t.name = f[1];
            t.type = f[2];
            t.x = apputil::ParseInt(f[3]);
            t.y = apputil::ParseInt(f[4]);
            targets.push_back(t);
        }
    }
}

void OccDataManager::Save() const {
    std::ofstream out(fileName);
    for (const OccTarget& t : targets) {
        out << "T|" << t.name << "|" << t.type << "|" << t.x << "|" << t.y << "\n";
    }
}

int OccDataManager::AddRecord(const OccTarget& rec) {
    targets.push_back(rec);
    return static_cast<int>(targets.size()) - 1;
}

void OccDataManager::UpdateRecord(size_t index, const OccTarget& rec) {
    if (index < targets.size()) {
        targets[index] = rec;
    }
}

void OccDataManager::DeleteRecord(size_t index) {
    if (index < targets.size()) {
        targets.erase(targets.begin() + static_cast<long>(index));
    }
}

// ---------------------------------------------------------------------------
// OccUnitManager / OccFireManager (Phase 2 persistence)
// Format: pipe-delimited text lines with a leading record tag.
//   OCCUNIT.DAT: U|name|type|status|ammo|x|y
//   OCCFIRE.DAT: F|unitName|targetName|targetType|fireType|time
// ---------------------------------------------------------------------------
void OccUnitManager::Load() {
    units.clear();
    std::ifstream in(fileName);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = apputil::Split(line);
        if (f[0] == "U" && f.size() >= 7) {
            OccUnit u;
            u.name = f[1];
            u.type = f[2];
            u.status = apputil::ParseInt(f[3]);
            u.ammo = apputil::ParseInt(f[4]);
            u.x = apputil::ParseInt(f[5]);
            u.y = apputil::ParseInt(f[6]);
            units.push_back(u);
        }
    }
}

void OccUnitManager::Save() const {
    std::ofstream out(fileName);
    for (const OccUnit& u : units) {
        out << "U|" << u.name << "|" << u.type << "|" << u.status << "|"
            << u.ammo << "|" << u.x << "|" << u.y << "\n";
    }
}

int OccUnitManager::AddUnit(const OccUnit& unit) {
    units.push_back(unit);
    return static_cast<int>(units.size()) - 1;
}

void OccUnitManager::UpdateUnit(size_t index, const OccUnit& unit) {
    if (index < units.size()) {
        units[index] = unit;
    }
}

void OccUnitManager::DeleteUnit(size_t index) {
    if (index < units.size()) {
        units.erase(units.begin() + static_cast<long>(index));
    }
}

void OccFireManager::Load() {
    orders.clear();
    std::ifstream in(fileName);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = apputil::Split(line);
        if (f[0] == "F" && f.size() >= 6) {
            OccFireOrder o;
            o.unitName = f[1];
            o.targetName = f[2];
            o.targetType = apputil::ParseInt(f[3]);
            o.fireType = apputil::ParseInt(f[4]);
            o.time = apputil::ParseInt(f[5]);
            orders.push_back(o);
        }
    }
}

void OccFireManager::Save() const {
    std::ofstream out(fileName);
    for (const OccFireOrder& o : orders) {
        out << "F|" << o.unitName << "|" << o.targetName << "|" << o.targetType
            << "|" << o.fireType << "|" << o.time << "\n";
    }
}

int OccFireManager::AddOrder(const OccFireOrder& order) {
    orders.push_back(order);
    return static_cast<int>(orders.size()) - 1;
}

void OccFireManager::UpdateOrder(size_t index, const OccFireOrder& order) {
    if (index < orders.size()) {
        orders[index] = order;
    }
}

void OccFireManager::DeleteOrder(size_t index) {
    if (index < orders.size()) {
        orders.erase(orders.begin() + static_cast<long>(index));
    }
}

// ---------------------------------------------------------------------------
// 목표 입력 다이얼로그
// ---------------------------------------------------------------------------
namespace {

class OccTargetDialog : public JKDialog {
public:
    using DoneFn = std::function<void(const OccTarget&)>;

    OccTargetDialog(bool modify, const OccTarget& base, DoneFn onDone)
        : JKDialog(modify ? "OCC - Modify Target" : "OCC - Add Target") {
        onDone_ = std::move(onDone);
        SetWindowRect(MakeRect(700, 340, 1220, 780));
        SetAttrFlags(WA_TITLEMOVEABLE);
        SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);

        auto addLabel = [&](int y, const char* text) {
            JKStatic* stc = new JKStatic(MakeRect(20, y, 120, y + 24), 0);
            stc->SetText(text);
            stc->SetAdjustFlag(ADJ_YCENTER);
            AddControl(std::unique_ptr<JKStatic>(stc));
        };

        addLabel(20, "Name");
        editName_ = new JKEdit(MakeRect(130, 20, 420, 46), 0, 20);
        editName_->SetText(base.name);
        AddControl(std::unique_ptr<JKEdit>(editName_));

        addLabel(56, "Type");
        comboType_ = new JKComboBox(MakeRect(130, 56, 420, 82), 0);
        int preselect = 0;
        for (size_t i = 0; i < kTargetTypeCount; ++i) {
            comboType_->AddString(kTargetTypes[i]);
            if (kTargetTypes[i] == base.type) {
                preselect = static_cast<int>(i);
            }
        }
        comboType_->SetSelectedIndex(preselect);
        AddControl(std::unique_ptr<JKComboBox>(comboType_));

        addLabel(92, "X (0..2000)");
        editX_ = new JKEdit(MakeRect(130, 92, 420, 118), 0, 6);
        editX_->SetText(IntToText(base.x));
        AddControl(std::unique_ptr<JKEdit>(editX_));

        addLabel(128, "Y (0..2000)");
        editY_ = new JKEdit(MakeRect(130, 128, 420, 154), 0, 6);
        editY_->SetText(IntToText(base.y));
        AddControl(std::unique_ptr<JKEdit>(editY_));

        JKButton* okBtn = new JKButton(MakeRect(130, 190, 250, 226), 0);
        okBtn->SetText("OK");
        okBtn->SetOnClick([this]() { OnOk(); });
        AddControl(std::unique_ptr<JKButton>(okBtn));

        JKButton* cancelBtn = new JKButton(MakeRect(290, 190, 420, 226), 0);
        cancelBtn->SetText("Cancel");
        cancelBtn->SetOnClick([this]() { Close(ResultCancel); });
        AddControl(std::unique_ptr<JKButton>(cancelBtn));
    }

private:
    static std::string IntToText(int v) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", v);
        return buf;
    }

    void OnOk() {
        if (editName_->GetText().empty()) {
            return;
        }
        OccTarget t;
        t.name = editName_->GetText();
        t.type = comboType_->GetSelectedString();
        t.x = apputil::ParseInt(editX_->GetText());
        t.y = apputil::ParseInt(editY_->GetText());
        if (onDone_) {
            onDone_(t);
        }
        Close(ResultOk);
    }

    JKEdit* editName_ = nullptr;
    JKComboBox* comboType_ = nullptr;
    JKEdit* editX_ = nullptr;
    JKEdit* editY_ = nullptr;
    DoneFn onDone_;
};

// ---------------------------------------------------------------------------
// 유닛 입력 다이얼로그 (Phase 2)
// ---------------------------------------------------------------------------
class OccUnitDialog : public JKDialog {
public:
    using DoneFn = std::function<void(const OccUnit&)>;

    OccUnitDialog(bool modify, const OccUnit& base, DoneFn onDone)
        : JKDialog(modify ? "OCC - Modify Unit" : "OCC - Add Unit") {
        onDone_ = std::move(onDone);
        SetWindowRect(MakeRect(700, 340, 1220, 880));
        SetAttrFlags(WA_TITLEMOVEABLE);
        SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);

        auto addLabel = [&](int y, const char* text) {
            JKStatic* stc = new JKStatic(MakeRect(20, y, 120, y + 24), 0);
            stc->SetText(text);
            stc->SetAdjustFlag(ADJ_YCENTER);
            AddControl(std::unique_ptr<JKStatic>(stc));
        };

        addLabel(20, "Name");
        editName_ = new JKEdit(MakeRect(130, 20, 420, 46), 0, 20);
        editName_->SetText(base.name);
        AddControl(std::unique_ptr<JKEdit>(editName_));

        addLabel(56, "Type");
        comboType_ = new JKComboBox(MakeRect(130, 56, 420, 82), 0);
        int preType = 0;
        for (size_t i = 0; i < kUnitTypeCount; ++i) {
            comboType_->AddString(kUnitTypes[i]);
            if (kUnitTypes[i] == base.type) {
                preType = static_cast<int>(i);
            }
        }
        comboType_->SetSelectedIndex(preType);
        AddControl(std::unique_ptr<JKComboBox>(comboType_));

        addLabel(92, "Status");
        comboStatus_ = new JKComboBox(MakeRect(130, 92, 420, 118), 0);
        int preStatus = base.status;
        if (preStatus < 0 || preStatus >= static_cast<int>(kUnitStatusCount)) {
            preStatus = 0;
        }
        for (size_t i = 0; i < kUnitStatusCount; ++i) {
            comboStatus_->AddString(kUnitStatus[i]);
        }
        comboStatus_->SetSelectedIndex(preStatus);
        AddControl(std::unique_ptr<JKComboBox>(comboStatus_));

        addLabel(128, "Ammo");
        editAmmo_ = new JKEdit(MakeRect(130, 128, 420, 154), 0, 6);
        editAmmo_->SetText(IntToText(base.ammo));
        AddControl(std::unique_ptr<JKEdit>(editAmmo_));

        addLabel(164, "X (0..2000)");
        editX_ = new JKEdit(MakeRect(130, 164, 420, 190), 0, 6);
        editX_->SetText(IntToText(base.x));
        AddControl(std::unique_ptr<JKEdit>(editX_));

        addLabel(200, "Y (0..2000)");
        editY_ = new JKEdit(MakeRect(130, 200, 420, 226), 0, 6);
        editY_->SetText(IntToText(base.y));
        AddControl(std::unique_ptr<JKEdit>(editY_));

        JKButton* okBtn = new JKButton(MakeRect(130, 260, 250, 296), 0);
        okBtn->SetText("OK");
        okBtn->SetOnClick([this]() { OnOk(); });
        AddControl(std::unique_ptr<JKButton>(okBtn));

        JKButton* cancelBtn = new JKButton(MakeRect(290, 260, 420, 296), 0);
        cancelBtn->SetText("Cancel");
        cancelBtn->SetOnClick([this]() { Close(ResultCancel); });
        AddControl(std::unique_ptr<JKButton>(cancelBtn));
    }

private:
    static std::string IntToText(int v) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", v);
        return buf;
    }

    void OnOk() {
        if (editName_->GetText().empty()) {
            return;
        }
        OccUnit u;
        u.name = editName_->GetText();
        u.type = comboType_->GetSelectedString();
        u.status = comboStatus_->GetSelectedIndex();
        u.ammo = apputil::ParseInt(editAmmo_->GetText());
        u.x = apputil::ParseInt(editX_->GetText());
        u.y = apputil::ParseInt(editY_->GetText());
        if (onDone_) {
            onDone_(u);
        }
        Close(ResultOk);
    }

    JKEdit* editName_ = nullptr;
    JKComboBox* comboType_ = nullptr;
    JKComboBox* comboStatus_ = nullptr;
    JKEdit* editAmmo_ = nullptr;
    JKEdit* editX_ = nullptr;
    JKEdit* editY_ = nullptr;
    DoneFn onDone_;
};

// ---------------------------------------------------------------------------
// 사격명령 다이얼로그 (Phase 2): 기존 명령 목록 + 새 명령 입력/삭제
// ---------------------------------------------------------------------------
class OccFireDialog : public JKDialog {
public:
    OccFireDialog(const OccDataManager& dataMan, const OccUnitManager& unitMan,
                  OccFireManager& fireMan)
        : JKDialog("OCC - Fire Orders"), dataMan_(dataMan), unitMan_(unitMan),
          fireMan_(fireMan) {
        SetWindowRect(MakeRect(640, 220, 1300, 900));
        SetAttrFlags(WA_TITLEMOVEABLE);
        SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);

        JKStatic* lstHead = new JKStatic(MakeRect(20, 20, 640, 44), 0);
        lstHead->SetText("  Current Fire Orders");
        lstHead->SetBackColor(BROWN_R, BROWN_G, BROWN_B);
        lstHead->SetTextColor(255, 255, 255);
        lstHead->SetAdjustFlag(ADJ_YCENTER);
        AddControl(std::unique_ptr<JKStatic>(lstHead));

        orderList_ = new JKListBox(MakeRect(20, 50, 640, 330), 0);
        orderList_->SetBackColor(255, 255, 255);
        orderList_->SetTextColor(0, 0, 0);
        AddControl(std::unique_ptr<JKListBox>(orderList_));

        auto addLabel = [&](int y, const char* text) {
            JKStatic* stc = new JKStatic(MakeRect(20, y, 120, y + 26), 0);
            stc->SetText(text);
            stc->SetAdjustFlag(ADJ_YCENTER);
            AddControl(std::unique_ptr<JKStatic>(stc));
        };

        addLabel(350, "Unit");
        comboUnit_ = new JKComboBox(MakeRect(140, 350, 420, 376), 0);
        for (const OccUnit& u : unitMan_.units) {
            comboUnit_->AddString(u.name);
        }
        if (!unitMan_.units.empty()) {
            comboUnit_->SetSelectedIndex(0);
        }
        AddControl(std::unique_ptr<JKComboBox>(comboUnit_));

        addLabel(386, "Target");
        comboTarget_ = new JKComboBox(MakeRect(140, 386, 420, 412), 0);
        for (const OccTarget& t : dataMan_.targets) {
            comboTarget_->AddString(t.name);
        }
        if (!dataMan_.targets.empty()) {
            comboTarget_->SetSelectedIndex(0);
        }
        AddControl(std::unique_ptr<JKComboBox>(comboTarget_));

        addLabel(422, "Fire");
        comboFire_ = new JKComboBox(MakeRect(140, 422, 420, 448), 0);
        for (size_t i = 0; i < kFireTypeCount; ++i) {
            comboFire_->AddString(kFireTypes[i]);
        }
        comboFire_->SetSelectedIndex(0);
        AddControl(std::unique_ptr<JKComboBox>(comboFire_));

        addLabel(458, "Time (min)");
        editTime_ = new JKEdit(MakeRect(140, 458, 420, 484), 0, 4);
        editTime_->SetText("0");
        AddControl(std::unique_ptr<JKEdit>(editTime_));

        JKButton* addBtn = new JKButton(MakeRect(140, 510, 260, 546), 0);
        addBtn->SetText("Add");
        addBtn->SetOnClick([this]() { OnAdd(); });
        AddControl(std::unique_ptr<JKButton>(addBtn));

        JKButton* delBtn = new JKButton(MakeRect(280, 510, 400, 546), 0);
        delBtn->SetText("Delete");
        delBtn->SetOnClick([this]() { OnDelete(); });
        AddControl(std::unique_ptr<JKButton>(delBtn));

        JKButton* closeBtn = new JKButton(MakeRect(420, 510, 540, 546), 0);
        closeBtn->SetText("Close");
        closeBtn->SetOnClick([this]() { Close(ResultCancel); });
        AddControl(std::unique_ptr<JKButton>(closeBtn));

        RefreshOrderList();
    }

    void RefreshOrderList() {
        if (!orderList_) {
            return;
        }
        orderList_->Clear();
        for (const OccFireOrder& o : fireMan_.orders) {
            const char* ft = (o.fireType >= 0 &&
                              o.fireType < static_cast<int>(kFireTypeCount))
                                 ? kFireTypes[static_cast<size_t>(o.fireType)]
                                 : "?";
            orderList_->AddString(
                apputil::PadRight(o.unitName, 14) +
                apputil::PadRight(o.targetName, 14) +
                apputil::PadRight(ft, 10) + std::to_string(o.time));
        }
    }

private:
    int TargetTypeOf(const std::string& targetName) const {
        for (const OccTarget& t : dataMan_.targets) {
            if (t.name == targetName) {
                for (size_t i = 0; i < kTargetTypeCount; ++i) {
                    if (t.type == kTargetTypes[i]) {
                        return static_cast<int>(i);
                    }
                }
            }
        }
        return 0;
    }

    void OnAdd() {
        if (unitMan_.units.empty() || dataMan_.targets.empty()) {
            return;
        }
        OccFireOrder o;
        o.unitName = comboUnit_->GetSelectedString();
        o.targetName = comboTarget_->GetSelectedString();
        o.targetType = TargetTypeOf(o.targetName);
        o.fireType = comboFire_->GetSelectedIndex();
        o.time = apputil::ParseInt(editTime_->GetText());
        fireMan_.AddOrder(o);
        fireMan_.Save();
        RefreshOrderList();
    }

    void OnDelete() {
        int sel = orderList_->GetSelectedIndex();
        if (sel < 0 || static_cast<size_t>(sel) >= fireMan_.orders.size()) {
            return;
        }
        fireMan_.DeleteOrder(static_cast<size_t>(sel));
        fireMan_.Save();
        RefreshOrderList();
    }

    const OccDataManager& dataMan_;
    const OccUnitManager& unitMan_;
    OccFireManager& fireMan_;
    JKListBox* orderList_ = nullptr;
    JKComboBox* comboUnit_ = nullptr;
    JKComboBox* comboTarget_ = nullptr;
    JKComboBox* comboFire_ = nullptr;
    JKEdit* editTime_ = nullptr;
};

} // namespace

// ---------------------------------------------------------------------------
// OccApp (원본 OCCMAIN.CPP/OCCWIN.CPP의 1단계 포팅)
// ---------------------------------------------------------------------------
OccApp::OccApp() = default;

OccApp::~OccApp() {
    dataMan_.Save();
    unitMan_.Save();
    fireMan_.Save();
}

void OccApp::OnInit() {
    dataMan_.Load();
    unitMan_.Load();
    fireMan_.Load();
    BuildMainWindow();
    SetMainWindow(std::move(mainWindow_));
}

void OccApp::OnClose() {
    dataMan_.Save();
    unitMan_.Save();
    fireMan_.Save();
}

void OccApp::BuildMainWindow() {
    mainWindow_ = std::make_unique<JKWindow>("OCC - 2CAOCC C2 (SDL2 Port)");
    mainWindow_->SetBackColor(LTGRAY_R, LTGRAY_G, LTGRAY_B);
    mainWindow_->SetAttrFlags(WA_TITLEMOVEABLE);

    auto menu = std::make_unique<JKMenu>(JKRect{ 0, 0, 1916, 20 }, 300);
    menu->AddMenu("File", {
        jk::JKMenuItem{ "Exit", 301, []() {
            SDL_Event quit;
            quit.type = SDL_QUIT;
            SDL_PushEvent(&quit);
        }},
    });
    menu->AddMenu("Data", {
        jk::JKMenuItem{ "Add Target", 302, [this]() { ShowTargetDialog(false); }},
        jk::JKMenuItem{ "Add Unit", 306, [this]() { ShowUnitDialog(false); }},
    });
    menu->AddMenu("Fire", {
        jk::JKMenuItem{ "Fire Orders", 307, [this]() { ShowFireDialog(); }},
    });
    menu->AddMenu("Help", {
        jk::JKMenuItem{ "About", 305, [this]() {
            if (aboutBox_ && !GetModalWindow()) {
                aboutBox_->Show();
            }
        }},
    });
    mainWindow_->AddControl(std::move(menu));

    JKStatic* targetHead = new JKStatic(MakeRect(10, 30, 610, 54), 0);
    targetHead->SetText("  Targets");
    targetHead->SetBackColor(BROWN_R, BROWN_G, BROWN_B);
    targetHead->SetTextColor(255, 255, 255);
    targetHead->SetAdjustFlag(ADJ_YCENTER);
    mainWindow_->AddControl(std::unique_ptr<JKStatic>(targetHead));

    targetList_ = new JKListBox(MakeRect(10, 60, 610, 480), 0);
    targetList_->SetBackColor(255, 255, 255);
    targetList_->SetTextColor(0, 0, 0);
    mainWindow_->AddControl(std::unique_ptr<JKListBox>(targetList_));

    auto addBtn = [&](int x1, int x2, int y1, int y2, const char* label,
                      std::function<void()> fn) {
        JKButton* btn = new JKButton(MakeRect(x1, y1, x2, y2), 0);
        btn->SetText(label);
        btn->SetOnClick(std::move(fn));
        mainWindow_->AddControl(std::unique_ptr<JKButton>(btn));
    };
    addBtn(10, 140, 490, 524, "Add", [this]() { ShowTargetDialog(false); });
    addBtn(150, 280, 490, 524, "Modify", [this]() { ShowTargetDialog(true); });
    addBtn(290, 420, 490, 524, "Delete", [this]() {
        int sel = targetList_->GetSelectedIndex();
        if (sel < 0 || static_cast<size_t>(sel) >= dataMan_.targets.size()) {
            ShowMessageModal("OCC", "Select a target first.");
            return;
        }
        ShowMessageModal("OCC", "Delete selected target?",
                         JKMessageBox::Buttons::YesNo, [this](int result) {
                             if (result == JKMessageBox::ResultYes) {
                                 int row = targetList_->GetSelectedIndex();
                                 if (row >= 0) {
                                     dataMan_.DeleteRecord(static_cast<size_t>(row));
                                     dataMan_.Save();
                                     RefreshTargetList();
                                 }
                             }
                         });
    });
    addBtn(430, 560, 490, 524, "Save", [this]() {
        dataMan_.Save();
        unitMan_.Save();
        fireMan_.Save();
        ShowMessageModal("OCC", "Saved to " + dataMan_.fileName);
    });

    // Units section (Phase 2): simplified BattalionManager view.
    JKStatic* unitHead = new JKStatic(MakeRect(10, 540, 610, 564), 0);
    unitHead->SetText("  Units");
    unitHead->SetBackColor(BROWN_R, BROWN_G, BROWN_B);
    unitHead->SetTextColor(255, 255, 255);
    unitHead->SetAdjustFlag(ADJ_YCENTER);
    mainWindow_->AddControl(std::unique_ptr<JKStatic>(unitHead));

    unitList_ = new JKListBox(MakeRect(10, 570, 610, 960), 0);
    unitList_->SetBackColor(255, 255, 255);
    unitList_->SetTextColor(0, 0, 0);
    mainWindow_->AddControl(std::unique_ptr<JKListBox>(unitList_));

    addBtn(10, 140, 972, 1006, "Add", [this]() { ShowUnitDialog(false); });
    addBtn(150, 280, 972, 1006, "Modify", [this]() { ShowUnitDialog(true); });
    addBtn(290, 420, 972, 1006, "Delete", [this]() {
        int sel = unitList_->GetSelectedIndex();
        if (sel < 0 || static_cast<size_t>(sel) >= unitMan_.units.size()) {
            ShowMessageModal("OCC", "Select a unit first.");
            return;
        }
        ShowMessageModal("OCC", "Delete selected unit?",
                         JKMessageBox::Buttons::YesNo, [this](int result) {
                             if (result == JKMessageBox::ResultYes) {
                                 int row = unitList_->GetSelectedIndex();
                                 if (row >= 0) {
                                     unitMan_.DeleteUnit(static_cast<size_t>(row));
                                     unitMan_.Save();
                                     RefreshUnitList();
                                 }
                             }
                         });
    });
    addBtn(430, 560, 972, 1006, "Fire", [this]() { ShowFireDialog(); });

    OccMapPanel* map = new OccMapPanel(MakeRect(620, 30, 1906, 960),
                                       dataMan_.targets, unitMan_.units);
    map->SetOnPickTarget([this](int32_t idx) {
        if (targetList_) {
            targetList_->SetSelectedIndex(idx);
        }
    });
    map->SetOnPickUnit([this](int32_t idx) {
        if (unitList_) {
            unitList_->SetSelectedIndex(idx);
        }
    });
    mapPanel_ = map;
    mainWindow_->AddControl(std::unique_ptr<JKControl>(map));

    targetList_->SetOnSelect([this](int32_t idx) {
        if (mapPanel_) {
            mapPanel_->SetSelectedIndex(idx);
        }
    });

    unitList_->SetOnSelect([this](int32_t idx) {
        if (mapPanel_) {
            mapPanel_->SetSelectedUnitIndex(idx);
        }
    });

    statusLine_ = new JKStatic(MakeRect(10, 1020, 800, 1044), 0);
    statusLine_->SetAdjustFlag(ADJ_YCENTER);
    mainWindow_->AddControl(std::unique_ptr<JKStatic>(statusLine_));

    JOClock* clock = new JOClock(MakeRect(1810, 1020, 1906, 1044), 0);
    clock->SetBackColor(BLUE_R, BLUE_G, BLUE_B);
    clock->SetTextColor(255, 255, 255);
    mainWindow_->AddControl(std::unique_ptr<JKControl>(clock));

    aboutBox_ = std::make_unique<JKMessageBox>(
        "About",
        "OCC - 2CAOCC C2 SDL2 Port (Phase 2)\nUnits + Fire Orders + 30s Timer\nOriginal: WINDBASE/2CAOCC",
        JKMessageBox::Buttons::Ok,
        [](int) { /* dismissed */ });

    RefreshTargetList();
    RefreshUnitList();
}

void OccApp::RefreshTargetList() {
    if (!targetList_) {
        return;
    }
    targetList_->Clear();
    for (const OccTarget& t : dataMan_.targets) {
        targetList_->AddString(apputil::PadRight(t.name, 16) +
                               apputil::PadRight(t.type, 12) +
                               apputil::PadRight(std::to_string(t.x), 6) +
                               std::to_string(t.y));
    }
    if (mapPanel_ && mapPanel_->GetSelectedIndex() >=
                         static_cast<int32_t>(dataMan_.targets.size())) {
        mapPanel_->SetSelectedIndex(-1);
    }
    UpdateStatusLine();
}

void OccApp::RefreshUnitList() {
    if (!unitList_) {
        return;
    }
    unitList_->Clear();
    for (const OccUnit& u : unitMan_.units) {
        const char* status =
            (u.status >= 0 && u.status < static_cast<int>(kUnitStatusCount))
                ? kUnitStatus[static_cast<size_t>(u.status)]
                : "?";
        unitList_->AddString(apputil::PadRight(u.name, 14) +
                             apputil::PadRight(u.type, 10) +
                             apputil::PadRight(status, 8) +
                             apputil::PadRight(std::to_string(u.ammo), 6) +
                             apputil::PadRight(std::to_string(u.x), 6) +
                             std::to_string(u.y));
    }
    if (mapPanel_ && mapPanel_->GetSelectedUnitIndex() >=
                         static_cast<int32_t>(unitMan_.units.size())) {
        mapPanel_->SetSelectedUnitIndex(-1);
    }
    UpdateStatusLine();
}

void OccApp::UpdateStatusLine() {
    if (!statusLine_) {
        return;
    }
    statusLine_->SetText(
        "Targets: " + std::to_string(dataMan_.targets.size()) +
        "   Units: " + std::to_string(unitMan_.units.size()) +
        "   FireOrders: " + std::to_string(fireMan_.orders.size()) +
        "   T+" + std::to_string(timerCounter_) + "s   (2CAOCC Phase 2)");
}

void OccApp::ShowTargetDialog(bool modify) {
    OccTarget base;
    int idx = -1;
    if (modify) {
        int sel = targetList_->GetSelectedIndex();
        if (sel < 0 || static_cast<size_t>(sel) >= dataMan_.targets.size()) {
            ShowMessageModal("OCC", "Select a target first.");
            return;
        }
        idx = sel;
        base = dataMan_.targets[static_cast<size_t>(idx)];
    }

    targetDlg_ = std::make_unique<OccTargetDialog>(
        modify, base, [this, idx](const OccTarget& t) {
            if (idx < 0) {
                dataMan_.AddRecord(t);
            } else {
                dataMan_.UpdateRecord(static_cast<size_t>(idx), t);
            }
            dataMan_.Save();
        });
    targetDlg_->SetOnClose([this](int) {
        if (g_currentJKApp) {
            g_currentJKApp->SetModalWindow(mainWindow_.get());
        }
        RefreshTargetList();
    });
    targetDlg_->Show();
}

void OccApp::ShowUnitDialog(bool modify) {
    OccUnit base;
    int idx = -1;
    if (modify) {
        int sel = unitList_->GetSelectedIndex();
        if (sel < 0 || static_cast<size_t>(sel) >= unitMan_.units.size()) {
            ShowMessageModal("OCC", "Select a unit first.");
            return;
        }
        idx = sel;
        base = unitMan_.units[static_cast<size_t>(idx)];
    }

    unitDlg_ = std::make_unique<OccUnitDialog>(
        modify, base, [this, idx](const OccUnit& u) {
            if (idx < 0) {
                unitMan_.AddUnit(u);
            } else {
                unitMan_.UpdateUnit(static_cast<size_t>(idx), u);
            }
            unitMan_.Save();
        });
    unitDlg_->SetOnClose([this](int) {
        if (g_currentJKApp) {
            g_currentJKApp->SetModalWindow(mainWindow_.get());
        }
        RefreshUnitList();
    });
    unitDlg_->Show();
}

void OccApp::ShowFireDialog() {
    if (unitMan_.units.empty() || dataMan_.targets.empty()) {
        ShowMessageModal("OCC", "Add at least one unit and one target first.");
        return;
    }
    fireDlg_ = std::make_unique<OccFireDialog>(dataMan_, unitMan_, fireMan_);
    fireDlg_->SetOnClose([this](int) {
        if (g_currentJKApp) {
            g_currentJKApp->SetModalWindow(mainWindow_.get());
        }
        UpdateStatusLine();
    });
    fireDlg_->Show();
}

void OccApp::OnTimerTick() {
    ++timerCounter_;
    // Original 2CAOCC shows a periodic status message every 30 seconds
    // (SetTimer(30000) + EvTimerHappen). Keep the same period here.
    if (timerCounter_ % 30 == 0) {
        if (statusLine_) {
            statusLine_->SetText(
                "Periodic check: verify target planning info. (T+" +
                std::to_string(timerCounter_) + "s)");
        }
    } else {
        UpdateStatusLine();
    }
}

bool OccApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer) {
        OnTimerTick();
    }
    return JKApplication::PreProcessMessage(ev);
}

void OccApp::ShowMessageModal(const std::string& title, const std::string& msg,
                              JKMessageBox::Buttons buttons,
                              const std::function<void(int)>& onResult) {
    apputil::ShowModalMessage(mainWindow_.get(), msgBox_, title, msg, buttons,
                              onResult);
}

} // namespace jk