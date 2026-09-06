#ifndef INSAAPP_H
#define INSAAPP_H

// 원본 WINDBASE/JANGO의 인사 관리(INSAWIN.CPP + PERSNMAN/PERSNREC)를
// SDL2 JKWindow 프레임워크로 포팅한 앱.

#include <JKDialog.h>
#include <JKMessageBox.h>
#include <memory>
#include <string>
#include <vector>

namespace jk {

class JKListBox;
class JKStatic;

// 원본 PERSNREC.H PersonRecord의 핵심 필드 포팅.
struct PersonRec {
    std::string name;       // 성명
    std::string rank;       // 계급: Officer / NCO / Enlisted (간부/하사/병)
    std::string serial;     // 군번 번호
    std::string unit;       // 부대
    std::string birth;      // 생년월일
    std::string enlist;     // 입대일
    std::string specialty;  // 특기
};

// 원본 PERSNMAN.CPP PersonManager의 메모리/텍스트 파일 버전.
class PersonManager {
public:
    void Load();
    void Save() const;

    int FindIndexByName(const std::string& name) const;
    int FindIndexBySerial(const std::string& serial) const;
    void AddRecord(const PersonRec& rec);
    void UpdateRecord(size_t index, const PersonRec& rec);
    void DeleteRecord(size_t index);
    void RankCounts(int& officers, int& ncos, int& enlisted) const;

    std::vector<PersonRec> persons;
    std::string fileName = "INSA.DAT";
};

// 원본 INSAWIN.CPP InsaWindow의 SDL2 포팅.
// 인원 명단 관리: 추가/삭제/수정/검색/통계.
class InsaDialog : public JKDialog {
public:
    explicit InsaDialog(const std::string& budae = std::string());
    ~InsaDialog() override;

    void RefreshAll();

private:
    void BuildControls();
    void RebuildList();
    void ShowInputDialog(bool modify);
    void ShowSearchDialog();
    void ShowTotals();
    void ConfirmDelete();
    void UpdateStatusLine();
    void ShowMessageModal(const std::string& title, const std::string& msg,
                          JKMessageBox::Buttons buttons = JKMessageBox::Buttons::Ok,
                          const std::function<void(int)>& onResult = nullptr);

    PersonManager personMan_;
    std::string budae_;
    JKListBox* listBox_ = nullptr;
    JKStatic* statusLine_ = nullptr;
    std::unique_ptr<JKDialog> inputDlg_;
    std::unique_ptr<JKMessageBox> msgBox_;
    std::vector<size_t> rows_;  // listBox_ 행 -> persons_ 인덱스
};

} // namespace jk

#endif // INSAAPP_H