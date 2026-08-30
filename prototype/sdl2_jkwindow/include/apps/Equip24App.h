#ifndef EQUIP24APP_H
#define EQUIP24APP_H

// 원본 WINDBASE/JANGO의 2.4G 장비 관리(EQP24APP.CPP/EQP24WIN.CPP)를
// SDL2 JKWindow 프레임워크로 포팅한 앱.

#include <JKDialog.h>
#include <JKMessageBox.h>
#include <memory>
#include <string>
#include <vector>

namespace jk {

class JKListBox;
class JKStatic;

// 원본 EQP24DEF.H의 NAME24 레코드.
struct Name24 {
    std::string division;   // 소속 (HQ / 1st Co / 2nd Co / 3rd Co)
    std::string attached;   // 배속 부대
    std::string name;       // 장비 명칭
    std::string number;     // 장비 등록 번호
};

// 원본 EQP24DEF.H의 KIND24 레코드. 원본의 FirstIndex/NextIndex/PrvIndex
// 연결 리스트 대신 nameIndex(부모 NAME24 인덱스)로 연결한다.
struct Kind24 {
    std::string name;       // 장비 명칭
    std::string number;     // 장비 등록 번호
    bool inUse = true;      // Inga: 사용 여부
    int a = 0;              // A중대 수량
    int b = 0;              // B중대 수량
    int c = 0;              // C중대 수량
    std::string date;       // 등록 일자
    int nameIndex = -1;     // NAME24 레코드 인덱스
};

// 원본 EQ24DMAN.CPP Equip24DataManager의 메모리/텍스트 파일 버전.
// 원본은 DataFileManager 기반 .DAT 파일을 직접 순회했으나, 여기서는
// EQP24.DAT 텍스트 파일로 동일한 CRUD를 제공한다.
class Equip24DataManager {
public:
    void Load();
    void Save() const;

    int FindNameIndexByNumber(const std::string& number) const;
    int AddRecord(const Name24& name, const Kind24& kind);
    void UpdateKind(size_t kindIndex, const Kind24& kind);
    void DeleteKind(size_t kindIndex);
    void DeleteName(size_t nameIndex);
    void CountTotals(int& totalKinds, int& totalA, int& totalB, int& totalC) const;

    std::vector<Name24> names;
    std::vector<Kind24> kinds;
    std::string fileName = "EQP24.DAT";
};

// 원본 EQP24WIN.CPP Equip24Window의 SDL2 포팅.
// 좌측: 장비 명(NAME24) 리스트, 우측: 2.4G 장비(KIND24) 리스트.
class Equip24Dialog : public JKDialog {
public:
    explicit Equip24Dialog(const std::string& budae = std::string());
    ~Equip24Dialog() override;

    void RefreshAll();

private:
    void BuildControls();
    void RebuildNameList();
    void RebuildKindList();
    void ShowInputDialog(bool modify);
    void ShowTotals();
    void ConfirmDelete();
    void UpdateStatusLine();
    void ShowMessageModal(const std::string& title, const std::string& msg,
                          JKMessageBox::Buttons buttons = JKMessageBox::Buttons::Ok,
                          const std::function<void(int)>& onResult = nullptr);

    Equip24DataManager dataMan_;
    std::string budae_;
    JKListBox* nameList_ = nullptr;
    JKListBox* kindList_ = nullptr;
    JKStatic* statusLine_ = nullptr;
    std::vector<size_t> kindRows_;  // kindList_ 행 -> kinds_ 인덱스
    std::unique_ptr<JKDialog> inputDlg_;
    std::unique_ptr<JKMessageBox> msgBox_;
};

} // namespace jk

#endif // EQUIP24APP_H