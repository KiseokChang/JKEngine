#ifdef _WIN32
// Avoid pulling in the full Windows headers, which conflict with JKENGINE's
// legacy typedef.h. We only need AllocConsole for the /? help path, the
// LoadLibrary trio for app module loading (--client/--jkx), and the temp-file
// trio for extracting a module out of a .jkx container.
extern "C" __declspec(dllimport) int __stdcall AllocConsole(void);
// Phase A (docs/44): --cwd sets the PTY spawn working directory.
extern "C" __declspec(dllimport) int __stdcall SetCurrentDirectoryA(
    const char* lpPathName);
extern "C" __declspec(dllimport) void* __stdcall LoadLibraryA(const char*);
extern "C" __declspec(dllimport) int __stdcall FreeLibrary(void*);
extern "C" __declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);
extern "C" __declspec(dllimport) unsigned long __stdcall GetTempPathA(
    unsigned long nBufferLength, char* lpBuffer);
extern "C" __declspec(dllimport) int __stdcall CreateDirectoryA(
    const char* lpPathName, void* lpSecurityAttributes);
extern "C" __declspec(dllimport) int __stdcall DeleteFileA(const char* lpFileName);
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentProcessId(void);
// GetModuleFileNameA 수기 선언은 소각 — exe-dir는 jk::fs::GetExecutablePath
// 어댑터가 소유(docs/68 W5).
extern "C" __declspec(dllimport) void* __stdcall GetStdHandle(int nStdHandle);
// PULARGE_INTEGER is really just a pointer to a 64-bit byte count; declaring
// it as unsigned long long* keeps windows.h out of this translation unit.
extern "C" __declspec(dllimport) int __stdcall GetDiskFreeSpaceExA(
    const char* lpDirectoryName,
    unsigned long long* lpFreeBytesAvailableToCaller,
    unsigned long long* lpTotalNumberOfBytes,
    unsigned long long* lpTotalNumberOfFreeBytes);
// Case 15 raw client (R-C5, docs/68 W8b): the jk::net adapter TU owns
// winsock2.h, so this TU hand-carries just the winsock exports the raw
// selftest client needs — same precedent as the AllocConsole block above.
extern "C" __declspec(dllimport) unsigned long long __stdcall socket(
    int af, int type, int protocol);
extern "C" __declspec(dllimport) int __stdcall connect(
    unsigned long long s, const void* name, int namelen);
extern "C" __declspec(dllimport) int __stdcall send(
    unsigned long long s, const char* buf, int len, int flags);
extern "C" __declspec(dllimport) int __stdcall recv(
    unsigned long long s, char* buf, int len, int flags);
extern "C" __declspec(dllimport) int __stdcall setsockopt(
    unsigned long long s, int level, int optname, const char* optval,
    int optlen);
extern "C" __declspec(dllimport) int __stdcall closesocket(
    unsigned long long s);
#else
// linux stage-3 task 7 — posix leg of the win32 hand-decl block above: chdir
// is the SetCurrentDirectoryA twin for the terminal --cwd leg.
// 플랜 G2: RunClientModule posix leg의 모듈 로더 트리오는 dlopen/dlsym/dlerror
// (win32의 LoadLibraryA/GetProcAddress 트리오 상대) — MinGW dlopen 아님,
// dlfcn include는 이 #else 안에서만.
#include <dlfcn.h>
#include <unistd.h>
#endif

#include <JKApplication.h>
#include <JKCrashHandler.h>
#include <JKTextAtlas.h>  // selftest: SfntFaceHasCff 가드 (docs/70 §8.4 판정 2)
#include <JKWindow.h>
#include <fs/JKFs.h>

#include <client/JKClientSurface.h>
#include <server/JKWindowServer.h>
#include <agent/JKAgentClient.h>
#include <crypto/JKSha256.h>
#include <ipc/JKWireEndpoints.h>
#include <ipc/JKWireProtocol.h>
#include <net/JKNet.h>
#include <process/JKProcess.h>
#include <theme/JKTheme.h>

#include <terminal/JKTerminalGrid.h>
#include <terminal/JKVtParser.h>
#include <terminal/JKConPtyBridge.h>
#include <terminal/JKGlyphAtlas.h>
#include <apps/JKTermSelection.h>
#include <apps/JKScrubClock.h>
#include <apps/JKTermInput.h>

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
#include <JKLibraryCatalog.h>  // selftest 1m — 라이브러리 카탈로그 3원 스캔
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
#include <apps/TerminalApp.h>
#include <apps/ClientScriptApp.h>
#include <apps/JKAppModule.h>
#include <apps/JKTerminalConfig.h>
#include <apps/JKWorkshopSeed.h>
#include <JKJkxFile.h>
#include "WANCODE.H"
#include <cstdint>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
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
        // 초기 창 420x420 기준 배치 — 250x250 시절엔 리스트/콤보가 창 밖으로 넘쳤다.
        auto button = std::make_unique<jk::JKButton>(jk::JKRect{ 10, 10, 110, 32 }, 101);
        button->SetText("Click Me");
        button->SetOnClick([]() { std::printf("Button clicked!\n"); });
        AddControl(std::move(button));

        auto clock = std::make_unique<jk::JOClock>(jk::JKRect{ 140, 10, 110, 26 }, 0);
        AddControl(std::move(clock));

        auto checkbox = std::make_unique<jk::JKCheckBox>(jk::JKRect{ 10, 52, 130, 26 }, 102);
        checkbox->SetText("Option");
        AddControl(std::move(checkbox));

        auto edit = std::make_unique<jk::JKEdit>(jk::JKRect{ 10, 88, 396, 30 }, 103, 100, false);
        edit->SetText("Type here");
        AddControl(std::move(edit));

        auto memo = std::make_unique<jk::JKEdit>(jk::JKRect{ 10, 126, 396, 130 }, 104, 1000, true);
        memo->SetText("Line 1\nLine 2\nLine 3");
        AddControl(std::move(memo));

        auto list = std::make_unique<jk::JKListBox>(jk::JKRect{ 10, 266, 190, 140 }, 105);
        list->AddString("Apple");
        list->AddString("Banana");
        list->AddString("Cherry");
        list->AddString("Date");
        list->AddString("Elderberry");
        AddControl(std::move(list));

        auto combo = std::make_unique<jk::JKComboBox>(jk::JKRect{ 212, 266, 194, 28 }, 106);
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
            newWin->SetWindowRect(jk::JKRect{ ev.x, ev.y, 420, 420 });
            newWin->AddDemoControls();

            auto innerBox = std::make_unique<ColorBox>(255, 165, 0);
            innerBox->SetRect(jk::JKRect{ 320, 10, 80, 80 });
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

        // 떠 있는 TestWindow: 250x250 시절엔 컨트롤이 창 밖으로 넘쳤다 — 420x420으로 확대.
        auto testWin = std::make_unique<TestWindow>();
        testWin->SetWindowRect(jk::JKRect{ 50, 50, 420, 420 });
        testWin->AddDemoControls();

        // TestWindow 클라이언트 영역에 배치된 자식 컨트롤
        auto innerBox = std::make_unique<ColorBox>(255, 165, 0);
        innerBox->SetRect(jk::JKRect{ 320, 10, 80, 80 });
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
// [uistall] 워치독(docs/50 §10.7)과 클라 진단의 착지점: 서버가 스폰할 때는
// (GUI 서브시스템, 핸들 비상속) stderr가 무효라 모든 fprintf가 소실된다.
// stderr가 무효일 때만 <exeDir>\client_<app>.log로 미러링한다 — 콘솔/프로브
// 런치(RedirectStandardError 파이프)는 자기 stderr를 그대로 쓴다. 1MiB 초과
// 시 open 시점에 잘라낸다(순환 버퍼 대신 최소 구현 — 스톨 판독은 최근분).
static void MirrorClientStderr(const char* modulePath) {
    constexpr int kStdErrorHandle = -12; // STD_ERROR_HANDLE
    void* errH = GetStdHandle(kStdErrorHandle);
    if (errH && errH != (void*)(intptr_t)-1) return;
    // jk::fs::GetExecutablePath 흡수 (docs/68 W5) — 원문 규약(후행 '\' 유지,
    // 실패/구분자 없음 시 조용히 중단) 유지.
    const std::string exe = jk::fs::GetExecutablePath();
    if (exe.empty()) return;
    const size_t slash = exe.find_last_of('\\');
    if (slash == std::string::npos) return;
    const std::string exeDir = exe.substr(0, slash + 1);
    // jkapp_<app>.dll -> <app>
    const char* base = std::strstr(modulePath, "jkapp_");
    base = base ? base + 6 : modulePath;
    std::string app(base);
    const size_t dot = app.rfind(".dll");
    if (dot != std::string::npos) app.resize(dot);
    const std::string logPath = exeDir + "client_" + app + ".log";
    std::FILE* f = std::fopen(logPath.c_str(), "rb");
    long size = 0;
    if (f) {
        std::fseek(f, 0, SEEK_END);
        size = std::ftell(f);
        std::fclose(f);
    }
    if (!std::freopen(logPath.c_str(), size > 1024 * 1024 ? "w" : "a", stderr))
        return;
    const std::time_t t = std::time(nullptr);
    std::fprintf(stderr, "[clientlog] open %s %s", app.c_str(), std::ctime(&t));
    std::fflush(stderr);
}
#else
// posix twin (플랜 G2): stderr 미러 자체가 win32 사정의 보상이다 — GUI
// 서브시스템 스폰의 비상속 핸들 때문에 fprintf가 소실되는 일은 posix 셸/파이프
// 런치에서 없으므로(콘솔/프로브 원문 런치와 동일 조건) 미러하지 않는다.
static void MirrorClientStderr(const char*) {}

// posix route leg 전용(플랜 G2): dlopen은 검색 계약이 win32 LoadLibraryA와
// 다르다 — 빈(슬래시 없는) 이름은 LD_LIBRARY_PATH·ld.so 캐시·/lib만 보고
// exe-dir도 cwd도 기본 검색하지 않는다. 그래서 클라 route leg는 exe-dir 접두로
// 절대화한다(플랜 G3 서버 스폰과 동일 위치 관측). 접미는 G1 AppModuleSuffix()
// — 단일 정의 원칙. 폴백: exe 경로를 못 얻으면 "./" 접두(슬래시 없는 이름은
// dlopen이 cwd도 검색하지 않아 확실히 실패 — 서버 선례(dirSelf 폴백 ".",
// "./jkapp_...")와 동형으로 cwd 절대화한다).
static std::string ClientModulePath(const char* appName) {
    const std::string exe = jk::fs::GetExecutablePath();
    const size_t slash = exe.find_last_of('/');
    const std::string prefix =
        (slash == std::string::npos) ? std::string("./") : exe.substr(0, slash + 1);
    return prefix + "jkapp_" + appName + jk::server::AppModuleSuffix();
}
#endif

