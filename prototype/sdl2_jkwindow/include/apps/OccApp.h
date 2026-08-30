#ifndef OCCAPP_H
#define OCCAPP_H

// 원본 WINDBASE/2CAOCC의 C2 앱(OCCMAIN.CPP/OCCWIN.CPP)을 SDL2
// JKWindow 프레임워크로 단계적으로 포팅하기 위한 진입점.
// 1단계: 메인 윈도우 + 목표(target) 목록/편집 + 지도 스케치 패널.

// Phase 2: units + fire orders + 30 s periodic timer status
// (OCCUNIT.DAT / OCCFIRE.DAT persistence, original SetTimer(30000)).

#include <JKApplication.h>
#include <JKMessageBox.h>
#include <memory>
#include <string>
#include <vector>

namespace jk {

class JKListBox;
class JKStatic;
class JKDialog;
class OccMapPanel;

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

// 2CAOCC 진입 앱: `jkproto_sdl2_jkwindow.exe occ`로 실행.
class OccApp : public JKApplication {
public:
    OccApp();
    ~OccApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    void BuildMainWindow();
    void RefreshTargetList();
    void RefreshUnitList();
    void UpdateStatusLine();
    void ShowTargetDialog(bool modify);
    void ShowUnitDialog(bool modify);
    void ShowFireDialog();
    void ShowMessageModal(const std::string& title, const std::string& msg,
                          JKMessageBox::Buttons buttons = JKMessageBox::Buttons::Ok,
                          const std::function<void(int)>& onResult = nullptr);
    void OnTimerTick();

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

#endif // OCCAPP_H