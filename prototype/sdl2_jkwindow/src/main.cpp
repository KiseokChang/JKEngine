#ifdef _WIN32
// Avoid pulling in the full Windows headers, which conflict with JKENGINE's
// legacy typedef.h. We only need AllocConsole for the /? help path.
extern "C" __declspec(dllimport) int __stdcall AllocConsole(void);
#endif

#include <JKApplication.h>
#include <JKWindow.h>

#include <client/JKClientSurface.h>
#include <server/JKWindowServer.h>

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
#include <JKHangulUtil.h>
#include <JKPlatform.h>
#include <SDL.h>
#include <filesystem>

// Do not let SDL2 rename main() to SDL_main; we use a plain main() entry point
// and initialize SDL explicitly in JKApplication.
#ifdef main
#undef main
#endif
#include <fstream>

using jk::Utf8ToKssm;
#include <apps/JangoApp.h>
#include <apps/Equip24App.h>
#include <apps/EquipApp.h>
#include <apps/InsaApp.h>
#include <apps/OccApp.h>
#include <apps/PcxApp.h>
#include <apps/VectorApp.h>
#include <apps/IconEditApp.h>
#include <apps/RecogApp.h>
#include <apps/VectorFontApp.h>
#include <apps/VectorPresApp.h>
#include <apps/MineSweeperApp.h>
#include <apps/TetrisApp.h>
#include "wancode.h"
#include <cstdint>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
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
                // 모달 대화상자/팝업은 각자 RespondMessage에서 Escape를 처리한다.
                // 모달이 열려 있을 때는 앱을 종료하지 않는다.
                if (GetModalWindow()) {
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

// 포팅된 앱들의 데이터 관리자(Equip24DataManager/BombManager/PersonManager)
// 로직을 검증하는 헤드리스 자기 테스트. "test" 인자로 실행한다.
static int RunAppSelfTest() {
    using namespace jk;
    int failures = 0;
    auto check = [&failures](bool cond, const char* name) {
        std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
        if (!cond) ++failures;
    };

    // 1. Equip24DataManager
    {
        Equip24DataManager man;
        man.fileName = "test_eqp24.dat";
        Name24 n;
        n.division = "HQ";
        n.attached = "Battalion";
        n.name = "Radio PRC-77";
        n.number = "24-0001";
        Kind24 k;
        k.name = n.name;
        k.number = n.number;
        k.inUse = true;
        k.a = 2;
        k.b = 1;
        k.c = 0;
        k.date = "2026-08-28";
        const int idx = man.AddRecord(n, k);
        check(idx == 0, "equip24 add record");
        check(man.names.size() == 1 && man.kinds.size() == 1, "equip24 counts");
        Kind24 up = k;
        up.c = 3;
        man.UpdateKind(0, up);
        check(man.kinds[0].c == 3, "equip24 update kind");
        man.Save();
        Equip24DataManager man2;
        man2.fileName = "test_eqp24.dat";
        man2.Load();
        check(man2.kinds.size() == 1 && man2.kinds[0].c == 3 &&
                  man2.kinds[0].nameIndex == 0,
              "equip24 save/load roundtrip");
        man2.DeleteKind(0);
        check(man2.kinds.empty() && man2.names.size() == 1,
              "equip24 delete kind keeps name");
        man2.DeleteName(0);
        check(man2.names.empty(), "equip24 delete name");
        std::remove("test_eqp24.dat");
    }

    // 2. BombManager
    {
        BombManager man;
        man.fileName = "test_eqbomb.dat";
        BombStock s;
        s.text = "HE";
        s.counts[0] = 10;
        s.counts[1] = 20;
        s.counts[2] = 30;
        s.counts[3] = 40;
        man.AddRecord(s);
        BombStock w;
        w.text = "WP";
        w.counts[3] = 5;
        man.AddRecord(w);
        int totals[4];
        man.UnitTotals(totals);
        check(totals[0] == 10 && totals[1] == 20 && totals[2] == 30 &&
                  totals[3] == 45,
              "bomb unit totals");
        man.Save();
        BombManager man2;
        man2.fileName = "test_eqbomb.dat";
        man2.Load();
        check(man2.stocks.size() == 2 && man2.FindIndexByText("WP") == 1,
              "bomb save/load roundtrip");
        man2.DeleteRecord(0);
        check(man2.stocks.size() == 1 && man2.stocks[0].text == "WP",
              "bomb delete record");
        std::remove("test_eqbomb.dat");
    }

    // 3. PersonManager
    {
        PersonManager man;
        man.fileName = "test_insa.dat";
        PersonRec p;
        p.name = "KIM";
        p.rank = "Officer";
        p.serial = "20-1234567";
        p.unit = "HQ";
        p.birth = "1980-01-01";
        p.enlist = "2000-03-01";
        p.specialty = "INF";
        man.AddRecord(p);
        PersonRec q;
        q.name = "PARK";
        q.rank = "Enlisted";
        q.serial = "23-7654321";
        man.AddRecord(q);
        int officers = 0, ncos = 0, enlisted = 0;
        man.RankCounts(officers, ncos, enlisted);
        check(officers == 1 && enlisted == 1 && ncos == 0, "person rank counts");
        man.Save();
        PersonManager man2;
        man2.fileName = "test_insa.dat";
        man2.Load();
        check(man2.persons.size() == 2, "person save/load roundtrip");
        check(man2.FindIndexByName("park") == 1, "person search by name");
        man2.DeleteRecord(1);
        check(man2.persons.size() == 1, "person delete");
        std::remove("test_insa.dat");
    }

    // 4. OccDataManager (2CAOCC)
    {
        OccDataManager man;
        man.fileName = "test_occ.dat";
        OccTarget t1;
        t1.name = "OBJ-1";
        t1.type = "Armor";
        t1.x = 250;
        t1.y = 750;
        man.AddRecord(t1);
        OccTarget t2;
        t2.name = "OBJ-2";
        t2.type = "Air";
        t2.x = 1500;
        t2.y = 300;
        man.AddRecord(t2);
        check(man.targets.size() == 2, "occ add record");
        OccTarget u;
        u.name = "OBJ-1U";
        u.type = "Artillery";
        u.x = 300;
        u.y = 400;
        man.UpdateRecord(0, u);
        check(man.targets[0].name == "OBJ-1U" && man.targets[0].x == 300,
              "occ update record");
        man.UpdateRecord(99, u);
        man.DeleteRecord(99);
        check(man.targets.size() == 2, "occ out-of-range keeps records");
        man.Save();
        OccDataManager man2;
        man2.fileName = "test_occ.dat";
        man2.Load();
        check(man2.targets.size() == 2 && man2.targets[1].type == "Air" &&
                  man2.targets[1].x == 1500,
              "occ save/load roundtrip");
        man2.DeleteRecord(0);
        check(man2.targets.size() == 1 && man2.targets[0].name == "OBJ-2",
              "occ delete record");
        std::remove("test_occ.dat");
    }

    // 5. OccUnitManager / OccFireManager (2CAOCC Phase 2)
    {
        OccUnitManager um;
        um.fileName = "test_occunit.dat";
        OccUnit u1;
        u1.name = "1BAT-1";
        u1.type = "Howitzer";
        u1.status = 1;
        u1.ammo = 120;
        u1.x = 400;
        u1.y = 600;
        um.AddUnit(u1);
        OccUnit u2;
        u2.name = "2ROK-1";
        u2.type = "Rocket";
        u2.status = 2;
        u2.ammo = 36;
        u2.x = 900;
        u2.y = 1400;
        um.AddUnit(u2);
        check(um.units.size() == 2, "occ unit add");
        OccUnit mu;
        mu.name = "1BAT-1U";
        mu.type = "Air";
        mu.status = 0;
        mu.ammo = 10;
        mu.x = 100;
        mu.y = 200;
        um.UpdateUnit(0, mu);
        check(um.units[0].name == "1BAT-1U" && um.units[0].ammo == 10,
              "occ unit update");
        um.UpdateUnit(99, mu);
        um.DeleteUnit(99);
        check(um.units.size() == 2, "occ unit out-of-range keeps records");
        um.Save();
        OccUnitManager um2;
        um2.fileName = "test_occunit.dat";
        um2.Load();
        check(um2.units.size() == 2 && um2.units[1].type == "Rocket" &&
                  um2.units[1].y == 1400,
              "occ unit save/load roundtrip");
        um2.DeleteUnit(0);
        check(um2.units.size() == 1 && um2.units[0].name == "2ROK-1",
              "occ unit delete");
        std::remove("test_occunit.dat");

        OccFireManager fm;
        fm.fileName = "test_occfire.dat";
        OccFireOrder o1;
        o1.unitName = "1BAT-1";
        o1.targetName = "OBJ-1";
        o1.targetType = 0;
        o1.fireType = 0;
        o1.time = 5;
        fm.AddOrder(o1);
        OccFireOrder o2;
        o2.unitName = "2ROK-1";
        o2.targetName = "OBJ-2";
        o2.targetType = 3;
        o2.fireType = 1;
        o2.time = 30;
        fm.AddOrder(o2);
        check(fm.orders.size() == 2, "occ fire add");
        OccFireOrder mo;
        mo.unitName = "1BAT-1";
        mo.targetName = "OBJ-9";
        mo.targetType = 2;
        mo.fireType = 1;
        mo.time = 60;
        fm.UpdateOrder(0, mo);
        check(fm.orders[0].targetName == "OBJ-9" && fm.orders[0].time == 60,
              "occ fire update");
        fm.UpdateOrder(99, mo);
        fm.DeleteOrder(99);
        check(fm.orders.size() == 2, "occ fire out-of-range keeps records");
        fm.Save();
        OccFireManager fm2;
        fm2.fileName = "test_occfire.dat";
        fm2.Load();
        check(fm2.orders.size() == 2 && fm2.orders[1].fireType == 1 &&
                  fm2.orders[1].time == 30,
              "occ fire save/load roundtrip");
        fm2.DeleteOrder(1);
        check(fm2.orders.size() == 1 && fm2.orders[0].unitName == "1BAT-1",
              "occ fire delete");
        std::remove("test_occfire.dat");
    }

    // JKFileDialog file browsing test (headless: direct filesystem calls).
    {
        namespace fs = std::filesystem;
        fs::path testDir = fs::temp_directory_path() / "jkfiledialog_test";
        fs::create_directories(testDir / "subfolder");
        fs::path fileA = testDir / "alpha.txt";
        fs::path fileB = testDir / "beta.dat";
        {
            std::ofstream(fileA) << "alpha";
            std::ofstream(fileB) << "beta";
        }

        auto dlg = std::make_unique<jk::JKFileDialog>("Test Open");
        dlg->SetInitialDir(testDir.string());
        dlg->SetFilter("*.txt");
        dlg->Show(); // registers modal, but g_currentJKApp is null in test mode

        // Show() already calls RefreshList. Verify filter is applied.
        check(dlg->FindControlByControlId(101) != nullptr,
              "file dialog listbox exists");
        auto* list = static_cast<jk::JKListBox*>(dlg->FindControlByControlId(101));
        bool hasAlpha = false, hasBeta = false;
        for (size_t i = 0; i < list->GetCount(); ++i) {
            std::string s = list->GetString(i);
            if (s == "alpha.txt") hasAlpha = true;
            if (s == "beta.dat") hasBeta = true;
        }
        check(hasAlpha, "file dialog shows matching file");
        check(!hasBeta, "file dialog filters out non-matching file");

        // Select alpha.txt and confirm.
        list->SetSelectedIndex(static_cast<int32_t>(list->GetCount()) - 1);
        while (list->GetSelectedIndex() >= 0 &&
               list->GetString(list->GetSelectedIndex()) != "alpha.txt") {
            list->SetSelectedIndex(list->GetSelectedIndex() - 1);
        }
        check(list->GetString(list->GetSelectedIndex()) == "alpha.txt",
              "file dialog selected alpha.txt");
        dlg->ActivateSelected();
        check(dlg->GetFileName() == fileA.string(),
              "file dialog returns selected full path");

        // Cleanup.
        fs::remove_all(testDir);
    }

    // JKEdit IME routing test (headless: no SDL window is required).
    {
        auto edit = std::make_unique<jk::JKEdit>(jk::JKRect{ 0, 0, 200, 24 }, 0, 256, false);
        edit->SetFocus();

        // Simulate Korean IME pre-edit updates for "한글".
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::TextEditing;
            std::strncpy(ev.text, "한그", sizeof(ev.text) - 1);
            ev.editStart = 2;
            edit->RespondMessage(ev);
        }
        check(edit->GetText().empty(),
              "ime pre-edit does not commit to buffer");

        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::TextEditing;
            std::strncpy(ev.text, "한글", sizeof(ev.text) - 1);
            ev.editStart = 2;
            edit->RespondMessage(ev);
        }
        check(edit->GetText().empty(),
              "ime pre-edit update still not committed");

        // The IME commits the final string via SDL_TEXTINPUT (JKEventType::Char).
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::Char;
            std::strncpy(ev.text, "한글", sizeof(ev.text) - 1);
            edit->RespondMessage(ev);
        }
        std::string expectedKssm = jk::Utf8ToKssm("한글");
        check(edit->GetText() == expectedKssm,
              "ime committed text stored as KSSM");

        // Internal automata fallback (F2) should still produce Hangul.
        edit->SetText("");
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::KeyDown;
            ev.keyCode = SDLK_F2;
            edit->RespondMessage(ev);
        }
        check(edit->GetInputMode() == jk::JKEdit::InputMode::InternalHangul,
              "f2 toggles internal hangul automata");
        for (const char* p = "gksrmf"; *p; ++p) {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::KeyDown;
            ev.keyCode = SDLK_a + (*p - 'a');
            edit->RespondMessage(ev);
        }
        check(edit->GetText().size() >= 4,
              "internal automata produces multi-byte KSSM");
    }

    // JKEdit read-only: navigation works, editing is blocked.
    {
        auto edit = std::make_unique<jk::JKEdit>(jk::JKRect{ 0, 0, 200, 24 }, 0, 256, false);
        edit->SetText("readonly");
        edit->SetReadOnly(true);
        edit->SetFocus();

        jk::JKEvent ev;
        ev.type = jk::JKEventType::Char;
        std::strncpy(ev.text, "X", sizeof(ev.text) - 1);
        edit->RespondMessage(ev);
        check(edit->GetText() == "readonly",
              "read-only edit rejects char input");

        ev.type = jk::JKEventType::KeyDown;
        ev.keyCode = SDLK_RIGHT;
        edit->RespondMessage(ev);
        check(edit->GetText() == "readonly",
              "read-only edit still allows cursor movement");

        ev.keyCode = SDLK_DELETE;
        edit->RespondMessage(ev);
        check(edit->GetText() == "readonly",
              "read-only edit rejects delete key");
    }

    // JKComboBox read-only: selection cannot be changed by keyboard.
    {
        auto combo = std::make_unique<jk::JKComboBox>(jk::JKRect{ 0, 0, 120, 24 });
        combo->AddString("A");
        combo->AddString("B");
        combo->AddString("C");
        combo->SetSelectedIndex(1);
        combo->SetReadOnly(true);
        combo->SetFocus();

        jk::JKEvent ev;
        ev.type = jk::JKEventType::KeyDown;
        ev.keyCode = SDLK_DOWN;
        combo->RespondMessage(ev);
        check(combo->GetSelectedIndex() == 1,
              "read-only combo rejects keyboard selection change");
    }

    // Hangul deletion integrity: KSSM byte pairs must be deleted atomically.
    {
        auto edit = std::make_unique<jk::JKEdit>(jk::JKRect{ 0, 0, 200, 24 }, 0, 256, false);
        edit->SetFocus();

        // Insert "한글" as KSSM via TEXTINPUT.
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::Char;
            std::strncpy(ev.text, "한글", sizeof(ev.text) - 1);
            edit->RespondMessage(ev);
        }
        std::string kssm = edit->GetText();
        check(kssm.size() == 4,
              "hangul delete test starts with two KSSM pairs");

        // Backspace once should remove the last Hangul character (2 bytes).
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::KeyDown;
            ev.keyCode = SDLK_BACKSPACE;
            edit->RespondMessage(ev);
        }
        check(edit->GetText().size() == 2,
              "backspace removes one KSSM pair atomically");

        // Insert "가나다" then delete forward from the front.
        edit->SetText("");
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::Char;
            std::strncpy(ev.text, "가나다", sizeof(ev.text) - 1);
            edit->RespondMessage(ev);
        }
        check(edit->GetText().size() == 6,
              "three KSSM pairs inserted for forward delete test");
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::KeyDown;
            ev.keyCode = SDLK_HOME;
            edit->RespondMessage(ev);
            ev.keyCode = SDLK_DELETE;
            edit->RespondMessage(ev);
        }
        check(edit->GetText().size() == 4,
              "delete removes one KSSM pair atomically from front");

        // Cursor movement across KSSM pairs should land on pair boundaries.
        edit->SetText("");
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::Char;
            std::strncpy(ev.text, "한글", sizeof(ev.text) - 1);
            edit->RespondMessage(ev);
        }
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::KeyDown;
            ev.keyCode = SDLK_END;
            edit->RespondMessage(ev);
        }
        check(edit->GetText().size() == 4,
              "end key preserves KSSM buffer");
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::KeyDown;
            ev.keyCode = SDLK_LEFT;
            edit->RespondMessage(ev);
        }
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::KeyDown;
            ev.keyCode = SDLK_DELETE;
            edit->RespondMessage(ev);
        }
        check(edit->GetText().size() == 2,
              "delete after left arrow removes one KSSM pair");

        // IME commit then backspace: committed KSSM pair must delete atomically.
        edit->SetText("");
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::TextEditing;
            std::strncpy(ev.text, "한", sizeof(ev.text) - 1);
            edit->RespondMessage(ev);
        }
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::Char;
            std::strncpy(ev.text, "한", sizeof(ev.text) - 1);
            edit->RespondMessage(ev);
        }
        check(edit->GetText().size() == 2,
              "ime-committed hangul stored as one KSSM pair");
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::KeyDown;
            ev.keyCode = SDLK_BACKSPACE;
            edit->RespondMessage(ev);
        }
        check(edit->GetText().empty(),
              "backspace after ime commit removes the KSSM pair cleanly");

        // Left arrow must cross KSSM pair boundaries (2 bytes), not single bytes.
        edit->SetText("");
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::Char;
            std::strncpy(ev.text, "한글", sizeof(ev.text) - 1);
            edit->RespondMessage(ev);
        }
        check(edit->GetText().size() == 4,
              "two KSSM pairs ready for cursor boundary test");
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::KeyDown;
            ev.keyCode = SDLK_LEFT;
            edit->RespondMessage(ev);
            ev.keyCode = SDLK_BACKSPACE;
            edit->RespondMessage(ev);
        }
        check(edit->GetText().size() == 2,
              "left arrow crosses KSSM pair boundary before backspace deletes atomically");
    }

    // JKEdit caret routing and focus visibility.
    {
        auto window = std::make_unique<jk::JKWindow>("Caret Test");
        window->SetWindowRect(jk::JKRect{ 0, 0, 200, 100 });

        auto edit1 = std::make_unique<jk::JKEdit>(jk::JKRect{ 10, 10, 80, 24 }, 1);
        auto edit1Raw = edit1.get();
        window->AddControl(std::move(edit1));

        auto edit2 = std::make_unique<jk::JKEdit>(jk::JKRect{ 10, 40, 80, 24 }, 2);
        auto edit2Raw = edit2.get();
        window->AddControl(std::move(edit2));

        window->FocusFirstChild();
        check(edit1Raw->IsFocused() && edit1Raw->IsCaretVisible(),
              "focused edit shows caret initially");

        // Timer event routed through JKWindow should toggle the focused edit's caret.
        {
            jk::JKEvent ev;
            ev.type = jk::JKEventType::Timer;
            ev.targetId = window->GetWinId();
            window->RespondMessage(ev);
        }
        check(!edit1Raw->IsCaretVisible(),
              "timer event through JKWindow toggles focused edit caret off");

        // Move focus to the second edit: first caret hides, second caret shows.
        window->FocusNextChild();
        check(!edit1Raw->IsFocused() && !edit1Raw->IsCaretVisible(),
              "caret disappears when edit loses focus");
        check(edit2Raw->IsFocused() && edit2Raw->IsCaretVisible(),
              "caret appears in newly focused edit");
    }

    // Minesweeper game logic tests.
    {
        jk::MineSweeperGame game;

        game.NewGame(-1, -1);
        game.CycleMark(0, 0);
        check(game.GetMark(0, 0) == jk::MineSweeperGame::Mark::Flag,
              "flag mark toggles on");
        game.CycleMark(0, 0);
        check(game.GetMark(0, 0) == jk::MineSweeperGame::Mark::Question,
              "flag mark cycles to question");
        game.CycleMark(0, 0);
        check(game.GetMark(0, 0) == jk::MineSweeperGame::Mark::None,
              "flag mark cycles back to none");

        game.NewGame(4, 4);
        check(!game.IsGameOver(), "new minesweeper game is not over");
        check(game.OpenCell(4, 4), "first open succeeds");
        check(!game.IsMine(4, 4), "first clicked cell is never a mine");
        check(game.IsRevealed(4, 4), "first clicked cell is revealed");

        game.NewGameWithMines(9, 9, {{0, 0}});
        check(game.OpenCell(0, 0), "clicking a known mine opens it");
        check(game.IsGameOver() && !game.IsWon(),
              "clicking a mine ends game without win");

        game.NewGame(-1, -1);
        check(!game.IsGameOver(), "restart clears game-over flag");
        check(!game.IsRevealed(0, 0), "restart clears revealed cells");

        game.NewGameWithMines(9, 9, {{0, 0}});
        for (int r = 0; r < game.GetRows(); ++r) {
            for (int c = 0; c < game.GetCols(); ++c) {
                if (r == 0 && c == 0) continue;
                game.OpenCell(r, c);
            }
        }
        check(game.IsWon(), "opened all safe cells on tiny board");
        check(game.IsGameOver(), "opening all safe cells ends the game");

        // Difficulty settings.
        auto beginner = jk::MineSweeperGame::GetSettings(
            jk::MineSweeperGame::Difficulty::Beginner);
        check(beginner.rows == 9 && beginner.cols == 9 && beginner.mines == 10,
              "beginner difficulty is 9x9 with 10 mines");

        auto intermediate = jk::MineSweeperGame::GetSettings(
            jk::MineSweeperGame::Difficulty::Intermediate);
        check(intermediate.rows == 16 && intermediate.cols == 16 && intermediate.mines == 40,
              "intermediate difficulty is 16x16 with 40 mines");

        auto expert = jk::MineSweeperGame::GetSettings(
            jk::MineSweeperGame::Difficulty::Expert);
        check(expert.rows == 16 && expert.cols == 30 && expert.mines == 99,
              "expert difficulty is 16x30 with 99 mines");

        // Intermediate first-click safety.
        game.SetDifficulty(jk::MineSweeperGame::Difficulty::Intermediate);
        game.NewGame(8, 8);
        check(game.OpenCell(8, 8), "intermediate first open succeeds");
        check(!game.IsMine(8, 8), "intermediate first clicked cell is never a mine");
        check(game.GetRows() == 16 && game.GetCols() == 16,
              "intermediate board has correct dimensions");

        // Chord reveal test: 3x3 board with one mine at (0,0) and the center
        // revealed showing count 1. Flag the mine, then chord the center to
        // reveal the rest.
        game.NewGameWithMines(3, 3, {{0, 0}});
        game.OpenCell(1, 1);
        check(game.GetAdjacent(1, 1) == 1, "center sees one adjacent mine");
        game.CycleMark(0, 0);
        check(game.GetMark(0, 0) == jk::MineSweeperGame::Mark::Flag,
              "mine is flagged for chord test");
        check(game.ChordReveal(1, 1), "chord reveal opens neighbors");
        check(game.IsRevealed(0, 2), "chord reveals top-right safe cell");
        check(game.IsWon(), "chord reveals all safe cells and wins");
    }

    std::printf("AppSelfTest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

int main(int argc, char* argv[]) {
    // GUI 앱이므로 콘솔 출력이 안 보인다. 디버깅용 파일 로그를 먼저 연다.
    std::FILE* logFile = std::fopen("jkproto_launch.log", "w");
    if (logFile) {
        std::fprintf(logFile, "[main] entered argc=%d\n", argc);
        std::fflush(logFile);
    }

    // Process-wide DPI awareness must be set before any window/SDL calls.
    if (logFile) std::fprintf(logFile, "[main] before InitializeProcessDpiAwareness\n"), std::fflush(logFile);
    jk::JKPlatform::InitializeProcessDpiAwareness();
    if (logFile) std::fprintf(logFile, "[main] after InitializeProcessDpiAwareness\n"), std::fflush(logFile);

    if (argc > 1 && (std::strcmp(argv[1], "--help") == 0 ||
                     std::strcmp(argv[1], "-h") == 0 ||
                     std::strcmp(argv[1], "/?") == 0)) {
#ifdef _WIN32
        // Windows GUI 앱에서도 콘솔로 도움말이 보이도록 할당.
        if (AllocConsole()) {
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);
        }
#endif
        std::printf("jkproto_sdl2_jkwindow - JKENGINE SDL2 prototype\n");
        std::printf("\n");
        std::printf("Usage: jkproto_sdl2_jkwindow.exe [COMMAND]\n");
        std::printf("\n");
        std::printf("Commands:\n");
        std::printf("  (none)      Default demo app\n");
        std::printf("  test        Built-in self-test mode\n");
        std::printf("  jango       JANGO launcher\n");
        std::printf("  occ         OCC / fire control demo\n");
        std::printf("  pcx FILE    256-color PCX viewer\n");
        std::printf("  vector      Bezier vector editor\n");
        std::printf("  iconedit    Icon/sprite editor\n");
        std::printf("  recog       Stroke recognition demo\n");
        std::printf("  vfont       Vector font window\n");
        std::printf("  vpres       Vector font presentation\n");
        std::printf("  minesweeper App launcher (Minesweeper + Tetris)\n");
        std::printf("  tetris      Tetris game\n");
        std::printf("  --server    Run as the window server (Phase 2 scaffolding)\n");
        std::printf("  --client    Run as a client of the window server\n");
        std::printf("  -h, --help, /?  Show this help message\n");
        return 0;
    }

    if (argc > 1 && std::strcmp(argv[1], "test") == 0) {
        return RunAppSelfTest();
    }

    bool runJango = (argc > 1 && std::strcmp(argv[1], "jango") == 0);
    bool runOcc = (argc > 1 && std::strcmp(argv[1], "occ") == 0);
    bool runPcx = (argc > 1 && std::strcmp(argv[1], "pcx") == 0);
    bool runVector = (argc > 1 && std::strcmp(argv[1], "vector") == 0);
    bool runIconEdit = (argc > 1 && std::strcmp(argv[1], "iconedit") == 0);
    bool runRecog = (argc > 1 && std::strcmp(argv[1], "recog") == 0);
    bool runVectorFont = (argc > 1 && std::strcmp(argv[1], "vfont") == 0);
    bool runVectorPres = (argc > 1 && std::strcmp(argv[1], "vpres") == 0);
    bool runMineSweeper = (argc > 1 && std::strcmp(argv[1], "minesweeper") == 0);
    bool runTetris = (argc > 1 && std::strcmp(argv[1], "tetris") == 0);
    bool runServer = (argc > 1 && std::strcmp(argv[1], "--server") == 0);
    bool runClient = (argc > 1 && std::strcmp(argv[1], "--client") == 0);

    if (runPcx) {
        jk::PcxApp app((argc > 2) ? argv[2] : "");
        if (!app.Init("PCX Viewer - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runVector) {
        jk::VectorApp app;
        if (!app.Init("Vector Bezier Editor - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runIconEdit) {
        jk::IconEditApp app;
        if (!app.Init("Icon Editor - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runRecog) {
        jk::RecogApp app;
        if (!app.Init("Stroke Recognition - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runVectorFont) {
        jk::VectorFontApp app;
        if (!app.Init("Vector Font Window - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runVectorPres) {
        jk::VectorPresApp app;
        if (!app.Init("Vector Presentation - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runOcc) {
        jk::OccApp app;
        if (!app.Init("OCC - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runJango) {
        jk::JangoApp app;
        if (!app.Init("JANGO - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runMineSweeper) {
        jk::MineSweeperApp app;
        if (!app.Init("App Launcher - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runTetris) {
        jk::TetrisApp app;
        if (!app.Init("Tetris - SDL2 Port", 1920, 1080)) {
            return 1;
        }
        return app.Run();
    }

    if (runServer) {
        jk::server::JKWindowServer server;
        if (!server.Init("JKENGINE Window Server", 1280, 720)) {
            return 1;
        }
        server.StartAcceptor("\\\\.\\pipe\\JKWindowServerPipe");
        server.Run();
        return 0;
    }

    if (runClient) {
        constexpr int kW = 400;
        constexpr int kH = 300;
        constexpr const char* kPipe = "\\\\.\\pipe\\JKWindowServerPipe";

        jk::client::JKClientSurface surface(kPipe, kW, kH, "Test Client");
        if (!surface.Connect()) {
            std::fprintf(stderr, "Failed to connect to window server\n");
            return 1;
        }

        // Simple animation: fill the surface with a cycling color.
        uint8_t phase = 0;
        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
            uint8_t* pixels = surface.Pixels();
            if (!pixels) break;

            uint8_t r = 128 + static_cast<uint8_t>(std::sin(phase * 0.05) * 127);
            uint8_t g = 128 + static_cast<uint8_t>(std::sin(phase * 0.05 + 2.0) * 127);
            uint8_t b = 128 + static_cast<uint8_t>(std::sin(phase * 0.05 + 4.0) * 127);

            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    uint8_t* p = pixels + (y * kW + x) * 4;
                    p[0] = r;
                    p[1] = g;
                    p[2] = b;
                    p[3] = 255;
                }
            }

            surface.CommitFull();
            ++phase;
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
        return 0;
    }

    MyApp app;
    if (!app.Init("JKENGINE SDL2 Prototype", 1920, 1080)) {
        return 1;
    }

    return app.Run();
}