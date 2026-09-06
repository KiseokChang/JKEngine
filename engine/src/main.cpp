#ifdef _WIN32
// Avoid pulling in the full Windows headers, which conflict with JKENGINE's
// legacy typedef.h. We only need AllocConsole for the /? help path, the
// LoadLibrary trio for app module loading (--client/--jkx), and the temp-file
// trio for extracting a module out of a .jkx container.
extern "C" __declspec(dllimport) int __stdcall AllocConsole(void);
extern "C" __declspec(dllimport) void* __stdcall LoadLibraryA(const char*);
extern "C" __declspec(dllimport) int __stdcall FreeLibrary(void*);
extern "C" __declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);
extern "C" __declspec(dllimport) unsigned long __stdcall GetTempPathA(
    unsigned long nBufferLength, char* lpBuffer);
extern "C" __declspec(dllimport) int __stdcall CreateDirectoryA(
    const char* lpPathName, void* lpSecurityAttributes);
extern "C" __declspec(dllimport) int __stdcall DeleteFileA(const char* lpFileName);
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentProcessId(void);
// PULARGE_INTEGER is really just a pointer to a 64-bit byte count; declaring
// it as unsigned long long* keeps windows.h out of this translation unit.
extern "C" __declspec(dllimport) int __stdcall GetDiskFreeSpaceExA(
    const char* lpDirectoryName,
    unsigned long long* lpFreeBytesAvailableToCaller,
    unsigned long long* lpTotalNumberOfBytes,
    unsigned long long* lpTotalNumberOfFreeBytes);
#endif

#include <JKApplication.h>
#include <JKWindow.h>

#include <client/JKClientSurface.h>
#include <server/JKWindowServer.h>

#include <terminal/JKTerminalGrid.h>
#include <terminal/JKVtParser.h>
#include <terminal/JKConPtyBridge.h>
#include <terminal/JKGlyphAtlas.h>

#include <stb_truetype.h>

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
#include <JKJkxFile.h>
#include <script/JKScriptHost.h>
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
#include <apps/PcxViews.h>
#include <apps/VectorApp.h>
#include <apps/IconEditApp.h>
#include <apps/RecogApp.h>
#include <apps/VectorFontApp.h>
#include <apps/VectorPresApp.h>
#include <apps/MineSweeperApp.h>
#include <apps/TetrisApp.h>
#include <apps/ClientScriptApp.h>
#include <apps/JKAppModule.h>
#include <apps/JKTerminalConfig.h>
#include <JKJkxFile.h>
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

// ---------------------------------------------------------------------------
// App module loading (Phase B) and .jkx container support (Phase C).
// ---------------------------------------------------------------------------

// Reads a whole file into out. Returns false when the file cannot be opened
// or read back fully.
static bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(size > 0 ? static_cast<size_t>(size) : 0);
    const bool ok = out.empty() || std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

#ifdef _WIN32
// Loads an app module DLL and runs it through the C ABI in apps/JKAppModule.h.
// All C++ (app construction, Init, Run, destruction) stays inside the module.
static int RunClientModule(const char* dllPath, const char* pipeName) {
    void* module = LoadLibraryA(dllPath);
    if (!module) {
        std::fprintf(stderr, "Cannot load app module '%s'\n", dllPath);
        return 1;
    }
    auto metaFn = reinterpret_cast<const jk::JKAppMeta* (*)()>(
        GetProcAddress(module, "jk_app_meta"));
    auto runFn = reinterpret_cast<int (*)(const char*)>(
        GetProcAddress(module, "jk_app_run_client"));
    if (!metaFn || !runFn) {
        std::fprintf(stderr,
                     "App module '%s' does not export jk_app_meta/jk_app_run_client\n",
                     dllPath);
        FreeLibrary(module);
        return 1;
    }

    const jk::JKAppMeta* meta = metaFn();
    std::printf("[client] module '%s' loaded: app='%s' title='%s' size=%dx%d\n",
                dllPath, meta->name, meta->title,
                static_cast<int>(meta->width), static_cast<int>(meta->height));
    std::fflush(stdout);
    const int rc = runFn(pipeName);
    // NOTE: no FreeLibrary() here. Unloading the MinGW-built app module
    // corrupts the heap in practice; the host process exits right after Run()
    // returns, so keeping the module resident until process termination is
    // both simpler and safer.
    return rc;
}

// --jkx <container>: open the .jkx file, extract the MODL entry to a per-process
// temp file, load and run it through the same C ABI, then delete the temp file.
// The module's jk_app_meta (not the manifest) is authoritative at runtime; the
// manifest identifies the entry and carries the launcher-side metadata.
static int RunClientFromJkx(const char* jkxPath, const char* pipeName) {
    jk::JKJkxFile jkx;
    if (!jkx.Open(jkxPath)) return 1;
    const jk::JkxManifest& mani = jkx.Manifest();

    int entry = jkx.FindEntry("MODL", mani.module);
    if (entry < 0) entry = jkx.FindEntry("MODL", "");
    if (entry < 0) {
        std::fprintf(stderr, "No module entry in '%s'\n", jkxPath);
        return 1;
    }
    std::vector<uint8_t> dll;
    if (!jkx.ReadEntry(entry, dll)) {
        std::fprintf(stderr, "Cannot read module entry from '%s'\n", jkxPath);
        return 1;
    }

    char tempDir[260] = ".";
    GetTempPathA(static_cast<unsigned long>(sizeof(tempDir) - 64), tempDir);
    // Unique temp name per process so several instances of the same .jkx can
    // run side by side; deleted again on exit.
    char tempPath[324] = {};
    std::snprintf(tempPath, sizeof(tempPath), "%sjkapp_%s_%lu.dll",
                  tempDir, mani.name.c_str(),
                  static_cast<unsigned long>(GetCurrentProcessId()));
    std::FILE* f = std::fopen(tempPath, "wb");
    if (!f) {
        std::fprintf(stderr, "Cannot extract module to '%s'\n", tempPath);
        return 1;
    }
    const size_t written = std::fwrite(dll.data(), 1, dll.size(), f);
    std::fclose(f);
    if (written != dll.size()) {
        // Short writes here are almost always a full temp volume (%TEMP% is
        // usually on C:), not a container bug — surface the free space so the
        // message is actionable instead of a bare "short write".
        unsigned long long freeBytes = 0, totalBytes = 0, totalFree = 0;
        if (GetDiskFreeSpaceExA(tempDir, &freeBytes, &totalBytes, &totalFree)) {
            std::fprintf(stderr,
                         "Short write extracting '%s' (wrote %zu of %zu bytes; "
                         "%.1f GiB free on the temp volume)\n",
                         tempPath, written, dll.size(),
                         freeBytes / (1024.0 * 1024 * 1024));
        } else {
            std::fprintf(stderr, "Short write extracting '%s' (wrote %zu of %zu bytes)\n",
                         tempPath, written, dll.size());
        }
        DeleteFileA(tempPath);
        return 1;
    }

    // Script apps (docs/27 §8.1): extract the manifest copy and the script
    // (SCRI entry) next to the DLL so the shared jkapp_script module can
    // locate its data via its own module path. The per-pid temp names keep
    // reruns from accumulating files — same rationale as the DLL above.
    if (!mani.script.empty() || jkx.FindEntry("SCRI", "") >= 0) {
        const int maniEntry = jkx.FindEntry("MANI", "manifest.txt");
        const int scriptEntry =
            jkx.FindEntry("SCRI", mani.script.empty() ? "app.js" : mani.script);
        std::vector<uint8_t> payload;
        if (maniEntry >= 0 &&
            jkx.ReadEntry(maniEntry, payload)) {
            const std::string side = std::string(tempPath) + ".manifest.txt";
            std::FILE* sf = std::fopen(side.c_str(), "wb");
            if (sf) {
                std::fwrite(payload.data(), 1, payload.size(), sf);
                std::fclose(sf);
            }
            payload.clear();
        }
        if (scriptEntry >= 0 && jkx.ReadEntry(scriptEntry, payload)) {
            const std::string side = std::string(tempPath) + ".app.js";
            std::FILE* sf = std::fopen(side.c_str(), "wb");
            if (!sf) {
                std::fprintf(stderr, "Cannot extract script to '%s'\n", side.c_str());
                return 1;
            }
            std::fwrite(payload.data(), 1, payload.size(), sf);
            std::fclose(sf);
        }
    }

    const int rc = RunClientModule(tempPath, pipeName);
    // NOTE: the temp DLL stays behind (it is still loaded — see the
    // FreeLibrary note in RunClientModule — so DeleteFileA would fail with a
    // sharing violation anyway). The per-pid name keeps reruns from
    // accumulating files.
    return rc;
}

