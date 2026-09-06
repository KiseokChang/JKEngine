#include <apps/ClientTestWindowApp.h>

#include <JKButton.h>
#include <JKCheckBox.h>
#include <JKComboBox.h>
#include <JKDC.h>
#include <JKEdit.h>
#include <JKEvent.h>
#include <JKHangulUtil.h>
#include <JKListBox.h>
#include <JKMessageBox.h>
#include <JKStatic.h>
#include <JKTypes.h>
#include <JKWindow.h>
#include <JOClock.h>

#include <apps/AppUtil.h>

#include <cstdio>
#include <string>

namespace jk {
namespace {

// MinGW <cmath>는 _USE_MATH_DEFINES 없이는 M_PI를 안 준다.
constexpr double kPi = 3.14159265358979323846;

// main.cpp와 동일: KSSM 특수문자는 0xD4xx 2바이트 쌍으로 TextOut에 직행.
std::string KssmSpecial(uint8_t idx) {
    return std::string{ static_cast<char>(0xD4), static_cast<char>(idx) };
}

// main.cpp의 TestWindow(250x250, 컨트롤 밀집)를 서버 모드용으로 넓게 재배치한
// 패널. 좌측 컬럼 = 입력 컨트롤, 우측 컬럼 = 텍스트/그리기 출력 데모, 하단 =
// 메시지 박스 트리거 + 상태줄. 컨트롤 간 세로 30px 이상, 영역 간 20px 이상의
// 여유 간격을 유지한다.
class TestPanelWindow : public JKWindow {
public:
    TestPanelWindow() : JKWindow("Test Window") {
        SetWindowRect(JKRect{ 0, 0, 760, 560 });
        SetBackColor(255, 255, 0);  // main.cpp TestWindow의 ClientColor[0]
        BuildControls();
        UpdateStatus("Ready.");
    }

