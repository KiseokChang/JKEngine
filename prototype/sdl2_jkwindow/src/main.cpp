#include <JKApplication.h>
#include <JKWindow.h>
#include <JKControl.h>
#include <JKStatic.h>
#include <JKButton.h>
#include <JKCheckBox.h>
#include <JOClock.h>
#include <JKEdit.h>
#include <JKScrollBar.h>
#include <JKListBox.h>
#include <JKComboBox.h>
#include <JKFileDialog.h>
#include <JKMenu.h>
#include <JKMessageBox.h>
#include <JKDataFile.h>
#include <JKDC.h>
#include <JKEvent.h>
#include <SDL.h>
#include "wancode.h"
#include <cstdint>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

class ColorBox : public jk::JKControl {
public:
    ColorBox(uint8_t r, uint8_t g, uint8_t b)
        : color_{ r, g, b, 255 } {
    }

    void OnPaintClient(jk::JKDC& dc) override {
        const jk::JKRect rc = GetScreenRect();
        dc.SetColor(color_.r, color_.g, color_.b, color_.a);
        dc.FillRect(rc);
        dc.SetColor(0, 0, 0, 255);
        dc.DrawRect(rc);
    }

    void RespondMessage(const jk::JKEvent& ev) override {
        if (ev.type == jk::JKEventType::MouseDown) {
            color_.r = static_cast<uint8_t>(255 - color_.r);
            color_.g = static_cast<uint8_t>(255 - color_.g);
            color_.b = static_cast<uint8_t>(255 - color_.b);
        }
    }

private:
    SDL_Color color_ = { 0, 0, 0, 255 };
};

// UTF-8 -> KSSM (KS C 5601-1987 combination-form Hangul/Hanja) helper.
// JKDC::TextOut expects 0x80-prefixed byte pairs in the KSSM/Johab encoding
// used by the original JKENGINE font renderer. The engine stores a
// completion-form -> KSSM conversion table in wCodeTable; we reverse it
// after converting UTF-8 to EUC-KR completion form. Hanja characters are
// mapped to the KSSM Hanja range using the inverse of the KSSM2KS formula.
#ifdef _WIN32
// Avoid including <windows.h> in this translation unit; it conflicts with the
// legacy typedef.h (BOOL/BYTE/WORD/etc.) pulled in by wancode.h. The two APIs
// below are in kernel32 and are linked implicitly on Windows.
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
#ifndef MB_ERR_INVALID_CHARS
#define MB_ERR_INVALID_CHARS 0x00000008
#endif

extern "C" __stdcall int MultiByteToWideChar(unsigned int CodePage,
                                             unsigned long dwFlags,
                                             const char* lpMultiByteStr,
                                             int cbMultiByte,
                                             wchar_t* lpWideCharStr,
                                             int cchWideChar);
extern "C" __stdcall int WideCharToMultiByte(unsigned int CodePage,
                                             unsigned long dwFlags,
                                             const wchar_t* lpWideCharStr,
                                             int cchWideChar,
                                             char* lpMultiByteStr,
                                             int cbMultiByte,
                                             const char* lpDefaultChar,
                                             int* lpUsedDefaultChar);