// jkx-pack <app>: bundle jkapp_<app>.dll + launcher icon PNGs + a generated
// manifest into apps/<app>.jkx. Metadata comes from the module's own
// jk_app_meta (single source of truth); icons are optional.
//
// Script apps (docs/27 단계 1): when scripts/apps/<app>/{manifest.txt,app.js}
// exists, the authored manifest is the metadata source and the shared
// jkapp_script.dll rides in as the MODL entry — no per-app native module.
static int RunJkxPack(const char* appName) {
    std::string base;
    if (char* p = SDL_GetBasePath()) {
        base = p;
        SDL_free(p);
    }

    std::string manifestText;
    std::string moduleName;

    // Script app branch: read the authored manifest + app.js.
    std::vector<uint8_t> scriptManifest;
    std::vector<uint8_t> appJs;
    const bool isScriptApp =
        ReadWholeFile(JK_SCRIPTS_DIR "/apps/" + std::string(appName) + "/manifest.txt",
                      scriptManifest) &&
        ReadWholeFile(JK_SCRIPTS_DIR "/apps/" + std::string(appName) + "/app.js",
                      appJs);
    jk::JkxManifest authored;
    if (isScriptApp) {
        std::string text(scriptManifest.begin(), scriptManifest.end());
        if (!authored.Parse(text)) {
            std::fprintf(stderr,
                         "jkx-pack: invalid manifest for script app '%s'\n",
                         appName);
            return 1;
        }
        manifestText += "name=" +
            (authored.name.empty() ? std::string(appName) : authored.name) + "\n";
        manifestText += "title=" +
            (authored.title.empty() ? std::string(appName) : authored.title) + "\n";
        manifestText += "width=" +
            std::to_string(authored.width > 0 ? authored.width : 320) + "\n";
        manifestText += "height=" +
            std::to_string(authored.height > 0 ? authored.height : 240) + "\n";
        manifestText += "module=jkapp_script.dll\n";
        manifestText += "script=app.js\n";
        moduleName = "jkapp_script.dll";
    } else {
        const std::string dllPath = base + "jkapp_" + appName + ".dll";
        void* module = LoadLibraryA(dllPath.c_str());
        if (!module) {
            std::fprintf(stderr, "jkx-pack: cannot load '%s'\n", dllPath.c_str());
            return 1;
        }
        auto metaFn = reinterpret_cast<const jk::JKAppMeta* (*)()>(
            GetProcAddress(module, "jk_app_meta"));
        if (!metaFn) {
            std::fprintf(stderr, "jkx-pack: '%s' exports no jk_app_meta\n", dllPath.c_str());
            return 1;
        }
        const jk::JKAppMeta* meta = metaFn();
        // NOTE: intentionally not FreeLibrary()-ing here. Unloading the app module
        // mid-process corrupted the heap in practice (crash on the next malloc),
        // and the packer is a short-lived process — keep the module resident.

        manifestText += "name=" + std::string(meta->name) + "\n";
        manifestText += "title=" + std::string(meta->title) + "\n";
        manifestText += "width=" + std::to_string(meta->width) + "\n";
        manifestText += "height=" + std::to_string(meta->height) + "\n";
        manifestText += "module=jkapp_" + std::string(appName) + ".dll\n";
        moduleName = std::string("jkapp_") + appName + ".dll";
    }

    std::vector<uint8_t> dll;
    if (!ReadWholeFile(base + moduleName, dll)) {
        std::fprintf(stderr, "jkx-pack: cannot read '%s'\n",
                     (base + moduleName).c_str());
        return 1;
    }

    // Launcher icon assets (see ARCHITECTURE_DOCS/20). The minesweeper launcher
    // icon is stored as launcher_mine for historical reasons.
    const std::string iconPrefix =
        (std::strcmp(appName, "minesweeper") == 0) ? "mine" : appName;
    const std::string icon1Name = "assets/icons/launcher_" + iconPrefix + "@1x.png";
    const std::string icon2Name = "assets/icons/launcher_" + iconPrefix + "@2x.png";
    std::vector<uint8_t> icon1Data;
    std::vector<uint8_t> icon2Data;
    const bool hasIcon1 = ReadWholeFile(base + icon1Name, icon1Data);
    const bool hasIcon2 = ReadWholeFile(base + icon2Name, icon2Data);

    if (hasIcon1) manifestText += "icon=launcher@1x.png\n";
    if (hasIcon2) manifestText += "icon2x=launcher@2x.png\n";

    std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
    std::vector<uint8_t> manifestBytes(manifestText.begin(), manifestText.end());
    entries.emplace_back("manifest.txt", std::move(manifestBytes));
    entries.emplace_back(moduleName, std::move(dll));
    if (isScriptApp) entries.emplace_back("app.js", std::move(appJs));
    if (hasIcon1) entries.emplace_back("launcher@1x.png", std::move(icon1Data));
    if (hasIcon2) entries.push_back({"launcher@2x.png", std::move(icon2Data)});

    CreateDirectoryA((base + "apps").c_str(), nullptr);
    const std::string outPath = base + "apps\\" + appName + ".jkx";
    if (!jk::JKJkxFile::Write(outPath, entries)) return 1;

    std::printf("packed %s\n", outPath.c_str());
    return 0;
}

// jkx-list <file>: print a container's header (version/codec) and TOC — the
// developer counterpart to hexdumping the file (docs/21).
static int RunJkxList(const char* path) {
    jk::JKJkxFile f;
    if (!f.Open(path)) return 1;
    std::printf("%s: version=%u codec=%u entries=%d\n", path, f.Version(),
                f.Codec(), f.EntryCount());
    const jk::JkxManifest& m = f.Manifest();
    if (!m.name.empty()) {
        std::printf("  manifest: name=%s title=%s module=%s", m.name.c_str(),
                    m.title.c_str(), m.module.c_str());
        if (!m.script.empty()) std::printf(" script=%s", m.script.c_str());
        if (m.width > 0) std::printf(" %dx%d", m.width, m.height);
        std::printf("\n");
    }
    for (const auto& e : f.Entries()) {
        std::printf("  %-4s %-32s %10u bytes @ 0x%08X\n", e.type, e.name.c_str(),
                    static_cast<unsigned>(e.size), static_cast<unsigned>(e.offset));
    }
    return 0;
}