    void OnPaintClient(JKDC& dc) override {
        JKWindow::OnPaintClient(dc);

        // 우측 데모 컬럼: 원본 TESTWIN의 KSSM 텍스트/특수문자/파이슬라인 출력.
        const JKRect client = GetScreenClientRect();
        const int dx = client.x + 600;
        dc.SetTextColor(0, 0, 0);
        dc.TextOut(JKPoint{ dx, client.y + 40 }, Utf8ToKssm("안녕, JKENGINE!").c_str());
        dc.TextOutX(JKRect{ dx, client.y + 64, 140, 20 },
                    Utf8ToKssm("중앙 정렬 텍스트").c_str(), ADJ_XYCENTER, false);
        dc.TextOut(JKPoint{ dx, client.y + 88 }, Utf8ToKssm("漢字: 漢字測試").c_str());
        std::string specialLine = Utf8ToKssm("특수: ");
        specialLine += KssmSpecial(0x01) + KssmSpecial(0x02) + KssmSpecial(0x03)
                      + KssmSpecial(0x10) + KssmSpecial(0x11) + KssmSpecial(0x12)
                      + KssmSpecial(0x20) + KssmSpecial(0x21) + KssmSpecial(0x22);
        dc.TextOut(JKPoint{ dx, client.y + 112 }, specialLine.c_str());

        dc.SetColor(0, 0, 255, 255);
        dc.Pieslice(JKPoint{ dx + 50, client.y + 330 }, kPi / 3.0,
                    kPi * 5.0 / 6.0, 56);
        dc.SetColor(255, 0, 0, 255);
        dc.Pieslice(JKPoint{ dx + 46, client.y + 328 }, kPi * 5.0 / 6.0,
                    kPi / 3.0, 56);
    }

private:
    void BuildControls() {
        // --- 1행: 버튼 / 시계 / 체크박스 ---
        btnClick_ = new JKButton(JKRect{ 20, 20, 170, 44 }, 101);
        btnClick_->SetText("Clicked: 0");
        btnClick_->SetOnClick([this]() {
            ++clickCount_;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Clicked: %d", clickCount_);
            btnClick_->SetText(buf);
            btnClick_->Invalidate();
            UpdateStatus("Button clicked.");
        });
        AddControl(std::unique_ptr<JKButton>(btnClick_));

        JOClock* clock = new JOClock(JKRect{ 210, 30, 110, 26 }, 102);
        AddControl(std::unique_ptr<JOClock>(clock));

        JKCheckBox* chkOption = new JKCheckBox(JKRect{ 360, 32, 190, 26 }, 103);
        chkOption->SetText("Option");
        AddControl(std::unique_ptr<JKCheckBox>(chkOption));

        // --- 2행: 한 줄 입력 / 콤보박스 ---
        JKEdit* edit = new JKEdit(JKRect{ 20, 100, 340, 34 }, 104, 100, false);
        edit->SetText("Type here");
        AddControl(std::unique_ptr<JKEdit>(edit));

        JKComboBox* combo = new JKComboBox(JKRect{ 400, 100, 170, 34 }, 105);
        combo->AddString("Red");
        combo->AddString("Green");
        combo->AddString("Blue");
        combo->SetSelectedIndex(0);
        AddControl(std::unique_ptr<JKComboBox>(combo));

        // --- 3행: 리스트박스 / 여러 줄 메모 ---
        list_ = new JKListBox(JKRect{ 20, 170, 210, 220 }, 106);
        list_->SetBackColor(255, 255, 255);
        list_->SetTextColor(0, 0, 0);
        list_->AddString("Apple");
        list_->AddString("Banana");
        list_->AddString("Cherry");
        list_->AddString("Date");
        list_->AddString("Elderberry");
        list_->AddString("Fig");
        list_->SetOnSelect([this](int32_t index) {
            if (index < 0) {
                return;
            }
            UpdateStatus("Selected: " + list_->GetString(static_cast<size_t>(index)));
        });
        AddControl(std::unique_ptr<JKListBox>(list_));

        JKEdit* memo = new JKEdit(JKRect{ 270, 170, 310, 220 }, 107, 1000, true);
        memo->SetText("Line 1\nLine 2\nLine 3");
        AddControl(std::unique_ptr<JKEdit>(memo));

        // --- 하단: 메시지 박스 트리거 + 상태줄 ---
        JKButton* btnMsgOk = new JKButton(JKRect{ 20, 430, 150, 44 }, 108);
        btnMsgOk->SetText("Message Box");
        btnMsgOk->SetOnClick([this]() {
            apputil::ShowModalMessage(this, msgBox_, "Test Window",
                                      "Hello from the\nTest Window!",
                                      JKMessageBox::Buttons::Ok,
                                      [this](int) {
                                          UpdateStatus("MessageBox: OK");
                                      });
        });
        AddControl(std::unique_ptr<JKButton>(btnMsgOk));

        JKButton* btnMsgYesNo = new JKButton(JKRect{ 220, 430, 150, 44 }, 109);
        btnMsgYesNo->SetText("Yes / No");
        btnMsgYesNo->SetOnClick([this]() {
            apputil::ShowModalMessage(this, msgBox_, "Test Window",
                                      "Pick one.", JKMessageBox::Buttons::YesNo,
                                      [this](int result) {
                                          UpdateStatus(
                                              result == JKMessageBox::ResultYes
                                                  ? "MessageBox: Yes"
                                                  : "MessageBox: No");
                                      });
        });
        AddControl(std::unique_ptr<JKButton>(btnMsgYesNo));

        status_ = new JKStatic(JKRect{ 20, 490, 710, 26 }, 110);
        status_->SetAdjustFlag(ADJ_YCENTER);
        AddControl(std::unique_ptr<JKStatic>(status_));
    }

    void UpdateStatus(const std::string& text) {
        if (status_) {
            status_->SetText(text);
            status_->Invalidate();
        }
    }

    JKButton* btnClick_ = nullptr;
    JKListBox* list_ = nullptr;
    JKStatic* status_ = nullptr;
    std::unique_ptr<JKMessageBox> msgBox_;
    int clickCount_ = 0;
};

} // namespace

class ClientTestWindowApp::Impl {
public:
    // minesweeper/tetris 모듈과 동일한 Impl 패턴 유지. 아이콘 로드 등 확장을
    // 위한 자리표.
};

ClientTestWindowApp::ClientTestWindowApp() : impl_(std::make_unique<Impl>()) {}
ClientTestWindowApp::~ClientTestWindowApp() = default;

void ClientTestWindowApp::OnInit() {
    auto main = std::make_unique<TestPanelWindow>();
    // 루트 윈도우는 서피스 전체를 채운다(타이틀 바 텍스트는 루트가 그리고
    // 닫기/이동/리사이즈 크롬은 서버가 오버레이한다).
    main->SetWindowRect(JKRect{ 0, 0, 760, 560 });

    SetMainWindow(std::move(main));
    SetTimerInterval(500);  // JOClock 갱신
}

} // namespace jk