std::string Utf8ToKssm(const char* utf8) {
    if (!utf8 || !utf8[0]) return {};

    // 1. UTF-8 -> EUC-KR (CP949) byte stream.
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
    if (wlen <= 0) return {};
    std::vector<wchar_t> wbuf(static_cast<size_t>(wlen));
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wbuf.data(), wlen);
    int elen = WideCharToMultiByte(949, 0, wbuf.data(), -1, nullptr, 0, nullptr, nullptr);
    if (elen <= 0) return {};
    std::string euc(static_cast<size_t>(elen) - 1, '\0');
    WideCharToMultiByte(949, 0, wbuf.data(), -1, &euc[0], elen, nullptr, nullptr);

    // 2. EUC-KR completion form -> KSSM combination form via wCodeTable.
    std::string out;
    out.reserve(euc.size());
    for (size_t i = 0; i < euc.size(); ) {
        unsigned char c1 = static_cast<unsigned char>(euc[i]);
        if (c1 < 0x80) {
            out.push_back(static_cast<char>(c1));
            ++i;
            continue;
        }
        if (i + 1 >= euc.size()) {
            out.push_back('?');
            ++i;
            continue;
        }
        unsigned char c2 = static_cast<unsigned char>(euc[i + 1]);
        uint16_t kssm = 0;
        if (c1 >= 0xB0 && c1 <= 0xC8) {
            int idx = (c1 - 0xB0) * 94 + (c2 - 0xA1);
            if (idx >= 0 && idx < NUMHANGUL) kssm = wCodeTable[idx];
        } else if (c1 == 0xA4 && c2 >= 0xA1 && c2 < 0xA1 + SINGLEHAN) {
            kssm = SingleHan[c2 - 0xA1];
        } else if (c1 >= 0xCA && c1 <= 0xFD && c2 >= 0xA1 && c2 <= 0xFE) {
            // EUC-KR Hanja -> KSSM Hanja (inverse of KSSM2KS).
            int tmp = (c1 - 0xCA) * 94 + (c2 - 0xA1);
            uint8_t kc1 = static_cast<uint8_t>(0xE0 + tmp / 188);
            uint8_t raw = static_cast<uint8_t>(0x31 + (tmp % 188));
            uint8_t kc2 = raw + (raw > 0x7E ? 18 : 0);
            kssm = static_cast<uint16_t>((kc1 << 8) | kc2);
        }
        if (kssm) {
            out.push_back(static_cast<char>(kssm >> 8));
            out.push_back(static_cast<char>(kssm & 0xFF));
        } else {
            out.push_back('?');
        }
        i += 2;
    }
    return out;
}
#else
std::string Utf8ToKssm(const char* utf8) { return utf8 ? utf8 : ""; }
#endif

// Build a raw KSSM special-character byte pair. The original JKENGINE stores
// special/glyph symbols in special.fnt and addresses them with first byte 0xD4
// and the glyph index as the second byte. These pairs bypass the UTF-8 conversion
// and are fed directly to JKDC::TextOut.
std::string KssmSpecial(uint8_t idx) {
    return std::string{ static_cast<char>(0xD4), static_cast<char>(idx) };
}

class TestWindow : public jk::JKWindow {
public:
    TestWindow() : jk::JKWindow("Multi Test Window") {
        SetAttrFlags(jk::WA_TITLEMOVEABLE | jk::WA_BORDERRESIZABLE);
        SetBackColor(255, 255, 0);   // ClientColor[0]
    }

    void AddDemoControls() {
        // 원본 TESTWIN의 버튼 / 시계 / 체크박스를 재현.
        auto button = std::make_unique<jk::JKButton>(jk::JKRect{ 10, 10, 90, 30 }, 101);
        button->SetText("Click Me");
        button->SetOnClick([]() { std::printf("Button clicked!\n"); });
        AddControl(std::move(button));

        auto clock = std::make_unique<jk::JOClock>(jk::JKRect{ 10, 48, 90, 22 }, 0);
        AddControl(std::move(clock));

        auto checkbox = std::make_unique<jk::JKCheckBox>(jk::JKRect{ 10, 80, 120, 24 }, 102);
        checkbox->SetText("Option");
        AddControl(std::move(checkbox));

        auto edit = std::make_unique<jk::JKEdit>(jk::JKRect{ 10, 112, 220, 26 }, 103, 100, false);
        edit->SetText("Type here");
        AddControl(std::move(edit));

        auto memo = std::make_unique<jk::JKEdit>(jk::JKRect{ 10, 146, 220, 90 }, 104, 1000, true);
        memo->SetText("Line 1\nLine 2\nLine 3");
        AddControl(std::move(memo));

        auto list = std::make_unique<jk::JKListBox>(jk::JKRect{ 10, 242, 120, 80 }, 105);
        list->AddString("Apple");
        list->AddString("Banana");
        list->AddString("Cherry");
        list->AddString("Date");
        list->AddString("Elderberry");
        AddControl(std::move(list));

        auto combo = std::make_unique<jk::JKComboBox>(jk::JKRect{ 140, 242, 90, 24 }, 106);
        combo->AddString("Red");
        combo->AddString("Green");
        combo->AddString("Blue");
        AddControl(std::move(combo));
    }