// Loads an app module shared library (win32 jkapp_<app>.dll via LoadLibraryA,
// posix jkapp_<app>.so via dlopen) and runs it through the C ABI in
// apps/JKAppModule.h. All C++ (app construction, Init, Run, destruction) stays
// inside the module.
static int RunClientModule(const char* modulePath, const char* pipeName) {
    MirrorClientStderr(modulePath);
#ifdef _WIN32
    void* module = LoadLibraryA(modulePath);
#else
    // RTLD_LOCAL: 모듈 심볼을 전역 네임스페이스에 흘려보내지 않는다 —
    // 여러 앱을 한 프로세스가 잡는 일은 없지만(=client 모델) 단일 앱이라도
    // 호스트 main의 심볼과 간섭하지 않는 게 원칙. 닫지 않는다(FreeLibrary
    // 주석 — 힙 손상 선례; posix leg도 동일: 호스트가 곧 종료됨).
    void* module = dlopen(modulePath, RTLD_NOW | RTLD_LOCAL);
#endif
    if (!module) {
#ifdef _WIN32
        std::fprintf(stderr, "Cannot load app module '%s'\n", modulePath);
#else
        // dlerror()는 실패 세부를 1회만 보고하고 NULL을 반환할 수 있다(표준,
        // 최종리뷰 LOW-rider) — NULL이면 "(no dlerror detail)"로 마킹해
        // %s에 NULL을 건네지 않는다.
        const char* dlErr = dlerror();
        std::fprintf(stderr, "Cannot load app module '%s' (%s)\n", modulePath,
                     dlErr ? dlErr : "(no dlerror detail)");
#endif
        return 1;
    }
#ifdef _WIN32
    auto metaFn = reinterpret_cast<const jk::JKAppMeta* (*)()>(
        GetProcAddress(module, "jk_app_meta"));
    auto runFn = reinterpret_cast<int (*)(const char*)>(
        GetProcAddress(module, "jk_app_run_client"));
#else
    auto metaFn = reinterpret_cast<const jk::JKAppMeta* (*)()>(
        dlsym(module, "jk_app_meta"));
    auto runFn = reinterpret_cast<int (*)(const char*)>(
        dlsym(module, "jk_app_run_client"));
#endif
    if (!metaFn || !runFn) {
        std::fprintf(stderr,
                     "App module '%s' does not export jk_app_meta/jk_app_run_client\n",
                     modulePath);
#ifdef _WIN32
        FreeLibrary(module);
#endif
        return 1;
    }

    const jk::JKAppMeta* meta = metaFn();
    std::printf("[client] module '%s' loaded: app='%s' title='%s' size=%dx%d\n",
                modulePath, meta->name, meta->title,
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

#ifdef _WIN32
    char tempDir[260] = ".";
    GetTempPathA(static_cast<unsigned long>(sizeof(tempDir) - 64), tempDir);
    // Unique temp name per process so several instances of the same .jkx can
    // run side by side; deleted again on exit.
    char tempPath[324] = {};
    std::snprintf(tempPath, sizeof(tempPath), "%sjkapp_%s_%lu.dll",
                  tempDir, mani.name.c_str(),
                  static_cast<unsigned long>(GetCurrentProcessId()));
#else
    // 플랜 G2 posix leg: GetTempPathA/GetCurrentProcessId → jk::fs::TempDir()
    // +getpid() — docs/68 stage-3 task 7이 같은 승계(tempDir 어댑터)를 이미
    // 했다: 후행 구분자 포함·실패 폴백 "."도 원문 tempDir 초기값과 동일 계약.
    // 접미만 플랫폼 값(.so). route는 v1 게이트(:3453 win32)라 여기는 도달
    // 불가하지만 몸통의 이식성은 확보한다(빌드/링크 성립용).
    const std::string tempDirStr = jk::fs::TempDir();
    long pid = static_cast<long>(getpid());
    char tempPath[324] = {};
    std::snprintf(tempPath, sizeof(tempPath), "%sjkapp_%s_%ld.so",
                  tempDirStr.c_str(), mani.name.c_str(), pid);
#endif
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
#ifdef _WIN32
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
#else
        // posix leg (플랜 G2): 여유 공간 조회만 플랫폼 분기 —
        // std::filesystem::space는 ec 오버로드만(throw/try 없음), 실패 시 0으로
        // 진단만 열화(원문 API 실패 폴백 문구와 같은 뜻). 삭제는 std::remove.
        std::error_code spaceEc;
        unsigned long long freeBytes = 0;
        const std::filesystem::space_info si = std::filesystem::space(
            std::filesystem::path(tempDirStr), spaceEc);
        if (!spaceEc) freeBytes = si.available;
        std::fprintf(stderr,
                     "Short write extracting '%s' (wrote %zu of %zu bytes; "
                     "%.1f GiB free on the temp volume)\n",
                     tempPath, written, dll.size(),
                     freeBytes / (1024.0 * 1024 * 1024));
        std::remove(tempPath);
#endif
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

#ifdef _WIN32
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
        // I2 필드 보존 (docs/67 단 2 룰링): 재생성 canonical 키를 조립한 뒤
        // JkxManifestMerge로 authored 원문(text)에 심는다 — scriptfile=, watch=,
        // 미래 키는 화이트리스트 폐기 대신 원문 행 위치 그대로 살아남는다.
        std::string regenerated;
        regenerated += "name=" +
            (authored.name.empty() ? std::string(appName) : authored.name) + "\n";
        regenerated += "title=" +
            (authored.title.empty() ? std::string(appName) : authored.title) + "\n";
        regenerated += "width=" +
            std::to_string(authored.width > 0 ? authored.width : 320) + "\n";
        regenerated += "height=" +
            std::to_string(authored.height > 0 ? authored.height : 240) + "\n";
        regenerated += "module=jkapp_script.dll\n";
        regenerated += "script=app.js\n";
        manifestText = jk::JkxManifestMerge(text, regenerated);
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

    // 아이콘 행은 병합 "이후"에 append된다(authored에 icon=이 이미 있으면
    // 병합 결과에 이중 등재 — 파서 last-wins라 값은 pack 것이 이기지만 행은
    // 남는다). 현행 authored manifest 3종(scriptdemo/workshop/passworddemo)
    // 는 icon= 없음 — 실질 영향 0; 세대별 아이콘 커스텀 패턴이 생기면
    // icon=을 regenerated에 심는 재배치 후보.
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
#endif // _WIN32 — jkx-pack은 LoadLibraryA로 메타를 소싱해 win32 전용 유지;
// 아래 slot-pack/jkx-list/jkx-extract는 순수 stdio+JKJkxFile(TOC 파서는
// 어댑터리)이라 docs/78 TX6에서 posix로 개방한다.

// slot-pack <slot> [out]: 워크숍 슬롯 → .jkx 출하 (스펙 2026-10-05-slot-ship
// -tool §2). 출하=워크숍 모드(MANI scriptfile=)+파묻힌 SCRI 시딩(§1 결정) —
// 수신 기기에서 게이트·배지·진실원 문화가 산다. jkctl pack(콘솔앱 zip 배포용,
// engine/tools/jkctl/main.cpp)과는 다른 도구 — 건드리지 않는다.
// 슬롯 파일은 읽기만 한다(쓰기·이력 접촉 금지 — 스펙 §2). 부재 시 오류 1
// 종료: 출하는 진실원이 있는 것만 팩한다(시딩-재시도 경로 없음).
static int RunSlotPack(const char* slotName, const char* outOverride) {
    std::string base;
    if (char* p = SDL_GetBasePath()) {
        base = p;
        SDL_free(p);
    }

    std::vector<uint8_t> script;
#ifdef _WIN32
    const std::string slotPath = base + "state\\scripts\\" + slotName + ".js";
#else   // docs/78 TX6 posix leg — 슬래시 경로(WSL·폰 동일 레이아웃)
    const std::string slotPath = base + "state/scripts/" + slotName + ".js";
#endif
    if (!ReadWholeFile(slotPath, script)) {
        std::fprintf(stderr, "slot-pack: no slot source '%s'\n",
                     slotPath.c_str());
        return 1;
    }
    // MODL 유실물 = *이 기기의* 공유 script 모듈(바이너리만 플랫폼 값).
    // 컨테이너 엔트리 이름은 "jkapp_script.dll"로 유지 — MANI module=
    // (SlotShipManifestText)과 소비 측 FindEntry("MODL", mani.module)의 키가
    // 될 뿐 파일명이 아니고, RunClientFromJkx는 풀어낸 바이너리를 temp
    // jkapp_<name>.<dll|so>에 넣어 로드하므로 키 이름은 플랫폼 무관 계약.
    std::vector<uint8_t> dll;
#ifdef _WIN32
    constexpr const char kScriptModuleName[] = "jkapp_script.dll";
#else
    constexpr const char kScriptModuleName[] = "jkapp_script.so";
#endif
    if (!ReadWholeFile(base + kScriptModuleName, dll)) {
        std::fprintf(stderr, "slot-pack: cannot read '%s'\n", kScriptModuleName);
        return 1;
    }

    // 능력 선언 = 사용량 자동 분석 — 게이트 토큰 표 단일 출처 재용(스펙 §3).
    // 도구가 표를 복제하지 않는다(표가 흔들리면 출하 선언도 같이 흔들린다).
    const std::string source(script.begin(), script.end());
    const std::vector<std::string> tokens =
        jk::JKScriptHost::CapabilityTokensForScript(source);
    std::string caps;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i) caps += ",";
        caps += tokens[i];
    }
    const std::string manifest = jk::SlotShipManifestText(slotName, tokens);

    // 3엔트리: MANI(신작 원문) + MODL(공유 script DLL) + SCRI(슬롯 원문) —
    // JkxFile::Write 레이아웃(pack_workshop.ps1과 동일 구조, docs/60 §3).
    std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
    entries.emplace_back("manifest.txt",
                         std::vector<uint8_t>(manifest.begin(), manifest.end()));
    entries.emplace_back("jkapp_script.dll", std::move(dll));
    entries.emplace_back("app.js", std::move(script));

    // CreateDirectoryA → create_directories: exists-ok 계약 동형(존재 시 자동
    // 통과, 실패 무음) — win32 도구 관측 출력에 변화 없음(TX6 개방 수반).
    std::error_code appsDirEc;
    (void)std::filesystem::create_directories(base + "apps", appsDirEc);
    const std::string outPath = (outOverride && outOverride[0])
        ? std::string(outOverride) : base +
#ifdef _WIN32
        "apps\\" + slotName
#else
        "apps/" + slotName
#endif
        + ".jkx";
    if (!jk::JKJkxFile::Write(outPath, entries)) return 1;

    // stale DLL 함정 — 재빌드 없이 팩하면 오래된 DLL이 파묻힌다(도구 출력에
    // 빌드 규율 경고 인쇄 — 스펙 §2, docs/60:334-339).
    std::fprintf(stderr,
                 "[slot-pack] 빌드 규율: 팩 직전 jkapp_script.dll이 현재 "
                 "소스인지 — stale DLL은 런타임 'is not defined'로만 발현 "
                 "(docs/60:334)\n");
    std::printf("packed %s (caps=%s)\n", outPath.c_str(), caps.c_str());
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
        // 출하 필드 인쇄(docs/74 — 슬롯 출하 라인, probe grep 몫): scriptfile=
        // 은 워크숍 분기의 신호, capabilities=는 게이트 선언 원문. 기존 행
        // 포맷 유지(개행은 width 뒤 한 번 — 기존 인쇄 체인 불변).
        if (!m.scriptfile.empty())
            std::printf(" scriptfile=%s", m.scriptfile.c_str());
        if (!m.capabilities.empty())
            std::printf(" capabilities=%s", m.capabilities.c_str());
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
    std::error_code xDirEc;
    (void)std::filesystem::create_directories(dir, xDirEc);  // exists_ok
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

    // 프레임 스트립 (docs/67 단 2 T2): 음수 y 자식이 타이틀 바 안에서 히트된다.
    {
        auto win = std::make_unique<JKWindow>("strip-selftest");
        win->SetWindowRect(JKRect{ 0, 0, 320, 240 });
        const JKRect strip{ 50, -21, 160, 22 };  // 클라이언트 좌표
        win->SetFrameStripRect(strip);
        auto combo = std::make_unique<JKComboBox>(strip, 0);
        JKComboBox* raw = combo.get();
        win->AddControl(std::move(combo));
        // 콤보 화면 좌표 = {52, 3, 160, 22}(surface y 3..25) — 중앙 y=14.
        check(win->HitTest(130, 14) == raw,  // strip 중앙 → 콤보
              "strip: hit reaches caption child");
        check(win->HitTestRegion(130, 14) == JKWindow::WindowRegion::Client,
              "strip: region is Client");
        check(win->HitTestRegion(130, 1) == JKWindow::WindowRegion::TitleBar,
              "strip: outside strip stays TitleBar");
        const JKRect s = win->GetFrameStripSurfaceRect();
        check(s.x == 52 && s.y == 3 && s.w == 160 && s.h == 22,
              "strip: surface rect = client + (kBorder,kTitle)");
        check(win->HitTest(10, 1) == win.get(),  // 타이틀 빈칸 → 자기 자신
              "strip: title gap still window");
    }

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

        // --- Semantic cursor act/snapshot logic layer (스펙 2026-09-22
        // -semantic-cursor §3/§8, fix round 1 MINOR-1/MINOR-2 논리 층).
        {
            jk::MineSweeperGame agame;

            agame.NewGameWithMines(9, 9, {{0, 0}});
            auto o = agame.Act("reveal", 0, 0);
            check(o.ok && o.opened == 1 && agame.Status() == std::string("lost"),
                  "act reveal on a known mine loses with opened=1");
            o = agame.Act("reveal", 5, 5);
            check(!o.ok && std::string(o.error) == "bad_state",
                  "act reveal after game over is bad_state");
            o = agame.Act("flag", 5, 5);
            check(!o.ok && std::string(o.error) == "bad_state",
                  "act flag after game over is bad_state");

            // MINOR-1 논리층 — reset은 언제든 유효 전이(스펙 §8). 죽은 게임
            // 위의 reset 뒤 전이가 정상 플레이로 돌아온다(뷰 래치 리셋은
            // MineGameWindow::Impl::ResetViewState — 뷰 레벨, e2e 프로브).
            o = agame.Act("reset", 0, 0);
            check(o.ok && !agame.IsGameOver() &&
                      agame.Status() == std::string("playing"),
                  "act reset recovers a dead game");
            o = agame.Act("reveal", 5, 5);
            check(o.ok, "act reveal after reset plays normally");
            o = agame.Act("reveal", 5, 5);
            check(!o.ok && std::string(o.error) == "bad_state",
                  "act reveal on an opened cell is bad_state");

            // MINOR-2 직렬화 규약 — 마크가 지뢰 공개보다 우선(SnapshotLines).
            agame.NewGameWithMines(9, 9, {{0, 0}, {8, 8}});
            check(agame.Act("question", 0, 0).ok, "act question sets the mark");
            check(agame.Act("reveal", 8, 8).ok &&
                      agame.Status() == std::string("lost"),
                  "unmarked mine boom enters loss reveal");
            auto lines = agame.SnapshotLines();
            check(lines.size() == 9, "snapshot has one line per row");
            check(lines[0][0] == '?',
                  "question-marked mine serializes as ? (MINOR-2)");
            check(lines[8][8] == '*', "unmarked mine serializes as * after loss");

            agame.NewGameWithMines(9, 9, {{0, 0}, {4, 4}});
            check(agame.Act("flag", 0, 0).ok, "act flag sets the mark");
            check(agame.Act("reveal", 4, 4).ok, "boom with flagged mine present");
            lines = agame.SnapshotLines();
            check(lines[0][0] == 'F',
                  "flag-marked mine keeps F after loss (standard notation)");

            agame.Act("reset", 0, 0);
            auto fresh = agame.SnapshotLines();
            bool allClosed = fresh.size() == 9;
            for (const std::string& l : fresh) {
                for (char ch : l) if (ch != '#') allClosed = false;
            }
            check(allClosed, "reset board serializes all-closed");

            jk::MineSweeperGame::ActKind parsed = jk::MineSweeperGame::ActKind::Reveal;
            check(!jk::MineSweeperGame::ParseActKind("detonate", parsed),
                  "unknown act kind token rejected");
            check(!agame.Act("detonate", 0, 0).ok, "act rejects unknown kind");

            // chord act (폰 실전 2판: 열린 숫자 칸의 펼치기 요구 — docs/64 §8).
            // 3x3 지뢰 (0,0), 중앙 (1,1) 개방 = 숫자 1. 이웃 8칸 중 (0,0) 빼고
            // 전부 무마크 닫힘 — 깃발 성립 후 chord면 7칸 일괄 개방 + 승리.
            agame.NewGameWithMines(3, 3, {{0, 0}});
            agame.OpenCell(1, 1);
            check(agame.GetAdjacent(1, 1) == 1, "chord act target is number 1");
            o = agame.Act("chord", 1, 1);
            check(!o.ok && std::string(o.error) == "bad_state",
                  "chord without satisfied flags is bad_state");
            check(agame.Act("flag", 0, 0).ok, "flag the mine for chord");
            o = agame.Act("chord", 1, 1);
            check(o.ok && o.opened == 7 &&
                      agame.Status() == std::string("won"),
                  "chord opens satisfied neighbors and wins");
            check(agame.ParseActKind("chord", parsed) &&
                      parsed == jk::MineSweeperGame::ActKind::Chord,
                  "chord parses to the Chord token");
        }
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

    // I2 — JkxManifestMerge: 필드 보존 (docs/67 단 2 룰링 — 재생성 화이트리스트
    // 폐기를 필드 보존으로). 슬롯 authored manifest의 scriptfile=/watch=/미래
    // capabilities= 가 재팩에 살아남는 게 이 함수의 유일 존재 이유.
    {
        const std::string authored =
            "name=slot1\n"
            "title=슬롯1\n"
            "width=320\n"
            "height=240\n"
            "module=jkapp_script.dll\n"
            "scriptfile=../slots/slot1/app.js\n"
            "watch=1\n"
            "capabilities=agent,timer\n"
            "\n"            // 빈 행 통과
            "# comment\n";  // 주석 통과
        const std::string regenerated =
            "name=slot1\n"
            "title=slot1\n"
            "width=320\n"
            "height=240\n"
            "module=jkapp_script.dll\n"
            "script=app.js\n";
        const std::string merged = jk::JkxManifestMerge(authored, regenerated);
        check(merged.find("scriptfile=../slots/slot1/app.js\n") != std::string::npos,
              "JkxManifestMerge preserves scriptfile verbatim");
        check(merged.find("watch=1\n") != std::string::npos,
              "JkxManifestMerge preserves watch verbatim");
        check(merged.find("capabilities=agent,timer\n") != std::string::npos,
              "JkxManifestMerge preserves unknown keys (future capabilities)");
        check(merged.find("# comment\n") != std::string::npos,
              "JkxManifestMerge passes through comments");
        check(merged.find("title=slot1\n") != std::string::npos &&
              merged.find("title=슬롯1\n") == std::string::npos,
              "canonical key replaced in place by regenerated value");
        check(merged.find("script=app.js\n") != std::string::npos,
              "authored-only missing regenerated key appended");
        // 빈 authored → regenerated 원문
        check(jk::JkxManifestMerge("", regenerated) == regenerated,
              "empty authored -> regenerated verbatim");
        // 행 순서 보존: 치환된 title은 authored 위치(2번째 행)에 있다.
        {
            size_t npos = std::string::npos;
            size_t atTitle = merged.find("title=slot1\n");
            size_t atName = merged.find("name=slot1\n");
            size_t atScriptfile = merged.find("scriptfile=");
            check(atTitle != npos && atName != npos && atScriptfile != npos &&
                      atName < atTitle && atTitle < atScriptfile,
                  "JkxManifestMerge keeps authored row order");
        }
        // 재팩 멱등: merged를 authored로 다시 병합해도 동일하다.
        check(jk::JkxManifestMerge(merged, regenerated) == merged,
              "JkxManifestMerge is idempotent");
    }

    // docs/74 능력 게이트 — MANI capabilities= 파스 (스펙 §3.2 — 원문 보존,
    // 토큰 분해는 소비자 JKScriptHost::EnableCapabilities 몫).
    {
        jk::JkxManifest m;
        check(m.Parse("name=x\nmodule=jkapp_script.dll\n"
                      "capabilities=Timer, input,network,weird\n"),
              "mani parse with capabilities succeeds");
        check(m.capabilities == "Timer, input,network,weird",
              "mani capabilities stored verbatim");
        jk::JkxManifest n;
        check(n.Parse("name=x\nmodule=jkapp_script.dll\ncapabilities=\n") &&
                  n.capabilities.empty(),
              "mani empty capabilities = no declaration");
        jk::JkxManifest o;
        check(o.Parse("name=x\nmodule=jkapp_script.dll\n") &&
                  o.capabilities.empty() && !o.scriptfile.empty() == false,
              "mani without capabilities parses as before");
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
        // Soft-wrap flag (docs/26 단계 4): the wrapped row is flagged, the
        // continuation row is not.
        check(grid.RowWrapped(0) && !grid.RowWrapped(1),
              "terminal: soft wrap flags the row");
        // ED 2 wipes the logical structure — flags reset with the cells.
        feed("\x1b[2J");
        check(!grid.RowWrapped(0), "terminal: ED2 clears wrap flags");

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

        // Reflow resize (docs/26 단계 4): short logical lines survive a
        // width change; content pads top (bottom-anchored assembly), the
        // cursor maps to its logical line.
        feed("\x1b[2J\x1b[1;1Haaa\r\nbbb\r\nccc");
        grid.Resize(10, 4);
        check(grid.Cols() == 10 && grid.Rows() == 4 &&
                  grid.Cell(0, 1).cp == 'a' && grid.Cell(0, 2).cp == 'b' &&
                  grid.Cell(0, 3).cp == 'c' &&
                  grid.GetCursor().x == 3 && grid.GetCursor().y == 3,
              "terminal: reflow keeps short lines");

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
        check(grid.ScrollbackLine(0).cells[0].cp == 'r' &&
                  grid.ScrollbackLine(0).cells[1].cp == '0' &&
                  !grid.ScrollbackLine(0).wrapped,
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

        // --- Reflow round trip (docs/26 단계 4) ------------------------------
        {
            jk::JKTerminalGrid g;
            jk::JKVtParser p;
            p.Attach(&g);
            auto feed2 = [&p](const char* s) {
                p.Feed(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
            };
            auto txt = [&g](int r) {
                std::string s;
                for (int c = 0; c < g.Cols(); ++c) {
                    const uint32_t cp = g.Cell(c, r).cp;
                    s.push_back(cp >= 0x20 && cp < 0x7F ? static_cast<char>(cp) : ' ');
                }
                while (!s.empty() && s.back() == ' ') s.pop_back();
                return s;
            };

            // 10x3: one soft-wrapped line (no hard newline between the
            // halves) + one hard line; cursor parked on the last line.
            g.Resize(10, 3);
            feed2("abcdefghijKLMNO\r\nPQ");
            check(g.RowWrapped(0) && !g.RowWrapped(1),
                  "reflow: wrap flag across the screen");
            check(g.GetCursor().x == 2 && g.GetCursor().y == 2,
                  "reflow: cursor after input");

            // Narrow to 6x3: the logical line rewraps abcdef|ghijKL|MNO and
            // the overflow row "abcdef" (itself soft-wrapped) goes to the
            // history; the cursor line stays on screen.
            g.Resize(6, 3);
            check(g.ScrollbackLines() == 1 &&
                      g.ScrollbackLine(0).cells[0].cp == 'a' &&
                      g.ScrollbackLine(0).wrapped,
                  "reflow: overflow row recorded as wrapped history");
            check(txt(0) == "ghijKL" && txt(1) == "MNO" && txt(2) == "PQ",
                  "reflow: rewrap to the narrower width");
            check(g.RowWrapped(0) && !g.RowWrapped(1) && !g.RowWrapped(2),
                  "reflow: rewrapped rows carry fresh flags");
            check(g.GetCursor().x == 2 && g.GetCursor().y == 2,
                  "reflow: cursor mapped to its logical line");

            // Widen back to 10x3: unwrap merges the history row back in —
            // the logical lines round-trip to the original layout.
            g.Resize(10, 3);
            check(g.ScrollbackLines() == 0, "reflow: history merges back in");
            check(txt(0) == "abcdefghij" && txt(1) == "KLMNO" && txt(2) == "PQ",
                  "reflow: round trip restores logical lines");
            check(g.GetCursor().x == 2 && g.GetCursor().y == 2,
                  "reflow: cursor survives the round trip");

            // A wide glyph never splits across a rewrap boundary: the chunk
            // ends before the glyph, which moves to the next row whole (with
            // its follower). Bottom-anchored: the 5-col layout shows only the
            // last produced row on screen, the wrapped "1234" goes to history.
            feed2("\x1b[2J\x1b[1;1H1234가");
            g.Resize(5, 1);
            check(g.Cell(0, 0).cp == 0xAC00 && g.Cell(1, 0).width == 0,
                  "reflow: wide glyph wraps whole at the boundary");
            check(g.ScrollbackLine(0).cells[3].cp == '4' &&
                      g.ScrollbackLine(0).cells[4].cp == 0 &&
                      g.ScrollbackLine(0).wrapped,
                  "reflow: chunk ends before the wide glyph");
        }

        // --- Wide-glyph diagnostics (docs/26 단계 1) --------------------------
        // docs/78 TX3 폰 실측: 아래 A/B/C는 모두 Windows 리터럴(C:/Windows/
        // Fonts, ConPTY powershell.exe)을 먹는다 — 폰(Termux)에서는 존재하지
        // 않는 것이 당연하므로 posix에서 스킵한다. WSL은 interop 덕에 우연히
        // 통과하던 것(cmd.exe·powershell.exe가 그대로 부트) — interop 유무로
        // 판정이 흔들리지 않게 _WIN32로 고정한다.
#ifdef _WIN32
        //
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
#endif  // _WIN32 — 와이드글리프 진단 A/B/C는 Windows 리터럴 전용

        // docs/70 §8.4 판정 2 봉합 — .ttcface-0/CFF 가드(순수 함수, 메모리 유닛:
        // 플랫폼 리터럴 없음 — ttf 실물 대신 합성 sfnt로 거리를 측정한다).
        {
            // 가짜 sfnt: version 1.0, numTables=1, 레코드 태그만 채운다.
            std::vector<uint8_t> sfnt(28, 0);
            sfnt[0] = 0x00; sfnt[1] = 0x01; sfnt[2] = 0x00; sfnt[3] = 0x00;
            sfnt[4] = 0x00; sfnt[5] = 0x01;  // numTables=1
            sfnt[12] = 'C'; sfnt[13] = 'F'; sfnt[14] = 'F'; sfnt[15] = ' ';
            check(jk::text::SfntFaceHasCff(sfnt.data(), sfnt.size(), 0),
                  "text: sfnt 'CFF ' face is rejected");
            sfnt[15] = 'x';  // tag 'CFFx' — CFF 아님
            check(!jk::text::SfntFaceHasCff(sfnt.data(), sfnt.size(), 0),
                  "text: sfnt non-CFF face passes the guard");
            sfnt[4] = 0x00; sfnt[5] = 0x02;  // numTables=2 — 레코드 1개 분량만
            check(jk::text::SfntFaceHasCff(sfnt.data(), sfnt.size(), 0),
                  "text: truncated sfnt dir is conservatively rejected");
            // ttcf 컬렉션 헤더 걷기(stb 원문 함수) — face-0 오프셋=0 계약.
            std::vector<uint8_t> ttcf(16, 0);
            ttcf[0] = 't'; ttcf[1] = 't'; ttcf[2] = 'c'; ttcf[3] = 'f';
            ttcf[5] = 0x01;                // version = 0x00010000 (stb 계약)
            ttcf[11] = 1;                  // numFonts=1 (be)
            const int off = stbtt_GetFontOffsetForIndex(ttcf.data(), 0);
            check(off == 0, "text: synthetic ttcf face-0 offset walks");
        }
    }

    // Terminal selection pure functions (docs/26 단계 2, JKTermSelection.h):
    // normalization, row extraction with UTF-8 re-encoding, paste sanitizer
    // with the bracketed-paste gate.
    {
        using jk::JKTermCell;
        using jk::JKTermSelRect;

        // NormalizeSel: min/max ordering of anchor/end.
        {
            const JKTermSelRect s = jk::NormalizeSel(5, 4, 1, 1, 20, 6);
            check(s.x0 == 1 && s.y0 == 1 && s.x1 == 5 && s.y1 == 4,
                  "termselect: normalize orders anchor/end");
        }
        // NormalizeSel: clamps out-of-range endpoints (drag past the edge).
        {
            const JKTermSelRect s = jk::NormalizeSel(-3, -2, 99, 99, 20, 6);
            check(s.x0 == 0 && s.y0 == 0 && s.x1 == 19 && s.y1 == 5,
                  "termselect: normalize clamps into the grid");
        }
        // NormalizeSel: degenerate grid -> empty rect.
        check(jk::NormalizeSel(0, 0, 0, 0, 0, 0).Empty(),
              "termselect: empty grid normalizes to empty rect");

        // Scrolled selection mapping — production code (TerminalView::
        // CellFromPoint delegates to ViewportRowToLive): a viewport row r maps
        // to the live grid row r - off, clamped into the grid; rows sitting
        // over scrollback snapshots (r < off) clamp onto live row 0 — v1
        // selects live rows only.
        {
            const int rows = 6, off = 2;
            check(jk::ViewportRowToLive(0, off, rows) == 0 &&
                      jk::ViewportRowToLive(2, off, rows) == 0 &&
                      jk::ViewportRowToLive(4, off, rows) == 2 &&
                      jk::ViewportRowToLive(5, off, rows) == 3,
                  "termselect: scrolled viewport row maps to live grid row");
            check(jk::ViewportRowToLive(99, off, rows) == 5 &&
                      jk::ViewportRowToLive(-5, off, rows) == 0,
                  "termselect: viewport row mapping over-clamps");
            check(jk::ViewportRowToLive(0, 0, 0) == 0,
                  "termselect: viewport row mapping on empty grid");
        }

        // Dummy 4x2 grid for the extractor (generic accessor, no JKTerminalGrid).
        std::vector<JKTermCell> cells(4 * 2);
        auto setCell = [&cells](int c, int r, uint32_t cp, uint8_t width = 1) {
            cells[static_cast<size_t>(r) * 4 + c].cp = cp;
            cells[static_cast<size_t>(r) * 4 + c].width = width;
        };
        auto cellAt = [&cells](int c, int r) -> const JKTermCell& {
            return cells[static_cast<size_t>(r) * 4 + c];
        };

        // Row truncation: cells up to the LAST non-empty cell only.
        setCell(0, 0, 'a');
        setCell(1, 0, 'b');
        {
            const std::string text =
                jk::ExtractSelectedText(cellAt, JKTermSelRect{0, 0, 2, 0});
            check(text == "ab", "termselect: row stops at last non-empty cell");
        }
        // Hangul re-encoding: 'a' 0x61, '가' 0xAC00 (wide, + width-0 follower
        // dummy) — must re-encode to the canonical 3 UTF-8 bytes; the follower
        // terminates the row naturally.
        setCell(0, 1, 'a');
        setCell(1, 1, 0xAC00, 2);
        setCell(2, 1, 0, 0);
        {
            const std::string text =
                jk::ExtractSelectedText(cellAt, JKTermSelRect{0, 1, 2, 1});
            check(text == "\x61\xEA\xB0\x80",
                  "termselect: hangul AC00 re-encodes to UTF-8");
        }
        // Empty rows are joined, never skipped; multi-row join with "\n".
        {
            const std::string text =
                jk::ExtractSelectedText(cellAt, JKTermSelRect{0, 0, 2, 1});
            check(text == "ab\na\xEA\xB0\x80",
                  "termselect: rows join with \\n");
        }
        // A selection over only-empty cells yields an empty line, not garbage.
        {
            const std::string text =
                jk::ExtractSelectedText(cellAt, JKTermSelRect{2, 1, 2, 1});
            check(text.empty(), "termselect: follower-only row extracts empty");
        }
        // Empty selection -> empty string.
        check(jk::ExtractSelectedText(cellAt, JKTermSelRect{}).empty(),
              "termselect: empty selection extracts nothing");

        // Interior gap copies as a space: an erased/never-written cell (cp==0,
        // width==1) between two written cells must not become a raw NUL byte
        // (a NUL inside std::string truncates SDL_SetClipboardText at that
        // byte).
        // Reuses row 1 after clearing it back to the 4x2 dummy grid's layout.
        setCell(0, 1, 'x');
        setCell(1, 1, 0);
        setCell(2, 1, 'y');
        {
            const std::string text =
                jk::ExtractSelectedText(cellAt, JKTermSelRect{0, 1, 2, 1});
            check(text == std::string("x y"),
                  "termselect: interior gap extracts as space");
        }
        // A mid-row wide follower stays silent (no phantom space between the
        // wide glyph and the cell after it — the hangul case above only
        // exercises a row-*ending* follower).
        setCell(0, 1, 'x');
        setCell(1, 1, 0xAC00, 2);  // 가: wide glyph, follower at (2,1)
        setCell(2, 1, 0, 0);       // follower: silent
        setCell(3, 1, 'y');
        {
            const std::string text =
                jk::ExtractSelectedText(cellAt, JKTermSelRect{0, 1, 3, 1});
            check(text == std::string("x") + "\xEA\xB0\x80" + "y",
                  "termselect: mid-row wide follower extracts no phantom space");
        }

        // Paste sanitizer: \r\n and lone \r both become \n; the ESC byte is
        // removed (the rest of a pasted-in VT sequence stays as plain text).
        {
            const std::string out =
                jk::SanitizeClipboardPaste("a\r\nb\rc\x1b[31md", false);
            check(out == "a\nb\nc[31md",
                  "termselect: paste newline + ESC sanitize");
        }
        // Bracketed paste gate: wrapper added, payload ESC still stripped.
        {
            const std::string out =
                jk::SanitizeClipboardPaste("hi\x1b[0m", true);
            check(out == "\x1b[200~hi[0m\x1b[201~",
                  "termselect: bracketed paste wraps sanitized payload");
        }
        // Bracketed off: no wrapper bytes.
        check(jk::SanitizeClipboardPaste("hi", false) == "hi",
              "termselect: unbracketed paste has no wrapper");
        // Stray C0 controls (incl. embedded NUL, which truncates at the
        // ConPTY input layer) are dropped; \n and \t survive.
        {
            // Octal escapes (\0, \3) are exactly bounded; a hex escape like
            // \x03b would greedily consume the 'b' (0x3B).
            const std::string out =
                jk::SanitizeClipboardPaste(std::string("a\0\3b\tc\nd", 8),
                                           false);
            check(out == "ab\tc\nd",
                  "termselect: paste drops C0 controls, keeps \\n and \\t");
        }

        // I3 — 전체 행 공간 헬퍼 (O6 스크롤백 선택, docs/65 수용→잔여 소각).
        {
            check(jk::ViewportRowToFull(0, 0, 100) == 100 &&
                  jk::ViewportRowToFull(3, 0, 100) == 103,
                  "ViewportRowToFull: offset 0 -> live rows shifted by hist");
            check(jk::ViewportRowToFull(2, 4, 100) == 98,
                  "ViewportRowToFull: scrolled-back viewport row lands in scrollback");
            const JKTermSelRect s = jk::NormalizeSelFull(5, 96, 1, 90, 20, 100, 6);
            check(s.x0 == 1 && s.y0 == 90 && s.x1 == 5 && s.y1 == 96 &&
                  !s.Empty() && s.Contains(3, 93),
                  "NormalizeSelFull: orders+clamps into full line space");
            check(jk::NormalizeSelFull(0, 0, 99, 299, 20, 100, 6)
                      .y1 == 105,
                  "NormalizeSelFull: rows clamp to hist+rows-1 (105)");
            // 클래식 휠 (비SGR 신고 — acknowledged gap 소각)
            check(jk::EncodeWheelX10(true, 0, 10, 5) ==
                      std::string("\x1b[M") + char(32 + 64) + char(32 + 10) +
                          char(32 + 5),
                  "EncodeWheelX10: up = Cb 64 classic bytes");
            check(jk::EncodeWheelX10(false, 16, 99, 250) ==
                      std::string("\x1b[M") + char(32 + 65 + 16) +
                          char(32 + 99) + char(32 + 223),
                  "EncodeWheelX10: down+ctrl clamps x to 223");
        }

        // IME pre-edit decode (docs/26 단계 5, spec §3): the pure UTF-8 ->
        // codepoint helper the TerminalView cursor overlay consumes.
        {
            const auto ascii = jk::DecodeUtf8("ab");
            check(ascii.size() == 2 && ascii[0] == 'a' && ascii[1] == 'b',
                  "termselect: decode utf8 ascii");
            const auto han = jk::DecodeUtf8("\xED\x95\x9C");   // U+D55C 한
            check(han.size() == 1 && han[0] == 0xD55C &&
                      jk::JKTermCharWidth(han[0]) == 2,
                  "termselect: decode utf8 hangul syllable is wide");
            // Round-trip through the encoder (AppendUtf8 is the mirror).
            std::string re;
            for (uint32_t cp : jk::DecodeUtf8("a\xEA\xB0\x80")) {
                jk::AppendUtf8(re, cp);
            }
            check(re == "a\xEA\xB0\x80", "termselect: decode/encode roundtrip");
            // Invalid leads, overlong forms and surrogates -> U+FFFD, with
            // resync at the next byte (counts are byte-position exact).
            const auto bad = jk::DecodeUtf8("\xFF\x41");   // stray lead + 'A'
            check(bad.size() == 2 && bad[0] == 0xFFFD && bad[1] == 'A',
                  "termselect: invalid utf8 lead decodes as FFFD");
            const auto over = jk::DecodeUtf8("\xC0\xAF");   // overlong 2-byte
            check(over.size() == 2 && over[0] == 0xFFFD &&
                      over[1] == 0xFFFD,
                  "termselect: overlong utf8 decodes as FFFD");
            const auto sur = jk::DecodeUtf8("\xED\xA0\x80");   // surrogate D800
            check(sur.size() == 3 && sur[0] == 0xFFFD &&
                      sur[1] == 0xFFFD && sur[2] == 0xFFFD,
                  "termselect: surrogate utf8 decodes as FFFD");
            // A truncated sequence emits FFFD for the lead and resyncs at the
            // orphan continuation byte.
            const auto trunc = jk::DecodeUtf8("\xEA\xB0");
            check(trunc.size() == 2 && trunc[0] == 0xFFFD && trunc[1] == 0xFFFD,
                  "termselect: truncated utf8 decodes as FFFD");
            // Beyond U+10FFFF (F4 90 80 80 = 0x110000) is not a scalar value:
            // FFFD per bad byte, then resync on the 'A'.
            const auto overflow = jk::DecodeUtf8("\xF4\x90\x80\x80" "A");
            check(overflow.size() == 5 && overflow[0] == 0xFFFD &&
                      overflow[1] == 0xFFFD && overflow[2] == 0xFFFD &&
                      overflow[3] == 0xFFFD && overflow[4] == 'A',
                  "termselect: >U+10FFFF utf8 decodes as FFFD + resync");
        }
    }

    // Scrub clock pure logic (docs/50 §11, JKScrubClock.h): chase rate clamp,
    // direction flips, target landing, session reset seeding.
    {
        jk::JKScrubClock c;
        c.Reset(10.0);
        check(c.Pos() == 10.0 && c.Target() == 10.0, "scrubclock: reset seeds D=T");
        c.SetTarget(11.0);
        check(c.Chase(30.0, 8.0, 120.0) > 10.0 && c.Pos() < 11.0,
              "scrubclock: chase advances but does not overshoot");
        for (int i = 0; i < 100; ++i) c.Chase(30.0, 8.0, 120.0);
        check(c.Pos() == 11.0, "scrubclock: chase lands exactly on target");
        c.SetTarget(9.0);
        c.Chase(30.0, 8.0, 120.0);
        check(c.Pos() < 11.0, "scrubclock: chase retreats backward");
        // Rate clamp: one UI frame moves at most flowMax frames.
        c.Reset(0.0);
        c.SetTarget(100.0);
        const double d0 = c.Chase(30.0, 8.0, 120.0);
        check(d0 - 0.0 <= 8.0 / 30.0 + 1e-9, "scrubclock: rate clamped to flowMax/fps");
        // Unknown fps falls back to 30.
        c.Reset(0.0);
        c.SetTarget(100.0);
        const double d1 = c.Chase(0.0, 8.0, 120.0);
        check(d1 <= 8.0 / 30.0 + 1e-9, "scrubclock: fps<=0 quantizes at 30");
        // Duration clamp.
        c.Reset(0.0);
        c.SetTarget(1000.0);
        for (int i = 0; i < 10000; ++i) c.Chase(30.0, 8.0, 50.0);
        check(c.Pos() == 50.0, "scrubclock: D clamps into [0,dur]");
    }

    // Terminal mouse-report + DECSCUSR parser tracking (docs/26 단계 3,
    // spec §1/§6): DECSET 1000/1002/1003/1006/1 set+reset, alt-screen swap
    // leaves mouse modes alone, DECSCUSR maps Ps to the grid cursor shape.
    {
        jk::JKTerminalGrid grid;
        jk::JKVtParser parser;
        parser.Attach(&grid);
        auto feed = [&parser](const char* s) {
            parser.Feed(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
        };
        using jk::TermMouseMode;
        using jk::CursorShape;

        grid.Resize(20, 6);   // alt-screen/erase paths touch dirtyRows_

        // Defaults: no mouse reporting, no app cursor keys, block cursor.
        check(parser.MouseMode() == TermMouseMode::Off &&
                  !parser.SgrMouse() && !parser.AppCursorKeys(),
              "termmouse: parser defaults are Off/no-sgr/no-appcursor");
        check(grid.GetCursorShape() == CursorShape::Block,
              "termmouse: default cursor shape is Block");

        // DECSET 1000 (normal tracking) set + reset.
        feed("\x1b[?1000h");
        check(parser.MouseMode() == TermMouseMode::Normal, "termmouse: 1000h → Normal");
        feed("\x1b[?1000l");
        check(parser.MouseMode() == TermMouseMode::Off, "termmouse: 1000l → Off");

        // DECSET 1002 (button-event tracking) set + reset.
        feed("\x1b[?1002h");
        check(parser.MouseMode() == TermMouseMode::Button, "termmouse: 1002h → Button");
        feed("\x1b[?1002l");
        check(parser.MouseMode() == TermMouseMode::Off, "termmouse: 1002l → Off");

        // DECSET 1003 (any-event tracking) set + reset; a later 1000 set
        // downgrades Any back to Normal (last DECSET wins, xterm behavior).
        feed("\x1b[?1003h");
        check(parser.MouseMode() == TermMouseMode::Any, "termmouse: 1003h → Any");
        feed("\x1b[?1000h");
        check(parser.MouseMode() == TermMouseMode::Normal,
              "termmouse: 1000h after 1003h downgrades to Normal");
        feed("\x1b[?1000l");
        check(parser.MouseMode() == TermMouseMode::Off, "termmouse: 1000l → Off");

        // DECSET 1006 (SGR encoding) is orthogonal to the mode bits.
        feed("\x1b[?1000h\x1b[?1006h");
        check(parser.MouseMode() == TermMouseMode::Normal && parser.SgrMouse(),
              "termmouse: 1000h+1006h → Normal + SGR");
        feed("\x1b[?1006l");
        check(parser.MouseMode() == TermMouseMode::Normal && !parser.SgrMouse(),
              "termmouse: 1006l clears SGR, keeps mode");
        feed("\x1b[?1000l\x1b[?1006h");
        check(parser.MouseMode() == TermMouseMode::Off && parser.SgrMouse(),
              "termmouse: SGR flag independent of mode reset");
        feed("\x1b[?1006l");

        // COMBINED DECSET params (probe_terminal_mouse regression): apps
        // enable mouse reporting with ONE CSI — "\x1b[?1000;1006h". The
        // private-mode handler used to apply only p[0], silently dropping
        // 1006 and leaving the view on classic X10 encoding.
        feed("\x1b[?1000;1006h");
        check(parser.MouseMode() == TermMouseMode::Normal && parser.SgrMouse(),
              "termmouse: combined ?1000;1006h → Normal + SGR");
        feed("\x1b[?1002;1006h");
        check(parser.MouseMode() == TermMouseMode::Button && parser.SgrMouse(),
              "termmouse: combined ?1002;1006h → Button + SGR");
        feed("\x1b[?1000;1006l");
        check(parser.MouseMode() == TermMouseMode::Off && !parser.SgrMouse(),
              "termmouse: combined ?1000;1006l → Off + no-SGR");

        // DECSET 1 (application cursor keys) set + reset.
        feed("\x1b[?1h");
        check(parser.AppCursorKeys(), "termmouse: 1h → app cursor keys");
        feed("\x1b[?1l");
        check(!parser.AppCursorKeys(), "termmouse: 1l → normal cursor keys");

        // Alt-screen enter/exit must NOT clear mouse modes (xterm standard —
        // the application enables/disables mouse reporting itself).
        feed("\x1b[?1002h\x1b[?1006h\x1b[?1h");
        feed("\x1b[?1049h");
        check(grid.InAltScreen() && parser.MouseMode() == TermMouseMode::Button &&
                  parser.SgrMouse() && parser.AppCursorKeys(),
              "termmouse: 1049h keeps mouse modes");
        feed("\x1b[?1049l");
        check(!grid.InAltScreen() && parser.MouseMode() == TermMouseMode::Button &&
                  parser.SgrMouse() && parser.AppCursorKeys(),
              "termmouse: 1049l keeps mouse modes");
        feed("\x1b[?1002l\x1b[?1006l\x1b[?1l");

        // DECSCUSR "CSI Ps SP q" → grid cursor shape (blink variants collapse).
        check(grid.GetCursorShape() == CursorShape::Block,
              "termmouse: shape default Block before DECSCUSR");
        feed("\x1b[2 q");
        check(grid.GetCursorShape() == CursorShape::Block, "termmouse: DECSCUSR 2 → Block");
        feed("\x1b[3 q");
        check(grid.GetCursorShape() == CursorShape::Underline,
              "termmouse: DECSCUSR 3 → Underline");
        feed("\x1b[4 q");
        check(grid.GetCursorShape() == CursorShape::Underline,
              "termmouse: DECSCUSR 4 (blink) → Underline");
        feed("\x1b[5 q");
        check(grid.GetCursorShape() == CursorShape::Bar, "termmouse: DECSCUSR 5 → Bar");
        feed("\x1b[6 q");
        check(grid.GetCursorShape() == CursorShape::Bar,
              "termmouse: DECSCUSR 6 (blink) → Bar");
        feed("\x1b[1 q");
        check(grid.GetCursorShape() == CursorShape::Block,
              "termmouse: DECSCUSR 1 → Block (default)");
        // Ps 0 is the reset shells actually emit — also Block.
        feed("\x1b[3 q");   // move off Block first so the reset is observable
        check(grid.GetCursorShape() == CursorShape::Underline,
              "termmouse: DECSCUSR 3 → Underline (pre-reset)");
        feed("\x1b[0 q");
        check(grid.GetCursorShape() == CursorShape::Block,
              "termmouse: DECSCUSR 0 → Block (reset)");

        // DECSCUSR survives alt-screen swap and grid resize — only RIS resets
        // it (review MINOR-3).
        feed("\x1b[5 q\x1b[?1049h\x1b[?1049l");
        grid.Resize(30, 8);
        check(grid.GetCursorShape() == CursorShape::Bar,
              "termmouse: shape survives alt swap + resize");
        feed("\x1b[5 q");
        check(grid.GetCursorShape() == CursorShape::Bar,
              "termmouse: shape is Bar before RIS");
        feed("\x1b" "c");   // RIS — full reset ("\x1bc" would lex as hex \x1BC)
        check(grid.GetCursorShape() == CursorShape::Block,
              "termmouse: RIS resets cursor shape to Block");
        feed("\x1b[2 q");

        // Plain CSI 'q' without the SP intermediate is ignored (not DECSCUSR).
        feed("\x1b[3q");
        check(grid.GetCursorShape() == CursorShape::Block,
              "termmouse: CSI 3 q without SP intermediate ignored");
    }

    // Terminal input encoding pure functions (docs/26 단계 3, JKTermInput.h,
    // spec §2/§4): SGR mouse press/release/motion/wheel + modifier wire bits,
    // X10 fallback with the 223 cap, arrow CSI/SS3/modifier forms, wheel →
    // arrow keys. Pure string builders — no grid or parser needed.
    {
        using jk::MouseKind;
        using jk::NavKey;

        // SGR press, no modifiers: "\x1b[<" + btn + ";" + x + ";" + y + 'M'
        // (coordinates arrive 1-based; the encoder prints them verbatim).
        check(jk::EncodeMouseSgr(0, 10, 5, MouseKind::Press, 0) ==
                  "\x1b[<0;10;5M",
              "terminput: SGR press left no mods");
        // Modifier wire bits (spec §2): +4 Shift, +8 Meta, +16 Ctrl.
        check(jk::EncodeMouseSgr(0, 1, 1, MouseKind::Press, 16) ==
                  "\x1b[<16;1;1M",
              "terminput: SGR press ctrl+left = b 16");
        check(jk::EncodeMouseSgr(2, 40, 20, MouseKind::Press, 4) ==
                  "\x1b[<6;40;20M",
              "terminput: SGR press shift+right = b 6");
        check(jk::EncodeMouseSgr(1, 40, 20, MouseKind::Press, 4 | 16) ==
                  "\x1b[<21;40;20M",
              "terminput: SGR press ctrl+shift+middle = b 21");
        // Release terminates with lowercase 'm'.
        check(jk::EncodeMouseSgr(0, 10, 5, MouseKind::Release, 0) ==
                  "\x1b[<0;10;5m",
              "terminput: SGR release uses 'm'");
        // Motion adds +32 to the button byte.
        check(jk::EncodeMouseSgr(0, 3, 4, MouseKind::Motion, 0) ==
                  "\x1b[<32;3;4M",
              "terminput: SGR motion = b 32");
        check(jk::EncodeMouseSgr(1, 3, 4, MouseKind::Motion, 0) ==
                  "\x1b[<33;3;4M",
              "terminput: SGR motion middle = b 33");
        // Wheel: btn 64 (up) / 65 (down) passed as the button.
        check(jk::EncodeMouseSgr(64, 7, 2, MouseKind::Press, 0) ==
                  "\x1b[<64;7;2M",
              "terminput: SGR wheel up = b 64");
        check(jk::EncodeMouseSgr(65, 7, 2, MouseKind::Press, 0) ==
                  "\x1b[<65;7;2M",
              "terminput: SGR wheel down = b 65");
        // Wheel reports carry the same wire modifier bits as button events.
        check(jk::EncodeMouseSgr(64, 7, 2, MouseKind::Press, 4) ==
                  "\x1b[<68;7;2M",
              "terminput: SGR wheel shift+up = b 68");
        check(jk::EncodeMouseSgr(65, 7, 2, MouseKind::Press, 16) ==
                  "\x1b[<81;7;2M",
              "terminput: SGR wheel ctrl+down = b 81");

        // Classic (non-SGR) encoding: "\x1b[M" + 32+Cb + 32+x + 32+y.
        // Press = Cb button (0/1/2); release = Cb 3 (xterm NORMAL/BUTTON
        // tracking — the classic protocol cannot name the released button;
        // press-only is DECSET 9, a mode we do not implement).
        {
            const std::string s = jk::EncodeMouseX10(0, 10, 5, MouseKind::Press);
            check(s.size() == 6 &&
                      static_cast<unsigned char>(s[3]) == 32 + 0 &&
                      static_cast<unsigned char>(s[4]) == 32 + 10 &&
                      static_cast<unsigned char>(s[5]) == 32 + 5,
                  "terminput: X10 press encodes 32-offset bytes");
        }
        {
            const std::string s = jk::EncodeMouseX10(1, 10, 5, MouseKind::Release);
            check(s.size() == 6 &&
                      static_cast<unsigned char>(s[3]) == 32 + 3 &&
                      static_cast<unsigned char>(s[4]) == 32 + 10 &&
                      static_cast<unsigned char>(s[5]) == 32 + 5,
                  "terminput: classic release = Cb 3 (btn ignored)");
        }
        // Out-of-range coordinates clamp to 1..223 (byte range floor/ceiling).
        {
            const std::string s = jk::EncodeMouseX10(1, 300, 0, MouseKind::Press);
            check(static_cast<unsigned char>(s[3]) == 32 + 1 &&
                      static_cast<unsigned char>(s[4]) == 32 + 223 &&
                      static_cast<unsigned char>(s[5]) == 32 + 1,
                  "terminput: X10 clamps x/y into 1..223");
        }
        check(jk::EncodeMouseX10(2, 223, 223, MouseKind::Press)[4] ==
                  static_cast<char>(255),
              "terminput: X10 max coordinate stays in one byte");
        // Classic MOTION — 갭 봉합 (I4): Cb = btn+32로 나간다 (호버 btn 3 →
        // Cb 35). 옛 v1 갭(docs/41 §7, empty) 검사는 여기로 승계.
        check(static_cast<unsigned char>(
                      jk::EncodeMouseX10(0, 10, 5, MouseKind::Motion)[3]) ==
                  32 + 32 &&
                  static_cast<unsigned char>(jk::EncodeMouseX10(
                      3, 10, 5, MouseKind::Motion)[3]) == 32 + 35,
              "terminput: classic motion = Cb btn+32 (hover 35)");

        // Arrows, no mods, normal cursor keys (CSI) — the legacy forms,
        // byte-for-byte.
        check(jk::EncodeArrow(NavKey::Up, 0, false) == "\x1b[A" &&
                  jk::EncodeArrow(NavKey::Down, 0, false) == "\x1b[B" &&
                  jk::EncodeArrow(NavKey::Right, 0, false) == "\x1b[C" &&
                  jk::EncodeArrow(NavKey::Left, 0, false) == "\x1b[D",
              "terminput: arrows CSI (no mods, !appCursor)");
        check(jk::EncodeArrow(NavKey::Home, 0, false) == "\x1b[H" &&
                  jk::EncodeArrow(NavKey::End, 0, false) == "\x1b[F",
              "terminput: home/end CSI (no mods, !appCursor)");
        // App cursor keys (DECSET 1): SS3 form.
        check(jk::EncodeArrow(NavKey::Up, 0, true) == "\x1bOA" &&
                  jk::EncodeArrow(NavKey::Down, 0, true) == "\x1bOB" &&
                  jk::EncodeArrow(NavKey::Right, 0, true) == "\x1bOC" &&
                  jk::EncodeArrow(NavKey::Left, 0, true) == "\x1bOD",
              "terminput: arrows SS3 (no mods, appCursor)");
        check(jk::EncodeArrow(NavKey::Home, 0, true) == "\x1bOH" &&
                  jk::EncodeArrow(NavKey::End, 0, true) == "\x1bOF",
              "terminput: home/end SS3 (appCursor)");
        // PgUp/PgDn keep the plain tilde form without mods (both modes).
        check(jk::EncodeArrow(NavKey::PgUp, 0, false) == "\x1b[5~" &&
                  jk::EncodeArrow(NavKey::PgDn, 0, false) == "\x1b[6~" &&
                  jk::EncodeArrow(NavKey::PgUp, 0, true) == "\x1b[5~" &&
                  jk::EncodeArrow(NavKey::PgDn, 0, true) == "\x1b[6~",
              "terminput: pgup/pgdn plain tilde form");
        // Modifier forms: m = 1 + shift1 + alt2 + ctrl4 (xterm CSI 1;<m>).
        check(jk::EncodeArrow(NavKey::Left, 4, false) == "\x1b[1;5D",
              "terminput: ctrl+left = CSI 1;5D");
        check(jk::EncodeArrow(NavKey::Right, 1, false) == "\x1b[1;2C",
              "terminput: shift+right = CSI 1;2C");
        check(jk::EncodeArrow(NavKey::Up, 2, false) == "\x1b[1;3A",
              "terminput: alt+up = CSI 1;3A");
        check(jk::EncodeArrow(NavKey::Down, 1 | 4, true) == "\x1b[1;6B",
              "terminput: ctrl+shift+down = m 6 (SS3 flag ignored)");
        check(jk::EncodeArrow(NavKey::Home, 4, false) == "\x1b[1;5H" &&
                  jk::EncodeArrow(NavKey::End, 2, true) == "\x1b[1;3F",
              "terminput: home/end modifier form shares 1;<m> family");
        // PgUp/PgDn with modifiers use the 5;/6; tilde form.
        check(jk::EncodeArrow(NavKey::PgUp, 4, false) == "\x1b[5;5~" &&
                  jk::EncodeArrow(NavKey::PgDn, 1, false) == "\x1b[6;2~",
              "terminput: pgup/pgdn modifier tilde form");

        // Wheel → arrow keys (alt screen, mouse reporting off): n sequences.
        check(jk::EncodeWheelAlt(true, 3) == "\x1b[A\x1b[A\x1b[A",
              "terminput: wheel-alt 3 up = 3 Up arrows");
        check(jk::EncodeWheelAlt(false, 2) == "\x1b[B\x1b[B",
              "terminput: wheel-alt 2 down = 2 Down arrows");
        check(jk::EncodeWheelAlt(true, 0).empty(),
              "terminput: wheel-alt n=0 sends nothing");
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

        // 1g) 워크숍 능력 게이트 (docs/74 결정, 스펙 §4/§6): fail-closed —
        //     미선언 호출은 Start 실패 + 고정 문구. 선언되면 통과. log/assert
        //     는 무조건 허용. 각 시나리오는 새 창을 소유한다(블록 원칙).
        {
            // (a) 미선언 차단 + 문구 단언
            writeScript("test_script_gate.js",
                "var tick = setInterval(function(){}, 16);\n");
            jk::JKWindow gwin("ScriptGateTest");
            gwin.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
            jk::JKScriptHost ghost;
            ghost.Attach(&gwin);
            std::vector<uint32_t> gateWinIds;
            uint64_t gateSeq = 0;
            ghost.SetTimerServices(makeTimerServices(gateWinIds, gateSeq));
            ghost.EnableCapabilities("");  // 선언 없음 = 능력 없음
            check(!ghost.Start("test_script_gate.js"),
                  "capability gate blocks undeclared setInterval");
            check(ghost.LastError().find(
                      "capability 'timer' not declared in MANI") !=
                      std::string::npos,
                  "capability gate error names the token");
            check(ghost.GateActive(),
                  "gate active after EnableCapabilities");
            ghost.Stop();

            // (b) 정규화(대문자·공백) + 선언 통과 + 미지 토큰 보존
            jk::JKWindow gwin2("ScriptGateNorm");
            gwin2.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
            jk::JKScriptHost ghost2;
            ghost2.Attach(&gwin2);
            ghost2.SetTimerServices(makeTimerServices(gateWinIds, gateSeq));
            ghost2.EnableCapabilities("Timer, input,network,weird ");
            check(ghost2.HasCapability("timer") && ghost2.HasCapability("input") &&
                      ghost2.HasCapability("network") && ghost2.HasCapability("weird") &&
                      !ghost2.HasCapability("widget"),
                  "capability list normalizes (trim+lowercase, unknown kept)");
            check(ghost2.Start("test_script_gate.js"),
                  "declared capability passes the gate");
            check(gateWinIds.size() >= 1,
                  "declared timer claims a timer winId");
            ghost2.Stop();

            // (c) 무조건 허용: 게이트 활성 상태에서 log/assert 통과
            writeScript("test_script_free.js",
                "log(\"gate-ok\");\n"
                "assert(true, \"assert stays free\");\n");
            jk::JKWindow fwin("ScriptGateFree");
            fwin.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
            jk::JKScriptHost fhost;
            fhost.Attach(&fwin);
            fhost.EnableCapabilities("");
            check(fhost.Start("test_script_free.js"),
                  "log/assert stay free under an active gate");
            fhost.Stop();

            // (d) 게이트 비활성(EnableCapabilities 미호출)은 기존 셀프테스트
            //     1·2가 회귀 검증으로 겸함 — 여기에 단언 없음(스펙 §6-5).
        }

        // 1h) 출하 선언 분석기 (스펙 2026-10-05-slot-ship-tool §3 — docs/76 §9의
        //     "개별 MANI 좁게 선언" 이행). 정적 순수 함수 — bind 호출 본체는
        //     무변경(전역 제약 3). 스크립트 소스 어휘 경계([A-Za-z0-9_$]) 매치.
        //     주석·문자열 유사 표기는 오버 방향 오탐만 낸다(방향성 계약: 언더는
        //     런타임 fail-closed가 잡는다).
        {
            auto toks = jk::JKScriptHost::CapabilityTokensForScript(
                "var t=setInterval(function(){clearInterval(t);},100);"
                "var cv=createCanvas({x:0,y:0,w:10,h:10},'');"
                "readConfig('cfg.json');declareCursor('x');");
            check(toks.size() == 4 && toks[0] == "timer" && toks[1] == "agent" &&
                      toks[2] == "fs" && toks[3] == "canvas",
                  "1h-a 사용 집합=표 순");   // widget/input/uiauto 미사용 미선언
            check(jk::JKScriptHost::CapabilityTokensForScript(
                      "mysetInterval(a,1);xsetIntervalX(b);").empty(),
                  "1h-b 어휘 경계 미적중");
            check(jk::JKScriptHost::CapabilityTokensForScript(
                      "setInterval(function(){},1);").size() == 1,
                  "1h-b2 경계 적중");
            check(jk::JKScriptHost::CapabilityTokensForScript(
                      "// setInterval 주석 — 오버 방향 오탐(문서화 계약)")
                      .size() == 1,
                  "1h-c 주석 폴스포짓=오버 방향");
            check(jk::JKScriptHost::CapabilityTokensForScript(
                      "log('hi');assert(true);assertEq(1,1);").empty(),
                  "1h-d 무조건 3은 선언 생성 않음");
            // 표↔런타임 핀(1h-e/f): 정적 표의 30 이름이 시작 호스트의 실제
            // 바인딩에 전부 존재 — 표가 bind 호출과 갈라지면 이 핀이 찬다.
            // BoundNames는 ctx_ 존재 전제(없으면 빈 목록). 1g의 호스트들은
            // 위 중괄호 스코프가 닫혀 Stop된 후라 재용 불가 — 1g의 시작 패턴
            // (창+Attach+무선언 게이트+무조건 log 스크립트)을 그대로 재현해
            // 새 호스트를 Start한다(브리프 주의 항 준수, bind 무변경).
            writeScript("test_script_1h_pin.js", "log('1h pin');\n");
            jk::JKWindow hwin("ScriptShipPin");
            hwin.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
            jk::JKScriptHost hhost;
            hhost.Attach(&hwin);
            hhost.EnableCapabilities("");  // log는 무조건 허용 — 게이트 밖 통과
            check(hhost.Start("test_script_1h_pin.js"), "1h-f0 핀 호스트 시작");
            const std::vector<std::string> bound = hhost.BoundNames();
            const std::vector<std::string> table =
                jk::JKScriptHost::HostBindingNames();
            check(table.size() == 30, "1h-e 정적 표=30");
            bool allBound = true;
            for (const auto& n : table)
                if (std::find(bound.begin(), bound.end(), n) == bound.end())
                    allBound = false;
            check(allBound, "1h-f 표↔BoundNames 전수 일치");
            hhost.Stop();
        }

        // 1j) 출하 시딩 원천 전환 (스펙 2026-10-05-slot-ship-tool §4/§5-3 —
        //     파묻힌 SCRI 우선, 템플릿 회귀 유지, 외부 진실원 존중).
        //     headless 파일 시나리오 — 임시 디렉터리 3+1 단계.
        //     읽기는 위 ReadWholeFile 재용, 쓰기는 std::ofstream 구문
        //     (1f2 파일 다이얼로그 블록 1150행과 동일 관습 — 새 헬퍼 신설 안 함).
        {
            namespace fs = std::filesystem;
            const fs::path dir = fs::temp_directory_path() / "jk_seed_test";
            fs::remove_all(dir);
            fs::create_directories(dir);
            const std::string ext = (dir / "ext.js").string();
            const std::string ship = (dir / "ship.js").string();
            const std::string tpl = "TEMPLATE";
            std::string seedErr;
            std::string got;
            {
                std::vector<uint8_t> bytes;
                // (a) 파묻힌 SCRI 존재 + 외부 부재 → 외부 = SCRI 원문
                { std::ofstream f(ship, std::ios::binary); f << "SHIPPED"; }
                const int rc = jk::WorkshopSeedScript(ext, ship, tpl, seedErr);
                bytes.clear();
                if (ReadWholeFile(ext, bytes))
                    got.assign(bytes.begin(), bytes.end());
                check(rc == 1 && got == "SHIPPED", "1j-a 파묻힌 SCRI 시딩");
            }
            // (b) SCRI 부재 → 템플릿 (현행 회귀)
            fs::remove(ext);
            fs::remove(ship);
            {
                std::vector<uint8_t> bytes;
                const int rc = jk::WorkshopSeedScript(ext, ship, tpl, seedErr);
                bytes.clear();
                if (ReadWholeFile(ext, bytes))
                    got.assign(bytes.begin(), bytes.end());
                check(rc == 2 && got == "TEMPLATE", "1j-b 템플릿 회귀");
            }
            // (c) 외부 존재 → 무변 (수신 기기 진실원 존중, 스펙 §1-3)
            {
                std::vector<uint8_t> bytes;
                const int rc = jk::WorkshopSeedScript(ext, ship, tpl, seedErr);
                bytes.clear();
                if (ReadWholeFile(ext, bytes))
                    got.assign(bytes.begin(), bytes.end());
                check(rc == 0 && got == "TEMPLATE", "1j-c 외부 우선");
            }
            // (d) 쓰기 실패 → -1 (반환 계약의 오류 끝단 — 지정 경로가
            //     디렉터리라 열리지 않는다; error에 원인 채움)
            check(jk::WorkshopSeedScript(dir.string(), ship, tpl, seedErr) == -1 &&
                      !seedErr.empty(),
                  "1j-d 쓰기 실패=-1+오류");
            fs::remove_all(dir);
        }

        // 1k) 출하 MANI 조립기 (스펙 2026-10-05-slot-ship-tool §3/§5.2 — docs/76
        //     §9 이행). 8행 캐노니컬 + Parse 통과 + 빈 tokens=능력 없음
        //     (fail-closed — 배지 "능력 없음" 그대로).
        {
            const std::string m =
                jk::SlotShipManifestText("bang-gu", {"timer", "canvas"});
            check(m ==
                      "name=bang-gu\ntitle=bang-gu\nwidth=360\nheight=280\n"
                      "module=jkapp_script.dll\nscript=app.js\n"
                      "scriptfile=state/scripts/bang-gu.js\n"
                      "capabilities=timer,canvas\n",
                  "1k-a 출하 MANI 캐노니컬 원문");
            jk::JkxManifest mm;
            check(mm.Parse(m) &&
                      mm.scriptfile == "state/scripts/bang-gu.js" &&
                      mm.capabilities == "timer,canvas",
                  "1k-b Parse 통과+선언 원문 재검");
            jk::JkxManifest m0;
            check(m0.Parse(jk::SlotShipManifestText("x", {})) &&
                      m0.capabilities.empty(),
                  "1k-c 빈 tokens=능력 없음(fail-closed)");
        }

        // 1m) 라이브러리 카탈로그 스캔 (스펙 2026-10-06-app-library §2 — 3원+1
        //   발견·.jkx 우선·능력 원문 보존). 가짜 apps/ 트리를 temp에서 조립
        //   — 실 기기 apps/에 의존하지 않는 pure 케이스.
        // 기대 계약(JKLibraryCatalog.h와 1:1):
        //   a) .jkx(name/title/capabilities/ICON 유무) → source=Jkx
        //   b) 콘솔 dir + manifest.json(name/cmd/desc) → source=Console
        //   c) .jkx와 동명 콘솔 → .jkx가 이긴다(스캔 순서 — 런처 규약)
        //   d) 내장 minesweeper는 항상; lf/hx는 파일 부재 시 제외
        //   e) MANI에 name/module 없는 컨테이너 → 스킵(Parse false 계약)
        {
            std::error_code ec;
            const std::string base =
                (std::filesystem::temp_directory_path(ec)
                 .append("jk_library_st_1m")).string();
            std::filesystem::remove_all(base, ec);
            std::filesystem::create_directories(base + "/apps/consoleapp", ec);
            // (a) .jkx 컨테이너 — JKJkxFile::Write 선례로 조립
            const std::string mani =
                "name=galapp\ntitle=갤 앱\ntitle2=ignored\nmodule=jkapp_gal.dll\n"
                "capabilities=widget,timer\nicon=icon@1x.png\n";
            std::vector<uint8_t> maniBytes(mani.begin(), mani.end());
            const bool jkxOk = jk::JKJkxFile::Write(
                base + "/apps/galapp.jkx", {{"manifest.txt", maniBytes}});
            // 콘솔 leg — ① 유니크 콘솔(name=conapp2, desc 표시명 전승 검증) +
            // ② .jkx 동명 콘솔(name=conapp ↔ conapp.jkx — .jkx-wins 스킵 검증).
            // manifest.json의 끝 개행은 파싱과 무관(길이 계약은 NUL 제외 전승).
            const bool conOk =
                std::filesystem::create_directories(base + "/apps/conapp", ec);
            const std::string conJson =
                "{\"name\":\"conapp2\",\"cmd\":\"apps-bin/y\",\"desc\":\"Console App\"}";
            const std::string conJson2 = "{\"name\":\"conapp\",\"cmd\":\"apps-bin/x\"}";
            {
                std::ofstream f1(base + "/apps/consoleapp/manifest.json",
                                 std::ios::binary);
                f1.write(conJson.data(),
                         static_cast<std::streamsize>(conJson.size()));
                std::ofstream f2(base + "/apps/conapp/manifest.json",
                                 std::ios::binary);
                f2.write(conJson2.data(),
                         static_cast<std::streamsize>(conJson2.size()));
            }
            // 동명 .jkx(conapp) + 무효 컨테이너(bad — name/module 부재,
            // Parse false 계약 → name 비어 스킵). 모두 스폰 가능선은 검증 대상
            // 아님 — 발견 규약만 검증한다.
            const std::string conMani =
                "name=conapp\ntitle=Con App\nmodule=jkapp_con.dll\n";
            std::vector<uint8_t> conManiBytes(conMani.begin(), conMani.end());
            const bool conJkxOk = jk::JKJkxFile::Write(
                base + "/apps/conapp.jkx", {{"manifest.txt", conManiBytes}});
            const std::string badMani = "title=nobody\ncapabilities=x\n";
            std::vector<uint8_t> badBytes(badMani.begin(), badMani.end());
            const bool badOk = jk::JKJkxFile::Write(
                base + "/apps/bad.jkx", {{"manifest.txt", badBytes}});
            std::vector<jk::LibraryEntry> got;
            const int n =
                (jkxOk && conOk && conJkxOk && badOk) ? jk::LibraryScan(base, got) : -1;
            // 조립 검증 — Write 실패는 스캔 개수 판정을 오염시키므로 앞에서
            // 따로 확정한다.
            check(jkxOk, "1m-0 컨테이너 조립");
            check(conOk && conJkxOk && badOk, "1m-0b 피스쳐 조립(콘솔+동명+무효)");
            // 정확 개수 — 여유 슬랙 없음: jkx 2(galapp·conapp) + 콘솔 1
            // (conapp2; 동명 conapp은 .jkx에 밀려 스킵) + 무효 bad 스킵 0 +
            // 내장 2(minesweeper·tetris — 이 base엔 apps-bin이 없어 lf/hx
            // 제외). = 5
            check(n == 5, "1m-1 스캔 개수(jkx2+콘솔1+내장2, 무효·동명 스킵=5)");
            const jk::LibraryEntry* g = nullptr;
            const jk::LibraryEntry* c = nullptr;
            int badFound = 0;
            int consoleWins = 0;
            int terminalKeys = 0;
            const jk::LibraryEntry* mine = nullptr;
            const jk::LibraryEntry* tet = nullptr;
            for (const auto& e : got) {
                if (e.appName == "galapp") g = &e;
                if (e.appName == "conapp2") c = &e;
                if (e.appName == "badapp") ++badFound;
                if (e.source == jk::LibrarySource::Console &&
                    e.appName == "conapp") ++consoleWins;
                if (e.appName.rfind("terminal:", 0) == 0) ++terminalKeys;
                if (e.appName == "minesweeper") mine = &e;
                if (e.appName == "tetris") tet = &e;
            }
            check(g != nullptr, "1m-2 jkx 발견");
            if (g) {
                check(g->title == "갤 앱", "1m-3 MANI title 전승");
                check(g->capabilities == "widget,timer",
                      "1m-4 능력 원문 보존(정규화 없음)");
                check(g->source == jk::LibrarySource::Jkx, "1m-5 source=Jkx");
                check(!g->hasIcon,
                      "1m-6 ICON 부재=hasIcon false(폰 기본값 경로)");
                check(g->sizeBytes > 0, "1m-7 크기 수령");
                check(g->path.find("galapp.jkx") != std::string::npos,
                      "1m-8 절대 경로");
            }
            check(c != nullptr, "1m-9 콘솔 발견(name=conapp2)");
            if (c) {
                check(c->source == jk::LibrarySource::Console,
                      "1m-10 source=Console");
                check(c->title == "Console App", "1m-11 콘솔 desc 표시명 전승");
                check(c->capabilities.empty() && c->sizeBytes == 0,
                      "1m-12 콘솔 능력 빈값+크기 0 계약");
                check(c->path.find("consoleapp") != std::string::npos,
                      "1m-13 콘솔 dir 절대 경로");
            }
            check(consoleWins == 0 && badFound == 0,
                  "1m-14 .jkx 우선(동명 콘솔 스킵)+무효 컨테이너 스킵");
            check(mine != nullptr && tet != nullptr &&
                      mine->source == jk::LibrarySource::Builtin &&
                      mine->title == "Minesweeper" && tet->title == "Tetris" &&
                      mine->path.empty() && tet->sizeBytes == 0,
                  "1m-15 내장 minesweeper·tetris 항상(Builtin)");
            check(terminalKeys == 0,
                  "1m-16 lf/hx 파일 부재=제외(폰 기본값 경로)");
            std::error_code ec2;
            check(std::filesystem::remove_all(base, ec2) > 0 && !ec2,
                  "1m-z 클린업");
        }

        // 1c2) 능력 배지 문구 (docs/74 — 빈 선언도 숨기지 않는다, 스펙 §5).
        check(jk::CapabilityBadgeText("agent,timer") == "능력: agent,timer",
              "capability badge text with declaration");
        check(jk::CapabilityBadgeText("") == "능력 없음",
              "capability badge text without declaration");

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

        // 1b) Canvas + input events (docs/60 §10): createCanvas registers a
        //     focusable JKScriptCanvas; draw bindings accept its id; the
        //     DispatchCanvas* dispatchers drive the script's globals with
        //     the documented signatures.
        writeScript("test_script_canvas.js",
            "var clabel = createLabel({x:0,y:0,w:120,h:20}, \"m-\");\n"
            "var klabel = createLabel({x:0,y:24,w:120,h:20}, \"k-\");\n"
            "var cv = createCanvas({x:0,y:48,w:100,h:80});\n"
            "function onCreate(){ canvasRect(cv, 5,5, 20,10, 0xFF0000, true); "
            "canvasPixel(cv, 1,1, \"#00ff00\"); canvasLine(cv, 0,0, 9,9, 255); "
            "canvasCircle(cv, 50,50, 8, \"#abc\", true); "
            "canvasText(cv, 2,2, \"AB\", \"fff\"); }\n"
            "function onMouse(type, x, y, cid){ if (cid === cv) "
            "setText(clabel, type + \":\" + x + \",\" + y); }\n"
            "function onKey(key, down){ setText(klabel, \"k\" + key + \":\" + down); }\n");
        jk::JKWindow cwin("ScriptCanvasTest");
        cwin.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
        jk::JKScriptHost chost;
        chost.Attach(&cwin);
        check(chost.Start("test_script_canvas.js"),
              "canvas script boots (bindings + color strings)");
        const uint16_t canvasId = 1002;  // after clabel/klabel
        jk::JKControl* ccv = cwin.FindControlByControlId(canvasId);
        check(ccv && ccv->IsFocusable(),
              "createCanvas registers a focusable control");
        chost.DispatchCanvasMouse(canvasId, 0, 5, 7, 1);
        jk::JKControl* clb = cwin.FindControlByControlId(1000);
        check(clb && clb->GetText() == "down:5,7",
              "dispatchcanvasmouse drives script onmouse");
        // v5.1 button arg (2026-09-24 폰 실전: 좌/우 구분) — right click
        // reaches the script with button=3.
        chost.DispatchCanvasMouse(canvasId, 0, 9, 9, 3);
        check(clb && clb->GetText() == "down:9,9",
              "dispatchcanvasmouse right-button reaches onmouse");
        chost.DispatchCanvasKey(100, true);
        jk::JKControl* klb = cwin.FindControlByControlId(1001);
        check(klb && klb->GetText() == "k100:true",
              "dispatchcanvaskey drives script onkey");
        chost.Stop();

        // 2) setInterval claims a winId from the script range and manual
        //    dispatch runs the interval callback.
        writeScript("test_script_timer.js",
            "var tick = 0;\n"
            "var tlabel = createLabel({x:0,y:0,w:80,h:20}, \"t0\");\n"
            // setInterval contract is (fn, ms) — callback first (2026-09-24
            // guard change; the LLM-familiar browser order).
            "function onCreate(){ setInterval(function(){ tick++; "
            "setText(tlabel, \"t\" + tick); }, 50); }\n");
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
            // docs/78 TX5: posix 기본 셸은 $SHELL(Win32 리터럴에서 승계) —
            // 케이스 단언도 플랫폼 값과 동형으로.
            check(bad.scrollback == 1000 &&
#if defined(_WIN32)
                      bad.shell == "powershell.exe -NoLogo",
#else
                      bad.shell == ::detail::TerminalShellDefault(),
#endif
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

    // 12) wire payload cap (docs/68 W1b): a header claiming a length above
    // kMaxWirePayload must fail ReadMessage closed — no huge assign
    // allocation. The StubTransport carries the header bytes only, so after
    // the cap check the protocol must not even attempt a payload Read
    // (payloadRead stays false — that is the no-waste ground truth; a 1 GiB
    // or 64 MiB+1 assign would also surface as a drained-but-false Read).
    // Note: the selftest's "11" slot is the pcx viewer, so the brief's case
    // number slides to 12.
    {
        struct StubTransport : public jk::ipc::IWireTransport {
            std::vector<uint8_t> buf;
            size_t pos = 0;
            bool payloadRead = false;
            bool Write(const void*, size_t) override { return true; }
            bool Read(void* d, size_t l) override {
                if (pos + l > buf.size()) { payloadRead = true; return false; }
                std::memcpy(d, buf.data() + pos, l);
                pos += l;
                return true;
            }
            void Close() override {}
            bool IsConnected() const override { return true; }
        } t;
        jk::ipc::WireHeader big{};
        big.length = jk::ipc::kMaxWirePayload + 1;  // cap+1 — rejected
        t.pos = 0;
        t.buf.assign(reinterpret_cast<uint8_t*>(&big),
                     reinterpret_cast<uint8_t*>(&big) + sizeof(big));
        jk::ipc::Message m;
        check(!jk::ipc::ReadMessage(t, m) && !t.payloadRead,
              "cap+1 rejected with no payload read attempt");
        jk::ipc::WireHeader ok{};
        ok.length = 8;
        const uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        t.pos = 0;
        t.buf.assign(reinterpret_cast<uint8_t*>(&ok),
                     reinterpret_cast<uint8_t*>(&ok) + sizeof(ok));
        t.buf.insert(t.buf.end(), payload, payload + 8);
        check(jk::ipc::ReadMessage(t, m) && m.payload.size() == 8,
              "payload within cap round-trips");
    }

    // 13) hand-rolled SHA-256 (docs/68 W2a): FIPS vectors — the command
    // fingerprints must stay byte-identical with the old BCrypt digests
    // ("sha256:"+64hex format is unchanged at every call site, W2b).
    // TDD: case added first → build RED (no <crypto/JKSha256.h>) → helper
    // implemented → all three vectors PASS (GREEN, jkdesktop.exe test).
    {
        check(jk::crypto::Sha256Hex("", 0) ==
                  "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
              "sha256 empty vector");
        check(jk::crypto::Sha256Hex("abc", 3) ==
                  "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
              "sha256 abc vector");
        // 55-byte boundary padding case: 55 + 1 (0x80) + 8 (bit length) is
        // exactly one block — the single-block padding tail. The digest below
        // is an observed value (cross-checked against an independent SHA-256
        // implementation, python hashlib), not a FIPS vector — empty/abc
        // above are the correctness anchors.
        const std::string pad(55, 'x');
        check(jk::crypto::Sha256Hex(pad.data(), pad.size()) ==
                  "d5e285683cd4efc02d021a5c62014694958901005d6f71e89e0989fac77e4072",
              "sha256 55-byte vector (observed, hashlib cross-checked)");
    }

    // 14) jk::process spawn adapter (docs/68 W4): a stub child echoes a reply
    // JSON through inherited stdio pipes — the JKLlmEngine spawn contract in
    // miniature (separate stdout/stderr pipes, parent keeps read ends,
    // write ends closed inside Spawn so the child's stdout EOFs). The
    // selftest is win32-only by convention and the cmd.exe stub child is a
    // test literal (JKLlmEngine.cpp:167 class), so this case stays out of
    // any posix port scope. Part B smokes the kill-on-close job trio.
    // docs/78 TX3 폰 실측: "by convention"이 실제 가드가 없어 posix에서도
    // 돌았다 — WSL은 interop(cmd.exe) 덕에 우연히 PASS, 폰은 FAIL. 리터럴이
    // 없는 기기에서 스킵이 정답이므로 _WIN32로 고정한다.
#ifdef _WIN32
    //
    {
        // windows.h stays out of this TU, so the two Win32 constants the
        // contract names are hand-carried here (values are ABI-stable).
        constexpr int kErrBrokenPipe = 109;      // ERROR_BROKEN_PIPE
        constexpr int kErrNoData = 232;          // ERROR_NO_DATA (pipe closed)
        constexpr uint32_t kStillActiveExit = 259;  // STILL_ACTIVE
        // Returns true iff the drain ended on an OBSERVED EOF/broken verdict
        // (review carry from the jk::process absorption — a contract-(a)
        // violation, i.e. the parent keeping a write end, is invisible to the
        // round-trip check: the child's output still arrives but stdout never
        // EOFs, and only the iters safety bail would end the loop. The safety
        // bail now fails the check below instead of passing silently.)
        auto drainPipe = [](void* pipe, std::string* sink) -> bool {
            char buf[4096];
            bool open = true;
            bool closed = false;
            int iters = 0;  // safety bail — EOF must land at iter 1-2 here
            while (open) {
                if (++iters > 500) break;
                uint32_t avail = 0;
                int broken = 0;
                const bool data =
                    jk::process::PeekPipeAvail(pipe, &avail, &broken);
                if (data && avail > 0) {
                    const int got =
                        jk::process::ReadPipeData(pipe, buf, sizeof(buf));
                    if (got > 0) {
                        sink->append(buf, static_cast<size_t>(got));
                        continue;
                    }
                    open = false;  // read==0/-1 -> "this pipe is done"
                    closed = true;
                } else if (broken == kErrBrokenPipe || broken == kErrNoData) {
                    // peer write end closed: PeekNamedPipe reports
                    // ERROR_BROKEN_PIPE(109) (observed here; ERROR_NO_DATA
                    // (232) is the alternate closed-pipe report).
                    open = false;
                    closed = true;
                } else {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(20));
                }
            }
            return closed;
        };

        // A) inherited-stdio spawn + stdout round-trip.
        jk::process::SpawnOptions opt;
        opt.commandLineUtf8 = "cmd.exe /c echo {\"ok\":true}";
        opt.hideWindow = true;
        opt.inheritedStdioPipes = true;
        const jk::process::SpawnResult r = jk::process::Spawn(opt);
        check(r.ok && r.process && r.pid != 0,
              "process spawn returns process+pid");
        check(r.stdoutRead && r.stderrRead,
              "process spawn returns both parent pipe read ends");
        std::string out, errOut;
        const bool outEof = drainPipe(r.stdoutRead, &out);
        const bool errEof = drainPipe(r.stderrRead, &errOut);
        // Quote-shape tolerant: cmd may or may not keep the inner quotes.
        check(out.find("ok") != std::string::npos &&
                  out.find("true") != std::string::npos,
              "stub child stdout round-trips through adapter pipes");
        check(outEof && errEof,
              "EOF observed on both pipes (contract a: parent holds read "
              "ends only)");
        uint32_t code = 0;
        bool exited = false;
        for (int i = 0; i < 500; ++i) {  // 10s budget — echo exits at once
            if (jk::process::GetExitCode(r.process, &code) &&
                code != kStillActiveExit) {
                exited = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        check(exited && code == 0, "stub child reaps cleanly (exit 0)");
        jk::process::CloseHandleLike(r.stdoutRead);
        jk::process::CloseHandleLike(r.stderrRead);
        jk::process::CloseHandleLike(r.process);

        // B) Job (contract b) smoke: CreateKillOnCloseJob -> AssignToJob ->
        //    TerminateJobTree, then observe the tree actually dies.
        jk::process::SpawnOptions slow;
        slow.commandLineUtf8 = "cmd.exe /c ping -n 4 127.0.0.1 >nul";
        slow.hideWindow = true;
        const jk::process::SpawnResult p = jk::process::Spawn(slow);
        check(p.ok && p.process, "job smoke child spawns (no stdio pipes)");
        void* job = jk::process::CreateKillOnCloseJob();
        check(job != nullptr && jk::process::AssignToJob(job, p),
              "kill-on-close job assigns spawned process");
        check(jk::process::TerminateJobTree(job, 1),
              "terminate job tree reports success");
        bool killed = false;
        for (int i = 0; i < 300; ++i) {  // 6s budget — job kill is immediate
            if (jk::process::GetExitCode(p.process, &code) &&
                code != kStillActiveExit) {
                killed = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        check(killed, "terminate job tree kills the child tree");
        jk::process::CloseHandleLike(p.process);
        jk::process::CloseHandleLike(job);  // close IS the kill; child dead
    }
#endif  // _WIN32 — case 14(stub 자식·job 소박)는 Windows 리터럴 전용

    // 15) jk::net winsock adapter (docs/68 W8b): ephemeral listen reports its
    // bound port (R-C4); a windows.h-clean RAW client (R-C5 — hand dllimports
    // above, no winsock headers in this TU) sends 5 bytes; adapter
    // Accept/RecvAll/Send(echo)/Close answer; bad bindIp must fail ListenTcp.
    // Win32-only: the raw client rides guarded dllimports (top of this TU);
    // on a first Linux build this block must compile away (opus final-review
    // NIT-7 — loud compile error without the guard).
#ifdef _WIN32
    {
        check(jk::net::Startup(), "net: WSAStartup succeeds");
        std::uint16_t boundPort = 0;
        const jk::net::Socket listener =
            jk::net::ListenTcp("127.0.0.1", 0, 1, &boundPort);
        check(listener != jk::net::kInvalidSocket && boundPort > 0,
              "net: ephemeral listen reports bound port");
        if (listener != jk::net::kInvalidSocket) {
            const std::string msg = "case5";
            bool clientOk = false;
            std::thread cli([&clientOk, boundPort, &msg] {
                struct RawSockaddrIn {  // 16 bytes — R-C5 ABI-stable layout
                    std::uint16_t family = 2, port = 0;  // AF_INET; net octets
                    std::uint32_t addr = 0x0100007Fu;    // 127.0.0.1, net order
                    std::uint8_t pad[8] = {};
                };
                RawSockaddrIn a;
                const unsigned long long s = socket(2 /*AF_INET*/, 1, 0);
                if (s == ~0ull) return;  // INVALID_SOCKET
                a.port = static_cast<std::uint16_t>(
                    ((boundPort & 0xFF) << 8) | (boundPort >> 8));
                if (connect(s, &a, sizeof(a)) != 0) { closesocket(s); return; }
                const int t = 5000;  // SOL_SOCKET 0xffff, SO_RCVTIMEO 0x1006
                setsockopt(s, 0xffff, 0x1006,
                           reinterpret_cast<const char*>(&t), sizeof(t));
                if (send(s, msg.data(), (int)msg.size(), 0) != (int)msg.size())
                    { closesocket(s); return; }
                char back[5] = {};
                int got = 0;
                while (got < (int)msg.size()) {
                    const int r =
                        recv(s, back + got, (int)sizeof(back) - got, 0);
                    if (r <= 0) { closesocket(s); return; }
                    got += r;
                }
                clientOk = std::string(back, msg.size()) == msg;
                closesocket(s);
            });
            const jk::net::Socket conn = jk::net::Accept(listener);
            check(conn != jk::net::kInvalidSocket, "net: accept returns conn");
            char buf[5] = {};
            check(jk::net::RecvAll(conn, buf, sizeof(buf)) &&
                      std::string(buf, sizeof(buf)) == msg,
                  "net: RecvAll round-trips the 5 raw-client bytes");
            check(jk::net::Send(conn, buf, (int)sizeof(buf)) > 0,
                  "net: Send serves the raw-client echo");
            jk::net::Close(conn);
            jk::net::Close(listener);
            cli.join();
            check(clientOk, "net: raw client sees the echo and closes");
        }
        std::uint16_t deadPort = 0;
        check(jk::net::ListenTcp("bogus-ip-for-bind-check", 0, 1, &deadPort) ==
                  jk::net::kInvalidSocket,
              "net: bad bindIp fails ListenTcp (bind observable)");
    }

#endif  // _WIN32 — case 15 (net adapter) is win32-only by selftest convention
    std::printf("AppSelfTest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

// Desktop Agent API CLI (spec §3): send one agent query to the running
// window server and print the reply JSON on stdout.
//   jkdesktop agentctl '{"tool":"ping","args":{}}'
static int RunAgentCtl(const char* requestJson) {
    jk::agent::JKAgentClient agent;
    if (!agent.Connect()) {
        std::fprintf(stderr, "agentctl: connect to window server failed\n");
        return 2;
    }
    std::string reply;
    if (!agent.QueryRaw(requestJson, reply)) {
        std::fprintf(stderr, "agentctl: query failed (server gone?)\n");
        return 3;
    }
    std::fputs(reply.c_str(), stdout);
    std::fputc('\n', stdout);
    return 0;
}

// Subscribe to desktop events and print them line by line for <seconds>
// seconds (probe harness for the event stream).
static int RunAgentEvents(int seconds) {
    jk::agent::JKAgentClient agent;
    if (!agent.Connect()) {
        std::fprintf(stderr, "agent-events: connect to window server failed\n");
        return 2;
    }
    if (!agent.SubscribeEvents(true)) {
        std::fprintf(stderr, "agent-events: subscribe failed\n");
        return 3;
    }
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        // The pipe transport's Read has no timeout, so the pump only advances
        // on a query round-trip. A cheap ping flushes any queued events.
        std::string reply;
        agent.QueryRaw("{\"tool\":\"ping\",\"args\":{}}", reply);
        std::vector<jk::agent::AgentEvent> events;
        agent.PollEvents(events);
        for (const auto& ev : events) {
            std::fputs(ev.json.c_str(), stdout);
            std::fputc('\n', stdout);
            // 리다이렉트된 stdout은 블록 버퍼링이라 이벤트가 종료 시까지
            // 묶여 있다(프로브가 런 중간에 빈 파일을 읽는다) — 행 단위 플러시.
            std::fflush(stdout);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return 0;
}

static int RunMain(int argc, char* argv[]) {
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

    // theme.json 프리셋 로딩 (P2 단계 2) — 파일 없으면 다크 기본값 유지
    jk::theme::loadPresetFromFile(jk::theme::DefaultThemePath());

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
        std::printf("  terminal    ConPTY terminal (single-process)\n");
        std::printf("  scriptdemo [FILE]  Script app demo (FILE: .jkx package or a direct app.js)\n");
        std::printf("  --server    Run as the window server (Phase 2 scaffolding)\n");
        std::printf("  --client minesweeper  Run Minesweeper as a window-server client\n");
        std::printf("  --client tetris     Run Tetris as a window-server client\n");
        std::printf("  --jkx FILE  Run an app from a .jkx container\n");
        std::printf("  jkx-pack APP  Bundle jkapp_<APP>.dll + icons + manifest into apps/<APP>.jkx\n");
        std::printf("  slot-pack SLOT [OUT]  Ship a workshop slot as a .jkx app (capability-declared MANI)\n");
        std::printf("  jkx-list FILE   Print a .jkx container's version/codec + TOC\n");
        std::printf("  jkx-extract FILE [ENTRY...]  Extract .jkx entries into <FILE>_x/\n");
        std::printf("  agentctl '<json>'  Send one Desktop Agent query to the server\n");
        std::printf("  agent-events SEC   Subscribe to desktop events for SEC seconds\n");
        std::printf("  -h, --help, /?  Show this help message\n");
        return 0;
    }

    if (argc > 1 && std::strcmp(argv[1], "test") == 0) {
        return RunAppSelfTest();
    }

    if (argc > 1 && std::strcmp(argv[1], "agentctl") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: jkdesktop agentctl '<request json>'\n"
                                 "  e.g. jkdesktop agentctl '{\"tool\":\"ping\",\"args\":{}}'\n");
            return 1;
        }
        return RunAgentCtl(argv[2]);
    }

    if (argc > 1 && std::strcmp(argv[1], "agent-events") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: jkdesktop agent-events <seconds>\n");
            return 1;
        }
        return RunAgentEvents(std::atoi(argv[2]));
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

    if (argc > 1 && std::strcmp(argv[1], "slot-pack") == 0) {
        // docs/78 TX6 개방: body는 순수 stdio+JKJkxFile — posix 동형(플랜 G2
        // RunClientFromJkx posix leg와 짝). win32 게이트 670 영역에서 나머지
        // jkx 도구는 이미 개방.
        if (argc < 3 || argc > 4) {
            std::fprintf(stderr, "Usage: slot-pack <slot> [out]  (bundles state/scripts/<slot>.js + jkapp_script.dll so into a workshop-mode .jkx)\n");
            return 1;
        }
        return RunSlotPack(argv[2], argc > 3 ? argv[3] : nullptr);
    }

    if (argc > 1 && std::strcmp(argv[1], "jkx-list") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: jkx-list <file.jkx>  (print container version/codec + TOC)\n");
            return 1;
        }
        return RunJkxList(argv[2]);
    }

    if (argc > 1 && std::strcmp(argv[1], "jkx-extract") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: jkx-extract <file.jkx> [entry ...]  (extract all/named entries into <file>_x/)\n");
            return 1;
        }
        return RunJkxExtract(argv[2], argc - 3, argv + 3);
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
    bool runTerminal = (argc > 1 && std::strcmp(argv[1], "terminal") == 0);
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

    if (runTerminal) {
        // Phase A 흡수 경로 (docs/44): terminal [--shell <cmdline>] [--cwd <dir>]
        // --cwd는 PTY 스폰 전 프로세스 작업 디렉토리를 바꾼다 (lf 시작 폴더).
        // --shell은 terminal.json shell 대신 띄울 명령줄.
        std::string shellOverride;
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--shell") == 0 && i + 1 < argc) {
                shellOverride = argv[++i];
            } else if (std::strcmp(argv[i], "--cwd") == 0 && i + 1 < argc) {
#ifdef _WIN32
                SetCurrentDirectoryA(argv[++i]);
#else
                // posix twin(stage-3 task 7) — 반환 무시도 원문 동형(원문은
                // BOOL을 검사하지 않았다).
                chdir(argv[++i]);
#endif
            }
        }
        jk::TerminalApp app;
        if (!shellOverride.empty()) app.SetShellOverride(shellOverride);
        if (!app.Init("Terminal", 800, 500)) {
            return 1;
        }
        return app.Run();
    }

    if (runServer) {
        // 크래시 증거+로그 보존(docs/57 §13) — 가드/Init보다 먼저: 서버가
        // 무음 사망하면(2026-09-20 16:12 실측 — WER 기록 0, 콘솔 로그 증발)
        // state\logs의 파일 로그+미니덤프가 유일한 진실원이 된다.
        jk::InstallCrashHandler("state/logs", "server");
        jk::MirrorLogToFiles("state/logs", "server");
        jk::server::JKWindowServer server;
        // 단일 인스턴스 가드(docs/59 §10) — Init 전에 봉쇄: 가드가 Init 뒤에
        // 있으면 거부 인스턴스가 앱 설치+아이콘 로드를 전부 수행한 뒤 죽는다
        // (2026-09-20 사용자 붙여넣기 로그 실측 — 낭비+로그 혼란).
        if (!server.TryAcquireSingleInstanceGuard(jk::ipc::kWindowServerPipeName)) {
            return 1;
        }
        if (!server.Init("JKENGINE Window Server", 1280, 720)) {
            return 1;
        }
        if (!server.StartAcceptor(jk::ipc::kWindowServerPipeName)) {
            return 1;
        }
        server.Run();
        return 0;
    }

    if (runClient) {
        constexpr const char* kPipe = jk::ipc::kWindowServerPipeName;
        const char* clientApp = (argc > 2) ? argv[2] : "";

        // Phase B: client apps are dynamically loaded modules
        // (win32 jkapp_<name>.dll / posix jkapp_<name>.so — 접미는
        // JKWindowServer.h AppModuleSuffix()). The module statically contains
        // its core code and is driven purely through the C ABI in
        // apps/JKAppModule.h — no C++ crosses the boundary.
#ifdef _WIN32
        const std::string dllName = std::string("jkapp_") + clientApp + ".dll";
        return RunClientModule(dllName.c_str(), kPipe);
#else
        // posix 클라 route 개통(플랜 G2): 플랜 E가 앱 모듈 .so 20종을 이미
        // 빌드하고 ABI jk_app_meta/jk_app_run_client 노출을 실측(nm -D GREEN).
        return RunClientModule(ClientModulePath(clientApp).c_str(), kPipe);
#endif
    }

    if (argc > 1 && std::strcmp(argv[1], "--filedlg") == 0) {
        // 파일 열기 대화상자 자식 (Task 1의 SpawnClient "filedlg:<json>" 접두
        // 스폰). 설계 D3: 모듈은 params를 file_dialog_params 쿼리로 회수하는
        // 것이 계약이라 argv[2]에 의존하지 않는다 — CRT 인용 규칙상 값 끝의
        // 이스케이프된 백슬래시는 SpawnClient의 인용 과정에서 변형될 수 있으므로
        // argv json은 전달 편의일 뿐 (모듈이 쿼리로 filter/start/title을 받는다).
        constexpr const char* kPipe = jk::ipc::kWindowServerPipeName;
#ifdef _WIN32
        return RunClientModule("jkapp_filedlg.dll", kPipe);
#else
        // posix 클라 route 개통(플랜 G2): 접미+위치만 플랫폼 값 — win32 원문 유지.
        return RunClientModule(ClientModulePath("filedlg").c_str(), kPipe);
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
        // stage-3 task 7: GetTempPathA/GetCurrentProcessId → jk::fs::TempDir()
        // (후행 구분자 포함 — GetTempPathA 계약 승계) + pid(win32 원문/
        // posix getpid). TempDir 실패 폴백 "."도 원문 tempDir 초기값과 동일.
        const std::string tempDir = jk::fs::TempDir();
        const std::string scriptPath =
            tempDir + "jkscript_" +
#ifdef _WIN32
            std::to_string(GetCurrentProcessId())
#else
            std::to_string(getpid())
#endif
            + ".app.js";
        std::FILE* sf = std::fopen(scriptPath.c_str(), "wb");
        if (!sf) {
            std::fprintf(stderr, "scriptdemo: cannot write '%s'\n",
                         scriptPath.c_str());
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
        constexpr const char* kPipe = jk::ipc::kWindowServerPipeName;
        if (argc < 3) {
            std::fprintf(stderr, "Usage: --jkx <container.jkx>\n");
            return 1;
        }
        // docs/78 TX6 개방 — RunClientFromJkx 몸통은 플랜 G2 posix leg(temp
        // 추출 .so·dlopen, short-write 진단)를 이미 갖고 있다 (원래 라우트만
        // win32로 잠겨 있었다).
        return RunClientFromJkx(argv[2], kPipe);
    }

    MyApp app;
    if (!app.Init("JKENGINE SDL2 Prototype", 1920, 1080)) {
        return 1;
    }

    return app.Run();
}

#ifdef _WIN32
// UTF-8 argv entry (docs/48 후속 CP949 레저). The ANSI CRT startup converts
// the (always wide) Windows command line with CP_ACP — CP949 on Korean
// Windows — so Korean --filedlg json / agentctl payloads arrived as invalid
// UTF-8. wmain (link with -municode) receives the true wide argv; convert
// to UTF-8 here so every RunMain consumer keeps its encoding contract.
extern "C" __declspec(dllimport) int __stdcall WideCharToMultiByte(
    unsigned int codePage, unsigned long dwFlags, const wchar_t* lpWideCharStr,
    int cchWideChar, char* lpMultiByteStr, int cbMultiByte,
    const char* lpDefaultChar, int* lpUsedDefaultChar);

int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::string> utf8(static_cast<size_t>(argc > 0 ? argc : 1));
    std::vector<char*> ptrs(static_cast<size_t>(argc > 0 ? argc : 1), nullptr);
    for (int i = 0; i < argc; ++i) {
        int n = WideCharToMultiByte(65001 /* CP_UTF8 */, 0, argv[i], -1,
                                    nullptr, 0, nullptr, nullptr);
        if (n <= 0) continue;
        utf8[static_cast<size_t>(i)].resize(static_cast<size_t>(n) - 1);
        WideCharToMultiByte(65001, 0, argv[i], -1,
                            utf8[static_cast<size_t>(i)].data(), n,
                            nullptr, nullptr);
        ptrs[static_cast<size_t>(i)] = utf8[static_cast<size_t>(i)].data();
    }
    return RunMain(argc, ptrs.data());
}
#else
// linux stage-3 task 7 — posix leg of the dual entry: exec already hands the
// process byte-wise argv, and the window server process spawner passes UTF-8,
// so RunMain consumes argv verbatim (no conversion leg — RunMain's encoding
// contract is UTF-8 on both platforms, the wmain leg above is the converter).
int main(int argc, char* argv[]) {
    return RunMain(argc, argv);
}
#endif // _WIN32