// jkx-extract <file> [entry ...]: dump all (or the named) entries into a
// "<file>_x/" directory — the same raw bytes the client host would extract.
static int RunJkxExtract(const char* path, int nameCount, char** names) {
    jk::JKJkxFile f;
    if (!f.Open(path)) return 1;
    const std::string dir = std::string(path) + "_x";
    CreateDirectoryA(dir.c_str(), nullptr);  // exists_ok
    int extracted = 0;
    for (int i = 0; i < f.EntryCount(); ++i) {
        const jk::JKJkxFile::Entry& e = f.Entries()[i];
        bool wanted = (nameCount == 0);
        for (int n = 0; n < nameCount && !wanted; ++n)
            wanted = (e.name == names[n]);
        if (!wanted) continue;
        std::vector<uint8_t> data;
        if (!f.ReadEntry(i, data)) {
            std::fprintf(stderr, "jkx-extract: read failed for '%s'\n", e.name.c_str());
            return 1;
        }
        const std::string outPath = dir + "/" + e.name;
        std::FILE* out = std::fopen(outPath.c_str(), "wb");
        if (!out || (!data.empty() &&
                     std::fwrite(data.data(), 1, data.size(), out) != data.size())) {
            std::fprintf(stderr, "jkx-extract: cannot write '%s'\n", outPath.c_str());
            if (out) std::fclose(out);
            return 1;
        }
        std::fclose(out);
        std::printf("  %s (%u bytes)\n", outPath.c_str(),
                    static_cast<unsigned>(e.size));
        ++extracted;
    }
    if (extracted == 0) {
        std::fprintf(stderr, "jkx-extract: no matching entries in '%s'\n", path);
        return 1;
    }
    return 0;
}
#endif // _WIN32

// test-script <file> (docs/27 단계 2): run an automation scenario headlessly.
// The script builds controls into a bare window and drives them through the
// v2 bindings (findControl/click/inject*/setText/getText); assert/assertEq
// failures decide the exit code.
static int RunScriptTestFile(const char* path) {
    jk::JKWindow root("script-test");
    root.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
    jk::JKScriptHost host;
    host.Attach(&root);
    jk::JKScriptTimerServices timers;  // no-op: scenarios stay synchronous
    timers.start = [](uint32_t, uint32_t) -> uint64_t { return 0; };
    timers.stop = [](uint64_t) {};
    host.SetTimerServices(std::move(timers));
    if (!host.Start(path)) {
        std::printf("[script-test] start failed: %s\n", host.LastError().c_str());
        std::fflush(stdout);
        return 1;
    }
    const int failures = host.AssertFailures();
    host.Stop();
    std::printf("[script-test] %s: %d/%d assertion(s) failed\n", path, failures,
                host.AssertChecks());
    std::fflush(stdout);
    return failures == 0 ? 0 : 1;
}