    void OnPaintClient(jk::JKDC& dc) override {
        jk::JKWindow::OnPaintClient(dc);
        // 원본 testwin의 커스텀 클라이언트 그리기를 간략화
        const jk::JKRect client = GetScreenClientRect();
        dc.SetColor(0, 0, 0, 255);
        dc.DrawLine(client.x + 10, client.y + 10,
                    client.x + client.w - 10, client.y + client.h - 10);

        // Bitmap-font text output test (ASCII + KSSM Hangul/Hanja/Special).
        dc.SetTextColor(0, 0, 0);
        dc.TextOut(jk::JKPoint{ client.x + 10, client.y + 20 },
                   Utf8ToKssm("안녕, JKENGINE!").c_str());
        // JKRect은 (x, y, w, h) 형식이므로 right/bottom이 아닌 너비/높이를 전달한다.
        dc.TextOutX(jk::JKRect{ client.x + 10, client.y + 40,
                                client.w - 20, 20 },
                    Utf8ToKssm("중앙 정렬 텍스트").c_str(),
                    jk::ADJ_XYCENTER, false);

        // Hanja output test.
        dc.TextOut(jk::JKPoint{ client.x + 10, client.y + 60 },
                   Utf8ToKssm("漢字: 漢字測試").c_str());

        // Special-character output test (raw KSSM 0xD4xx indices into special.fnt).
        std::string specialLine = Utf8ToKssm("특수: ");
        specialLine += KssmSpecial(0x01) + KssmSpecial(0x02) + KssmSpecial(0x03)
                      + KssmSpecial(0x10) + KssmSpecial(0x11) + KssmSpecial(0x12)
                      + KssmSpecial(0x20) + KssmSpecial(0x21) + KssmSpecial(0x22);
        dc.TextOut(jk::JKPoint{ client.x + 10, client.y + 80 }, specialLine.c_str());

        // 원본 TESTWIN의 Pieslice 그리기 테스트.
        dc.SetColor(0, 0, 255, 255);
        dc.Pieslice(jk::JKPoint{ client.x + 150, client.y + 150 },
                    M_PI / 3.0, M_PI * 5.0 / 6.0, 60);
        dc.SetColor(255, 0, 0, 255);
        dc.Pieslice(jk::JKPoint{ client.x + 146, client.y + 148 },
                    M_PI * 5.0 / 6.0, M_PI / 3.0, 60);
    }
};

// 메인 윈도우: 오른쪽 마우스 버튼을 누르면 새 TestWindow를 생성한다.
class MainWindow : public jk::JKWindow {
public:
    MainWindow() : jk::JKWindow("JKENGINE SDL2 Prototype") {
    }

    void RespondMessage(const jk::JKEvent& ev) override {
        if (ev.type == jk::JKEventType::MouseDown && ev.detail == SDL_BUTTON_RIGHT) {
            auto newWin = std::make_unique<TestWindow>();
            newWin->SetWindowRect(jk::JKRect{ ev.x, ev.y, 250, 250 });
            newWin->AddDemoControls();

            auto innerBox = std::make_unique<ColorBox>(255, 165, 0);
            innerBox->SetRect(jk::JKRect{ 110, 10, 80, 80 });
            innerBox->SetControlId(100 + windowCounter_);
            newWin->AddControl(std::move(innerBox));

            AddControl(std::move(newWin));
            ++windowCounter_;
            return;
        }
        jk::JKWindow::RespondMessage(ev);
    }

private:
    int windowCounter_ = 1;
};

