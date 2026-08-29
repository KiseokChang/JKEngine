#ifndef EQUIPAPP_H
#define EQUIPAPP_H

// 원본 WINDBASE/JANGO의 구(舊) 장비 관리(EQUIP.CPP/EQUIPWIN.CPP)를
// SDL2 JKWindow 프레임워크로 포팅한 앱.

#include <JKDialog.h>
#include <JKMessageBox.h>
#include <memory>
#include <string>
#include <vector>

namespace jk {

class JKListBox;
class JKStatic;

// 원본 BOMBMAN.H BombBase의 SDL2 포팅.
// text: 탄약 종류, counts: 본부/1중대/2중대/3중대 보유 수량.
struct BombStock {
    std::string text;
    int counts[4] = { 0, 0, 0, 0 };
};

// 원본 BOMBMAN.CPP BombManager의 메모리/텍스트 파일 버전.
class BombManager {
public:
    void Load();
    void Save() const;

    int FindIndexByText(const std::string& text) const;
    void AddRecord(const BombStock& rec);
    void UpdateRecord(size_t index, const BombStock& rec);
    void DeleteRecord(size_t index);
    void UnitTotals(int out[4]) const;

    std::vector<BombStock> stocks;
    std::string fileName = "EQBOMB.DAT";
};

// 원본 EQUIPWIN.CPP EquipWindow의 SDL2 포팅.
// 탄약 종류별 4개 부대(본부/1/2/3중대) 보유 수량을 관리한다.
class EquipDialog : public JKDialog {
public:
    explicit EquipDialog(const std::string& budae = std::string());
    ~EquipDialog() override;

    void RefreshAll();

private:
    void BuildControls();
    void ShowInputDialog(bool modify);
    void ShowTotals();
    void ConfirmDelete();
    void UpdateStatusLine();
    void ShowMessageModal(const std::string& title, const std::string& msg,
                          JKMessageBox::Buttons buttons = JKMessageBox::Buttons::Ok,
                          const std::function<void(int)>& onResult = nullptr);

    BombManager bombMan_;
    std::string budae_;
    JKListBox* listBox_ = nullptr;
    JKStatic* statusLine_ = nullptr;
    std::unique_ptr<JKDialog> inputDlg_;
    std::unique_ptr<JKMessageBox> msgBox_;
};

} // namespace jk

#endif // EQUIPAPP_H