// 포팅된 앱들의 데이터 관리자(Equip24DataManager/BombManager/PersonManager)
// 로직을 검증하는 헤드리스 자기 테스트. "test" 인자로 실행한다.
static int RunAppSelfTest() {
    using namespace jk;
    int failures = 0;
    auto check = [&failures](bool cond, const char* name) {
        std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
        std::fflush(stdout);
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
        dlg->Show(); // registers modal, but g_jkAppHost is null in test mode

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

    // .jkx container roundtrip: pack, reopen, verify manifest and payloads.
    {
        const std::string path = "test_container.jkx";
        std::vector<uint8_t> payloadA(300);
        for (size_t i = 0; i < payloadA.size(); ++i) payloadA[i] = static_cast<uint8_t>(i & 0xFF);
        std::vector<uint8_t> payloadB(64, 0xAB);

        std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
        entries.emplace_back("manifest.txt",
                             std::vector<uint8_t>{ 'n','a','m','e','=','t','e','s','t','\n',
                                                   'm','o','d','u','l','e','=','a','p','p','.','d','l','l','\n',
                                                   'w','i','d','t','h','=','3','2','0','\n' });
        entries.emplace_back("app.dll", std::move(payloadA));
        entries.emplace_back("launcher@1x.png", std::move(payloadB));
        check(jk::JKJkxFile::Write(path, entries), "jkx pack writes container");

        jk::JKJkxFile read;
        check(read.Open(path), "jkx reopen container");
        check(read.EntryCount() == 3, "jkx entry count");
        check(read.Manifest().name == "test" && read.Manifest().module == "app.dll" &&
                  read.Manifest().width == 320,
              "jkx manifest parsed");
        check(read.FindEntry("MODL", "app.dll") == 1, "jkx finds MODL by name");
        check(read.FindEntry("ICON", "launcher@1x.png") == 2, "jkx finds ICON by name");

        std::vector<uint8_t> dllBytes;
        check(read.ReadEntry(1, dllBytes) && dllBytes.size() == 300, "jkx MODL payload size");
        bool payloadOk = true;
        for (size_t i = 0; i < dllBytes.size(); ++i) {
            if (dllBytes[i] != static_cast<uint8_t>(i & 0xFF)) { payloadOk = false; break; }
        }
        check(payloadOk, "jkx MODL payload roundtrip");
        std::vector<uint8_t> iconBytes;
        check(read.ReadEntry(2, iconBytes) && iconBytes.size() == 64, "jkx ICON payload size");
        std::remove(path.c_str());
    }

    // Terminal VT parser + grid (docs/22 §4/§5): golden scenarios.
    {
        jk::JKTerminalGrid grid;
        jk::JKVtParser parser;
        parser.Attach(&grid);
        auto feed = [&parser](const char* s) {
            parser.Feed(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
        };
        auto rowText = [&grid](int r) {
            std::string s;
            for (int c = 0; c < grid.Cols(); ++c) {
                const uint32_t cp = grid.Cell(c, r).cp;
                s.push_back(cp >= 0x20 && cp < 0x7F ? static_cast<char>(cp) : ' ');
            }
            while (!s.empty() && s.back() == ' ') s.pop_back();
            return s;
        };

        grid.Resize(20, 6);

        // Plain text + newline handling.
        feed("hello\r\nworld");
        check(rowText(0) == "hello", "terminal: plain text row 0");
        check(rowText(1) == "world", "terminal: LF moves to next row");

        // Absolute cursor positioning (ConPTY repaint style).
        feed("\x1b[1;1HJK");
        check(rowText(0) == "JKllo", "terminal: CUP overwrite at 1;1");

        // SGR colors are applied to written cells (cursor at col 2 after "JK").
        feed("\x1b[31mR\x1b[0m");
        const jk::JKTermCell& redCell = grid.Cell(2, 0);
        check(redCell.cp == 'R' && redCell.fg == 0xcd0000,
              "terminal: SGR 31 sets red fg on cell");

        // 256-color and truecolor SGR.
        feed("\x1b[1;1H\x1b[38;5;196mX");
        check(grid.Cell(0, 0).fg == 0xff0000, "terminal: 256-color fg lookup");
        feed("\x1b[2;1H\x1b[38;2;12;34;56mY");
        check(grid.Cell(0, 1).fg == 0x0c2238, "terminal: truecolor fg lookup");

        // Erase display (ED 2) clears content.
        feed("\x1b[2J");
        check(rowText(0).empty() && rowText(1).empty(), "terminal: ED2 clears");

        // Deferred wrap: 20 cols → the 21st char wraps to row 1.
        feed("\x1b[1;1H01234567890123456789Z");
        check(rowText(0) == "01234567890123456789" && rowText(1) == "Z",
              "terminal: deferred wrap at last column");

        // Scroll region (DECSTBM) + LF scrolls only inside margins: rows 1..3
        // (0-based) shift up, pulling "line1" into row 2.
        grid.ClearDirty();
        feed("\x1b[2;4r\x1b[4;1Hline1\r\nline2");
        check(rowText(1).empty(), "terminal: region shift empties top row");
        check(rowText(2) == "line1", "terminal: region scroll pulls line1 up");
        check(rowText(3) == "line2", "terminal: LF inside region writes line2");
        feed("\x1b[r");

        // Alt screen (1049): swap out, erase, swap back restores content.
        feed("\x1b[?1049h");
        check(grid.InAltScreen(), "terminal: 1049 enters alt screen");
        feed("\x1b[2Jalt");
        check(rowText(0) == "alt", "terminal: alt screen content");
        feed("\x1b[?1049l");
        check(!grid.InAltScreen(), "terminal: 1049 leaves alt screen");
        check(rowText(2) == "line1", "terminal: main screen restored after 1049 off");

        // OSC 0 title.
        feed("\x1b]0;my title\x07");
        check(grid.Title() == "my title", "terminal: OSC 0 sets title");

        // DSR 6 reply accumulates and TakeReplies clears it.
        feed("\x1b[2;3H");
        feed("\x1b[6n");
        check(parser.TakeReplies() == "\x1b[2;3R",
              "terminal: DSR 6 reports cursor position");
        check(parser.TakeReplies().empty(), "terminal: TakeReplies clears buffer");

        // UTF-8 (Korean): wide glyphs take their cell plus a width-0 follower
        // dummy cell (docs/26 단계 1) so col index == pixel column.
        feed("\x1b[1;1H한글");
        check(grid.Cell(0, 0).cp == 0xD55C && grid.Cell(0, 0).width == 2 &&
                  grid.Cell(1, 0).cp == 0 && grid.Cell(1, 0).width == 0 &&
                  grid.Cell(2, 0).cp == 0xAE00 && grid.Cell(2, 0).width == 2,
              "terminal: hangul decodes wide with follower dummies");

        // Mixed narrow/wide layout: 'a가b' → a | 가 + dummy | b.
        feed("\x1b[2;1Ha가b");
        check(grid.Cell(0, 1).cp == 'a' && grid.Cell(1, 1).cp == 0xAC00 &&
                  grid.Cell(2, 1).width == 0 && grid.Cell(3, 1).cp == 'b',
              "terminal: mixed narrow/wide layout");

        // Backspace steps over the width-0 follower onto the wide glyph.
        // (BS = 0x08: the parser ignores DEL 0x7F per the VT state machine.)
        feed("\x1b[3;1Ha가b");
        feed("\x08\x08");
        check(grid.GetCursor().x == 1 && grid.Cell(1, 2).cp == 0xAC00,
              "terminal: BS skips follower onto wide glyph");

        // Resize preserves top-left content and clamps the cursor.
        grid.Resize(10, 4);
        check(grid.Cols() == 10 && grid.Rows() == 4 &&
                  grid.Cell(0, 0).cp == 0xD55C,
              "terminal: resize preserves content");

        // A wide glyph at the last column wraps to the next row whole
        // instead of splitting across the edge.
        feed("\x1b[1;10H가");
        check(grid.GetCursor().x == 2 && grid.GetCursor().y == 1 &&
                  grid.Cell(0, 1).cp == 0xAC00 && grid.Cell(1, 1).width == 0,
              "terminal: wide glyph at last column wraps whole");

        // Dirty tracking: MarkAllDirty then ClearDirty.
        check(grid.IsDirty(), "terminal: resize marks dirty");
        grid.ClearDirty();
        check(!grid.IsDirty(), "terminal: ClearDirty resets flag");

        // Scrollback: a scroll at the top margin records the departing row.
        grid.ClearDirty();
        feed("\x1b[1;1Hr0\r\nr1\r\nr2\r\nr3\r\nr4");
        check(grid.ScrollbackLines() == 1, "terminal: top scroll records line");
        check(grid.ScrollbackLine(0)[0].cp == 'r' && grid.ScrollbackLine(0)[1].cp == '0',
              "terminal: scrollback line content");

        // Alt-screen scrolling never records (grid is 4 rows; CUP clamps to
        // the last row so each LF scrolls the alt screen).
        feed("\x1b[?1049h\x1b[5;4H\r\n\r\n\r\n\r\n\x1b[?1049l");
        check(grid.ScrollbackLines() == 1, "terminal: alt screen scroll not recorded");

        // RIS clears the scrollback with everything else. ("\x1b" "c", not
        // "\x1bc" — a trailing 'c' is a valid hex digit and would parse as a
        // single 0x1BC escape.)
        feed("\x1b" "c");
        check(grid.ScrollbackLines() == 0, "terminal: RIS clears scrollback");

        // --- Wide-glyph diagnostics (docs/26 단계 1) --------------------------
        // A. Malgun Gothic must rasterize a Hangul syllable at the scale
        // InitFallback computes (mirrors the formula; validates stbtt+font).
        {
            FILE* mf = nullptr;
#ifdef _WIN32
            fopen_s(&mf, "C:/Windows/Fonts/malgun.ttf", "rb");
#else
            mf = std::fopen("C:/Windows/Fonts/malgun.ttf", "rb");
#endif
            check(mf != nullptr, "terminal: malgun.ttf opens");
            if (mf) {
                std::fseek(mf, 0, SEEK_END);
                const long msz = std::ftell(mf);
                std::fseek(mf, 0, SEEK_SET);
                std::vector<uint8_t> mdata(static_cast<size_t>(msz));
                const size_t mread = std::fread(mdata.data(), 1, mdata.size(), mf);
                std::fclose(mf);
                stbtt_fontinfo minfo;
                if (mread == mdata.size() &&
                    stbtt_InitFont(&minfo, mdata.data(), 0)) {
                    int advW = 0, lsb = 0;
                    stbtt_GetCodepointHMetrics(&minfo, 0xAC00, &advW, &lsb);
                    int asc = 0, desc = 0, lg = 0;
                    stbtt_GetFontVMetrics(&minfo, &asc, &desc, &lg);
                    float scale = (advW > 0)
                        ? 16.0f / static_cast<float>(advW)
                        : 16.0f / static_cast<float>(asc - desc);
                    float emPx = static_cast<float>(asc - desc) * scale;
                    if (emPx > 16.0f && asc - desc > 0) {
                        scale *= 16.0f / emPx;
                        emPx = 16.0f;
                    }
                    int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
                    stbtt_GetCodepointBitmapBox(&minfo, 0xAC00, scale, scale,
                                                &bx0, &by0, &bx1, &by1);
                    const int bw = bx1 - bx0, bh = by1 - by0;
                    std::printf("[i] malgun advW=%d asc=%d desc=%d scale=%.6f "
                                "box=%dx%d off=(%d,%d)\n",
                                advW, asc, desc, scale, bw, bh, bx0, by0);
                    check(bw > 0 && bh > 0, "terminal: hangul bitmap box nonempty");
                    if (bw > 0 && bh > 0) {
                        std::vector<uint8_t> cov(static_cast<size_t>(bw) * bh, 0);
                        stbtt_MakeCodepointBitmap(&minfo, cov.data(), bw, bh, bw,
                                                  scale, scale, 0xAC00);
                        int lit = 0;
                        for (const uint8_t v : cov) lit += (v != 0);
                        std::printf("[i] hangul coverage %d/%d px\n",
                                    lit, bw * bh);
                        check(lit > 0, "terminal: hangul raster has coverage");
                    }
                } else {
                    check(false, "terminal: stbtt inits malgun");
                }
            }
        }

        // B. Real conhost emission: the shell prints a Hangul syllable through
        // the console API; ConPTY must deliver UTF-8 that our parser lands in
        // a wide cell (+ follower dummy).
        {
            jk::JKTerminalGrid pgrid;
            pgrid.Resize(80, 25);
            jk::JKVtParser pparser;
            pparser.Attach(&pgrid);
            jk::JKConPtyBridge pty;
            if (pty.Start("powershell.exe -NoLogo -Command \"[char]0xAC00\"",
                          80, 25)) {
                for (int i = 0; i < 300; ++i) {
                    std::string out;
                    pty.DrainOutput(out);
                    if (!out.empty()) {
                        pparser.Feed(reinterpret_cast<const uint8_t*>(out.data()),
                                     out.size());
                    }
                    if (pty.ShellExited()) {
                        std::string tail;
                        pty.DrainOutput(tail);
                        pparser.Feed(reinterpret_cast<const uint8_t*>(tail.data()),
                                     tail.size());
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                bool found = false;
                for (int r = 0; r < pgrid.Rows() && !found; ++r) {
                    for (int c = 0; c < pgrid.Cols() && !found; ++c) {
                        if (pgrid.Cell(c, r).cp == 0xAC00 &&
                            pgrid.Cell(c, r).width == 2) {
                            found = true;
                        }
                    }
                }
                check(found, "terminal: conhost emits hangul into wide cell");
                pty.Stop();
            } else {
                check(false, "terminal: conpty probe spawns shell");
            }
        }

        // C. Real fallback-page raster (regression): the page slot for a
        // Hangul syllable must contain lit pixels — the stamp used to ignore
        // the slot ROW offset, so every page row past the first stayed empty
        // and wide glyphs blitted transparent (blank output).
        {
            jk::JKGlyphAtlas atlas;
            if (atlas.Init("C:/Windows/Fonts/consola.ttf", 8, 16) &&
                atlas.InitFallback("C:/Windows/Fonts/malgun.ttf")) {
                std::vector<uint8_t> rgba;
                int pw = 0, ph = 0;
                if (atlas.RasterizeFallbackPageForTest(0x000000, false, 0xAC00,
                                                       &rgba, &pw, &ph)) {
                    const jk::JKRect src = atlas.GlyphSrc(0xAC00);
                    int lit = 0;
                    for (int y = 0; y < src.h; ++y) {
                        for (int x = 0; x < src.w; ++x) {
                            const size_t idx =
                                (static_cast<size_t>(src.y + y) * pw +
                                 src.x + x) * 4 + 3;
                            if (idx + 3 < rgba.size() && rgba[idx]) ++lit;
                        }
                    }
                    std::printf("[i] fallback slot (%d,%d,%d,%d) lit=%d\n",
                                src.x, src.y, src.w, src.h, lit);
                    check(lit > 0,
                          "terminal: fallback page slot has hangul pixels");
                } else {
                    check(false, "terminal: fallback page rasterizes");
                }
            } else {
                check(false, "terminal: atlas inits for page raster test");
            }
        }
    }

    // Script bridge (docs/27 단계 1): host boot, click dispatch, timer
    // plumbing, exception policy, binding/contract introspection, SCRI
    // container. Each scenario owns a fresh JKWindow — a stopped host leaves
    // its controls in the window (no child-removal API) so sharing one window
    // would leak control-id collisions across scenarios.
    {
        auto writeScript = [](const char* name, const char* text) {
            std::FILE* f = std::fopen(name, "wb");
            if (!f) return;
            std::fwrite(text, 1, std::strlen(text), f);
            std::fclose(f);
        };
        auto bytes = [](const char* s) {
            return std::vector<uint8_t>(s, s + std::strlen(s));
        };
        auto makeTimerServices = [](std::vector<uint32_t>& claimed,
                                    uint64_t& handleSeq) {
            jk::JKScriptTimerServices ts;
            ts.start = [&claimed, &handleSeq](uint32_t winId, uint32_t) -> uint64_t {
                claimed.push_back(winId);
                return ++handleSeq;
            };
            ts.stop = [](uint64_t) {};
            return ts;
        };

        // 1) Boot + onCreate + control creation + click dispatch.
        writeScript("test_script_app.js",
            "var label = createLabel({x:10,y:10,w:120,h:24}, \"idle\");\n"
            "var btn = createButton({x:10,y:44,w:100,h:30}, \"hit\");\n"
            "function onCreate(){ log(\"boot\"); }\n"
            "function onClick(id){ if (id === btn) setText(label, \"clicked:\" + id); }\n");
        jk::JKWindow win("ScriptTest");
        win.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
        jk::JKScriptHost host;
        host.Attach(&win);
        std::vector<uint32_t> claimedWinIds;
        uint64_t handleSeq = 0;
        host.SetTimerServices(makeTimerServices(claimedWinIds, handleSeq));
        check(host.Start("test_script_app.js"), "script host boots app.js");
        check(host.IsRunning() && host.EntryPath() == "test_script_app.js",
              "script host running after start");
        const uint16_t labelId = 1000;  // first auto-assigned id
        const uint16_t btnId = 1001;
        jk::JKControl* label = win.FindControlByControlId(labelId);
        jk::JKControl* btn = win.FindControlByControlId(btnId);
        check(label && btn, "script created label+button controls");
        check(claimedWinIds.empty(),
              "boot without setInterval claims no timers");
        host.DispatchClick(btnId);
        check(label && label->GetText() == "clicked:" + std::to_string(btnId),
              "dispatchclick drives script onclick");
        host.Stop();

        // 2) setInterval claims a winId from the script range and manual
        //    dispatch runs the interval callback.
        writeScript("test_script_timer.js",
            "var tick = 0;\n"
            "var tlabel = createLabel({x:0,y:0,w:80,h:20}, \"t0\");\n"
            "function onCreate(){ setInterval(50, function(){ tick++; "
            "setText(tlabel, \"t\" + tick); }); }\n");
        jk::JKWindow twin("ScriptTimerTest");
        twin.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
        jk::JKScriptHost thost;
        thost.Attach(&twin);
        uint64_t handleSeq2 = 0;
        thost.SetTimerServices(makeTimerServices(claimedWinIds, handleSeq2));
        check(thost.Start("test_script_timer.js"), "timer script boots");
        check(claimedWinIds.size() == 1 &&
                  claimedWinIds[0] == jk::JKScriptHost::ScriptTimerWinIdBase,
              "setInterval claims script timer winid");
        thost.DispatchTimerAt(0);
        jk::JKControl* tlabel = twin.FindControlByControlId(1000);
        check(tlabel && tlabel->GetText() == "t1",
              "dispatchtimerat runs interval callback");
        thost.Stop();

        // 3) Exception policy: the script error fails Start and lands in
        //    LastError with the exception message (docs/27 §3.2).
        writeScript("test_script_throw.js",
            "function onCreate(){ throw new Error(\"boom\"); }\n");
        jk::JKWindow ewin("ScriptThrowTest");
        ewin.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
        jk::JKScriptHost ehost;
        ehost.Attach(&ewin);
        check(!ehost.Start("test_script_throw.js"),
              "script exception fails start");
        check(ehost.LastError().find("boom") != std::string::npos,
              "script exception recorded in lasterror");
        check(!ehost.IsRunning(), "failed script leaves host stopped");

        // 4) jk.d.ts machine check (docs/27 §2.4): every declared function
        //    must be visible to a running script (runtime introspection is
        //    the ground truth).
        writeScript("test_script_empty.js", "");
        jk::JKWindow nwin("ScriptNamesTest");
        nwin.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
        jk::JKScriptHost nhost;
        nhost.Attach(&nwin);
        check(nhost.Start("test_script_empty.js"), "empty script boots");
        std::vector<std::string> bound = nhost.BoundNames();
        std::vector<uint8_t> dtsBytes;
        if (ReadWholeFile(JK_SCRIPTS_DIR "/jk.d.ts", dtsBytes)) {
            std::string dts(dtsBytes.begin(), dtsBytes.end());
            int missing = 0;
            size_t pos = 0;
            for (;;) {
                const size_t d = dts.find("declare function ", pos);
                if (d == std::string::npos) break;
                const size_t nameBegin = d + std::strlen("declare function ");
                const size_t nameEnd = dts.find('(', nameBegin);
                if (nameEnd == std::string::npos) break;
                const std::string name =
                    dts.substr(nameBegin, nameEnd - nameBegin);
                bool found = false;
                for (const auto& n : bound) {
                    if (n == name) { found = true; break; }
                }
                if (!found) {
                    std::printf("      missing binding: %s\n", name.c_str());
                    ++missing;
                }
                pos = d + 1;
            }
            check(missing == 0,
                  "jk.d.ts declared functions all bound at runtime");
        } else {
            check(false, "jk.d.ts readable for introspection check");
        }
        nhost.Stop();

        // 5) SCRI container roundtrip (docs/27 §3.1 .jkx extension; the TOC
        // type field is a 4cc, so the script type is "SCRI" not "SCRPT").
        {
            std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
            entries.emplace_back("manifest.txt",
                bytes("name=x\nmodule=jkapp_script.dll\nscript=app.js\n"));
            entries.emplace_back("jkapp_script.dll", bytes("stub"));
            entries.emplace_back("app.js", bytes("log(1);"));
            check(jk::JKJkxFile::Write("test_script.jkx", entries),
                  "jkx write with app.js entry");
            jk::JKJkxFile rd;
            check(rd.Open("test_script.jkx") &&
                      rd.FindEntry("SCRI", "app.js") >= 0,
                  "app.js stored as SCRI entry");
            std::vector<uint8_t> out;
            check(rd.ReadEntry(rd.FindEntry("SCRI", "app.js"), out) &&
                      out.size() == 7,
                  "SCRI payload roundtrip");
            check(rd.Version() == 1 && rd.Codec() == 0,
                  "jkx header records version/codec");
        }

        // 6) Forward compat (docs/21 codec registry): a container claiming an
        // unregistered codec must fail Open instead of misparsing payloads.
        {
            std::vector<uint8_t> raw;
            if (ReadWholeFile("test_script.jkx", raw) && raw.size() >= 20) {
                raw[16] = 7;  // codec field — 7 is unregistered
                if (std::FILE* bf = std::fopen("test_badcodec.jkx", "wb")) {
                    std::fwrite(raw.data(), 1, raw.size(), bf);
                    std::fclose(bf);
                }
                jk::JKJkxFile bad;
                check(!bad.Open("test_badcodec.jkx"), "unknown codec rejected");
                std::remove("test_badcodec.jkx");
            }
            std::remove("test_script.jkx");  // test artifact — keep the repo clean
        }

        // 7) test-script runner (docs/27 단계 2): the scenario passes, and an
        // intentionally broken scenario is detected — the defect-detection
        // equivalence the probes used to provide (§5 단계 2 검증).
        check(RunScriptTestFile(JK_SCRIPTS_DIR "/tests/uiauto.js") == 0,
              "test-script scenario passes");
        check(RunScriptTestFile(JK_SCRIPTS_DIR "/tests/uiauto_broken.js") != 0,
              "test-script detects injected defect");

        // 8) dialog scenario (docs/27 단계 3): modal dialog bindings work
        // headless — create/add/show/close/reopen, result codes, and the
        // shared control registry (findControl/click/setText across dialogs).
        check(RunScriptTestFile(JK_SCRIPTS_DIR "/tests/dialog.js") == 0,
              "dialog scenario passes");

        // 9) config scenario (docs/27 단계 4): readConfig parses a JSON file
        // next to the entry script; missing files, traversal, and absolute
        // paths return null.
        check(RunScriptTestFile(JK_SCRIPTS_DIR "/tests/config.js") == 0,
              "readConfig scenario passes");

        // 10) terminal.json reader (docs/26 단계 5 via docs/27 단계 4): keys
        // applied, malformed root keeps defaults, missing file reported.
        {
            writeScript("test_terminal.json",
                "{ \"shell\": \"cmd.exe /k demo\", \"scrollback\": 2500,\n"
                "  \"themeBg\": \"#112233\", \"bogus\": [1,2,3] }");
            jk::JKTerminalConfig cfg;
            check(cfg.Load("test_terminal.json"), "terminal config opens");
            check(cfg.shell == "cmd.exe /k demo" && cfg.scrollback == 2500 &&
                      cfg.themeBg == 0x112233,
                  "terminal config keys applied");
            writeScript("test_terminal.json", "{ not json ");
            jk::JKTerminalConfig bad;
            check(bad.Load("test_terminal.json"),
                  "malformed terminal config does not fail startup");
            check(bad.shell == "powershell.exe -NoLogo" && bad.scrollback == 1000,
                  "malformed terminal config keeps defaults");
            std::remove("test_terminal.json");
            jk::JKTerminalConfig missing;
            check(!missing.Load("test_terminal_missing.json"),
                  "missing terminal config reported");
            check(missing.scrollback == 1000,
                  "missing terminal config keeps defaults");
        }
    }

    // 11) pcx image viewer (2026-09-06 rewrite): PCX decode + zoom ladder.
    // A 4x3 256-color PCX is written programmatically (no asset needed) and
    // driven through real window event dispatch — clicks hit the toolbar
    // buttons via JKWindow::RespondMessage hit-testing, wheel via the app
    // forwarder. The synthetic-click SDL path cannot be used here (touch-
    // capable machines swallow stationary injected buttons), so this headless
    // block is the regression net for the viewer's interaction logic.
    {
        namespace fs = std::filesystem;
        const fs::path pcxPath = fs::temp_directory_path() / "jk_selftest.pcx";
        {
            unsigned char header[128];
            std::memset(header, 0, sizeof(header));
            header[0] = 0x0A;  // ZSoft PCX
            header[1] = 5;     // version 3.0 (256-color palette)
            header[2] = 1;     // RLE encoding
            header[3] = 8;     // bits per pixel
            header[4] = 0; header[5] = 0;   // x1
            header[6] = 0; header[7] = 0;   // y1
            header[8] = 3; header[9] = 0;   // x2 = 3 (width 4)
            header[10] = 2; header[11] = 0; // y2 = 2 (height 3)
            header[65] = 1;  // numPlanes
            header[66] = 4; header[67] = 0; // bytesPerLine
            header[68] = 1; header[69] = 0; // paletteInfo

            FILE* fp = std::fopen(pcxPath.string().c_str(), "wb");
            if (fp) {
                std::fwrite(header, 1, sizeof(header), fp);
                // Literal (non-RLE) scanlines: all indices < 0xC0.
                const unsigned char rows[3][4] = {
                    { 5, 5, 5, 5 }, { 6, 7, 8, 9 }, { 10, 11, 12, 13 } };
                for (int y = 0; y < 3; ++y) {
                    std::fwrite(rows[y], 1, 4, fp);
                }
                // 256-color palette (marker 0x0C + 256*3 BGR triplets).
                unsigned char palette[1 + 256 * 3] = { 0x0C };
                palette[1 + 5 * 3 + 0] = 10;  palette[1 + 5 * 3 + 1] = 20;
                palette[1 + 5 * 3 + 2] = 30;
                palette[1 + 13 * 3 + 0] = 44; palette[1 + 13 * 3 + 1] = 55;
                palette[1 + 13 * 3 + 2] = 66;
                std::fwrite(palette, 1, sizeof(palette), fp);
                std::fclose(fp);
            }

            auto win = jk::CreatePcxViewerWindow(pcxPath.string());
            auto* viewer = static_cast<jk::ImageViewerWindow*>(win.get());
            check(viewer->HasImage(), "pcx viewer decodes generated file");
            const jk::ViewerImage* img = viewer->Image();
            check(img && img->w == 4 && img->h == 3, "pcx viewer dimensions 4x3");
            check(img && img->rgba.size() == static_cast<size_t>(4) * 3 * 4,
                  "pcx viewer rgba size");
            if (img && img->rgba.size() >= 8) {
                check(img->rgba[4] == 10 && img->rgba[5] == 20 &&
                          img->rgba[6] == 30 && img->rgba[7] == 255,
                      "pcx palette expansion spot pixel (1,0)");
                const size_t last = (2 * 4 + 3) * 4;
                check(img->rgba[last] == 44 && img->rgba[last + 1] == 55 &&
                          img->rgba[last + 2] == 66,
                      "pcx palette expansion spot pixel (3,2)");
            }

            // Toolbar clicks through real window dispatch.
            auto clickBtn = [&](uint16_t id) {
                jk::JKControl* btn = win->FindControlByControlId(id);
                if (!btn) return;
                const jk::JKRect r = btn->GetScreenClientRect();
                jk::JKEvent ev;
                ev.type = jk::JKEventType::MouseDown;
                ev.x = r.x + r.w / 2;
                ev.y = r.y + r.h / 2;
                win->RespondMessage(ev);
                ev.type = jk::JKEventType::MouseUp;
                win->RespondMessage(ev);
            };
            // From fit: + enters the ladder at 125%.
            clickBtn(5);
            check(!viewer->IsFit() && viewer->ZoomPercent() == 125,
                  "pcx zoom in from fit lands at 125%");
            clickBtn(5);
            check(viewer->ZoomPercent() == 150, "pcx zoom in steps ladder");
            clickBtn(4);
            check(viewer->ZoomPercent() == 125, "pcx zoom out steps ladder");
            clickBtn(3);
            check(viewer->ZoomPercent() == 100, "pcx 1:1 resets to 100%");
            viewer->ForwardWheel(1);
            check(viewer->ZoomPercent() == 125, "pcx wheel up zooms");
            viewer->ForwardWheel(-1);
            check(viewer->ZoomPercent() == 100, "pcx wheel down zooms");
            clickBtn(2);
            check(viewer->IsFit(), "pcx fit returns to fit mode");
            // Ladder clamps: 20 wheel-ups from 100% cap at 800%.
            for (int i = 0; i < 20; ++i) viewer->ForwardWheel(1);
            check(viewer->ZoomPercent() == 800, "pcx zoom ladder caps at 800%");
            for (int i = 0; i < 20; ++i) viewer->ForwardWheel(-1);
            check(viewer->ZoomPercent() == 25, "pcx zoom ladder floors at 25%");
            // Missing file keeps the previous content.
            viewer->OpenPath((fs::temp_directory_path() / "jk_selftest_missing.pcx").string());
            check(viewer->HasImage(), "pcx failed load keeps previous content");
            std::remove(pcxPath.string().c_str());
        }
    }

    std::printf("AppSelfTest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

int main(int argc, char* argv[]) {
    // GUI 앱이므로 콘솔 출력이 안 보인다. 디버깅용 파일 로그를 먼저 연다.
    std::FILE* logFile = std::fopen("jkdesktop_launch.log", "w");
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
        std::printf("jkdesktop - JKENGINE SDL2 prototype\n");
        std::printf("\n");
        std::printf("Usage: jkdesktop.exe [COMMAND]\n");
        std::printf("\n");
        std::printf("Commands:\n");
        std::printf("  (none)      Default demo app\n");
        std::printf("  test        Built-in self-test mode\n");
        std::printf("  test-script FILE  Run a UI automation scenario (exit code = assertion failures)\n");
        std::printf("  jango       JANGO launcher\n");
        std::printf("  occ         OCC / fire control demo\n");
        std::printf("  pcx FILE    image viewer (PCX/PNG/JPG/BMP)\n");
        std::printf("  vector      Bezier vector editor\n");
        std::printf("  iconedit    Icon/sprite editor\n");
        std::printf("  recog       Stroke recognition demo\n");
        std::printf("  vfont       Vector font window\n");
        std::printf("  vpres       Vector font presentation\n");
        std::printf("  minesweeper App launcher (Minesweeper + Tetris)\n");
        std::printf("  tetris      Tetris game\n");
        std::printf("  scriptdemo [FILE]  Script app demo (FILE: .jkx package or a direct app.js)\n");
        std::printf("  --server    Run as the window server (Phase 2 scaffolding)\n");
        std::printf("  --client minesweeper  Run Minesweeper as a window-server client\n");
        std::printf("  --client tetris     Run Tetris as a window-server client\n");
        std::printf("  --jkx FILE  Run an app from a .jkx container\n");
        std::printf("  jkx-pack APP  Bundle jkapp_<APP>.dll + icons + manifest into apps/<APP>.jkx\n");
        std::printf("  jkx-list FILE   Print a .jkx container's version/codec + TOC\n");
        std::printf("  jkx-extract FILE [ENTRY...]  Extract .jkx entries into <FILE>_x/\n");
        std::printf("  -h, --help, /?  Show this help message\n");
        return 0;
    }

    if (argc > 1 && std::strcmp(argv[1], "test") == 0) {
        return RunAppSelfTest();
    }

    if (argc > 1 && std::strcmp(argv[1], "test-script") == 0) {
        if (argc < 3) {
            std::fprintf(stderr,
                         "Usage: test-script <scenario.js>  (headless UI automation; "
                         "exit code = assertion failures)\n");
            return 1;
        }
        return RunScriptTestFile(argv[2]);
    }

    if (argc > 1 && std::strcmp(argv[1], "jkx-pack") == 0) {
#ifdef _WIN32
        if (argc < 3) {
            std::fprintf(stderr, "Usage: jkx-pack <app>  (bundles jkapp_<app>.dll + icons + manifest into apps/<app>.jkx)\n");
            return 1;
        }
        return RunJkxPack(argv[2]);
#else
        std::fprintf(stderr, "jkx-pack is Windows-only in this prototype\n");
        return 1;
#endif
    }

    if (argc > 1 && std::strcmp(argv[1], "jkx-list") == 0) {
#ifdef _WIN32
        if (argc < 3) {
            std::fprintf(stderr, "Usage: jkx-list <file.jkx>  (print container version/codec + TOC)\n");
            return 1;
        }
        return RunJkxList(argv[2]);
#else
        std::fprintf(stderr, "jkx-list is Windows-only in this prototype\n");
        return 1;
#endif
    }

    if (argc > 1 && std::strcmp(argv[1], "jkx-extract") == 0) {
#ifdef _WIN32
        if (argc < 3) {
            std::fprintf(stderr, "Usage: jkx-extract <file.jkx> [entry ...]  (extract all/named entries into <file>_x/)\n");
            return 1;
        }
        return RunJkxExtract(argv[2], argc - 3, argv + 3);
#else
        std::fprintf(stderr, "jkx-extract is Windows-only in this prototype\n");
        return 1;
#endif
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
        if (!app.Init("Image Viewer", 1280, 680)) {
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
        constexpr const char* kPipe = "\\\\.\\pipe\\JKWindowServerPipe";
        const char* clientApp = (argc > 2) ? argv[2] : "";

        // Phase B: client apps are dynamically loaded modules (jkapp_<name>.dll).
        // The module statically contains its core code and is driven purely
        // through the C ABI in apps/JKAppModule.h — no C++ crosses the boundary.
#ifdef _WIN32
        const std::string dllName = std::string("jkapp_") + clientApp + ".dll";
        return RunClientModule(dllName.c_str(), kPipe);
#else
        (void)clientApp;
        std::fprintf(stderr, "--client is Windows-only in this prototype\n");
        return 1;
#endif
    }

    // scriptdemo [path]: packaged script app (docs/27 단계 1) in single-process
    // mode — open the .jkx next to the exe (or an explicit path), extract the
    // script payload to %TEMP%, and run ScriptAppT<JKApplication> on it. This
    // is the dev counterpart of the server spawning the same container.
    if (argc > 1 && std::strcmp(argv[1], "scriptdemo") == 0) {
        const std::string argPath = (argc > 2) ? argv[2] : std::string();

        // Direct app.js path (dev loop): load the source file itself so
        // JK_SCRIPT_WATCH=1 hot reload fires on the actual edit — no .jkx
        // repack and no %TEMP% copy in between.
        const bool directJs = argPath.size() >= 3 &&
            argPath.compare(argPath.size() - 3, 3, ".js") == 0;
        if (directJs) {
            size_t slash = argPath.find_last_of("/\\");
            std::string stem = argPath.substr((slash == std::string::npos) ? 0 : slash + 1);
            const size_t dot = stem.rfind('.');
            if (dot != std::string::npos) stem = stem.substr(0, dot);
            const std::string title = stem.empty() ? "Script App" : stem;
            jk::ScriptAppT<jk::JKApplication> app;
            app.SetScriptInfo(title, argPath);
            if (!app.Init(title, 320, 240)) return 1;
            return app.Run();
        }

        std::string jkxPath = argPath;
        if (jkxPath.empty()) {
            if (char* p = SDL_GetBasePath()) {
                jkxPath = std::string(p) + "apps\\scriptdemo.jkx";
                SDL_free(p);
            } else {
                jkxPath = "apps/scriptdemo.jkx";
            }
        }
        jk::JKJkxFile jkx;
        if (!jkx.Open(jkxPath)) {
            std::fprintf(stderr,
                         "scriptdemo: cannot open '%s' (build the jkx_packages "
                         "target or pass a .jkx path)\n", jkxPath.c_str());
            return 1;
        }
        const jk::JkxManifest& mani = jkx.Manifest();
        const int scriptEntry =
            jkx.FindEntry("SCRI", mani.script.empty() ? "app.js" : mani.script);
        if (scriptEntry < 0) {
            std::fprintf(stderr, "scriptdemo: no script entry in '%s'\n",
                         jkxPath.c_str());
            return 1;
        }
        std::vector<uint8_t> appJs;
        if (!jkx.ReadEntry(scriptEntry, appJs)) {
            std::fprintf(stderr, "scriptdemo: cannot read script entry\n");
            return 1;
        }
        // JKScriptHost::Start takes a file path — drop the payload in %TEMP%
        // under a per-pid name so reruns never collide (same scheme as --jkx).
        char tempDir[260] = ".";
        GetTempPathA(static_cast<unsigned long>(sizeof(tempDir) - 64), tempDir);
        char scriptPath[324] = {};
        std::snprintf(scriptPath, sizeof(scriptPath), "%sjkscript_%lu.app.js",
                      tempDir, static_cast<unsigned long>(GetCurrentProcessId()));
        std::FILE* sf = std::fopen(scriptPath, "wb");
        if (!sf) {
            std::fprintf(stderr, "scriptdemo: cannot write '%s'\n", scriptPath);
            return 1;
        }
        std::fwrite(appJs.data(), 1, appJs.size(), sf);
        std::fclose(sf);

        jk::ScriptAppT<jk::JKApplication> app;
        app.SetScriptInfo(mani.title, scriptPath);
        if (!app.Init(mani.title, mani.width > 0 ? mani.width : 320,
                      mani.height > 0 ? mani.height : 240)) {
            return 1;
        }
        return app.Run();
    }

    if (argc > 1 && std::strcmp(argv[1], "--jkx") == 0) {
        constexpr const char* kPipe = "\\\\.\\pipe\\JKWindowServerPipe";
        if (argc < 3) {
            std::fprintf(stderr, "Usage: --jkx <container.jkx>\n");
            return 1;
        }
#ifdef _WIN32
        return RunClientFromJkx(argv[2], kPipe);
#else
        std::fprintf(stderr, "--jkx is Windows-only in this prototype\n");
        return 1;
#endif
    }

    MyApp app;
    if (!app.Init("JKENGINE SDL2 Prototype", 1920, 1080)) {
        return 1;
    }

    return app.Run();
}