class MyApp : public jk::JKApplication {
public:
    void OnInit() override {
        // JKApplication::Init이 mainWindow를 물리 픽셀 렌더러 크기로 맞춘다.
        auto main = std::make_unique<MainWindow>();

        // mainWindow 클라이언트 영역에 직접 배치된 색상 박스
        auto box1 = std::make_unique<ColorBox>(200, 50, 50);
        box1->SetRect(jk::JKRect{ 20, 20, 100, 100 });
        box1->SetControlId(1);
        main->AddControl(std::move(box1));

        auto box2 = std::make_unique<ColorBox>(50, 150, 50);
        box2->SetRect(jk::JKRect{ 140, 20, 100, 100 });
        box2->SetControlId(2);
        main->AddControl(std::move(box2));

        auto box3 = std::make_unique<ColorBox>(50, 50, 200);
        box3->SetRect(jk::JKRect{ 260, 20, 100, 100 });
        box3->SetControlId(3);
        main->AddControl(std::move(box3));

        // 떠 있는 TestWindow: 원본 testwin과 동일한 초기 위치/크기
        auto testWin = std::make_unique<TestWindow>();
        testWin->SetWindowRect(jk::JKRect{ 50, 50, 250, 250 });
        testWin->AddDemoControls();

        // TestWindow 클라이언트 영역에 배치된 자식 컨트롤
        auto innerBox = std::make_unique<ColorBox>(255, 165, 0);
        innerBox->SetRect(jk::JKRect{ 110, 10, 80, 80 });
        innerBox->SetControlId(10);
        testWin->AddControl(std::move(innerBox));

        main->AddControl(std::move(testWin));

        SetMainWindow(std::move(main));

        // Phase 2 layout demonstration: a box anchored to the bottom-right corner.
        {
            auto anchorBox = std::make_unique<ColorBox>(200, 50, 50);
            (*anchorBox).SetRect(jk::JKRect{ 0, 0, 80, 80 });
            (*anchorBox).SetAnchor(jk::ANCHOR_RIGHT | jk::ANCHOR_BOTTOM);
            (*anchorBox).SetMargins(20, 20, 20, 20);
            (*anchorBox).SetControlId(100);
            (*GetMainWindow()).AddControl(std::move(anchorBox));
        }

        // Phase 4 data demonstration: create a JKDBASE-compatible file and read it back.
        {
            jk::JKDataFile db;
            if (db.Create("prototest", 0x1234, 0, 32)) {
                std::vector<uint8_t> rec(32, 'A');
                int16_t idx = db.AddRecord(rec);
                std::vector<uint8_t> read = db.ReadRecord(static_cast<uint16_t>(idx));
                std::printf("JKDataFile test: added record %d, read back %zu bytes\n",
                            idx, read.size());
            }
        }

        fileDialog_ = std::make_unique<jk::JKFileDialog>();
        fileDialog_->SetFilter("*.*");
        fileDialog_->SetOnOk([](const std::string& path) {
            std::printf("JKFileDialog OK: %s\n", path.c_str());
        });
        fileDialog_->SetOnCancel([]() {
            std::printf("JKFileDialog Cancel\n");
        });

        aboutBox_ = std::make_unique<jk::JKMessageBox>(
            "About", "JKENGINE SDL2 Prototype - Phase 0 UI Controls",
            jk::JKMessageBox::Buttons::Ok,
            [](int) { std::printf("About box closed\n"); });

        auto menu = std::make_unique<jk::JKMenu>(jk::JKRect{ 0, 0, 640, 20 }, 200);
        menu->AddMenu("File", {
            jk::JKMenuItem{ "Open", 201, [this]() {
                if (fileDialog_ && !GetModalWindow()) fileDialog_->Show();
            }},
            jk::JKMenuItem{ "Save", 202, []() { std::printf("Save clicked\n"); }},
            jk::JKMenuItem{ "Exit", 203, []() { std::printf("Exit clicked\n"); }}
        });
        menu->AddMenu("Help", {
            jk::JKMenuItem{ "About", 204, [this]() {
                if (aboutBox_ && !GetModalWindow()) aboutBox_->Show();
            }}
        });
        GetMainWindow()->AddControl(std::move(menu));
    }

    bool PreProcessMessage(const jk::JKEvent& ev) override {
        if (ev.type == jk::JKEventType::KeyDown) {
            std::printf("KeyDown: %u\n", ev.keyCode);
            if (ev.keyCode == SDLK_ESCAPE) {
                if (GetModalWindow()) {
                    // 모달이 열려 있으면 먼저 닫고 앱은 종료하지 않는다.
                    GetModalWindow()->RequestClose();
                    SetModalWindow(nullptr);
                    return true;
                }
                return false; // 루프 종료
            }
            if (ev.keyCode == SDLK_f && fileDialog_ && !GetModalWindow()) {
                fileDialog_->Show();
            }
            if (ev.keyCode == SDLK_m && aboutBox_ && !GetModalWindow()) {
                aboutBox_->Show();
            }
        }
        return jk::JKApplication::PreProcessMessage(ev);
    }

private:
    std::unique_ptr<jk::JKFileDialog> fileDialog_;
    std::unique_ptr<jk::JKMessageBox> aboutBox_;
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    MyApp app;
    if (!app.Init("JKENGINE SDL2 Prototype", 640, 480)) {
        return 1;
    }

    return app.Run();
}