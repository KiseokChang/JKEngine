#ifndef OCCUI_H
#define OCCUI_H

// OCC (2CAOCC C2) 데이터 모델 + UI. 단일 프로세스 OccApp과 서버 모드
// 클라이언트 모듈 ClientOccApp이 함께 사용하는 단일 구현이다.

#include <JKControl.h>
#include <JKDialog.h>
#include <JKMessageBox.h>
#include <JKTypes.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jk {

class JKListBox;
class JKMenu;
class JKStatic;
class JKWindow;

// 원본 METABANK/POSREC(위치 데이터)의 축소판: 목표 엔티티.
struct OccTarget {
    std::string name;    // 목표 명칭
    std::string type;    // 유형: Armor / Infantry / Artillery / Air
    int x = 0;           // 세계 좌표 X (0..2000)
    int y = 0;           // 세계 좌표 Y (0..2000)
};

// 원본 POSDTMAN(위치 데이터 관리자)의 메모리/텍스트 파일 버전.
class OccDataManager {
public:
    void Load();
    void Save() const;

    int AddRecord(const OccTarget& rec);
    void UpdateRecord(size_t index, const OccTarget& rec);
    void DeleteRecord(size_t index);

    std::vector<OccTarget> targets;
    std::string fileName = "OCCDATA.DAT";
};

// 2CAOCC Battalion record (simplified BattalionRecord).
struct OccUnit {
    std::string name;        // Unit name
    std::string type;        // Type: Howitzer / Rocket / Air
    int status = 0;          // Status: 0=Standby / 1=Firing / 2=Moving
    int ammo = 0;            // Ammo count
    int x = 0;               // World coord X
    int y = 0;               // World coord Y
};

// 2CAOCC Fire order record (simplified FireRecord).
struct OccFireOrder {
    std::string unitName;    // Firing unit name
    std::string targetName;  // Target name
    int targetType = 0;      // Target type (0=Armor,1=Infantry,2=Artillery,3=Air)
    int fireType = 0;        // Fire type: 0=Immediate / 1=Planned
    int time = 0;            // Fire time (minutes)
};

// Unit data manager (simplified BattalionManager).
class OccUnitManager {
public:
    void Load();
    void Save() const;
    int AddUnit(const OccUnit& unit);
    void UpdateUnit(size_t index, const OccUnit& unit);
    void DeleteUnit(size_t index);
    std::vector<OccUnit> units;
    std::string fileName = "OCCUNIT.DAT";
};

// Fire order manager (simplified FireManager).
class OccFireManager {
public:
    void Load();
    void Save() const;
    int AddOrder(const OccFireOrder& order);
    void UpdateOrder(size_t index, const OccFireOrder& order);
    void DeleteOrder(size_t index);
    std::vector<OccFireOrder> orders;
    std::string fileName = "OCCFIRE.DAT";
};

// 지도 스케치 패널: 원본 REAL2PIX/CORDVIEW의 1단계 대체물.
// 세계 좌표(0..2000)를 패널 크기에 선형 스케일해서 격자와 목표를 그린다.
// 유형별 전술 심볼(장갑: 사각+타원, 보병: 사각+X, 포병: 사각+점, 항공: 다이아몬드),
// 격자에 세계 좌표 라벨, 하단 범례, 선택 목표 십자선 표시를 지원하며
// 심볼 클릭 시 onPickTarget 콜백으로 목록 선택과 연동된다.
class OccMapPanel : public JKControl {
public:
    OccMapPanel(const JKRect& rect, const std::vector<OccTarget>& targets,
                const std::vector<OccUnit>& units);

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

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

private:
    static JKPoint WorldToScreen(const JKRect& rc, const OccTarget& t);
    static JKPoint WorldToScreenU(const JKRect& rc, const OccUnit& u);
    static size_t TypeIndexOf(const std::string& type);
    void DrawGrid(JKDC& dc, const JKRect& rc) const;
    void DrawTypeGlyph(JKDC& dc, int px, int py, size_t ti) const;
    void DrawTarget(JKDC& dc, const JKRect& rc, size_t i) const;
    void DrawUnitGlyph(JKDC& dc, int px, int py) const;
    void DrawUnit(JKDC& dc, const JKRect& rc, size_t i) const;
    void DrawLegend(JKDC& dc, const JKRect& rc) const;

    const std::vector<OccTarget>& targets_;
    const std::vector<OccUnit>& units_;
    int32_t selectedIndex_ = -1;
    int32_t selectedUnitIndex_ = -1;
    std::function<void(int32_t)> onPick_;
    std::function<void(int32_t)> onPickUnit_;
};

// ---------------------------------------------------------------------------
// OCC UI: 윈도우/컨트롤 트리 구축 + 다이얼로그 + 30초 타이머 상태 갱신.
// onExit은 Exit 메뉴(단일 프로세스: SDL_QUIT, 클라이언트: RequestQuit)를
// 호출하는 콜백이다.
// ---------------------------------------------------------------------------
class OccUI {
public:
    explicit OccUI(std::function<void()> onExit);
    ~OccUI();

    void LoadAll();
    void SaveAll();

    void BuildMainWindow();
    // 셸이 SetMainWindow(std::move(...))로 호스트에 넘긴다.
    std::unique_ptr<JKWindow> TakeMainWindow();

    void RefreshTargetList();
    void RefreshUnitList();
    void UpdateStatusLine();
    void OnTimerTick();
    void ShowMessageModal(const std::string& title, const std::string& msg,
                          JKMessageBox::Buttons buttons = JKMessageBox::Buttons::Ok,
                          const std::function<void(int)>& onResult = nullptr);

private:
    void BuildMainWindowImpl();
    void ShowTargetDialog(bool modify);
    void ShowUnitDialog(bool modify);
    void ShowFireDialog();

    std::function<void()> onExit_;
    std::unique_ptr<JKWindow> mainWindow_;
    OccDataManager dataMan_;
    OccUnitManager unitMan_;
    OccFireManager fireMan_;
    JKListBox* targetList_ = nullptr;
    JKListBox* unitList_ = nullptr;
    JKStatic* statusLine_ = nullptr;
    OccMapPanel* mapPanel_ = nullptr;
    std::unique_ptr<JKDialog> targetDlg_;
    std::unique_ptr<JKDialog> unitDlg_;
    std::unique_ptr<JKDialog> fireDlg_;
    std::unique_ptr<JKMessageBox> msgBox_;
    std::unique_ptr<JKMessageBox> aboutBox_;
    int timerCounter_ = 0;
};

} // namespace jk

#endif // OCCUI_H