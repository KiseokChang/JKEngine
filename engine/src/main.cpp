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

#include <client/JKActivityGate.h>  // selftest 2i — 클라 활동 게이트 순수 부품(#89 T1)
#include <client/JKClientSurface.h>
#include <server/JKFrameDirty.h>  // selftest 1p — 더티 계산기 순수 단정 (T1)
#include <server/JKWindowServer.h>
#include <agent/JKAgentClient.h>
#include <agent/JKLlmEngine.h>  // selftest 1n-d — 동기 턴 브리지+ollama-direct leg
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
#include <apps/ChatRouter.h>   // selftest 1n — 채팅 명령 라우터(스펙 §1.3)
#include <apps/GalleryModel.h>  // selftest 2g — 갤러리 순수 부품(스펙 2026-10-09-gallery-design)
#include <apps/MusicModel.h>  // selftest 2m/2s/2u — 뮤직 순수 부품(스펙 2026-10-09-music-library-design+2026-10-11-music-tags-design+2026-10-25-music-tagwrite — 2u ①-⑦ T1·⑧-⑭ T2)
#include <apps/MusicDirStore.h>  // selftest 2o — 뮤직 폴더 저장소(플랜 2026-10-10-music-dirs-ui)
#include <apps/MusicSpatialLeg.h>  // selftest 2n — 뮤직 spatial leg 순수 부품(스펙 2026-10-10-music-spatial-leg-design)
#include <voice/SttVoice.h>  // selftest 2v-b/c — 전사 사슬 순수 부품(#112 T2·스펙 2026-10-25-stt-voice-design)
#include <apps/ClientChatApp.h>  // selftest 2z — 구두 응답 TTS 결정 부품(#117 T1·스펙 2026-10-27-tts-response-design)
#include <text/JKTextConv.h>  // selftest 2q — 경로 인코딩 사슬 변환 쌍(#94)
#include <apps/ClientIdlePolicy.h>  // selftest 2i-c — 앱 Timer→더티 조건화 산치(#89 T2)
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

// jkx-pack <app>: bundle jkapp_<app>.dll + launcher icon PNGs + a generated
// manifest into apps/<app>.jkx. Metadata comes from the module's own
// jk_app_meta (single source of truth); icons are optional.
//
// Script apps (docs/27 단계 1): when scripts/apps/<app>/{manifest.txt,app.js}
// exists, the authored manifest is the metadata source and the shared
// jkapp_script.dll rides in as the MODL entry — no per-app native module.
//
// posix leg (앱 커버리지 확대 1단 — docs/70 §제외의 jkx-pack 항목 재개):
// 원래 win32 전용이던 LoadLibraryA 메타 소싱을 dlopen 트리오로 승계해 개방
// (플랜 G2 RunClientModule posix leg 동형 — RTLD_NOW|RTLD_LOCAL·dlerror 세부).
// 분기는 모듈 로드 함수뿐 — 매니페스트 조립·컨테이너 레이아웃·이름 규약은 양
// 플랫폼 코드 공유. manifest module=와 MODL 엔트리명은 플랫폼 무관 ".dll"
// 계약(slot-pack 선례 — RunClientFromJkx FindEntry("MODL", mani.module)의
// *키*로만 소비되지, 추출 파일명은 플랫폼 temp 규약이 별도로 정한다); 디스크에서
// 읽어 파묻는 바이너리만 AppModuleSuffix(.dll/.so) 단일 출처.
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
        regenerated += "module=jkapp_script.dll\n";  // 플랫폼 무관 키 계약 — 아래 posix leg 주석
        regenerated += "script=app.js\n";
        manifestText = jk::JkxManifestMerge(text, regenerated);
        moduleName = "jkapp_script.dll";
    } else {
        // 실제 파일 접미만 플랫폼 값(.dll/.so — JKWindowServer.h AppModuleSuffix
        // 단일 출처); 경로 조립과 그 외 로직은 양 플랫폼 동일.
        const std::string dllPath = base + "jkapp_" + appName +
                                    jk::server::AppModuleSuffix();
#ifdef _WIN32
        void* module = LoadLibraryA(dllPath.c_str());
        if (!module) {
            std::fprintf(stderr, "jkx-pack: cannot load '%s'\n", dllPath.c_str());
            return 1;
        }
        auto metaFn = reinterpret_cast<const jk::JKAppMeta* (*)()>(
            GetProcAddress(module, "jk_app_meta"));
#else
        // 플랜 G2 RunClientModule posix leg 동형: RTLD_NOW|RTLD_LOCAL. 닫지
        // 않는다(FreeLibrary 힙손상 선례 — 팩커는 단명 프로세스). dlerror는
        // 실패 세부를 1회만 보고하므로 NULL 가드(RunClientModule 동일 관습).
        void* module = dlopen(dllPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!module) {
            const char* dlErr = dlerror();
            std::fprintf(stderr, "jkx-pack: cannot load '%s' (%s)\n",
                         dllPath.c_str(), dlErr ? dlErr : "(no dlerror detail)");
            return 1;
        }
        auto metaFn = reinterpret_cast<const jk::JKAppMeta* (*)()>(
            dlsym(module, "jk_app_meta"));
#endif
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
    // 파묻힐 모듈 바이트는 *디스크의 실제 공유 모듈*(플랫폼 접미 — slot-pack
    // kScriptModuleName 선례); 엔트리·manifest 키(moduleName, ".dll")는 플랫폼
    // 무관 계약이라 모듈 stem에서 접미를 다시 붙여 읽는다.
    const std::string moduleBytesPath =
        base + (isScriptApp ? std::string("jkapp_script")
                            : std::string("jkapp_") + appName) +
        jk::server::AppModuleSuffix();
    if (!ReadWholeFile(moduleBytesPath, dll)) {
        std::fprintf(stderr, "jkx-pack: cannot read '%s'\n", moduleBytesPath.c_str());
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

    // CreateDirectoryA → create_directories: exists-ok 계약 동형(slot-pack
    // 855-858 선례 — POSIX 개방 수반 시 동일 승계; 실패 무음 계약 유지).
    std::error_code appsDirEc;
    (void)std::filesystem::create_directories(base + "apps", appsDirEc);
    // 출력 경로 구분자만 플랫폼 값 — win32 원문(역슬래시)·posix는 전진 구분자
    // (drvfs U+F05x 이변 방지 — 인벤토리 (D) drvfs 경고: 역슬래시 성분이
    // /mnt/i 쓰기에 그대로 새면 윈도 측에서 읽히지 않는 사유명이 나온다).
    const std::string outPath = base +
#ifdef _WIN32
        "apps\\" + appName
#else
        "apps/" + appName
#endif
        + ".jkx";
    if (!jk::JKJkxFile::Write(outPath, entries)) return 1;

    std::printf("packed %s\n", outPath.c_str());
    return 0;
}
// win32 전용 유지 주석은 폐기(위 posix leg) — 아래 slot-pack/jkx-list/
// jkx-extract는 순수 stdio+JKJkxFile(TOC 파서는 어댑터리)이라 docs/78 TX6에서
// 이미 posix 개방돼 있었다; jkx-pack이 그 라인에 승계됐다.

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

// library-list [BASE]: 라이브러리 카탈로그 스캔 CLI(스펙 2026-10-06-app-library
// §6 — 서버 불요 검증, jkx-list의 posix 개방 라우트 선례). jkapp_library 클라
// 앱과 같은 jk::LibraryScan 진실원을 먹는다 — CLI가 늘어나면 클라가 변질된
// 것(스캔 규약 서버리 검증). BASE 생략 = exe dir(런치 존재 검증과 같은 기점;
// 뒤 구분자 없음 — LibraryScan 계약), exe 경로 미수령이면 cwd 폴백. 스캔
// 부재(apps/ 없음)도 count=0으로 정당 상태라 실패가 아니다(JKLibraryCatalog.h
// 계약 — 오류 전파 없음).
static int RunLibraryList(int argc, char** argv) {
    std::string base;
    if (argc >= 3) {
        base = argv[2];  // 인수 기점 — 셀프테스트 1m 가짜 트리 케이스 동형
    } else {
        const std::string exe = jk::fs::GetExecutablePath();
        const size_t slash = exe.find_last_of("/\\");
        base = (slash == std::string::npos) ? std::string(".") : exe.substr(0, slash);
        if (base.empty()) base = ".";
    }
    std::vector<jk::LibraryEntry> out;
    const int n = jk::LibraryScan(base, out);
    for (const auto& e : out) {
        // caps 빈값도 caps=로 인쇄 — 원문 계약: 빈 선언을 숨기지 않는다(docs/76
        // 배지 계약 동형). 콘솔·내장은 능력 선언이 없어 늘 빈값이 온다.
        std::printf("name=%s title=%s source=%s caps=%s size=%lld path=%s\n",
                    e.appName.c_str(), e.title.c_str(),
                    e.source == jk::LibrarySource::Jkx           ? "jkx"
                        : e.source == jk::LibrarySource::Console ? "console"
                                                                 : "builtin",
                    e.capabilities.c_str(), e.sizeBytes, e.path.c_str());
    }
    std::printf("count=%d base=%s\n", n, base.c_str());
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

// selftest 1n-d(T3) 전용 Done 싱크 — busy 게이트 검증에서 StartTurn을 직접 잡을
// 때 결과 도착을 기록한다(함수 포인터 계약이라 람다 캡처 대신 파일 스코프 함수).
namespace selftest_llm {
struct Sink {
    std::string   result;
    bool          done = false;
};
inline void SinkDone(jk::agent::LlmTurnResult&& r, void* user) {
    Sink* s = static_cast<Sink*>(user);
    s->result = std::move(r.result);
    s->done = true;
}
}  // namespace selftest_llm

// 구두(STT) leg fail-closed 판정부 (#112 T1 — 스펙 2026-10-25-stt-voice-design
// §3 ②·§2.4·R1, selftest 2v-a): WHISPER_ROOT env 계약의 런타임 한 쪽.
// **미설정/바이너리 부재/모델 부재 = 구두 leg 끔+정직 표기** — 나머지 시스템
// 전부 무영향(spatial leg fail-closed 원존 계약의 문자 그대로 계승). 순수
// 함수 — 입력 3요소(env 유무·whisper-cli 존재·ggml 모델 존재)로 판정하고
// 스폰은 하지 않는다.
// T3 승계: 본 수형이 main.cpp에서 include/voice/SttVoice.h(jk::stt)로 승계됐다
// — 런타임 게이트(jk::voice::turn::RunVoiceTurn — T3)와 표면(T4 jkapp_chat —
// main.cpp 심볼에 접점 없다)이 같은 진실원을 먹어야 하기 때문. reason 원문
// 4행은 구약 등호(2v-a 어설션 원문 무변조 — 호출점 위치 승계만).

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
    // (T3 계보 — kTitle은 셀 메트릭 연동으로 승격돼 플랫폼값이 베이크 상수가
    // 아니다: Win s=1.0 등호 24·posix 1.5 = 32) 그래서 이 케이스의 좌표 기대를
    // 창이 계산한 client rect(OnRectChanged가 세운 {kBorder, titleH})에서
    // 파생한다 — 계약 자체는 불변("surface rect = client + (kBorder,kTitle)").
    {
        auto win = std::make_unique<JKWindow>("strip-selftest");
        win->SetWindowRect(JKRect{ 0, 0, 320, 240 });
        const JKRect clientRect = win->GetClientRect();  // {2, titleH, ...}
        const JKRect strip{ 50, -21, 160, 22 };  // 클라이언트 좌표
        win->SetFrameStripRect(strip);
        auto combo = std::make_unique<JKComboBox>(strip, 0);
        JKComboBox* raw = combo.get();
        win->AddControl(std::move(combo));
        // 콤보 화면 좌표 = client + {50,-21,160,22}(surface y titleH-21..+1) —
        // 중앙 y = titleH-10(원문 kTitle=24 관측 {52,3} 중앙 14).
        const int comboCy = clientRect.y - 10;
        check(win->HitTest(130, comboCy) == raw,  // strip 중앙 → 콤보
              "strip: hit reaches caption child");
        check(win->HitTestRegion(130, comboCy) == JKWindow::WindowRegion::Client,
              "strip: region is Client");
        check(win->HitTestRegion(130, 1) == JKWindow::WindowRegion::TitleBar,
              "strip: outside strip stays TitleBar");
        const JKRect s = win->GetFrameStripSurfaceRect();
        check(s.x == clientRect.x + 50 && s.y == clientRect.y - 21 &&
                  s.w == 160 && s.h == 22,
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
        //   b) 콘솔 dir + manifest.json(name/cmd/desc) → source=Console, appName
        //      = "terminal:<cmd>" 전체(런치 접두 면제 계약 — 1m-9/1m-18)
        //   c) .jkx와 동명 콘솔 → .jkx가 이긴다(스캔 순서 — 런처 규약)
        //   d) 내장 minesweeper·tetris·chat는 항상; lf/hx는 파일 부재 시 제외
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
            // 내장 3(minesweeper·tetris·chat — 이 base엔 apps-bin이 없어
            // lf/hx 제외). = 6 (chat 내장화는 스펙 2026-10-07-desktop-chat-app
            // T2 — 실측으로 기대치 갱신)
            check(n == 6, "1m-1 스캔 개수(jkx2+콘솔1+내장3, 무효·동명 스킵=6)");
            const jk::LibraryEntry* g = nullptr;
            const jk::LibraryEntry* c = nullptr;
            int badFound = 0;
            int consoleWins = 0;
            int terminalKeys = 0;
            const jk::LibraryEntry* mine = nullptr;
            const jk::LibraryEntry* tet = nullptr;
            const jk::LibraryEntry* chat = nullptr;
            for (const auto& e : got) {
                if (e.appName == "galapp") g = &e;
                // 콘솔 발견 키 = "terminal:<cmd>" 전체(final review Item 1 —
                // launch_app 접두 면제 계약; manifest 이름은 스폰 키가 아니다).
                if (e.appName == "terminal:apps-bin/y") c = &e;
                if (e.appName == "badapp") ++badFound;
                if (e.source == jk::LibrarySource::Console &&
                    e.appName == "terminal:apps-bin/x") ++consoleWins;
                // lf/hx 내장 terminal: 카운터 — 콘솔 엔트리도 terminal: 접두를
                // 쓴다(1m-18 계약)라 내장(Builtin) 한정으로 센다.
                if (e.appName.rfind("terminal:", 0) == 0 &&
                    e.source == jk::LibrarySource::Builtin) ++terminalKeys;
                if (e.appName == "minesweeper") mine = &e;
                if (e.appName == "tetris") tet = &e;
                if (e.appName == "chat") chat = &e;
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
                check(g->manifestRaw.find("capabilities=widget,timer") !=
                          std::string::npos &&
                          !g->manifestRaw.empty(),
                      "1m-17 MANI 원문 전승(비공백+원문 매치)");
            }
            check(c != nullptr, "1m-9 콘솔 발견(appName=terminal:apps-bin/y)");
            if (c) {
                check(c->source == jk::LibrarySource::Console,
                      "1m-10 source=Console");
                check(c->title == "Console App", "1m-11 콘솔 desc 표시명 전승");
                check(c->capabilities.empty() && c->sizeBytes == 0,
                      "1m-12 콘솔 능력 빈값+크기 0 계약");
                check(c->path.find("consoleapp") != std::string::npos,
                      "1m-13 콘솔 dir 절대 경로");
                check(c->appName.rfind("terminal:", 0) == 0 &&
                          c->appName.find("apps-bin/y") != std::string::npos,
                      "1m-18 terminal: 접두+cmd 포함(런치 계약)");
                check(!c->manifestRaw.empty() &&
                          c->manifestRaw.find("\"cmd\":\"apps-bin/y\"") !=
                              std::string::npos,
                      "1m-19 콘솔 manifest.json 원문 전승");
            }
            check(consoleWins == 0 && badFound == 0,
                  "1m-14 .jkx 우선(동명 콘솔 스킵)+무효 컨테이너 스킵");
            check(mine != nullptr && tet != nullptr && chat != nullptr &&
                      mine->source == jk::LibrarySource::Builtin &&
                      chat->source == jk::LibrarySource::Builtin &&
                      mine->title == "Minesweeper" && tet->title == "Tetris" &&
                      chat->title == "Chat" &&
                      mine->path.empty() && tet->sizeBytes == 0 &&
                      chat->path.empty() && chat->sizeBytes == 0,
                  "1m-15 내장 minesweeper·tetris·chat 항상(Builtin)");
            check(terminalKeys == 0,
                  "1m-16 lf/hx 파일 부재=제외(폰 기본값 경로)");
            std::error_code ec2;
            check(std::filesystem::remove_all(base, ec2) > 0 && !ec2,
                  "1m-z 클린업");
        }

        // 1m-t) 콘솔 설치 트윈 스폰 키 (앱 커버리지 Task 3 — manifest
        //   "cmd_posix" 확장 계약). 가짜 apps/ 트리를 temp에서 별도 조립 —
        //   1m 본 트리(n==6 계약)와 무접촉. 기대 계약(JKLibraryCatalog.cpp와
        //   1:1):
        //   a) cmd_posix 유+트윈 파일 존재 → posix 스폰 키 = "terminal:" +
        //      cmd_posix(basePath 상대 — posix SpawnProcess child cwd=exe dir
        //      실측 계약, 내장 lf 키와 같은 기점), win32는 cmd 원문(트윈 필드
        //      무시 — Windows 카탈로그 계약 불변).
        //   b) cmd_posix 유+트윈 파일 결손 → posix 스킵(fail-closed — lf/hx
        //      파일 존재 게이트 동형), win32는 무게이트 카운트(계약 불변).
        //   c) cmd_posix 무 → cmd 원문(1m leg 계약 — 무트윈 매니페스트 호환).
        {
            std::error_code ec;
            const std::string base =
                (std::filesystem::temp_directory_path(ec)
                 .append("jk_library_st_1mt")).string();
            std::filesystem::remove_all(base, ec);
            std::filesystem::create_directories(base + "/apps/twincmdapp", ec);
            std::filesystem::create_directories(base + "/apps/missingtwin", ec);
            std::filesystem::create_directories(base + "/apps/barecmd", ec);
            {
                const std::string twinJson =
                    "{\"name\":\"twincmd\",\"cmd\":\"twincmd.cmd\","
                    "\"cmd_posix\":\"apps/twincmdapp/twincmd.sh\"}";
                const std::string missingJson =
                    "{\"name\":\"missingtwin\",\"cmd\":\"missingtwin.cmd\","
                    "\"cmd_posix\":\"apps/missingtwin/missingtwin.sh\"}";
                const std::string bareJson =
                    "{\"name\":\"barecmd\",\"cmd\":\"apps-bin/bare\"}";
                std::ofstream f1(base + "/apps/twincmdapp/manifest.json",
                                 std::ios::binary);
                f1.write(twinJson.data(), static_cast<std::streamsize>(
                                              twinJson.size()));
                // 트윈 파일 — 스캔은 존재만 본다(더미 원문으로 충분).
                std::ofstream f2(base + "/apps/twincmdapp/twincmd.sh",
                                 std::ios::binary);
                f2.write("#!/bin/sh\n", 10);
                std::ofstream f3(base + "/apps/missingtwin/manifest.json",
                                 std::ios::binary);
                f3.write(missingJson.data(), static_cast<std::streamsize>(
                                                 missingJson.size()));
                std::ofstream f4(base + "/apps/barecmd/manifest.json",
                                 std::ios::binary);
                f4.write(bareJson.data(), static_cast<std::streamsize>(
                                              bareJson.size()));
            }
            std::vector<jk::LibraryEntry> got;
            const int n = jk::LibraryScan(base, got);
            // 개수 — 내장 3 + barecmd 1 은 양축 공통. 트윈 leg만 갈린다:
            //   win32 = twincmd·missingtwin 전부 카운트(무게이트) = 6
            //   posix = twincmd만(결손 트윈 스킵) = 5
#ifdef _WIN32
            check(n == 6, "1mt-0 스캔 개수(win32 = 내장3+barecmd+트윈2 = 6)");
#else
            check(n == 5, "1mt-0 스캔 개수(posix = 내장3+barecmd+트윈1 = 5)");
#endif
            const jk::LibraryEntry* t = nullptr;
            const jk::LibraryEntry* m = nullptr;
            const jk::LibraryEntry* b = nullptr;
            for (const auto& e : got) {
                if (e.source != jk::LibrarySource::Console) continue;
                if (e.appName.find("twincmd") != std::string::npos)
                    t = &e;  // 매치 — 키는 플랫폼별(아래에서 판정)
                if (e.appName == "missingtwin" ||
                    e.appName == "terminal:missingtwin.cmd")
                    m = &e;
                if (e.appName == "terminal:apps-bin/bare") b = &e;
            }
            check(t != nullptr, "1mt-1 트윈 존재 leg 발견");
            if (t) {
#ifdef _WIN32
                check(t->appName == "terminal:twincmd.cmd",
                      "1mt-2 win32 트윈 필드 무시(cmd 원문 키 — 계약 불변)");
#else
                check(t->appName == "terminal:apps/twincmdapp/twincmd.sh",
                      "1mt-2 posix 트윈 스폰 키(terminal:+cmd_posix)");
#endif
                check(t->source == jk::LibrarySource::Console,
                      "1mt-3 트윈 leg source=Console");
                check(t->path.find("twincmdapp") != std::string::npos,
                      "1mt-4 트윈 leg 콘솔 dir 절대 경로");
            }
#ifdef _WIN32
            check(m != nullptr,
                  "1mt-5a win32 결손 트윈 무게이트 카운트(계약 불변)");
#else
            check(m == nullptr,
                  "1mt-5b posix 결손 트윈 스킵(fail-closed 존재 게이트)");
#endif
            check(b != nullptr && b->appName == "terminal:apps-bin/bare",
                  "1mt-6 cmd_posix 무 매니페스트 = cmd 원문(1m leg 계약 유지)");
            std::error_code ec3;
            check(std::filesystem::remove_all(base, ec3) > 0 && !ec3,
                  "1mt-z 클린업");
        }

        // 1n) 채팅 명령 라우터 (스펙 2026-10-07-desktop-chat-app §1.3 — stub
        //   턴 백엔드의 뇌). Offline pure 룩업 — 서버·창 무접촉으로 어휘 4종
        //   +불인+별명 도표를 잠근다. 기대 계약(ChatRouter.h와 1:1):
        //   a) "지뢰찾기 켜줘" → Launch, app=minesweeper(별명표 해소)
        //   b) 표 밖 앱어 → 그대로 통과(fail-open — 서버 unknown_app 정직 회신)
        //   c) 조사 절단·무공백 접어체("테트리스를 켜줘"/"테트리스켜줘")
        //   d) 앱어 부재("켜줘") → Info+안내문(지시 불성립 — Launch 아님)
        //   e) Close 무앱=app ""(포커스 창 위임), Close 앱어=해소 키
        //   f) 인식 불가 → Info, 도구 지시 없음(app "")
        {
            jk::ChatAction a;
            // (a) Launch 별명 — 한국어 → 라이브러리 appName 규약
            std::string resp = jk::ChatRouterRoute("지뢰찾기 켜줘", a);
            check(a.kind == jk::ChatAction::Launch && a.app == "minesweeper",
                  "1n-1 별명 런치(지뢰찾기→minesweeper)");
            check(resp.find("실행") != std::string::npos &&
                      resp.find("지뢰찾기") != std::string::npos,
                  "1n-2 런치 응답 확인법(한국어+발화어 반영)");
            // 영문 스폰 키 직기입 + 응답은 발화어(키) 그대로
            resp = jk::ChatRouterRoute("tetris 열어줘", a);
            check(a.kind == jk::ChatAction::Launch && a.app == "tetris",
                  "1n-3 영문 키 런치(tetris 항목 그대로)");
            check(resp.find("tetris") != std::string::npos,
                  "1n-3b 런치 응답에 영문 키 전승");
            // (c) 조사+무공백 접어체 — 조사 절단과 byte 접미 매핑
            (void)jk::ChatRouterRoute("테트리스를 켜줘", a);
            check(a.kind == jk::ChatAction::Launch && a.app == "tetris",
                  "1n-4a 조사 절단(테트리스를→tetris)");
            (void)jk::ChatRouterRoute("지뢰찾기켜줘", a);
            check(a.kind == jk::ChatAction::Launch && a.app == "minesweeper",
                  "1n-4b 무공백 접어체(지뢰찾기켜줘)");
            // (b) 표 밖 앱어 통과 — 카탈로그 appName 규약 그대로
            resp = jk::ChatRouterRoute("계산기 켜줘", a);
            check(a.kind == jk::ChatAction::Launch && a.app == "계산기",
                  "1n-5 표 밖 앱어 통과(fail-open 계약)");
            // (d) 앱어 부재 — Launch 오보 금지, Info+안내문
            resp = jk::ChatRouterRoute("켜줘", a);
            check(a.kind == jk::ChatAction::Info && a.app.empty() &&
                      resp.find("명령") != std::string::npos,
                  "1n-6 앱어 부재=Info(지시 불성립)");
            // Close — 무앱=포커스 창 위임, 앱어=해소 키
            resp = jk::ChatRouterRoute("꺼줘", a);
            check(a.kind == jk::ChatAction::Close && a.app.empty(),
                  "1n-7 Close 무앱(app=\"\" = 포커스 창 위임)");
            (void)jk::ChatRouterRoute("테트리스 닫아줘", a);
            check(a.kind == jk::ChatAction::Close && a.app == "tetris",
                  "1n-8 Close 앱어+별명 해소");
            // Focus 2형 — 트리거 긴 형이 짧은 형에 잡아먹히지 않음(표 순서)
            (void)jk::ChatRouterRoute("창 포커스", a);
            check(a.kind == jk::ChatAction::Focus && a.app.empty(),
                  "1n-9a focus_window 지시");
            (void)jk::ChatRouterRoute("앞으로 가져와", a);
            check(a.kind == jk::ChatAction::Focus && a.app.empty(),
                  "1n-9b 긴 트리거 우선(앞으로 가져와≠앞으로)");
            // ListWindows 2형 — 접어체·띄어쓰기 변형 전부
            (void)jk::ChatRouterRoute("창 목록", a);
            check(a.kind == jk::ChatAction::ListWindows && a.app.empty(),
                  "1n-10a list_windows 지시");
            (void)jk::ChatRouterRoute("뭐 떠 있어", a);
            check(a.kind == jk::ChatAction::ListWindows,
                  "1n-10b 변형 트리거(뭐 떠 있어)");
            // 공백·개행 정규화 — 채팅 입력 잔공백 흡수
            (void)jk::ChatRouterRoute(" 창 목록 \n", a);
            check(a.kind == jk::ChatAction::ListWindows,
                  "1n-11 앞뒤 공백 정규화");
            // (f) 불인 — 지시 없음+안내문이 어휘표 훑기에서 나온다
            resp = jk::ChatRouterRoute("세상엔 채팅이 이렇게 어려웠나", a);
            check(a.kind == jk::ChatAction::Info && a.app.empty(),
                  "1n-12 불인=Info(app 빈값 — 도구 지시 없음)");
            check(resp.find("켜줘") != std::string::npos &&
                      resp.find("명령") != std::string::npos,
                  "1n-13 Info 안내문이 어휘 표에서 조립");
        }

        // 1n-s) close_window/focus_window 서버 타깃 해소 (채팅 F1 — plan
        //   2026-10-08-chat-close-fix, docs/80 §4 귀속 백로그 착지). Resolver는
        //   순수 로직(AgentWindowRef 스냅샷만 먹는다 — JKWindowServer.h 계약
        //   전문)이라 서버 프로세스·컴포지터 무접촉으로 도표를 잠근다:
        //   a) argless+포커스=그 창(폰 "닫아줘") · b) argless+무포커스=0
        //   (window_not_found 정직 계약) · c) app 제목 매칭 대소문자 무시
        //   (minesweeper→"Minesweeper") · d) 배지 제목 부분 일치("key (N)")
        //   · e) 무매칭=0 · f) id 직접호출 계약 보존.
        {
            using jk::server::AgentWindowRef;
            using jk::server::ResolveAgentWindowTarget;
            const std::vector<AgentWindowRef> wins = {
                {101, "Minesweeper", false},
                {102, "Notes (3)", false},
                {103, "chat", true},
            };
            // (a) argless = 포커스 창 — 채팅 얼굴 "닫아줘"의 본 경로
            check(ResolveAgentWindowTarget(wins, false, 0, "") == 103,
                  "1n-s1 argless=포커스 창 해소(닫아줘 — list_windows 근거)");
            // (b) 포커스 없음 → 0 = window_not_found 승계(정직 회신 계약)
            const std::vector<AgentWindowRef> noFocus = {
                {101, "Minesweeper", false},
                {102, "chat", false},
            };
            check(ResolveAgentWindowTarget(noFocus, false, 0, "") == 0,
                  "1n-s2 argless 무포커스=0(window_not_found — 거짓 성공 없음)");
            // (c) app 지명 — 라우터가 해소한 키(minesweeper)와 실제 창 제목
            //     (Minesweeper)의 대소문자 무시 정확 일치
            check(ResolveAgentWindowTarget(wins, false, 0, "minesweeper") == 101,
                  "1n-s3 app 지명 제목 매칭(대소문자 무시 — minesweeper→Minesweeper)");
            // (d) 부분 일치 — notify 배지 제목("key (N)")을 정확 키로 닫는다
            check(ResolveAgentWindowTarget(wins, false, 0, "notes") == 102,
                  "1n-s4 app 지명 부분 일치(notes→Notes (3) 배지 선례)");
            // (e) 무매칭 → 0 = window_not_found 승계
            check(ResolveAgentWindowTarget(wins, false, 0, "nosuchwindow") == 0,
                  "1n-s5 app 무매칭=0(window_not_found 정직 계약)");
            // (f) id 직접호출 계약 보존 — 존재 id는 그 id 그대로(제목 무관),
            //     목록 밖 id는 0(기존 window_not_found와 동치)
            check(ResolveAgentWindowTarget(wins, true, 102, "minesweeper") == 102,
                  "1n-s6 id 직접호출 보존(존재 id 승계 — app 인자 무시)");
            check(ResolveAgentWindowTarget(wins, true, 999, "") == 0,
                  "1n-s7 id 미존재=0(기존 window_not_found 동치)");
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

    // 1n-d) LLM 동기 턴 브리지 + ollama-direct leg (T3 — 스펙
    //   2026-10-08-chat-llm-promotion 설계 결정 2·3). TurnSync는 StartTurn 위
    //   condition_variable 래퍼일 뿐(스폰·파이프·stream-json 파서·kill 계약
    //   재용 — 파서 복제 금지), ollama-direct는 stdout 일반 텍스트를 전부
    //   수집해 결과로. 서버·창 무접촉 — chat.json(engine/direct_cmd)을
    //   exe-dir state에 시딩해 스폰을 echo 스터브로 대체한다. 기본 조립식이
    //   실 ollama를 쏘면 캐논이 그 기기의 설치/네트워크에 의존하게 되므로
    //   실 ollama 의존 금지(환경 의존 함정) — 조립식은 BuildOllamaDirectCmd
    //   원문 단정으로 잠그고 스폰은 스터브 대체. 기존 chat.json은 백업 후 복원
    //   (실 소비자 jkbridge의 사용자 파일 보존).
    {
        using jk::agent::ChatConfig;
        using jk::agent::JKLlmEngine;
        using jk::agent::LlmTurnResult;
        namespace fsx = std::filesystem;

        const std::string exePath = jk::fs::GetExecutablePath();
        const size_t sep = exePath.find_last_of("\\/");
        const std::string stateDir =
            sep == std::string::npos ? std::string("state")
                                     : exePath.substr(0, sep) + "/state";
        const std::string cfgPath = stateDir + "/chat.json";
        // 무손실 백업 — LoadChatConfig는 4096바이트만 읽지만 복원은 원문 전체.
        std::string cfgBackup;
        bool hadCfg = false;
        if (std::FILE* bf = std::fopen(cfgPath.c_str(), "rb")) {
            std::fseek(bf, 0, SEEK_END);
            const long sz = std::ftell(bf);
            std::fseek(bf, 0, SEEK_SET);
            if (sz > 0) {
                cfgBackup.resize(static_cast<size_t>(sz));
                const size_t n = std::fread(&cfgBackup[0], 1, cfgBackup.size(),
                                            bf);
                cfgBackup.resize(n);
                hadCfg = n > 0;
            }
            std::fclose(bf);
        }
        auto WriteCfg = [&cfgPath](const std::string& json) -> bool {
            std::FILE* f = std::fopen(cfgPath.c_str(), "wb");
            if (!f) return false;
            const size_t w = std::fwrite(json.data(), 1, json.size(), f);
            std::fclose(f);
            return w == json.size();
        };
        auto Seed = [&WriteCfg, &exePath](const std::string& directCmd) -> bool {
            // directory도 시딩한다 — cfg 기본값은 repo 절대 경로(윈32 표기)라
            // posix 어댑터의 chdir이 실패해 스폰 단정 자체가 묻힌다(WSL 실측
            // 3 FAIL 함정). exe-dir는 양축이 다녀간 적 있는 실제 경로.
            std::string dirJ = exePath;
            const size_t cut2 = dirJ.find_last_of("\\/");
            if (cut2 != std::string::npos) dirJ = dirJ.substr(0, cut2);
            std::string dirEsc;
            for (char ch : dirJ) {  // JSON 인용 이스케이프 — win32 백슬래시 경로
                if (ch == '"') dirEsc += "\\\"";
                else if (ch == '\\') dirEsc += "\\\\";
                else dirEsc += ch;
            }
            return WriteCfg(std::string(
                       "{\"engine\":\"ollama-direct\",\"model\":"
                       "\"glm-test-stub:cloud\",\"directory\":\"") +
                       dirEsc + "\",\"direct_cmd\":\"" + directCmd + "\"}");
        };

        std::error_code ec2;
        check(fsx::create_directories(stateDir, ec2) || fsx::exists(stateDir),
              "1n-d0 state dir ready (exe-dir chat.json seam)");

        // (a) echo 스터브 stdout 원문 왕복 — plain-text leg의 본 계약.
        //   echo 리터럴은 win32(cmd.exe /c 접두·worker가 붙인다)와 posix
        //   (/bin/sh -c) 양축이 같은 문자열로 개통된다.
        check(Seed("echo jk-ollama-direct-stub-3361"),
              "1n-d0b chat.json seeded (engine=ollama-direct + direct_cmd)");
        JKLlmEngine eng;  // 파일 표기: Llm 대문자 L — docs/81 §4 오타 유예는 #83 전수 정화로 소각 (2026-10-09)
        LlmTurnResult r;
        check(eng.TurnSync("안녕", r), "1n-d1 echo stub turn returns true");
        check(r.ok && r.result == "jk-ollama-direct-stub-3361",
              "1n-d2 stdout collected verbatim, boundary-ws trimmed");
        check(r.sessionId.empty(), "1n-d3 sessionId empty (plain-text leg)");
        check(!r.streamed, "1n-d4 plain-text leg raises no delta (streamed=0)");

        // (b) 조립식 원문 단정(kStubShellCmd* 선례의 동형 — 스폰 대체 없이
        //   컴포지션만 잠근다). 특수문자는 플랫폼 이스케이프 규약이 갈린다:
        //   win32는 CRT argv 규칙(원문 보존), posix는 sh 이중 따옴표 라이브
        //   문자(가역 이스케이프). 프리앰블 접두는 plain-text leg에도 공통
        //   (docs/60 ⑥ 계약 유지).
        {
            ChatConfig cc;
            cc.model = "glm-test:cloud";
            const std::string cmd =
                jk::agent::BuildOllamaDirectCmd(cc, "say \"hi\" $(id)");
            const std::string prefix =
                "ollama run --hidethinking --nowordwrap \"";
            check(cmd.compare(0, prefix.size(), prefix) == 0,
                  "1n-d5 composed cmd prefix `ollama run "
                  "--hidethinking --nowordwrap \"` (2026-10-26 계약 변경 — "
                  "결합만이 완건 단일행 JSON, kOllamaRunRenderFlags)");
            check(cmd.back() == '"',
                  "1n-d6 composed cmd keeps the prompt quote span closed");
            check(cmd.find("\"glm-test:cloud\"") != std::string::npos,
                  "1n-d7 model lands inside its quoted span");
            check(cmd.find("[시스템 지시]") != std::string::npos &&
                      cmd.find("[사용자] say") != std::string::npos,
                  "1n-d8 preamble rides the plain-text leg too");
#ifdef _WIN32
            check(cmd.find("say \"\"hi\"\" $(id)") != std::string::npos,
                  "1n-d9a win32 composes doubled content quotes, keeps $ "
                  "live (T3 fix r1)");
#else
            check(cmd.find("say \\\"hi\\\" \\$(id)") != std::string::npos,
                  "1n-d9b posix escapes the sh-dollar before it executes");
#endif
        }

        // (c) 정직 실패 2형 — 비영(stdout 공백)과 미지 명령(stderr 근거).
        //   exit 0/true는 win32(cmd.exe)/posix(sh) 모두 공백 stdout이고,
        //   미지 명령은 양축 셸이 근거 문자열(jk-nosuch-cmd-3361)을 stderr로
        //   보낸다 — 어댑터 스폰 불가 경로가 아닌 "셸 수준" 정직 계약.
#ifdef _WIN32
        check(Seed("exit 0"), "1n-d10 cfg re-seeded (empty-stdout stub)");
#else
        check(Seed("true"), "1n-d10 cfg re-seeded (empty-stdout stub)");
#endif
        check(!eng.TurnSync("x", r) && !r.ok,
              "1n-d11 empty stdout reports ok=false (honest)");
        check(Seed("jk-nosuch-cmd-3361"), "1n-d12a cfg re-seeded (dead cmd)");
        check(!eng.TurnSync("x", r) && !r.ok &&
                  r.result.find("jk-nosuch-cmd-3361") != std::string::npos,
              "1n-d12b unknown command surfaces the shell's stderr evidence");

        // (d) 타임아웃 — Done 미도착 = ok=false, 예산 준수, 그리고 지연 Done
        //   흡수(worker가 살아 돌아가 busy를 사후에 푼다 — refcount 계약의
        //   관측 가능한 얼굴). 중복 kill 없음: 자식은 StartTurn의
        //   kill-on-close job 계약 안에서 자연 종료.
        //   (a)~(c)와 같은 cfg 셋업에서 sleeper 스터브로 갈아탄다 — echo
        //   스터브는 즉발이라 타임아웃을 유도할 수 없다. 3361 마커는 어디에도
        //   인쇄되지 않는다.
        const char* sleeper =
#ifdef _WIN32
            "ping -n 2 127.0.0.1 > nul";  // ≈1s — no stdin, console-free
#else
            "sleep 2";
#endif
        check(Seed(sleeper), "1n-d13 cfg re-seeded (sleeper stub)");
        const auto tD = std::chrono::steady_clock::now();
        check(!eng.TurnSync("x", r, 300), "1n-d14 timeout(300ms) reports false");
        const double elapsedD =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - tD)
                .count();
        std::printf("  1n-d: timeout path returned after %.0f ms\n",
                    elapsedD);
        std::fflush(stdout);
        check(elapsedD < 2500.0, "1n-d15 timeout honored the 300ms budget");
        bool settled = false;
        for (int i = 0; i < 500 && !settled; ++i) {  // 10s budget
            settled = !eng.Busy();
            if (!settled) std::this_thread::sleep_for(
                               std::chrono::milliseconds(20));
        }
        check(settled && !r.ok,
              "1n-d16 late Done absorbed, busy gate released (refcount "
              "contract observable)");

        // (e) busy 게이트 — StartTurn 진행 중인 엔진의 동기 브리지는 씽크다
        //   없이 거부하고 사유를 실는다(스펙 fallback 원칙의 상면).
        selftest_llm::Sink sink;
        check(eng.StartTurn("hold", "", nullptr, &selftest_llm::SinkDone,
                            &sink),
              "1n-d17 async hold turn accepted (StartTurn direct)");
        LlmTurnResult rb;
        check(!eng.TurnSync("x", rb), "1n-d18 TurnSync refused while busy");
        check(!rb.ok && rb.result.find("busy") != std::string::npos,
              "1n-d19 busy refusal carries the honest reason");
        bool holdDone = false;
        for (int i = 0; i < 500 && !holdDone; ++i) {  // 10s budget
            holdDone = sink.done && !eng.Busy();
            if (!holdDone) std::this_thread::sleep_for(
                                std::chrono::milliseconds(20));
        }
        check(holdDone, "1n-d20 hold turn settles (Done + busy released)");

        // 복원 — 시딩한 chat.json을 정리한다(없었던 기기는 삭제, 있던 기기는
        // 원문 복구 — selftest가 사용자 파일을 훼손하지 않는다).
        if (hadCfg) {
            check(WriteCfg(cfgBackup), "1n-d21 chat.json restored verbatim");
        } else {
            fsx::remove(cfgPath, ec2);
            check(!ec2 && !fsx::exists(cfgPath),
                  "1n-d21 seeded chat.json removed (scratch-free teardown)");
        }
    }

    // 1n-e) F3-1 — cmd 토글 주입 수리 단정(T3 fix r1, 2026-10-08). 배경:
    //   win32 leg(cmd.exe /c 접두)의 조립식은 cmd 토글 + CRT argv 재파싱의
    //   이중 파서를 통과한다. 개선 전의 내용 따옴표 이스케이프(`\"`)는 CRT에는
    //   리터럴이지만 cmd에는 그대로 토글이어서, 컨텐츠의 불균형 따옴표가 cmd의
    //   인용 지역을 일찍 닫고 뒤따르는 & 를 살린다 — 실측(claude/ollama 양 leg,
    //   probe_claude_leg A): stream-json 플래그가 전부 도둑맞고 echo가 별행
    //   개통. 수리는 ShellDqEscape의 win32 분기를 이중화(`""`)로 갈아탔다:
    //   cmd는 토글 짝(중립 — 지역이 끝까지 열려 메타문자 사망), node(msvc CRT
    //   — claude.cmd 사슬)는 지역 안 리터럴 따옴표(원문 도달), shell32/Go
    //   (ollama.exe)는 리터럴+토글(균형 인용 원문 도달 — probe_ollama_leg3 C2;
    //   불균형은 첫 따옴표 뒤 안전 잘림이 남는다 — probe_ollama_leg S, 개선
    //   전의 주입+플래그 도난보다 안전 우선). 케릿 갑옷은 F2 실측상 인용 안에서
    //   리터럴로 살아 컨텐츠를 변형하므로 기각.
    //   (a) 조립식 — 적대 컨텐츠의 메타문자가 cmd/sh 인용 국소에 전부 착지
    //       (토글 시뮬레이션) + 수리 형태의 원문 단정. (b) 라이브 cmd — 양성
    //       대조(주입 개통 자체)와 수리 형태의 무주입을 실 cmd.exe에서.
    //       스폰 자식은 echo 셸 리터럴뿐 — 추가 바이너리 금지. posix는 무변
    //   (조건부 실행 계약 c): sh 걷기 동형 단정 + 원문은 1n-d9b가 잠근다.
    //   알려진 잔여(원장): `%VAR%`은 cmd가 인용 안에서도 사전 확장(F6 실측)하고
    //   케릿이 인용 안 리터럴이라 갑옷 불가 — 확장 텍스트가 인용 파티티를 깨는
    //   환경값 의존 벡터(공격자 머신 로컬)는 미커버, 재발 시 케릿 존을 벗어나는
    //   별도 설계.
    {
        using jk::agent::ChatConfig;
        // 적대 컨텐츠 — 불균형 인용 + cmd 메타문자 + sh 라이브 문자. %는 잔여로
        // 남기고 본 단정 밖(위 원장 줄).
        const std::string hostile =
            "she said \"hi & echo INJECTED-MARKER-3361 <in|out> $(id)";

        // 플랫폼 지역 걷기 — 조립식(빌드 이전 문자열) 단정: 인용 토글을
        // 시뮬레이션해 컨텐츠 메타문자가 전부 "인용 국소"(state=1)에 착지하는지.
        // win32: cmd 규약(따옴표 = 토글, 백슬래시는 이스케이프 아님 — 실측).
        // posix: /bin/sh -c 규약(지역 안 `\\x` = 리터럴 x — 지역 유지). 조립식의
        // "국소 외 메타문자 없음 + 종료 시 지역 닫힘"이 주입 방어 그 자체다.
        auto MetaShielded = [](const std::string& composed,
                               const std::string& metaSet,
                               bool swallowBackslashPair) {
            int state = 0;
            for (size_t i = 0; i < composed.size(); ++i) {
                const char ch = composed[i];
                if (swallowBackslashPair && ch == '\\' && state == 1 &&
                    i + 1 < composed.size()) {
                    ++i;  // sh dq 지역 안 \x — 다음 문자와 함께 리터럴 소화
                    continue;
                }
                if (ch == '"') state ^= 1;
                else if (state == 0 && metaSet.find(ch) != std::string::npos)
                    return false;
            }
            return state == 0;
        };
        ChatConfig cc;
        cc.model = "glm-test:cloud";
        const std::string cmd =
            jk::agent::BuildOllamaDirectCmd(cc, hostile);
#ifdef _WIN32
        check(MetaShielded(cmd, "&|<>", false),
              "1n-e1 composed region shields the hostile metacharacters "
              "(cmd toggle walk)");
#else
        check(MetaShielded(cmd, "&|<>", true),
              "1n-e1 composed region shields the hostile metacharacters "
              "(sh region walk)");
#endif
#ifdef _WIN32
        check(cmd.find("she said \"\"hi & echo INJECTED-MARKER-3361") !=
                      std::string::npos &&
                  cmd.find("$(id)") != std::string::npos &&
                  cmd.find("\\\"") == std::string::npos,
              "1n-e2 win32 composes doubled quotes, no bkslash-quote form");
#else
        check(cmd.find("she said \\\"hi & echo INJECTED-MARKER-3361") !=
                      std::string::npos,
              "1n-e2 posix keeps the sh bkslash-quote form (leg unchanged)");
#endif

#ifdef _WIN32
        // 라이브 cmd — 같은 cmd.exe /c 셸 접두에 echo 자식을 태운다(추가
        // 바이너리 금지). 검출기: 주입이 개통되면 마커가 별행으로 인쇄된다
        // (양성 대조 캘리브레이션 실측: `INJECTED-MARKER-3361"` 단독 행).
        auto DrainPipe = [](void* pipe, std::string* sink) {
            char buf[8192];
            bool open = true;
            while (open) {
                uint32_t avail = 0;
                int broken = 0;
                if (jk::process::PeekPipeAvail(pipe, &avail, &broken) &&
                    avail > 0) {
                    const int got =
                        jk::process::ReadPipeData(pipe, buf, sizeof(buf));
                    if (got > 0) {
                        sink->append(buf, static_cast<size_t>(got));
                        continue;
                    }
                    open = false;
                } else if (broken == 109 || broken == 232) {
                    open = false;
                } else {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(10));
                }
            }
        };
        auto SpawnAndDrain = [&DrainPipe](const std::string& cmdline,
                                          std::string* out) -> bool {
            jk::process::SpawnOptions o;
            o.commandLineUtf8 = cmdline;
            o.hideWindow = true;
            o.inheritedStdioPipes = true;
            const jk::process::SpawnResult r = jk::process::Spawn(o);
            if (!r.ok) return false;
            DrainPipe(r.stdoutRead, out);
            jk::process::CloseHandleLike(r.stdoutRead);
            jk::process::CloseHandleLike(r.stderrRead);
            jk::process::WaitForExit(r.process, 5000);
            jk::process::CloseHandleLike(r.process);
            return true;
        };
        auto MarkerOwnLine = [](const std::string& out) {
            // 마커로 시작하는 행이 존재하고 그 행이 프롬프트 컨텐츠(`she said`)
            //를 실지 않으면 — 주입 별행(양성 대조의 모양, 캘리브레이션 실측).
            size_t at = 0;
            while ((at = out.find("INJECTED-MARKER-3361", at)) !=
                   std::string::npos) {
                const size_t bol = out.find_last_of('\n', at);
                size_t next = out.find('\n', at);
                if (next == std::string::npos) next = out.size();
                const std::string line = out.substr(
                    (bol == std::string::npos ? 0 : bol + 1),
                    next - (bol == std::string::npos ? 0 : bol + 1));
                if (line.compare(0, 20, "INJECTED-MARKER-3361") == 0)
                    return true;
                at += 20;
            }
            return false;
        };

        // 양성 대조 — 개선 전 수형(`\"` 토글)을 손수 적어 검출기가 주입을
        // 보는 것을 먼저 증명한다(검출기 무기력이면 수리 단정이 공허).
        std::string vuln;
        check(SpawnAndDrain("cmd.exe /c echo \"she said \\\"hi & echo "
                            "INJECTED-MARKER-3361\"",
                            &vuln) &&
                  MarkerOwnLine(vuln),
              "1n-e3 live cmd: legacy bkslash shape injects (positive "
              "control)");

        // 수리 형태 — 조립식(BuildOllamaDirectCmd)의 실 스폰 형태 중 컨텐츠
        // 지역을 그대로 echo 자식에 실어 실 cmd.exe에서 증명. 모델 스팬 뒤
        // = `"..."` 프롬프트 지역 전체(첫따옴표부터 닫는따옴표까지).
        const std::string modelSpan = "\"glm-test:cloud\" ";
        const std::string region = cmd.substr(
            cmd.find(modelSpan) + modelSpan.size());
        std::string fixed;
        check(SpawnAndDrain("cmd.exe /c echo " + region, &fixed) &&
                  !MarkerOwnLine(fixed) &&
                  fixed.find("she said \"") != std::string::npos &&
                  fixed.find("INJECTED-MARKER-3361") != std::string::npos,
              "1n-e4 live cmd: composed region carries the marker as "
              "content, no injection");
#endif  // _WIN32 — 1n-e3/e4 라이브 cmd 전용
    }

    // 1n-f) 자연어 승격 배선 (T4 — 스펙 2026-10-08-chat-llm-promotion 설계
    //   결정 3·4). jkweb HandleTalk·jktalk ProcessTurn의 뇌호출 자리가 먹는
    //   jk::ChatRouteTurn의 배선 순서 도표: ① 정확 트리거 매치 → 기존 즉발
    //   (LLM 왕복 0) ② 비매치 + cfg 구성 → TurnSync 동기 턴 + 행동 JSON
    //   파싱 ③ LLM 실패/파싱 불가 → 기존 stub 안내문 폴백(원문 그대로).
    //   스폰은 T3 direct_cmd 스터브 주입 재용(실 ollama 의존 금지 — 환경
    //   의존 함정: 캐논이 그 기기의 ollama 설치/네트워크를 먹지 않게).
    //   chat.json은 1n-d와 같은 백업→시딩→복원 계약(엔진 worker가 cfg를
    //   재로드하므로 시딩이 필수 — ChatRouteTurn 인자 cfg는 배선 판정만
    //   소관이라 실 스폰 leg는 파일의 engine/direct_cmd가 진실원).
    {
        using jk::agent::ChatConfig;
        namespace fsx = std::filesystem;

        const std::string exePath = jk::fs::GetExecutablePath();
        const size_t sepF = exePath.find_last_of("\\/");
        const std::string stateDir =
            sepF == std::string::npos ? std::string("state")
                                      : exePath.substr(0, sepF) + "/state";
        const std::string cfgPath = stateDir + "/chat.json";
        // 1n-d와 같은 무손실 백업(엔진 worker의 재로드가 이 파일을 먹는다).
        std::string cfgBackupF;
        bool hadCfgF = false;
        if (std::FILE* bf = std::fopen(cfgPath.c_str(), "rb")) {
            std::fseek(bf, 0, SEEK_END);
            const long sz = std::ftell(bf);
            std::fseek(bf, 0, SEEK_SET);
            if (sz > 0) {
                cfgBackupF.resize(static_cast<size_t>(sz));
                const size_t n =
                    std::fread(&cfgBackupF[0], 1, cfgBackupF.size(), bf);
                cfgBackupF.resize(n);
                hadCfgF = n > 0;
            }
            std::fclose(bf);
        }
        auto WriteCfgF = [&cfgPath](const std::string& json) -> bool {
            std::FILE* f = std::fopen(cfgPath.c_str(), "wb");
            if (!f) return false;
            const size_t w = std::fwrite(json.data(), 1, json.size(), f);
            std::fclose(f);
            return w == json.size();
        };
        auto SeedF = [&WriteCfgF, &exePath](const std::string& directCmd,
                                            const char* engine) -> bool {
            // directory도 시딩 — cfg 기본값은 repo 절대 경로(chdir 실패 함정,
            // 1n-d 렛슨). directCmd는 chat.json JSON 문자열 안에 실리므로
            // 인용·백슬래시를 이중 이스케이프(수기 인게스트 경고 원문 —
            // `directory "I:\\progwork\\JKENGINE"` 계약의 코드판).
            std::string dirJ = exePath;
            const size_t cut = dirJ.find_last_of("\\/");
            if (cut != std::string::npos) dirJ = dirJ.substr(0, cut);
            std::string dirEsc, cmdEsc;
            for (char ch : dirJ) {
                if (ch == '"') dirEsc += "\\\"";
                else if (ch == '\\') dirEsc += "\\\\";
                else dirEsc += ch;
            }
            for (char ch : directCmd) {
                if (ch == '"' || ch == '\\') { cmdEsc += '\\'; cmdEsc += ch; }
                else cmdEsc += ch;
            }
            return WriteCfgF(std::string(
                       "{\"engine\":\"") +
                       engine + "\",\"model\":\"glm-test-stub:cloud\","
                       "\"directory\":\"" + dirEsc + "\",\"direct_cmd\":\"" +
                       cmdEsc + "\"}");
        };

        std::error_code ecF;
        check(fsx::create_directories(stateDir, ecF) || fsx::exists(stateDir),
              "1n-f0 state dir ready (promotion wiring seam)");

        // (a) 순수 파서 도표 — 코드펜스 감쌈·해설 혼입(T1 R4 3회 실측 재현)
        //   + 스키마 검증(스킴 밖 값·무app launch는 정직 폴백 대상).
        {
            jk::ChatAction a;
            std::string note;
            check(jk::ChatLlmActionParse(
                      "```json\n{\"action\":\"launch\",\"app\":\"minesweeper\","
                      "\"text\":\"지뢰찾기를 실행합니다.\"}\n```", a, note) &&
                      a.kind == jk::ChatAction::Launch &&
                      a.app == "minesweeper" &&
                      note.find("지뢰찾기를 실행합니다") != std::string::npos,
                  "1n-f1 code-fenced action JSON parses (fence strip)");
            check(jk::ChatLlmActionParse(
                      "테트리스 열어드릴게요.\n{\"action\":\"launch\","
                      "\"app\":\"tetris\",\"text\":\"테트리스를 실행합니다.\"}"
                      "\n(오래 걸리면 말씀하세요)", a, note) &&
                      a.kind == jk::ChatAction::Launch && a.app == "tetris" &&
                      note.find("테트리스를 실행합니다") != std::string::npos &&
                      note.find("열어드릴게요") != std::string::npos &&
                      note.find("오래 걸리면") != std::string::npos,
                  "1n-f2 JSON buried under commentary parses, both sides ride "
                  "the guide (병기 계약)");
            check(jk::ChatLlmActionParse(
                      "{\"action\":\"talk\",\"text\":\"안녕하세요, 무엇을 도"
                      "와줄까요?\"}", a, note) &&
                      a.kind == jk::ChatAction::Info && a.app.empty() &&
                      note == "안녕하세요, 무엇을 도와줄까요?",
                  "1n-f3 talk=행동 없는 정보성(Info — text만 회신)");
            check(!jk::ChatLlmActionParse("설명만 남긴 해설", a, note),
                  "1n-f4 no JSON at all → parse fail (honest fallback feed)");
            check(!jk::ChatLlmActionParse(
                      "{\"action\":\"minesweeper\",\"app\":\"x\"}", a, note),
                  "1n-f5 unknown action value → parse fail (schema guard)");
            check(!jk::ChatLlmActionParse(
                      "{\"action\":\"launch\"}", a, note) && a.kind ==
                      jk::ChatAction::Info,
                  "1n-f6 launch without app → parse fail (지시 불성립 — 라우터"
                  " 1n-6 동형 판정)");
            check(jk::ChatLlmActionParse(
                      "{\"action\":\"close\"}", a, note) &&
                      a.kind == jk::ChatAction::Close && a.app.empty(),
                  "1n-f7 close without app keeps the argless form (포커스 창 —"
                  " 서버 해소 계약)");
        }

        // (b) 프롬프트 본문 도표 — 트리거 표·별명 표·스키마가 라우터 단일
        //   진실원에서 조립되고, 프리앰블을 복제하지 않는다(엔진 접두 단일
        //   출처 계약 — kLlmTurnPreamble은 BuildEngineCmd가 한 번 붙인다).
        {
            const std::string body = jk::ChatLlmTurnPrompt("지뢰찾기 좀 띄워줘");
            // 무따옴표·무파이프 스키마(ollama leg 재인용 층 실측 — ChatRouter
            // 원장): 본문에 문자 " 와 | 가 없음이 곧 스키마 도표의 정직함.
            check(body.find("action: launch / close / focus / list / talk") !=
                          std::string::npos &&
                      body.find('"') == std::string::npos &&
                      body.find('|') == std::string::npos,
                  "1n-f8 prompt body carries the schema line (quote·pipe "
                  "free — ollama leg 재인용 수리 계약)");
            check(body.find("켜줘") != std::string::npos &&
                      body.find("앞으로 가져와") != std::string::npos &&
                      body.find("닫아줘") != std::string::npos,
                  "1n-f9 prompt body carries the trigger table (router 표 "
                  "승계)");
            check(body.find("지뢰찾기") != std::string::npos &&
                      body.find("minesweeper") != std::string::npos,
                  "1n-f10 prompt body carries the alias table (app key 승계)");
            check(body.find("[발화] 지뢰찾기 좀 띄워줘") != std::string::npos,
                  "1n-f11 prompt body ends with the utterance");
            check(body.find("[시스템 지시]") == std::string::npos,
                  "1n-f12 preamble NOT duplicated in the prompt body "
                  "(engine preprends it once — 복제 금지 단일 출처)");
        }

        // (c) 배선 ① — 정확 트리거 매치는 LLM을 우회한다(TurnSync 0). cfg는
        //   구성(ollama-direct)이고 스폰 스터브는 구분 가능한 Focus JSON을
        //   내놓게 시딩 — LLM이 잘못 불렸다면 kind/app/guide가 전부 오염
        //   된다(오염 관측 = 우회 단정의 무기). guide는 기존 라우터 guide
        //   원문과의 동일 단정(폴백 계약 원문이 같은 검산기를 쓴다).
        //   스터브 echo는 플랫폼 인용 규약 차(cmd는 인용 원문 인쇄, sh는
        //   이중 따옴표 벗김 — 1n-e3 실측 좌표)로 갈린다.
#ifdef _WIN32
        const std::string seedBypass =
            "echo {\"action\":\"focus\",\"text\":\"LLM-TOOK-THE-TURN-3361\"}";
#else
        const std::string seedBypass =
            "echo '{\"action\":\"focus\",\"text\":\"LLM-TOOK-THE-TURN-3361\"}'";
#endif
        check(SeedF(seedBypass, "ollama-direct"),
              "1n-f13 chat.json seeded (llm focus marker, ollama-direct)");
        {
            ChatConfig mc;               // 구성 — 엔진이 살아 있어야 우회가
            mc.engine = "ollama-direct"; // 의미를 갖는다. fileKnown은 구성의
            mc.fileKnown = true;         // 표지(파일 부재=미구성 — 1n-f29)
            mc.directCmd = seedBypass;
            jk::ChatAction a;
            bool used = true;            // 오염 증거 — 배선이 지우지 못하면 실패
            const std::string guide =
                jk::ChatRouteTurn("지뢰찾기 켜줘", a, mc, &used);
            check(!used && a.kind == jk::ChatAction::Launch &&
                      a.app == "minesweeper",
                  "1n-f14 exact trigger match bypasses the LLM turn (cfg "
                  "구성에도 — usedLlm=false)");
            jk::ChatAction ar;
            check(guide == jk::ChatRouterRoute("지뢰찾기 켜줘", ar),
                  "1n-f15 bypass guide is the legacy router guide verbatim");
        }

        // (d) 배선 ③ — cfg 미구성(engine="stub"·미지 값)은 LLM 왕복·스포른
        //   없이 즉발 폴백(기존 라우터 원문 그대로 — 지연 0 계약).
        {
            ChatConfig mc;          // 미구성 — 스폰 자체를 안 한다. fileKnown
            mc.engine = "stub";     // 세움 = engine "값" 게이트만을 대상(표지
            mc.fileKnown = true;    // 계약은 1n-f29에서 별도 단정).
            jk::ChatAction a;
            bool used = true;
            const std::string guide = jk::ChatRouteTurn(
                "세상엔 채팅이 이렇게 어려웠나", a, mc, &used);
            check(!used && a.kind == jk::ChatAction::Info,
                  "1n-f16 unconfigured cfg (engine=stub) falls back with no "
                  "LLM turn");
            jk::ChatAction ar;
            check(guide == jk::ChatRouterRoute("세상엔 채팅이 이렇게 어려웠나",
                                               ar),
                  "1n-f17 fallback guide is the legacy router guide verbatim");
            check(guide.find("인식하지 못했습니다") != std::string::npos &&
                      guide.find("켜줘") != std::string::npos,
                  "1n-f18 fallback is the stub InfoGuide (trigger table "
                  "안내문 원문)");
            ChatConfig mb;               // 미지 값도 미구성 — 같은 즉발 폴백
            mb.engine = "banana-wasm";   // (값 게이트 — 표지는 세우고 검증)
            mb.fileKnown = true;
            jk::ChatAction b;
            bool usedB = true;
            (void)jk::ChatRouteTurn("뜬금없는 말 3361", b, mb, &usedB);
            check(!usedB && b.kind == jk::ChatAction::Info,
                  "1n-f19 unknown engine value = unconfigured (same "
                  "no-spawn fallback)");
        }

        // (e) NR4-1 (T4 fix r1) — 파일 부재·engine 키 누락 = 미구성(승격은
        //   opt-in 가법 — 컨트롤러 룰링). ChatConfig.engine의 구조 기본값은
        //   "ollama"(jkbridge/jkchat 원계약)라 값만 보면 파일 부재 기기를
        //   구성으로 오인한다 — fileKnown 표지가 문을 잠근 것을 단정한다.
        //   (a) 부재: 파일 소각 → LoadChatConfig → 표지 false·기본값 무변 →
        //   비매치 발화 스폰 0건 + stub 안내문. (b) 키 누락 파일: 표지 false.
        //   (c) 구성 파일: 표지 true → 같은 발화가 TurnSync 시도(스터브 마커
        //   회신의 usedLlm=true가 시도의 영수증).
        {
            fsx::remove(cfgPath, ecF);
            const ChatConfig absentF = jk::agent::LoadChatConfig();
            check(!absentF.fileKnown && absentF.engine == "ollama",
                  "1n-f29 chat.json absent → fileKnown=false (engine 기본값 "
                  "원계약 무변 — jkbridge/jkchat 소비자 영향 0)");
            check(!jk::ChatLlmEngineConfigured(absentF),
                  "1n-f30 file-absent cfg = unconfigured (승격 opt-in "
                  "가법 — NR4-1)");
            jk::ChatAction a;
            bool used = true;            // 오염 증거 — 스폰하면 지워지지 않음
            const std::string guide =
                jk::ChatRouteTurn("세상엔 채팅이 이렇게 어려웠나", a,
                                  absentF, &used);
            check(!used && a.kind == jk::ChatAction::Info,
                  "1n-f31 file-absent non-match utterance spawns 0 LLM "
                  "turns (stub fallback)");
            jk::ChatAction ar;
            check(guide == jk::ChatRouterRoute("세상엔 채팅이 이렇게"
                                               " 어려웠나", ar),
                  "1n-f32 file-absent fallback guide is the legacy router "
                  "guide verbatim");
            check(WriteCfgF("{\"model\":\"glm-mute:cloud\"}"),
                  "1n-f33 chat.json seeded without engine key");
            const ChatConfig noKeyF = jk::agent::LoadChatConfig();
            check(!noKeyF.fileKnown && !jk::ChatLlmEngineConfigured(noKeyF),
                  "1n-f34 engine key missing = unconfigured (engine 키 실제 "
                  "구성 계약 — NR4-1)");
            jk::ChatAction b;
            bool usedB = true;
            (void)jk::ChatRouteTurn("뜬금없는 말 3361", b, noKeyF, &usedB);
            check(!usedB && b.kind == jk::ChatAction::Info,
                  "1n-f35 engine-key-missing non-match spawns 0 LLM turns");
            // (c) 구성된 cfg의 같은 발화 → TurnSync 실제 시도 — 시딩 파일로
            //   LoadChatConfig을 재로드(진실원이 실 파일임을 함께 단정)하고
            //   스터브 마커 회신의 usedLlm=true가 시도의 영수증이다.
#ifdef _WIN32
            const std::string seedOptIn =
                "echo {\"action\":\"talk\",\"text\":\"OPT-IN-3361\"}";
#else
            const std::string seedOptIn =
                "echo '{\"action\":\"talk\",\"text\":\"OPT-IN-3361\"}'";
#endif
            check(SeedF(seedOptIn, "ollama-direct"),
                  "1n-f36 chat.json re-seeded (engine key back, stub marker)");
            const ChatConfig fromFileF = jk::agent::LoadChatConfig();
            check(fromFileF.fileKnown &&
                      jk::ChatLlmEngineConfigured(fromFileF),
                  "1n-f37 LoadChatConfig raises fileKnown for an engine-keyed "
                  "file (gate opens — NR4-1의 반쪽)");
            jk::ChatAction c;
            bool usedC = false;
            const std::string guideC =
                jk::ChatRouteTurn("아무 말 3361", c, fromFileF, &usedC);
            check(usedC && c.kind == jk::ChatAction::Info &&
                      guideC == "OPT-IN-3361",
                  "1n-f38 configured-from-file non-match attempts the "
                  "TurnSync (stub marker = attempt receipt)");
        }

        // (e) 배선 ② — 비매치 + cfg 구성 → TurnSync 동기 턴 → 행동 JSON 파싱
        //   → ChatAction. 스폰 스터브(echo)가 모델 응답 모양의 행동 JSON을
        //   내놓는 실측 자리(실 ollama 의존 0).
#ifdef _WIN32
        const std::string seedLaunch =
            "echo {\"action\":\"launch\",\"app\":\"tetris\","
            "\"text\":\"TETRIS-LLM-GUIDE-3361\"}";
#else
        const std::string seedLaunch =
            "echo '{\"action\":\"launch\",\"app\":\"tetris\","
            "\"text\":\"TETRIS-LLM-GUIDE-3361\"}'";
#endif
        check(SeedF(seedLaunch, "ollama-direct"),
              "1n-f20 chat.json re-seeded (launch JSON stub)");
        {
            ChatConfig mc;
            mc.engine = "ollama-direct";
            mc.fileKnown = true;   // 구성 표지 — 게이트 개통(1n-f36 반쪽)
            mc.directCmd = seedLaunch;
            jk::ChatAction a;
            bool used = false;
            const std::string guide =
                jk::ChatRouteTurn("테트리스 좀 부탁할게", a, mc, &used);
            check(used && a.kind == jk::ChatAction::Launch &&
                      a.app == "tetris",
                  "1n-f21 non-match + configured cfg → LLM turn parses into "
                  "a Launch action");
            check(guide == "TETRIS-LLM-GUIDE-3361",
                  "1n-f22 model text becomes the guide (안내문 병기 계약)");
        }

        // (f) 배선 ③ — 스폰은 개통돼도 응답 파싱이 불성립하면 stub 안내문
        //   폴백. T3 stub 엔진 회신 모양(kStubShellCmd* 단일 출처 상수 —
        //   "result"/"session_id"는 행동 스키마 밖)으로 정직 폴백을 증명.
#ifdef _WIN32
        check(SeedF(jk::agent::kStubShellCmdWin32, "ollama-direct"),
              "1n-f23 chat.json re-seeded (stub-echo JSON, no action "
              "schema)");
#else
        check(SeedF(jk::agent::kStubShellCmdPosix, "ollama-direct"),
              "1n-f23 chat.json re-seeded (stub-echo JSON, no action "
              "schema)");
#endif
        {
            ChatConfig mc;
            mc.engine = "ollama-direct";
            mc.fileKnown = true;   // 개통돼야 턴이 시도되고 파싱이 실패한다
            jk::ChatAction a;
            bool used = true;
            const std::string guide =
                jk::ChatRouteTurn("뜬금없는 발화 3361", a, mc, &used);
            check(!used && a.kind == jk::ChatAction::Info,
                  "1n-f24 unparsable engine reply falls back honestly");
            jk::ChatAction ar;
            check(guide == jk::ChatRouterRoute("뜬금없는 발화 3361", ar),
                  "1n-f25 fallback guide is the legacy router guide verbatim");
        }

        // (g) 배선 ② talk — 행동 없는 정보성 응답은 text만 회신(스펙 결정 4
        //   "정보성 응답에는 모델 텍스트를 안내문으로") — 도구 지시 0.
#ifdef _WIN32
        const std::string seedTalk =
            "echo {\"action\":\"talk\",\"text\":\"TALK-GUIDE-3361\"}";
#else
        const std::string seedTalk =
            "echo '{\"action\":\"talk\",\"text\":\"TALK-GUIDE-3361\"}'";
#endif
        check(SeedF(seedTalk, "ollama-direct"),
              "1n-f26 chat.json re-seeded (talk JSON stub)");
        {
            ChatConfig mc;
            mc.engine = "ollama-direct";
            mc.fileKnown = true;   // 구성 표지 — 턴 시도 계약(1n-f37 좌표)
            jk::ChatAction a;
            bool used = false;
            const std::string guide =
                jk::ChatRouteTurn("오늘 기분은 어때", a, mc, &used);
            check(used && a.kind == jk::ChatAction::Info && a.app.empty(),
                  "1n-f27 talk turn stays on Info (no tool dispatch)");
            check(guide == "TALK-GUIDE-3361",
                  "1n-f28 talk reply text is the whole guide");
        }

        // 복원 — 시딩한 chat.json 정리(없던 기기는 소각, 있던 기기는 원문).
        if (hadCfgF) {
            check(WriteCfgF(cfgBackupF), "1n-fz chat.json restored verbatim");
        } else {
            fsx::remove(cfgPath, ecF);
            check(!ecF && !fsx::exists(cfgPath),
                  "1n-fz seeded chat.json removed (scratch-free teardown)");
        }
    }

    // 1p) 프레임 더티 계산기(T1 — 스펙 2026-10-08-dirty-present 설계 결정 4:
    // 순수 로직 단정). 렌더러 생성 0 — 합집합/매핑/역치만 SDL_Rect 타입 위에서
    // 단정한다. 케이스 6종(plan verbatim): 매핑(+스케일 2.0)·합집합 병합·
    // 역치(full 전환)·빈=TakeDirty false·AddLayerMove 이전∪새·ForceFull.
    {
        // 1p-1) 매핑(+스케일 2.0): 표면 100x50, 스케일 2.0, 레이어 화면 원점
        // (40, 60) — 표면 rect {10,20,20,10} → 화면 {60,100,40,20}.
        server::FrameDirtyAccumulator acc(800, 600);
        acc.AddSurfaceRect(7, 100, 50, 2.0f, 2.0f, 40, 60,
                           ipc::DirtyRect{10, 20, 20, 10});
        std::vector<SDL_Rect> out;
        check(acc.TakeDirty(out) && out.size() == 1 && out[0].x == 60 &&
                  out[0].y == 100 && out[0].w == 40 && out[0].h == 20,
              "1p-1 매핑(+스케일 2.0) 표면rect→화면rect");
        // 오버플레이·화면 경계 클램프: 표면을 초과하는 dirty는 레이어 dst까지
        // 절단(레이어 바깥 화면 면적을 부채질하지 않는다).
        server::FrameDirtyAccumulator over(800, 600);
        over.AddSurfaceRect(7, 100, 50, 2.0f, 2.0f, 40, 60,
                            ipc::DirtyRect{0, 0, 200, 200});
        check(over.TakeDirty(out) && out.size() == 1 && out[0].x == 40 &&
                  out[0].y == 60 && out[0].w == 200 && out[0].h == 100,
              "1p-1b 오버플레이·화면 경계 클램프(dst 절단)");
        // 반올림 좌표(스펙: 스케일 매핑 = 반올림) — 스케일 1.5, 원점 (0,0):
        // {3,5,4,6} → {lround(4.5)=5, lround(7.5)=8, 6, 9}(half-away 반올림).
        server::FrameDirtyAccumulator rnd(800, 600);
        rnd.AddSurfaceRect(7, 100, 50, 1.5f, 1.5f, 0, 0,
                           ipc::DirtyRect{3, 5, 4, 6});
        check(rnd.TakeDirty(out) && out.size() == 1 && out[0].x == 5 &&
                  out[0].y == 8 && out[0].w == 6 && out[0].h == 9,
              "1p-1c 반올림 좌표 매핑(스케일 1.5, lround)");

        // 1p-2) 합집합 병합: 인접+중첩은 하나로 뭉치고, 떨어진 rect는 유지.
        server::FrameDirtyAccumulator merge(800, 600);
        merge.AddDirtyLayerRect(1, {0, 0, 100, 100});
        merge.AddDirtyLayerRect(2, {100, 0, 100, 100});  // 인접(1px 접촉)
        merge.AddDirtyLayerRect(3, {50, 50, 100, 100});  // 중첩
        merge.AddDirtyLayerRect(4, {500, 500, 10, 10});  // 떨어짐
        check(merge.TakeDirty(out) && out.size() == 2 &&
                  out[0].x == 0 && out[0].y == 0 && out[0].w == 200 &&
                  out[0].h == 150 && out[1].x == 500 && out[1].y == 500 &&
                  out[1].w == 10 && out[1].h == 10,
              "1p-2 합집합 병합(인접·중첩 정리, 이격 유지)");

        // 1p-3) 역치(full 전환): 40% 경계 — 미만은 부분 rect 제시, 도달은 full.
        server::FrameDirtyAccumulator thr(100, 100);
        thr.AddDirtyLayerRect(1, {0, 0, 39, 99});  // 3861/10000 = 38.6% < 40%
        check(!thr.IsFull(), "1p-3a 역치 미만 = IsFull false");
        check(thr.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 39 && out[0].h == 99,
              "1p-3b 역치 미만 = 부분 rect 제시(제시 유지)");
        thr.AddDirtyLayerRect(1, {0, 0, 50, 80});  // 4000/10000 = 정확 40%
        check(thr.IsFull(), "1p-3c 누적 40% 도달 = full 전환(≥ 경계)");
        check(thr.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 100 && out[0].h == 100,
              "1p-3d full 전환 = 화면 전체 rect 단건");
        check(!thr.TakeDirty(out), "1p-3e TakeDirty 후 재초기화(빈=false)");

        // 1p-4) 빈=TakeDirty false(제시 스킵 원료) — 오염된 out도 비운다.
        server::FrameDirtyAccumulator none(800, 600);
        out.push_back(SDL_Rect{9, 9, 1, 1});
        check(!none.TakeDirty(out) && out.empty(),
              "1p-4 빈 계산기 = TakeDirty false(out 비움)");
        none.ForceFull();
        check(none.TakeDirty(out) && out.size() == 1 &&
                  out[0].w == 800 && out[0].h == 600,
              "1p-4b 사건 0 + ForceFull = full rect(강제는 사건 무관)");

        // 1p-5) AddLayerMove = 이전∪새(이동 궤적 양쪽 착지).
        server::FrameDirtyAccumulator mv(800, 600);
        mv.AddLayerMove(5, {0, 0, 10, 10}, {50, 50, 10, 10});
        bool sawOld = false, sawNew = false;
        check(mv.TakeDirty(out) && out.size() == 2,
              "1p-5a AddLayerMove = 이전∪새 두 후보 rect");
        for (const SDL_Rect& r : out) {
            if (r.x == 0 && r.y == 0 && r.w == 10 && r.h == 10) sawOld = true;
            if (r.x == 50 && r.y == 50 && r.w == 10 && r.h == 10) sawNew = true;
        }
        check(sawOld && sawNew, "1p-5b 이전 dst·새 dst 모두 이동 사건에 있음");
        // 인접 dst 무브는 합집합 정리로 한 rect에 뭉친다.
        server::FrameDirtyAccumulator mv2(800, 600);
        mv2.AddLayerMove(5, {0, 0, 16, 16}, {16, 0, 16, 16});
        check(mv2.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 32 && out[0].h == 16,
              "1p-5c 인접 dst 무브 = 단건 병합(이전∪새)");

        // 1p-6) ForceFull: 포커스 재정렬/오버레이 훅 — 작은 rect에도 전체로.
        server::FrameDirtyAccumulator ff(800, 600);
        check(!ff.IsFull(), "1p-6a 초기 IsFull false");
        ff.AddDirtyLayerRect(1, {0, 0, 4, 4});
        ff.ForceFull();
        check(ff.IsFull(), "1p-6b ForceFull = IsFull true");
        check(ff.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 800 && out[0].h == 600,
              "1p-6c ForceFull = 화면 전체 rect 단건(사건 상쇄 정리)");

        // 1p-7) 커밋 배선 수집(T2): posix case 18 쌍둥이 — CommitSurface가
        // 폐기하던 DirtyRect[]를 보류 큐→Composite 소비 AddSurfaceRect 매핑으로
        // 흘리는 배선의 계산기 단면(스펙 결정 2 "와이어 원용"). SDL 렌더러
        // 없이 단정 — SW 부분 업로드 본체는 기계 실측.
        server::FrameDirtyAccumulator wired(800, 600);
        const jk::ipc::DirtyRect commitRects[3] = {
            jk::ipc::DirtyRect{0, 0, 10, 10}, jk::ipc::DirtyRect{5, 5, 10, 10},
            jk::ipc::DirtyRect{30, 40, 20, 15}};
        for (const jk::ipc::DirtyRect& dr : commitRects) {
            wired.AddSurfaceRect(11, 120, 80, 1.0f, 1.0f, 10, 20, dr);
        }
        check(wired.TakeDirty(out) && out.size() == 2 &&
                  out[0].x == 10 && out[0].y == 20 && out[0].w == 15 &&
                  out[0].h == 15 &&  // {0,0,10,10}∪{5,5,10,10} 병합
                  out[1].x == 40 && out[1].y == 60 && out[1].w == 20 &&
                  out[1].h == 15,
              "1p-7 커밋 DirtyRect[] 수집→매핑→병합 목록(rect 개수 = out.size())");
        // 배선 불변의 반쪽: 커밋 rect가 없는 dirty 레이어(이동·alpha 등)는 dst
        // 전체 봉합 — AddDirtyLayerRect(이미 화면 좌표 계약).
        server::FrameDirtyAccumulator fallback(800, 600);
        fallback.AddDirtyLayerRect(11, {10, 20, 120, 80});
        check(fallback.TakeDirty(out) && out.size() == 1 &&
                  out[0].x == 10 && out[0].y == 20 && out[0].w == 120 &&
                  out[0].h == 80,
              "1p-7b 커밋 rect 없는 dirty = dst 전체 봉합(배선 불변 반쪽)");
        server::FrameDirtyAccumulator two(800, 600);
        two.AddSurfaceRect(11, 120, 80, 1.0f, 1.0f, 10, 20,
                           jk::ipc::DirtyRect{0, 0, 10, 10});
        two.AddSurfaceRect(12, 120, 80, 2.0f, 2.0f, 100, 100,
                           jk::ipc::DirtyRect{0, 0, 10, 10});
        check(two.TakeDirty(out) && out.size() == 2,
              "1p-7c 레이어 2종의 커밋 rect가 같은 프레임에 누적(레이어별 매핑)");

        // 1p-8) T1 리뷰 F1 승계: 역치 면적 = 병합 목록 총합(중복 가산 아님).
        // 원장 산치(fix r1 NT2-2 표기 정정 — 케이스 로직 무변경): 가산 5000
        // ≥ 4000 = full이어야 답하나 **병합 목록 총합 3600**(rect 2건이 병합
        // 되어 목록에 오르는 것은 merged-bbox {0,0,60,60} = 3600 / **정확
        // 합집합 3400보다 bbox 과대 — full 조기 보수 방향**) < 4000 = IsFull
        // false. 정확 합집합 3400 = 5000 − 중첩 1600 → 오프셋 (10,10)의
        // {10,10,50,50}(원장 표기 {30,30,50,50}은 중첩 400·합집합 4600이어서
        // 산치와 어긋남 — T2 리포트 concern 원장 정정).
        server::FrameDirtyAccumulator f1(100, 100);
        f1.AddDirtyLayerRect(1, {0, 0, 50, 50});
        f1.AddDirtyLayerRect(2, {10, 10, 50, 50});
        check(!f1.IsFull(),
              "1p-8a 가산 5000이어도 병합 목록 총합 3600 = IsFull false(F1 승계)");
        check(f1.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 60 && out[0].h == 60,
              "1p-8b 병합 총합 <역치 = 부분 제시(bbox 병합 목록 유지)");
        server::FrameDirtyAccumulator f2(100, 100);
        f2.AddDirtyLayerRect(1, {0, 0, 50, 50});
        f2.AddDirtyLayerRect(2, {0, 50, 50, 50});  // 인접·합집합 5000
        check(f2.IsFull(), "1p-8c 병합 총합 5000(=합집합) = full 전환(중첩 없음)");
    }

    // 2t) 텍스트 배율 결선 산치 (T2 — 스펙 2026-10-09-phone-text-scale):
    // posix에서 셀 기하만 커지고 글리프는 비트맵 고정이던 결함(T1 원장 §결론
    // 후보 ⑤)의 수리 부품을 순수 부품으로 잠근다. SDL 렌더러 0. GetCellMetrics
    // 는 settings.json을 exe-dir 직독하므로(환경 의존 — 엔진 런타임 진실원)
    // 여기선 단정 금지: 쌍둥이는 ComputeCellMetrics(명시 스케일)과 소자 기본
    // 상수(DefaultFontScale), 확대 매핑 헬퍼만 잠근다.
    {
        // 2t-a) ComputeCellMetrics(1.5f) 산술 — posix 소자 기본 배율의 셀
        // 격자(hanW=2×engW 불변식 — docs/65 O4).
        const jk::text::CellMetrics c15 = jk::text::ComputeCellMetrics(1.5f);
        check(c15.engW == 12 && c15.hanW == 24 && c15.cellH == 24,
              "2t-a ComputeCellMetrics(1.5) == {12,24,24} (hanW=2×engW)");
        // 2t-b) 미설정 기본 배율 플랫폼 단정 — DefaultFontScale은 컴파일타임
        // 플랫폼 상수(스펙 사용자 확정: posix/WSL/폰 축 1.5, Win 축 1.0).
        // 쌍둥이는 각자 기대 상수로 단정한다(하나의 케이스, 플랫폼별 진실).
#if defined(_WIN32)
        check(jk::text::DefaultFontScale() == 1.0f,
              "2t-b Windows 미설정 기본 배율 = 1.0 (픽셀동일 계약 승계)");
        const jk::text::CellMetrics cd =
            jk::text::ComputeCellMetrics(jk::text::DefaultFontScale());
        check(cd.engW == 8 && cd.hanW == 16 && cd.cellH == 16,
              "2t-c Windows 미설정 셀 = 비트맵 격자 {8,16,16} (무변)");
#else
        check(jk::text::DefaultFontScale() == 1.5f,
              "2t-b posix 미설정 기본 배율 = 1.5 (스펙 사용자 확정)");
        const jk::text::CellMetrics cd =
            jk::text::ComputeCellMetrics(jk::text::DefaultFontScale());
        check(cd.engW == 12 && cd.hanW == 24 && cd.cellH == 24,
              "2t-c posix 미설정 셀 = {12,24,24} (글리프도 함께 1.5)");
#endif
        // 2t-d) 확대 매핑 항등 — 소스 스팬 == 목표 스팬이면 모든 샘플이 자기
        // 자신(Windows 폴백 픽셀동일 단정의 산치: s=1.0 셀 8x16/16x16에서
        // srcW=8/16·srcH=8/16 → 매핑 항등).
        check(jk::text::StretchNearestIndex(0, 8, 8) == 0 &&
                  jk::text::StretchNearestIndex(7, 8, 8) == 7 &&
                  jk::text::StretchNearestIndex(15, 16, 16) == 15,
              "2t-d 확대 매핑 항등(스팬 동일 = 샘플 그대로, s=1.0 픽셀동일)");
        // 2t-e) 1.5 확대 샘플: src 8 → dst 12 = idx*8/12; 세로 밴드 가교
        // (8행 소스 ×1.5 → 24 — 8x8 폴백의 cellH/2 밴드 매핑).
        check(jk::text::StretchNearestIndex(0, 8, 12) == 0 &&
                  jk::text::StretchNearestIndex(3, 8, 12) == 2 &&
                  jk::text::StretchNearestIndex(5, 8, 12) == 3 &&
                  jk::text::StretchNearestIndex(11, 8, 12) == 7 &&
                  jk::text::StretchNearestIndex(23, 8, 24) == 7,
              "2t-e 1.5 확대 샘플 = idx*src/dst (nearest 격자)");
        // 2t-f) 2.0 확대 샘플 + 병적 입력 방어선(0/1-스팬 수렴).
        check(jk::text::StretchNearestIndex(0, 8, 16) == 0 &&
                  jk::text::StretchNearestIndex(15, 8, 16) == 7 &&
                  jk::text::StretchNearestIndex(1, 8, 1) == 7 &&
                  jk::text::StretchNearestIndex(-1, 8, 8) == 0,
              "2t-f 2.0 확대 샘플 + 병적 입력 첫 샘플 수렴(방어선)");
        // 3b) 크롬 타이틀 밴드 높이 산식 (T3 — 스펙 2026-10-09-phone-text-scale
        // 결정 1): 현행 상수 24 = 비트맵 셀 16 + 여백 8(4+4)의 합이라는 원문
        // 실측의 산치를 순수 부품으로 잠근다. 소비처 양축 — JKWindow.cpp
        // kTitle(클라 표면 안의 밴드 그리기)·JKWindowServer.cpp 히트테스트 존+
        // 승인 배너 밴드 두께(서버 크롬 — "MUST stay in sync").
        check(jk::text::ComputeChromeTitleBarHeight(16) == 24,
              "3b-a s=1.0 셀 16 → 밴드 24 (현행 상수와 정확 등호 — "
              "Windows 창 타이틀 픽셀동일 산치)");
        check(jk::text::ComputeChromeTitleBarHeight(24) == 32,
              "3b-b posix 기본 1.5 셀 24 → 밴드 32 (1.5 타이틀 글리프 클립 방지)");
        check(jk::text::ComputeChromeTitleBarHeight(8) == 24 &&
                  jk::text::ComputeChromeTitleBarHeight(48) == 56,
              "3b-c 최소 현행값 24 보장(하단 방어선) + 상단 s=3.0 셀 48 → 56");
        // (T3 fix r1) 앱 본문 상단 오프셋 = 밴드 산식 + 본문 여백 6 — ImGui
        // 앱 9곳+minesweeper의 고정 리터럴(topY 30·밴드 24)의 분해가 소비
        // 소스로 모였다. s=1.0 항등 단정 — 교체 후 Windows 무변의 산치.
        check(jk::text::ComputeAppContentTopOffset(16) == 30,
              "3b-d s=1.0 앱 상단 오프셋 = 밴드 24 + 여백 6 = 30 "
              "(기존 ImGui topY 리터럴과 정확 등호 — Windows 무변 산치)");
        check(jk::text::ComputeAppContentTopOffset(24) == 38,
              "3b-e posix 기본 1.5 앱 상단 오프셋 38 — 본문이 밴드 32와 "
              "겹치지 않는다(I-2 상단 사각지대 해소 산치)");
        // KSSM 쌍 폴백은 반올림 좌표에서 4/9px 오차(8→15px 등 홀수 폭)를
        // 허용한다 — 비트맵 폴백 한계(스펙 fail-safe 명시; 벡터 아틀라스가
        // 정상 경로). 단정치 않고 수용 계약만 여기에 기록한다.
    }

    // 2g) 갤러리 순수 부품 (T1 — 스펙 2026-10-09-gallery-design): 모듈 골격의
    // 리졸버/산치/열거/캐시 키를 헤더(jk::gallery, apps/GalleryModel.h) 직소비
    // 로 잠근다. 썸네일 디코드·디스크 캐시(T3)가 같은 헤더를 소비한다.
    {
        namespace fs = std::filesystem;
        using jk::gallery::GalleryDirList;
        // 2g-a) 순수 리졸버 — settings 없음(빈 텍스트) → 기본 1건
        // {exeDir/state/screenshots}(fail-safe); gallery.dirs 배열 2건 →
        // 기본이 앞(촬영 원전)·유저 폴더 뒤; 기본 중복 유저 경로는 제거.
        const std::vector<std::string> noSettings =
            GalleryDirList("X:/exe", "");
        check(noSettings.size() == 1 &&
                  noSettings[0] == "X:/exe/state/screenshots",
              "2g-a settings 없음 = 기본 1건(fail-safe)");
        const std::vector<std::string> withDirs = GalleryDirList(
            "X:/exe",
            R"({"gallery":{"dirs":["P:/pics","Q:/cam"]}})");
        check(withDirs.size() == 3 && withDirs[0] == "X:/exe/state/screenshots" &&
                  withDirs[1] == "P:/pics" && withDirs[2] == "Q:/cam",
              "2g-a dirs 2건 = 기본이 앞+유저 순서 보존");
        const std::vector<std::string> dup = GalleryDirList(
            "X:/exe",
            R"({"gallery":{"dirs":["X:/exe/state/screenshots","P:/pics"]}})");
        check(dup.size() == 2 && dup[0] == "X:/exe/state/screenshots" &&
                  dup[1] == "P:/pics",
              "2g-a 기본 중복 유저 경로 제거(첫 등장 유지)");
        const std::vector<std::string> broken = GalleryDirList(
            "X:/exe", R"json({"gallery":{"dirs":[)json");
        check(broken.size() == 1 && broken[0] == "X:/exe/state/screenshots",
              "2g-a 파손 settings = 기본 1건(fail-safe)");
        const std::vector<std::string> notArray =
            GalleryDirList("X:/exe", R"({"gallery":{"dirs":"P:/pics"}})");
        check(notArray.size() == 1 && notArray[0] == "X:/exe/state/screenshots",
              "2g-a dirs 비배열 = 무시, 기본 1건(원문 보존 소비)");
        const std::vector<std::string> emptyExe = GalleryDirList("", "");
        check(emptyExe.size() == 1 && emptyExe[0] == "state/screenshots",
              "2g-a exeDir 빈값 = 상대 기본 1건(jk::fs 빈값 계약 승계)");

        // 2g-b) 경로 정규화 — 뒤 구분자 중복 제거·빈 성분 제거·첫 등장 유지.
        // (backslash 입력은 Win 원문 계약 — posix는 '\'를 성분 문자로 받는다
        // 이므로 이 쌍은 Win 축 소유.)
#if defined(_WIN32)
        const std::vector<std::string> norm = jk::gallery::NormalizeDirs(
            {"C:/a//", "c:\\b\\", "", "//", "C:/a"});
        check(norm.size() == 2 && norm[0] == "C:/a" && norm[1] == "c:/b",
              "2g-b 뒤 구분자 중복+backslash 정규화+빈 성분 제거(win)");
#else
        const std::vector<std::string> norm = jk::gallery::NormalizeDirs(
            {"srv/share//", "", "/", "srv/share"});
        check(norm.size() == 1 && norm[0] == "srv/share",
              "2g-b 뒤 구분자 중복+빈 성분 제거+첫 등장 유지(posix)");
#endif

        // 2g-c) 썸네일 박스 산치 — fitThumb(w,h,maxW,maxH)=배율 산치(s=1.0=
        // 원본, min 축 지배 = 극단 종횡비 포함).
        const jk::gallery::FitSize same = jk::gallery::FitThumb(160, 120, 160, 120);
        check(same.w == 160.f && same.h == 120.f,
              "2g-c 정합 입력 = s 1.0(원본)");
        const jk::gallery::FitSize small = jk::gallery::FitThumb(80, 60, 160, 120);
        check(small.w == 80.f && small.h == 60.f,
              "2g-c 작은 원본 = 확대 금지(s 1.0 상한)");
        const jk::gallery::FitSize big = jk::gallery::FitThumb(3200, 2400, 160, 120);
        check(big.w == 160.f && big.h == 120.f,
              "2g-c 큰 원본 = 박스 정합 축소");
        const jk::gallery::FitSize wide = jk::gallery::FitThumb(10000, 10, 160, 120);
        check(wide.w == 160.f && std::abs(wide.h - 0.16f) < 1e-3f,
              "2g-c 극단 가로 종횡비 = min 축 지배(비율 유지)");
        const jk::gallery::FitSize tall = jk::gallery::FitThumb(10, 10000, 160, 120);
        check(std::abs(tall.w - 0.12f) < 1e-3f && tall.h == 120.f,
              "2g-c 극단 세로 종횡비 = min 축 지배(비율 유지)");
        const jk::gallery::FitSize deg = jk::gallery::FitThumb(0, 100, 160, 120);
        check(deg.w == 0.f && deg.h == 0.f, "2g-c 퇴화 입력 = {0,0}");

        // 2g-d) FNV-1a 캐시 키 — 참조 벡터 대신 결정론성+충돌 부재 3쌍 단정
        // (T3 디스크 캐시의 키 전제: 축 무관 결정론). basis 항등(빈 문자열 =
        // offset basis)은 구조 상수 단정(구현 식 검증).
        check(jk::gallery::Fnv1a("a.png") == jk::gallery::Fnv1a("a.png"),
              "2g-d 결정론성(같은 입력 = 같은 해시)");
        check(jk::gallery::Fnv1a("shot_1.png") != jk::gallery::Fnv1a("shot_2.png") &&
                  jk::gallery::Fnv1a("p1.jpg") != jk::gallery::Fnv1a("p1.jpeg") &&
                  jk::gallery::Fnv1a("a/b.png") != jk::gallery::Fnv1a("a/b.png "),
              "2g-d 충돌 부재 3쌍(대쉬 1문자·형제 확장자·꼬리 공백)");
        check(jk::gallery::Fnv1a("") == 2166136261u,
              "2g-d 빈 문자열 = offset basis(구조 상수)");

        // 2g-e) 폴더 열거 — 없는 폴더=목록 비움+ok false, 빈 폴더=비움+ok true
        // (ec 중립형 — ListImageFiles 원문 계약), 실제 열거는 최신순 정렬+
        // 비이미지/디렉터리 성분 스킵. 임시 폴더에서 실측(사후 소각).
        const fs::path gdir = fs::temp_directory_path() / "jk_gallery_selftest";
        fs::remove_all(gdir);
        {
            std::vector<std::string> out;
            bool ok = true;
            out = jk::gallery::ListImageFiles((gdir / "missing").string(), &ok);
            check(out.empty() && !ok, "2g-e 없는 폴더 = 목록 비움+ok false");
            fs::create_directories(gdir);
            out = jk::gallery::ListImageFiles(gdir.string(), &ok);
            check(out.empty() && ok, "2g-e 빈 폴더 = 목록 비움+ok true");

            std::ofstream(gdir / "b.png").put('x');
            std::ofstream(gdir / "a.png").put('x');
            std::ofstream(gdir / "c.txt").put('x');
            fs::create_directories(gdir / "sub");
            std::ofstream(gdir / "sub" / "in.png").put('x');  // 서브디렉터리 성분 스킵
            // mtime을 명시 세트 — 플랫폼 시계 분해능 무관한 최신순 단정.
            const auto later = fs::file_time_type::clock::now() +
                               std::chrono::hours(1);
            fs::last_write_time(gdir / "b.png", later);
            out = jk::gallery::ListImageFiles(gdir.string(), &ok);
            check(ok && out.size() == 2 && out[0] == "b.png" && out[1] == "a.png",
                  "2g-e 열거 = 최신순(mtime desc)+비이미지/서브디렉터리 스킵");
            fs::remove_all(gdir);
        }

        // 2g-f) 전체 보기 핏 산치 (T2 — 스펙 결정 3 "핏 표시"): FitFull(w,h,
        // vpW,vpH) = min 축 지배 순수 비율. 썸네일 산치(2g-c)와 달리 s=1.0
        // 상한이 없다(작은 원본 확대 허용 — shot 표시 수형 동형), 화면 배율/
        // 폰트 스케일 상태와 무관한 순수 비율 계산(fit-scale 함정 원장 존중).
        const jk::gallery::FitSize half =
            jk::gallery::FitFull(1920, 1080, 960, 540);
        check(half.w == 960.f && half.h == 540.f,
              "2g-f 절반 축소 = 뷰포트 정합(s=min 축 지배)");
        const jk::gallery::FitSize up = jk::gallery::FitFull(80, 60, 160, 120);
        check(up.w == 160.f && up.h == 120.f,
              "2g-f 작은 원본 = 확대 허용(s 상한 부재 — 2g-c와 반대 수형)");
        const jk::gallery::FitSize fwide =
            jk::gallery::FitFull(10000, 10, 160, 120);
        check(fwide.w == 160.f && std::abs(fwide.h - 0.16f) < 1e-3f,
              "2g-f 극단 가로 종횡비 = min 축 지배(비율 유지)");
        const jk::gallery::FitSize ftall =
            jk::gallery::FitFull(10, 10000, 160, 120);
        check(std::abs(ftall.w - 0.12f) < 1e-3f && ftall.h == 120.f,
              "2g-f 극단 세로 종횡비 = min 축 지배(비율 유지)");
        const jk::gallery::FitSize fdeg = jk::gallery::FitFull(0, 100, 160, 120);
        const jk::gallery::FitSize fdegVp =
            jk::gallery::FitFull(100, 100, 0.f, 120.f);
        check(fdeg.w == 0.f && fdeg.h == 0.f && fdegVp.w == 0.f &&
                  fdegVp.h == 0.f,
              "2g-f 퇴화 입력(이미지·뷰포트) = {0,0}");
        // 2g-f 이전/다음 wrap-around — 끝 지점 순환(전체 보기 이동 계약).
        check(jk::gallery::WrapStep(0, -1, 3) == 2 &&
                  jk::gallery::WrapStep(2, 1, 3) == 0,
              "2g-f wrap = 끝 지점 순환(이전·다음)");
        check(jk::gallery::WrapStep(1, 1, 3) == 2 &&
                  jk::gallery::WrapStep(1, -1, 3) == 0,
              "2g-f wrap = 범위 내 이동");
        check(jk::gallery::WrapStep(5, 0, 3) == 2 &&
                  jk::gallery::WrapStep(7, 1, 3) == 2,
              "2g-f wrap = 범위 밖 인덱스 수렴(모듈로 정규화)");
        check(jk::gallery::WrapStep(0, 1, 0) == 0 &&
                  jk::gallery::WrapStep(0, -1, 0) == 0,
              "2g-f wrap = 빈 목록 무접촉(무변)");

        // 2g-g) nearest 박스 축소 (T3 — T2 폴백 확대 동형 기법 역방향): dst
        // (x,y) = src(x*srcW/dstW, y*srcH/dstH) 소스 샘플 그대로. s=1.0 항등
        // 등호·0.5 축소 격자·1.5 요청(상한 눌림 = 항등)·퇴화 입력 방어선.
        {
            // 4x4 소스 — 픽셀 R채널 = 샘플 인덱스(x + y*4)로 식별 가능하게.
            jk::LoadedImage src;
            src.w = 4;
            src.h = 4;
            src.rgba.resize(16 * 4);
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    uint8_t* px = src.rgba.data() + (y * 4 + x) * 4;
                    px[0] = static_cast<uint8_t>(x + y * 4);
                    px[1] = 0;
                    px[2] = 0;
                    px[3] = 255;
                }
            const jk::LoadedImage id = jk::gallery::MakeThumb(src, 4, 4);
            check(id.w == 4 && id.h == 4 &&
                      std::memcmp(id.rgba.data(), src.rgba.data(), 16 * 4) == 0,
                  "2g-g s=1.0 = 항등(치수+픽셀 정확 등호)");
            const jk::LoadedImage half = jk::gallery::MakeThumb(src, 2, 2);
            const uint8_t* hp = half.rgba.data();
            check(half.w == 2 && half.h == 2 && hp[0] == 0 && hp[4] == 2 &&
                      hp[8] == 8 && hp[12] == 10,
                  "2g-g 0.5 축소 샘플 = x*src/dst 격자(사분면 0·2·8·10)");
            const jk::LoadedImage up = jk::gallery::MakeThumb(src, 6, 6);
            check(up.w == 4 && up.h == 4 &&
                      std::memcmp(up.rgba.data(), src.rgba.data(), 16 * 4) == 0,
                  "2g-g 1.5 요청 = s 1.0 상한 눌림 항등(확대 금지 — 2g-c 계약)");
            const jk::LoadedImage degBox =
                jk::gallery::MakeThumb(src, 0, 4);
            const jk::LoadedImage degEmpty =
                jk::gallery::MakeThumb(jk::LoadedImage(), 4, 4);
            check(degBox.rgba.empty() && degBox.w == 0 && degBox.h == 0 &&
                      degEmpty.rgba.empty(),
                  "2g-g 퇴화 입력(박스 0·빈 픽셀) = 빈 LoadedImage(placeholder 유지)");
        }

        // 2g-h) 캐시 키 (T3 — 2g-d Fnv1a 소비): 경로+size+mtime 조합. 같은
        // 이름·내용 변화(크기 또는 mtime) = 키 변화(스메리 캐시 방지), 스탬프
        // 실패(부재) = 빈 키(열외), 캐시 파일 경로 합성 계약.
        const std::string kBase =
            jk::gallery::ThumbKey("P:/pics/a.png", 100, 5);
        check(!kBase.empty() && kBase.size() == 8 &&
                  kBase == jk::gallery::ThumbKey("P:/pics/a.png", 100, 5),
              "2g-h 결정론성+8자리 hex 형식(빈 키 부재)");
        check(jk::gallery::ThumbKey("P:/pics/a.png", 101, 5) != kBase &&
                  jk::gallery::ThumbKey("P:/pics/a.png", 100, 6) != kBase,
              "2g-h size 변화·mtime 변화 = 키 변화(내용 변화 반영)");
        check(jk::gallery::ThumbKey("P:/pics/b.png", 100, 5) != kBase,
              "2g-h 경로 변화 = 키 변화(이름·내용 같아도)");
        check(jk::gallery::GalleryThumbPath("X:/exe", "0a1b2c3d") ==
                      "X:/exe/state/gallery/thumbs/0a1b2c3d.png" &&
                  jk::gallery::GalleryThumbPath("X:/exe", "").empty(),
              "2g-h 캐시 경로 합성 = thumbs/<key>.png (빈 키 = 빈 경로 방어선)");
        {
            // 실측 한쌍 — 생존 파일은 키가 성립하고 2회차 조명도 같은 키(재부팅
            // 재조명 방지의 가교), 부재 파일은 ec 중립형 열외(빈 키).
            const fs::path tdir = fs::temp_directory_path() / "jk_gallery_key";
            fs::remove_all(tdir);
            fs::create_directories(tdir);
            {
                std::ofstream out(tdir / "a.png");
                for (int i = 0; i < 10; ++i) out.put('a');
            }
            const std::string live =
                jk::gallery::ThumbKeyFor((tdir / "a.png").string());
            const std::string missed =
                jk::gallery::ThumbKeyFor((tdir / "no.png").string());
            check(missed.empty() && !live.empty() &&
                      live == jk::gallery::ThumbKeyFor((tdir / "a.png").string()),
                  "2g-h 실측 = 생존 파일 키 성립+재조명 동일, 부재 = 빈 키(열외)");
            fs::remove_all(tdir);
        }

        // 2g-i) LRU 퇴출 산치 (T3 fix r1 — 동일 프레임 기록 텍스처 파괴 방지):
        // 이번 프레임(useFrame == curFrame) 접촉 슬롯은 후보에서 **전부** 제외,
        // 후보 중 최소 세대, 동세대 동률 = 앞 인덱스, 후보 0 = -1(placeholder
        // 유지). 예전 가드(`tick == cur` 1건)의 C1 결함 수형을 직단정한다.
        check(jk::gallery::PickLruVictim({9, 9, 9}, 9) == -1 &&
                  jk::gallery::PickLruVictim({9, 3, 9, 7}, 9) == 1 &&
                  jk::gallery::PickLruVictim({9, 7, 7, 9}, 9) == 1 &&
                  jk::gallery::PickLruVictim({}, 9) == -1,
              "2g-i 퇴출 산치 = 동일 프레임 세대 전부 제외+최소 세대(동률 앞 인덱스)+후보 0 = -1");

        // 2g-j) 컬 산치 (T3 fix r2 — C2 영구 기아 봉합): 셀 박스 수직 스팬이
        // 클립 스팬과 **교차**할 때만 풀에 요청한다. 포함·위 절반·아래 절반
        // 교차 = 가시(요청), 전체 위/아래 = 비가시(요청 없음 — 0 높이 교차는
        // 픽셀이 없으므로 경계 접촉도 비가시). 이상 경계(가시 셀 > 96 = 풀
        // 상한)는 동세대 접촉만 남아 후보 0 = -1 → placeholder 유지 계약이라
        // 2g-i의 후보 0 수형과 연결(파괴 없음, 기아는 컬이 이미 봉합 — 가시>
        // 96 극단만 placeholder).
        check(jk::gallery::ThumbRowVisible(10.f, 130.f, 0.f, 200.f) &&
                  jk::gallery::ThumbRowVisible(-50.f, 100.f, 0.f, 200.f) &&
                  jk::gallery::ThumbRowVisible(100.f, 300.f, 0.f, 200.f) &&
                  !jk::gallery::ThumbRowVisible(200.f, 300.f, 0.f, 200.f) &&
                  !jk::gallery::ThumbRowVisible(-10.f, 0.f, 0.f, 200.f),
              "2g-j 컬 산치 = 클립 교차 셀만 요청(경계 접촉 = 비가시 — 0 높이 교차 금지)");
        check(jk::gallery::PickLruVictim({9, 9, 9, 9}, 9) == -1,
              "2g-j 이상 경계(가시 셀 > 96 = 풀 상한) = 후보 0 → placeholder 유지(파괴 없음)");
    }

    // 2i) 클라 활동 게이트 (#89 T1 — 스펙 2026-10-09-client-idle): Run() 루프의
    // wantRender 판정을 순수 부품(client/JKActivityGate.h)으로 뽑아 렌더러·서버
    // 없이 단정한다. 근거 = .superpowers/sdd/2026-10-09-clt-spin/spike-report.md
    // §1a — Run()(JKClientApplication.cpp)이 타이머 채널 소비 수>0을 활동으로
    // 계수해 docs/78 게이트를 매 16ms 틱마다 무력화, 폰 갤러리 클라가 무변화
    // 19fps 풀코어 100% 스핀이 됐다(프루프 영수증). 계약: **타이머 틱 = 배송일
    // 뿐 활동이 아니다** — 입력·에이전트·툴콜·테마만 활동으로 계수, 폴백 1s
    // (장면 더티 게이트)는 안전망으로 유지. 캐논 계보(기존 Win 569/WSL 546/
    // posix 277 — 2g 계열 다음 신설 2i, 쌍둥이 = tools/posix_selftest/main.cpp
    // TestActivityGate).
    {
        // 2i-a) 타이머 틱 단독 = 렌더 유발 안 함(T1 핵심 수형). renderedOnce
        // 도달·장면 더티 부재·나머지 채널 조용 상태에서 타이머 소비 불만 있어도
        // 게이트는 false — 타이머가 자기 틱으로 스스로 렌더를 부활시키던 spike
        // 수형의 직단정. 더티 조회 프레디케이트는 폴백 비도달(또는 게이트
        // 선행 참) 동안 열람 0회(HitDirtyWindows 매 이터레이션 열람 방지 —
        // Run() 원문 구조).
        const auto neverDirty = [] { return false; };
        check(!jk::client::GateWantRender(
                  /*timerDelivered=*/true, /*inputDrained=*/false,
                  /*agentEvent=*/false, /*toolCall=*/false,
                  /*themeChanged=*/false, /*frameDirty=*/false,
                  /*renderedOnce=*/true, /*fallback=*/false, neverDirty),
              "2i-a 타이머 틱 단독 = 렌더 유발 안 함(활동 게이트 원문 수형)");

        int probeCount = 0;
        {
            const auto probeAbsent = [&probeCount] {
                ++probeCount;
                return false;
            };
            const bool decided = jk::client::GateWantRender(
                true, false, false, false, false, false, true,
                /*fallback=*/true, probeAbsent);
            // 폴백 도달 + 장면 더티 부재 = 스킵이며, 이때만 더티가 열린다
            // (probeCount == 1 — 폴백만이 유발한 커밋 제거 원문).
            check(!decided && probeCount == 1,
                  "2i-a 폴백 도달+더티 부재 = 스킵(더티 조회 1회 원문)");
        }
        {
            probeCount = 0;
            const auto probePresent = [&probeCount] {
                ++probeCount;
                return true;
            };
            const bool decided = jk::client::GateWantRender(
                true, false, false, false, false, false, true,
                /*fallback=*/true, probePresent);
            check(decided && probeCount == 1,
                  "2i-a 폴백 도달+장면 더티 = 렌더(1s 폴백 안전망 유지)");
        }
        {
            // 부팅 첫 프레임(renderedOnce = false): 채널 조용해도 즉시 1프레임.
            const bool firstFrame = jk::client::GateWantRender(
                false, false, false, false, false, /*frameDirty=*/false,
                /*renderedOnce=*/false, /*fallback=*/false, neverDirty);
            check(firstFrame, "2i-a 부팅 첫 프레임 = 이벤트 없이 즉시 렌더");
            // IsFrameDirty 오버라이드 앱(vplayer·터미널·ImGui 계열) 경로 원문:
            // 더티만으로 렌더 — 대조군 앱의 더티 구동이 게이트 스윕에서 무사.
            const bool dirtyFrame = jk::client::GateWantRender(
                false, false, false, false, false, /*frameDirty=*/true,
                /*renderedOnce=*/true, /*fallback=*/false, neverDirty);
            check(dirtyFrame, "2i-a frameDirty = 게이트 첫 항 원문 유지");
        }

        // 2i-b) 입력/에이전트/툴콜/테마 = 활동 유지 회귀(T1이 툴콜 등 활동을
        // 죽이지 않음을 단정 — 각 채널 단독으로도 렌더).
        const auto channel = [](bool i, bool a, bool t, bool th) {
            // renderedOnce·더티 부재 폴백 비도달로 두고 채널 유무만 판정.
            return jk::client::GateWantRender(
                /*timerDelivered=*/false, i, a, t, th, /*frameDirty=*/false,
                /*renderedOnce=*/true, /*fallback=*/false,
                [] { return false; });
        };
        check(channel(/*input=*/true, false, false, false),
              "2i-b 입력 이벤트 = 활동(렌더 유지)");
        check(channel(false, /*agent=*/true, false, false),
              "2i-b 에이전트 이벤트 = 활동(렌더 유지)");
        check(channel(false, false, /*toolCall=*/true, false),
              "2i-b 에이전트 툴콜 = 활동(렌더 유지)");
        check(channel(false, false, false, /*theme=*/true),
              "2i-b 테마 변경 = 활동(렌더 유지)");
        check(jk::client::GateWantRender(
                  /*timerDelivered=*/true, /*inputDrained=*/true,
                  /*agentEvent=*/false, /*toolCall=*/false,
                  /*themeChanged=*/false, /*frameDirty=*/false,
                  /*renderedOnce=*/true, /*fallback=*/false, neverDirty),
              "2i-b 타이머 배송+입력 공존 = 활동(타이머 불참여가 활동을 누르지 않음)");
    }

    // 2i-c) 16 ImGui 앱 Timer→더티 조건화 산치 (#89 T2 — 스펙 설계 2-3).
    // 근거 = 스파이크 원장 §1a — "Timer 이벤트마다 frameDirty_=true" 무조건
    // 관용구가 idle 스핀 진원. 앱의 타이머 콜백은 **이번 틱에 실제로 바뀌는
    // 내용(진행 중 상태)이 있을 때만** 더티를 내며, 산치 원문은
    // include/apps/ClientIdlePolicy.h(jk::idle) 단일 진실원 — 앱(gallery·
    // vplayer·taskmgr·notify·imguidemo)과 이 쌍둥이가 같은 수형을 소비한다.
    // 정적 앱 11곳의 타이머 더티 분기 삭제는 자체 정적 계약(분기 부재)이고,
    // 무더티 정적 앱이 매 틱 렌더를 부활시키지 않는 건 게이트 수형 2i-a가
    // 단정한다 — 아래 합성 단정(타이머 틱+더티 부재 = 렌더 유발 안 함)은
    // 그 표 수형의 게이트 쪽 원문 재단정이다.
    {
        // vplayer — 진행 중 상태 합산(스펙 설계 3). 재생·비동기 열기·스크럽
        // ·OSD 애니메이션만 true, 정지(일시정지·미개·완결)는 false.
        const auto prog = [](bool o, bool p, bool s, bool osd) {
            return jk::idle::Progressing(
                jk::idle::VplayerProgress{o, p, s, osd});
        };
        check(prog(true, false, false, false), "2i-c vplayer 비동기 열기 = 진행 중(더티)");
        check(prog(false, true, false, false), "2i-c vplayer 재생 중 = 프레임마다(스펙 예외 조항)");
        check(prog(false, false, true, false), "2i-c vplayer 조그/휠/시크/역재생 = 진행 중");
        check(prog(false, false, false, true), "2i-c vplayer 극장 OSD 페이드/카운트다운 = 진행 중");
        check(!prog(false, false, false, false),
              "2i-c vplayer 일시정지/미개/종료 = 무더티(정지 화면 — 무렌더 의도)");
        // notify — 토스트는 도착 프레임(풀알파 3s 정적), 말미 페이드 2s만
        // 진행 중, 소멸 경계는 앱 래치로 1프레임(2i-a의 1fps 폴백 도합 원문).
        check(!jk::idle::ToastFading(5000) && !jk::idle::ToastFading(2000) &&
                  jk::idle::ToastFading(1999) && jk::idle::ToastFading(1) &&
                  !jk::idle::ToastFading(0),
              "2i-c notify 토스트 페이드 창 = [0,2000)만 진행 중(풀알파 3s 정적)");
        // taskmgr — 500ms 샘플 경계만 내용(CPU%·플롯)이 바뀐다.
        check(!jk::idle::SampleDue(499) && jk::idle::SampleDue(500) &&
                  jk::idle::SampleDue(501),
              "2i-c taskmgr 샘플 경계 = 500ms 이상 틱만 더티(무변화 틱 제거)");
        // 정적 앱 11곳(gallery·files·notes·settings·chat·library·agentmgr·
        // filedialog·imguidemo·palette·shot·snap — snap은 응답 대기 자기유지
        // 만 조건 잔존, gallery는 썸네일 요 실패 조건 잔존): 타이머 콜백이
        // "조건 없는 더티"가 아님의 게이트 쪽 합성 단정 — 내용 변화 없는
        // 틱(frameDirty=false)은 어떤 틱 잔존으로도 렌더를 부활시키지 않는다.
        check(!jk::client::GateWantRender(
                  /*timerDelivered=*/true, /*inputDrained=*/false,
                  /*agentEvent=*/false, /*toolCall=*/false,
                  /*themeChanged=*/false, /*frameDirty=*/false,
                  /*renderedOnce=*/true, /*fallback=*/false,
                  [] { return false; }),
              "2i-c 정적 앱 = 무변화 틱 무렌더(16앱 스윕의 게이트 합성 원문)");
        check(!jk::client::GateWantRender(
                  true, false, false, false, false, false, true,
                  /*fallback=*/true, [] { return false; }),
              "2i-c 정적 앱 idle 1s+장면 클린 = 폴백도 스킵(자기유지 더티 소각)");
    }

    // 2m) 뮤직 순수 부품 (T1 — 스펙 2026-10-09-music-library-design): 모듈 골격
    // 의 리졸버/정규화/재귀 열거/이름 필터를 헤더(jk::music, apps/MusicModel.h)
    // 직소비로 잠근다. 갤러리 2g 쌍둥이 구성 — D2(파일명 스캔)·D3(재귀)·
    // D1(재생은 vplayer 위임 — 이 블록 단정 범위 밖).
    {
        namespace fs = std::filesystem;
        using jk::music::MusicDirList;
        // 2m-a) 순수 리졸버 — settings 없음(빈 텍스트) → 기본 1건
        // {exeDir/state/music}(fail-safe); music.dirs 배열 → 기본이 앞(기본
        // 폴더 규약)·유저 폴더 뒤; "dirs" 비배열·파손 settings = 기본 1건.
        const std::vector<std::string> withMusic = MusicDirList(
            "X:/exe", R"({"music":{"dirs":["P:/sounds"]}})");
        check(withMusic.size() == 2 &&
                  withMusic[0] == "X:/exe/state/music" &&
                  withMusic[1] == "P:/sounds",
              "2m-a music.dirs 1건 = 기본 앞+유저 뒤");
        const std::vector<std::string> noMusicSettings =
            MusicDirList("X:/exe", "");
        check(noMusicSettings.size() == 1 &&
                  noMusicSettings[0] == "X:/exe/state/music",
              "2m-a settings 없음 = 기본 1건(fail-safe)");
        const std::vector<std::string> notArray =
            MusicDirList("X:/exe", R"({"music":{"dirs":"P:/sounds"}})");
        check(notArray.size() == 1 &&
                  notArray[0] == "X:/exe/state/music",
              "2m-a dirs 문자열(비배열) = 무시, 기본 1건(원문 보존 소비)");
        const std::vector<std::string> broken =
            MusicDirList("X:/exe", R"json({"music":{"dirs":[)json");
        check(broken.size() == 1 && broken[0] == "X:/exe/state/music",
              "2m-a 파손 settings = 기본 1건(fail-safe)");
        check(jk::music::AudioDirFallback::Path("X:/exe") ==
                      "X:/exe/state/music" &&
                  jk::music::AudioDirFallback::Path("") == "state/music",
              "2m-a AudioDirFallback = state/music 합성(빈 exeDir = 상대 1건)");

        // 2m-b) 경로 정규화 — 중복 제거(첫 등장 유지)+빈 성분 제거+뒤 구분자
        // 정규화. backslash 수형은 2g-b가 Win 축 원문 계약으로 이미 소유 —
        // 이 쌍은 축 무관(슬래시만) 수형으로 중복 제거를 단정한다.
        const std::vector<std::string> norm = jk::music::NormalizeDirs(
            {"srv/mus//", "", "/", "srv/mus"});
        check(norm.size() == 1 && norm[0] == "srv/mus",
              "2m-b 뒤 구분자 정규화+빈 성분 제거+중복(첫 등장 유지)");
        const std::vector<std::string> normTrail =
            jk::music::NormalizeDirs({"A:/x", "A:/x/", "A:/x"});
        check(normTrail.size() == 1 && normTrail[0] == "A:/x",
              "2m-b 3중복(구분자 철자만 다름) = 1건");

        // 2m-c) 오디오 열거 — 없는 루트=독립 실패(빈 목록), 실트리는 재귀 3건
        // 수취+txt 제외+mtime desc(명시 세트 — 시계 분해능 무관 결정론)+rel에
        // 하위폴더 슬래시 표기. 임시 폴더에서 실측(사후 소각).
        const fs::path mdir = fs::temp_directory_path() / "jk_music_selftest";
        fs::remove_all(mdir);
        {
            std::vector<jk::music::Track> out;
            out = jk::music::ListAudioFiles((mdir / "missing").string());
            check(out.empty(), "2m-c 없는 루트 = 목록 비움(독립 스캔 실패)");
            fs::create_directories(mdir / "sub");
            std::ofstream(mdir / "a.mp3").put('x');
            std::ofstream(mdir / "b.MP3").put('x');  // 대문자 확장자 수형
            std::ofstream(mdir / "c.txt").put('x');
            std::ofstream(mdir / "sub" / "w.wav").put('x');
            // mtime을 명시 세트 — a(+1h 최신)·b(지금)·sub/w(-1h 최구)로
            // 분해능 무관한 desc 단정(동점 tiebreak 미접촉).
            const long long past =
                fs::file_time_type::clock::now().time_since_epoch().count();
            fs::last_write_time(
                mdir / "a.mp3",
                fs::file_time_type::clock::now() + std::chrono::hours(1));
            fs::last_write_time(
                mdir / "sub" / "w.wav",
                fs::file_time_type::clock::now() - std::chrono::hours(1));
            out = jk::music::ListAudioFiles(mdir.string());
            check(out.size() == 3 && out[0].name == "a.mp3" &&
                      out[1].name == "b.MP3" && out[2].name == "w.wav",
                  "2m-c 재귀 열거 = 트리 전체 3건 수취+mtime desc 최신순");
            check(std::find_if(out.begin(), out.end(),
                               [](const jk::music::Track& t) {
                                   return t.name == "c.txt";
                               }) == out.end(),
                  "2m-c txt = 오디오 확장자 밖 열외(대문자 MP3는 포함)");
            const jk::music::Track& sub = out[2];
            check(sub.rel == "sub/w.wav" &&
                      !sub.full.empty() && sub.size == 1 &&
                      sub.mtime < past,
                  "2m-c rel = 루트 기준 슬래시 표기+스탬프(size·mtime) 성립");
            fs::remove_all(mdir);
        }

        // 2m-d) 이름 필터 — 빈 필터=전부 참, 대소문자 무시 부분일치(ASCII
        // fold만), 한글은 이진 비교(그래서 부분일치 자체는 성립한다).
        check(jk::music::MatchFilter("night_mix.mp3", "") &&
                  jk::music::MatchFilter("", ""),
              "2m-d 빈 필터 = 전부 참(필터 꺼짐)");
        check(jk::music::MatchFilter("night_mix.mp3", "mix") &&
                  !jk::music::MatchFilter("night_mix.mp3", "dawn"),
              "2m-d 부분일치 = 어느 지점이든 히트·불일치는 열외");
        check(jk::music::MatchFilter("MySong.mp3", "song") &&
                  jk::music::MatchFilter("MYSONG.mp3", "song"),
              "2m-d 대소문자 무시 = ASCII fold만");
        check(jk::music::MatchFilter("가요1.mp3", "가요"),
              "2m-d 한글 = 이진 비교(fold 없이 부분일치)");

        // 2m-e) 재귀 순환 가드 (T1 fix r1 — 리뷰 I-1): 심링크·junction
        // 디렉터리 순환 트리에서 ListAudioFiles가 유한 시간에 종료하고(아래
        // 수행 자체가 증거 — 무한 재귀면 테스트가 복귀하지 않는다) 링크
        // 디렉터리가 이중 계수를 만들지 않는다. Windows는 junction(mklink /J
        // — 특권 불요), posix는 create_directory_symlink(권한 무관) — 생성
        // 실패는 skip 없이 FAIL(사유 명시 계약; 실패 조건 = mkrc/ec 원문).
        const fs::path cdir = fs::temp_directory_path() / "jk_music_cycle";
        // 시작 정리 — 이전 런 크래시 잔산(링크 포함)에도 안전한 선제거:
        // 링크를 먼저 링크 자체로 제거해 remove_all의 junction 재귀를 회피.
        std::error_code clec;
        fs::remove(cdir / "sub" / "loop", clec);
        fs::remove(cdir / "sub2", clec);
        fs::remove_all(cdir, clec);
        fs::create_directories(cdir / "sub");
        std::ofstream(cdir / "a.mp3").put('x');
        std::ofstream(cdir / "top.flac").put('x');
        std::ofstream(cdir / "sub" / "w.wav").put('x');
        {
#if defined(_WIN32)
            // mklink /J — junction은 일반 사용자 권한으로 성립(실측). 명령
            // 출력은 콘솔 스폰으로 사라지므로 rc+존재로 판정한다.
            const int mkrc = std::system(
                ("cmd /c mklink /J \"" + (cdir / "sub" / "loop").string() +
                 "\" \"" + cdir.string() + "\"")
                    .c_str());
            std::error_code mkEc;
            check(mkrc == 0 && fs::exists(cdir / "sub" / "loop", mkEc),
                  "2m-e 순환 링크 생성(sub/loop→루트 junction) 성립 — 실패는 FAIL"
                  " (exFAT TEMP 등 비NTFS 볼륨에서 환경 실패 — 리포트 부기)");
            const int mk2rc = std::system(
                ("cmd /c mklink /J \"" + (cdir / "sub2").string() + "\" \"" +
                  (cdir / "sub").string() + "\"")
                    .c_str());
            check(mk2rc == 0,
                  "2m-e 링크 2(sub2→sub junction) 성립 — 이중 계수 가드 수형");
#else
            std::error_code ce;
            fs::create_directory_symlink(cdir, cdir / "sub" / "loop", ce);
            check(!ce,
                  "2m-e 순환 링크 생성(sub/loop→루트 심링크) 성립 — 실패는 FAIL"
                  " (fs가 symlink를 만들지 못하는 환경 실패 — 리포트 부기)");
            std::error_code ce2;
            fs::create_directory_symlink(cdir / "sub", cdir / "sub2", ce2);
            check(!ce2, "2m-e 링크 2(sub2→sub 심링크) 성립 — 이중 계수 가드 수형");
#endif
            const std::vector<jk::music::Track> out =
                jk::music::ListAudioFiles(cdir.string());
            check(out.size() == 3,
                  "2m-e 순환 트리 = 유한 종료+재귀 가드(링크 디렉터리 미재방문)");
            int seenA = 0, seenTop = 0, seenW = 0;
            for (const jk::music::Track& t : out) {
                if (t.name == "a.mp3") ++seenA;
                if (t.name == "top.flac") ++seenTop;
                if (t.name == "w.wav") ++seenW;
            }
            check(seenA == 1 && seenTop == 1 && seenW == 1,
                  "2m-e 이중 계수 0 = 각 트랙 정확 1회(링크 경유 재등장 무접수)");
            // 정리 — 링크 디렉터리를 먼저 **링크 자체**로 제거한다:
            // remove_all은 junction을 따라 재귀한다(libstdc++ 실측 — loop
            // 경로가 소진될 때까지 파고들다 실패 throwable, fix r1 소각
            // 과정 실측 원문). 링크 제거(follow 없이) 후 나머지 remove_all.
            std::error_code de;
            fs::remove(cdir / "sub" / "loop", de);
            fs::remove(cdir / "sub2", de);
            fs::remove_all(cdir, de);
            check(!de && !fs::exists(cdir, de),
                  "2m-e 정리 = 링크 선제거 후 remove_all(사후 소각 — 무잔산)");
        }

        // 2m-f) 스캔 워커 쌍둥이 케이스 (T2): ClientMusicApp의 표기 데이터 =
        // ListAudioFiles 결과 벡터 **동형**(앱은 재정렬·파생 없이 그 벡터를
        // 소비한다 — 필터는 순서 보존 열외만). 2m-c가 mtime desc를 이미
        // 잠그지만 동점(mtime tie → rel asc tiebreak, T1 본문 재량 ③)은
        // 미접촉 수형 — 앱 표기 순서의 결정론이 이 케이스의 소관이다. mtime을
        // 명시 동일치로 세워(시계 분해능 무관 결정론 — 2m-c 원문 수형) 동점을
        // 강제하고, rel asc 타이브레이크가 서열을 정하는 단정 1건(브리프 "신설
        // 어설션 1건"). 지정 2m-e는 T1 fix r1이 순환 가드로 선점 — 브리프
        // 원문 기록 2m-e는 이 케이스(2m-f)로 승계(리포트 §6 정정 부기).
        const fs::path tdir = fs::temp_directory_path() / "jk_music_tie";
        fs::remove_all(tdir);
        {
            fs::create_directories(tdir / "sub");
            std::ofstream(tdir / "zz.mp3").put('x');
            std::ofstream(tdir / "sub" / "aa.mp3").put('x');
            const auto stamp =
                fs::file_time_type::clock::now() - std::chrono::hours(2);
            fs::last_write_time(tdir / "zz.mp3", stamp);
            fs::last_write_time(tdir / "sub" / "aa.mp3", stamp);
            const std::vector<jk::music::Track> out =
                jk::music::ListAudioFiles(tdir.string());
            check(out.size() == 2 && out[0].rel == "sub/aa.mp3" &&
                      out[1].rel == "zz.mp3" &&
                      out[0].mtime == out[1].mtime && out[0].size == 1,
                  "2m-f 스캔 결과 = 표기 데이터 동형(mtime 동점 → rel asc, "
                  "rel 열 원문)");
            fs::remove_all(tdir);
        }

        // 2m-g) 재생 위임 꾸러미 (T3): jk::music::OpenRequestJson — app_tool
        // open 인자 전문 조립 원문(브리프 T3 순수 부품 — {"app":"vplayer",
        // "tool":"open","args":{"path":"<full>"}}), vplayer 도구 선언
        // ("open" — path 필수)과 relay 후보 역매칭 계약에 정확히 맞춘 리터럴.
        // windowId 미기술(단일 후보=직행 — 지정하지 않으므로 서버 추측 없음).
        // 3케이스: 일반 경로·역슬래시 정규화·공백 경로(재청구 폴백이 쓰는
        // 경로판 overload와의 등가도 동반 단정 — 같은 core 소비).
        {
            jk::music::Track t1;                      // 일반 경로
            t1.full = "music/sub/song.mp3";
            check(jk::music::OpenRequestJson(t1) ==
                      "{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":"
                      "{\"path\":\"music/sub/song.mp3\"}}",
                  "2m-g 일반 경로 = app_tool open 꾸러미 조립 원문(app/tool "
                  "리터럴·windowId 미기술)");
            jk::music::Track t2;                      // 역슬래시(Windows 수형)
            t2.full = "C:\\music\\sub\\a.mp3";
            check(jk::music::OpenRequestJson(t2) ==
                      "{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":"
                      "{\"path\":\"C:/music/sub/a.mp3\"}}",
                  "2m-g 역슬래시 경로 = 슬래시 정규화(JSON 이스케이프 축적 없음"
                  " — generic_string 원형)");
            jk::music::Track t3;                      // 공백 경로
            t3.full = "my music/cold song.mp3";
            check(jk::music::OpenRequestJson(t3) ==
                      "{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":"
                      "{\"path\":\"my music/cold song.mp3\"}}",
                  "2m-g 공백 경로 = 원문 수용(JSON 문자열 감싸기 — % 이스케이프"
                  " 불요) + 경로판 재청구 overload 등가");
            check(jk::music::OpenRequestJson(t3) ==
                      jk::music::OpenRequestJsonPath(t3.full),
                  "2m-g 재청구 경로판(OpenRequestJsonPath) = Track판 등가"
                  "(폴백 재청구 1발 동기화)");
        }

        // 2m-h) 위임 답신 본문 재판정 (T3 fix r3): music::DelegationReplyVerdict
        // — 봉투 ok 고정 1에 무신하는 본문 진실원(T4 결함 원장 "폴백 자기
        // 소멸" 봉합). 봉투 원문 2건(T4 위임 원장 원문 문자열) — ①즉답 거부
        // 원문(sync 경로: 봉투 ok=1·본문 unknown_app_tool 거부 — 봉투만 믿으면
        // 성공 오독) ②핫 경로 원문(지연응답: 본문 ok=true+result) — 각각 본문
        // 판정 단정.
        {
            const jk::music::DelegationVerdict cold =
                jk::music::DelegationReplyVerdict(
                    true, "{\"ok\":false,\"error\":\"unknown_app_tool\"}");
            const jk::music::DelegationVerdict hot =
                jk::music::DelegationReplyVerdict(
                    true,
                    "{\"ok\":true,\"windowId\":37,\"result\":"
                    "{\"accepted\":true}}");
            check(!cold.ok && cold.err == "unknown_app_tool" && hot.ok &&
                      hot.err.empty(),
                  "2m-h 봉투 ok=1 원문 2건 = 본문 재판정(즉답 거부 → 실패+"
                  "unknown_app_tool 재청구 대상·핫 경로 → 성공) — 봉투 무신 "
                  "본문 진실원");
        }

        // 2n) spatial leg 골격 (T1 — 스펙 2026-10-10-music-spatial-leg-design):
        // jk::music::leg 순수 부품 — ①OfExt(스펙 D4 표 — 지원 5·미지원 3)
        // ②Apply 상태 머신 4전이 ③ToolJson app 도구 허브 요청 원문 3종
        // (2m-g OpenRequestJson 쌍둥이) ④포맷 표 정합. audio_core 소비가 없는
        // 순수 헤더 직링이라 **env 미설정(fail-closed) 빌드에서도 전부 컴파일/
        // 실행된다** — 배선이 selftest를 조건부로 하지 않는 게 T1의 하드 계약.
        {
            using jk::music::leg::LegEvent;
            using jk::music::leg::LegState;
            using jk::music::leg::LegSupport;
            using jk::music::leg::Apply;
            using jk::music::leg::ToolJson;

            // 2n-a) LegSupport::OfExt — D4 표(대소문자 무시·숨김 확장자·
            // 빈 문자열·표 밖 확장자 — 보수 파 Unsupported).
            check(LegSupport::OfExt("music/sub/song.mp3") ==
                      LegSupport::State::Supported,
                  "2n-a mp3 = 지원(D4 표원문 — [spatial] 활성군)");
            check(LegSupport::OfExt("C:\\MUSIC\\song.FLaC") ==
                      LegSupport::State::Supported,
                  "2n-a 대소문자 무시(.FLaC) = 지원(ASCII fold — Stricmp)");
            check(LegSupport::OfExt("b.OGG") ==
                              LegSupport::State::Supported &&
                          LegSupport::OfExt("b.vorbis") ==
                              LegSupport::State::Supported &&
                          LegSupport::OfExt("b.WAV") ==
                              LegSupport::State::Supported,
                  "2n-a ogg·vorbis·wav = 지원(컨테이너/코덱 표 합동)");
            check(LegSupport::OfExt("a.m4a") == LegSupport::State::Unsupported,
                  "2n-a m4a = 미지원(회색 정직 표기군)");
            check(LegSupport::OfExt(".mp3") == LegSupport::State::Unsupported,
                  "2n-a 숨김 파일(.mp3 도트포함 항목) = 미지원(확장자 없음 — "
                  "extension() 공문자, 보수 파)");
            check(LegSupport::OfExt("") == LegSupport::State::Unsupported &&
                      LegSupport::OfExt("track") ==
                          LegSupport::State::Unsupported &&
                      LegSupport::OfExt("song.txt") ==
                          LegSupport::State::Unsupported,
                  "2n-a 빈 문자열·무확장자·표 밖 = 미지원(보수 파)");

            // 2n-b) Apply 순수 전이 — 4전이+순수성(원본 무변조). UI 라벨 소비
            // 계약: err는 kDelegationHint 원문 그대로 라벨로 그린다.
            LegState idle;                       // 처음 — Idles
            idle.path = "music/sub/song.mp3";
            idle.durSec = 180.0;
            idle.posSec = 0;
            const LegState playing = Apply(idle, LegEvent::Start);
            check(playing.active && playing.deviceOk &&
                      playing.posSec == 0.0 && playing.err.empty() &&
                      playing.path == "music/sub/song.mp3",
                  "2n-b Start = 재생 개시(active·deviceOk 전제·pos 0·고장 "
                  "표기 소멸·path 무변조)");
            const LegState failed = Apply(playing, LegEvent::DeviceFailed);
            check(!failed.active && !failed.deviceOk &&
                      failed.err == jk::music::leg::kDelegationHint,
                  "2n-b DeviceFailed = 고장 종착(active 해제·deviceOk 해제·"
                  "안내 원문 — UI 라벨 소비 계약)");
            const LegState stopped =
                Apply(playing, LegEvent::StopRequested);
            check(!stopped.active && stopped.deviceOk &&
                      stopped.err.empty(),
                  "2n-b StopRequested = 정지 성공 종착(deviceOk·고장 표기 "
                  "보존 — 정지는 고장이 아니다)");
            LegState mid = Apply(idle, LegEvent::Start);
            mid.posSec = 12.5;
            mid.durSec = 180.0;
            const LegState eos = Apply(mid, LegEvent::Eos);
            check(!eos.active && eos.posSec == eos.durSec,
                  "2n-b Eos = 자연 종료(진행 표기 끝까지 — pos==dur)");
            check(idle.active == false && idle.posSec == 0.0 &&
                      idle.durSec == 180.0 && idle.err.empty(),
                  "2n-b 순수성 = 입력 상태 무변조(값 반환이라 원본 보존 — "
                  "전이는 사건 1건만 적용)");

            // 2n-c) ToolJson — app 도구 허브 요청 원문 3종(2m-g 쌍둥이 형식 —
            // {"app":"music","tool":...}). path 이스케이프 원문도 2m-g 수형
            // 대조(공백·역슬래시).
            check(ToolJson(jk::music::leg::kToolPlay,
                           "my music/cold song.mp3") ==
                      "{\"app\":\"music\",\"tool\":\"spatial_play\",\"args\":"
                      "{\"path\":\"my music/cold song.mp3\"}}",
                  "2n-c spatial_play = app_tool 요청 원문(app/music 도구 허브 "
                  "릴레이 계약)");
            check(ToolJson(jk::music::leg::kToolPlay, "C:\\music\\a.mp3") ==
                      "{\"app\":\"music\",\"tool\":\"spatial_play\",\"args\":"
                      "{\"path\":\"C:/music/a.mp3\"}}",
                  "2n-c play 역슬래시 경로 = 슬래시 정규화(2m-g 원문 수형 — "
                  "JSON 이스케이프 축적 없음)");
            check(ToolJson(jk::music::leg::kToolStop) ==
                      "{\"app\":\"music\",\"tool\":\"spatial_stop\",\"args\":"
                      "{}}",
                  "2n-c spatial_stop = 무인자(args {} 원문)");
            check(ToolJson(jk::music::leg::kToolStatus) ==
                      "{\"app\":\"music\",\"tool\":\"spatial_status\",\"args\":"
                      "{}}",
                  "2n-c spatial_status = 무인자(args {} 원문 — 관측 표면)");

            // 2n-d) D4 표 정합 — 지원 5종(mp3/flac/ogg/vorbis/wav)·미지원
            // 3종(m4a/aac/wma) 개수 등호(스펙 D4 원문 개수).
            const char* const supported[] = {"s.mp3", "s.flac", "s.ogg",
                                             "s.vorbis", "s.wav"};
            const char* const unsupported[] = {"s.m4a", "s.aac", "s.wma"};
            bool allSupported = true, allUnsupported = true;
            for (const char* const p : supported)
                if (LegSupport::OfExt(p) != LegSupport::State::Supported)
                    allSupported = false;
            for (const char* const p : unsupported)
                if (LegSupport::OfExt(p) !=
                    LegSupport::State::Unsupported)
                    allUnsupported = false;
            check(allSupported, "2n-d D4 지원군 5종 전원 = Supported");
            check(allUnsupported, "2n-d D4 미지원군 3종 전원 = Unsupported");
        }

        // 2o) 폴더 저장소 (T1 — 플랜 2026-10-10-music-dirs-ui): jk::music::store
        // — settings.json "music.dirs" 원자적 쓰기(같은 폴더 .tmp+fs::rename)
        // 의 실측 봉합. ①부재 신설 원문 2건 ②기존 settings(audio·retention)
        // 보존 원문 2건·music.dirs 합성 ③중복 Add no-op·Remove 미존재 no-op
        // (정직) ④부적합 JSON 원본 보존+err 1행 ⑤UserDirs 파싱 2건 — 실파일
        // leg라 2m-c 원문 수형으로 임시 폴더 실측(사후 소각).
        {
            namespace st = jk::music::store;
            const fs::path odir =
                fs::temp_directory_path() / "jk_music_dirs_store";
            fs::remove_all(odir);  // 선제거 — 이전 런 잔산(2m-c 원문 수형)

            // ①) 부재 파일 신설 — settings.json 원문 부재 = 신설 JSON 1건
            // (music.dirs만·기본형). 경로는 역슬래시 원문 전달 — 저장은
            // 슬래시 정규형(2m-g 원문 수형·기본 폴더와 동일 규약).
            const std::string exeDir = (odir / "exe").string();
            const std::string sp = st::SettingsPath(exeDir);
            const jk::music::DirWriteResult a1 =
                st::AddDir(exeDir, "P:\\sounds");
            std::string created;
            st::ReadAll(sp, created);
            check(a1.ok && created ==
                      "{\"music\":{\"dirs\":[\"P:/sounds\"]}}",
                  "2o-a 부재 파일 신설 = dirs 1건 music.dirs만 기본형 원문"
                  "(역슬래시 = 슬래시 정규화 — 2m-g 원문 수형)");
            const jk::music::DirWriteResult a2 = st::AddDir(exeDir, "Q:/disc");
            std::string grown;
            st::ReadAll(sp, grown);
            check(a2.ok &&
                      st::UserDirs(grown).size() == 2 &&
                      st::UserDirs(grown)[0] == "P:/sounds" &&
                      st::UserDirs(grown)[1] == "Q:/disc",
                  "2o-a 신설 문서 제2폴더 = music 키 유지+dirs 순서 보존"
                  "(재독 파싱 — 기본형 유지)");

            // ②) 기존 settings 보존 — audio·retention 상위 키는 원문 슬라이스
            // 재조립으로 무손상(하드 계약 — build/state/settings.json 실측
            // 원문 형식), music.dirs는 말미 신설.
            const std::string exeDir2 = (odir / "exe2").string();
            const std::string sp2 = st::SettingsPath(exeDir2);
            fs::create_directories(fs::path(sp2).parent_path());
            std::ofstream(sp2, std::ios::binary)
                << "{\"audio\":{\"mute\":0,\"volume\":80},"
                   "\"retention\":{\"days\":7}}";
            const jk::music::DirWriteResult b1 = st::AddDir(exeDir2, "N:\\mix");
            std::string kept;
            st::ReadAll(sp2, kept);
            check(b1.ok &&
                      kept ==
                          "{\"audio\":{\"mute\":0,\"volume\":80},"
                          "\"retention\":{\"days\":7},"
                          "\"music\":{\"dirs\":[\"N:/mix\"]}}",
                  "2o-b 기존 settings = audio·retention 원문 보존(무손상 하드 "
                  "계약)+music.dirs 말미 합성 원문");
            std::string kept2;
            st::ReadAll(sp2, kept2);
            check(st::UserDirs(kept2).size() == 1 &&
                      st::UserDirs(kept2)[0] == "N:/mix",
                  "2o-b music.dirs 합성 = 재독 UserDirs 1건(정규형 순서)");

            // ③) no-op 정직 — 중복(정규화 후 철자 상이) Add와 미존재 Remove는
            // ok만 보고하고 파일 바이트를 건드리지 않는다.
            std::string before5;
            st::ReadAll(sp2, before5);
            const jk::music::DirWriteResult c1 =
                st::AddDir(exeDir2, "N:\\mix\\");  // 정규화 후 동일 철자
            std::string after5;
            st::ReadAll(sp2, after5);
            check(c1.ok && after5 == before5,
                  "2o-c 중복 Add(정규화 후 동일) = no-op ok+원문 무변조");
            std::string before6;
            st::ReadAll(sp2, before6);
            const jk::music::DirWriteResult c2 =
                st::RemoveDir(exeDir2, "R:/nowhere");
            std::string after6;
            st::ReadAll(sp2, after6);
            check(c2.ok && after6 == before6,
                  "2o-c Remove 미존재 = no-op ok(정직)+원문 무변조");

            // ④) 부적합 JSON — 파손 원문은 손대지 않고(오염 금지 — 원본 보존
            // 하드 계약) err 1행으로 정직 거부.
            const std::string junk = "{\"music\":{\"dirs\":[";
            std::ofstream(sp2, std::ios::binary) << junk;
            const jk::music::DirWriteResult d1 =
                st::AddDir(exeDir2, "S:/late");
            std::string preserved;
            st::ReadAll(sp2, preserved);
            check(!d1.ok && !d1.err.empty() &&
                      d1.err.find('\n') == std::string::npos,
                  "2o-d 부적합 JSON = ok=false+err 1행(정직 거부)");
            check(preserved == junk,
                  "2o-d 부적합 JSON = 원본 바이트 보존(오염 금지)");

            // ⑤) UserDirs 파싱 — 배열 원문 3건(중복·철자 상이 포함)의 순서
            // 보존+정규화 접힘, 파손 원문은 빈 목록(뷰 전용 — 오류 비표기).
            const std::vector<std::string> parsed = st::UserDirs(
                R"({"music":{"dirs":["A:/m","N:\\v","A:/m/"]}})");
            check(parsed.size() == 2 && parsed[0] == "A:/m" &&
                      parsed[1] == "N:/v",
                  "2o-e UserDirs 파싱 = 순서 보존+정규화 접힘(중복 철자 상이)");
            check(st::UserDirs(R"json({"music":{"dirs":[)json").empty() &&
                      st::UserDirs("").empty(),
                  "2o-e UserDirs 파손·빈 원문 = 빈 목록(뷰 전용 — 오류 비표기)");

            // 2o-f) T1 fix r1 보강 — 리뷰 I-1 필수(CP949 무음 소각 봉합)+
            // I-2(RemoveDir 성공 경로 최초 실측)+M-1(빈 리터럴 부적합 출하)+
            // M-2(AddDir 기본 폴더 거부 — RemoveDir 방어선과 대칭). 기존
            // 2o 10건 원문 무변조 — 추가만.
            const std::string cp949 = std::string("\xB0\xA1\xBF\xE4");
            const std::string exeDir3 = (odir / "exe3").string();
            const std::string sp3 = st::SettingsPath(exeDir3);
            fs::create_directories(fs::path(sp3).parent_path());
            std::ofstream(sp3, std::ios::binary)
                << std::string("{\"music\":{\"dirs\":[\"") + cp949 +
                       "\",\"W:/win\"]}}";
            const jk::music::DirWriteResult f1 = st::AddDir(exeDir3, "P:/new");
            std::string cpAfter;
            st::ReadAll(sp3, cpAfter);
            check(f1.ok && cpAfter.find(cp949) != std::string::npos &&
                      st::UserDirs(cpAfter).size() == 3,
                  "2o-f CP949 기존 항목 = AddDir 후 바이트 원문 보존+dirs 수취"
                  "(무음 소각 봉합 — dirs 한정 스캐너 재합성)");
            const std::string exeDir4 = (odir / "exe4").string();
            const jk::music::DirWriteResult g1a =
                st::AddDir(exeDir4, "I:/in");
            const jk::music::DirWriteResult g1 =
                st::RemoveDir(exeDir4, "I:\\in");
            std::string emptied;
            st::ReadAll(st::SettingsPath(exeDir4), emptied);
            check(g1a.ok && g1.ok && st::UserDirs(emptied).empty() &&
                      emptied == "{\"music\":{\"dirs\":[]}}",
                  "2o-f RemoveDir 성공 경로 = 등록→제거→원문 0건(music.dirs "
                  "기본형 유지)");
            std::vector<st::JsonField> strict;
            check(!st::ScanTopLevelFields("{\"a\": ,}", strict),
                  "2o-f 빈 리터럴({\"a\": ,}) = 재합성 부적합 출하(파서 단정)");
            const jk::music::DirWriteResult h1 = st::AddDir(
                exeDir2, jk::music::AudioDirFallback::Path(exeDir2));
            check(!h1.ok && h1.err == "default dir not appendable",
                  "2o-f AddDir 기본 폴더(state/music) = 저장소 거부(RemoveDir "
                  "방어선과 대칭)");

            // 2o-g) T2 — 서버 보존 합성 수형 실측(캐논 계보: 서버 selftest 축이
            // 부재라 순수 부품 자리에서 봉합 — JKWindowServer WriteSettingsKv가
            // 같은 부품을 소비해 미관리 상위 키(music/gallery·장래 키)를
            // 소각하지 않는다(T1 리뷰 C1 교차 작성자 충돌 봉합). 미관리 키는
            // 원문 슬라이스 바이트 그대로(CP949 혼입 원문 포함), 관리 키는 같은
            // 자리 재기입, 부재 관리 키는 말미 신설 — ComposeKeyed 계약.
            std::vector<st::JsonField> svFields;
            // 원문은 서버 기록형(관리 3키 선두+music/gallery 미관리)과 같은
            // 자리 — 관리 3키 같은 자리 재기입+미관리 키 원문 슬라이스를 동시
            // 단정한다(폰 CP949 문서 corpus와 동일 — T1 fix r1 원문).
            const std::string svOrig =
                std::string("{\"audio\":{\"mute\":1,\"volume\":30},") +
                "\"retention\":{\"days\":7},"
                "\"text\":{\"font_path\":\"\",\"font_fallback\":\"\","
                "\"font_scale\":\"1.0\"},"
                "\"music\":{\"dirs\":[\"" + cp949 + "\",\"W:/win\"]}," +
                "\"gallery\":{\"x\":1}}";
            const std::vector<st::JsonField> managed = {
                {"audio", "{\"mute\":0,\"volume\":42}"},
                {"retention", "{\"days\":14}"},
                {"text", "{\"font_path\":\"\",\"font_fallback\":\"\","
                         "\"font_scale\":\"1.0\"}"}};
            check(st::ScanTopLevelFields(svOrig, svFields) &&
                      st::ComposeKeyed(svFields, managed) ==
                          std::string("{\"audio\":{\"mute\":0,\"volume\":42},") +
                              "\"retention\":{\"days\":14},"
                              "\"text\":{\"font_path\":\"\","
                              "\"font_fallback\":\"\","
                              "\"font_scale\":\"1.0\"},"
                              "\"music\":{\"dirs\":[\"" + cp949 +
                              "\",\"W:/win\"]},\"gallery\":{\"x\":1}}",
                  "2o-g 서버 보존 합성 = 미관리 키(music/gallery) 원문 슬라이스 "
                  "바이트 보존(CP949 포함)+관리 3키 같은 자리 재기입(C1 — "
                  "WriteSettingsKv 소비 수형, 캐논 계보: 서버 축 부재로 여기 "
                  "봉합)");
            std::vector<st::JsonField> svFields2;
            check(st::ScanTopLevelFields("{\"gallery\":{\"x\":1}}",
                                         svFields2) &&
                      st::ComposeKeyed(svFields2, managed) ==
                          std::string("{\"gallery\":{\"x\":1},") +
                              "\"audio\":{\"mute\":0,\"volume\":42},"
                              "\"retention\":{\"days\":14},\"text\":{"
                              "\"font_path\":\"\",\"font_fallback\":\"\","
                              "\"font_scale\":\"1.0\"}}",
                  "2o-g 보존 합성 부재 관리 키 = 원문 순서 유지+말미 신설(무원문 "
                  "폴백 경로와 짝 — 미관리 키 원문 무변조)");
            fs::remove_all(odir);  // 사후 소각(무잔산 — 2m-c 원문 수형)
        }

        // 2p) 스캔 취소 (#93 T1 — 플랜 2026-10-10-music-scan-cancel):
        // ListAudioFiles 취소판 — ScanCancelFn(참=취소)을 재귀 경계(진입 =
        // 자식 재귀 직전 동일 경계 — callee 진입 판정이 그 경계를 소유)마다
        // 청구하고, 엔트리 256개마다 보조 체크(단일 디렉터리 폭주 leg도 취소에
        // 응답). 취소 = 부분 수집 폐기(빈 반환). 무인자 오버로드는 항상-false
        // 위임(원존 시맨틱 무변조). 2m/2n/2o 원존 무변조 — 추가만.
        {
            const fs::path pdir = fs::temp_directory_path() / "jk_music_cancel";
            fs::remove_all(pdir);  // 선제거 — 이전 런 잔산(2m-c 원문 수형)
            fs::create_directories(pdir / "sub");
            std::ofstream(pdir / "a.mp3").put('x');
            std::ofstream(pdir / "sub" / "b.mp3").put('x');
            for (int i = 0; i < 300; ++i) {  // 256 보조 체크 도달 트리(302 엔트리)
                std::ofstream(pdir / ("n" + std::to_string(i) + ".flac"))
                    .put('x');
            }
            check(fs::exists(pdir / "a.mp3") &&
                      fs::exists(pdir / "sub" / "b.mp3") &&
                      fs::exists(pdir / "n299.flac"),
                  "2p-a 트리 구성 = 루트 1건+sub 1건+flac 300건 성립(사후 소각 전제)");

            // ①) 사전 취소 — 콜백이 처음부터 참: 진입 경계 1회만 청구되고
            // 트리 열거에 접촉하지 않는다(빈 반환 — 원문 단정).
            int calls1 = 0;
            const std::vector<jk::music::Track> c1 = jk::music::ListAudioFiles(
                pdir.string(), [&calls1] { ++calls1; return true; });
            check(c1.empty() && calls1 == 1,
                  "2p-① 사전 취소 = 즉시 빈 반환(진입 경계 콜백 1회 원문 — "
                  "트리 열거 미접촉)");

            // ②) 카운터 콜백 — 경계 3회째(루트 진입+루트 256 보조+sub 진입)
            // 에 참: 그 전에 이미 수 집된 부분 결과를 폐기해 빈 목록을 반환
            // 하고, 콜백 청구는 첫 참에서 전체 해소 — 상한 = 경계 3곳.
            int calls2 = 0;
            const std::vector<jk::music::Track> c2 = jk::music::ListAudioFiles(
                pdir.string(), [&calls2] { return ++calls2 >= 3; });
            check(c2.empty(),
                  "2p-② 경계 3회째 취소 = 빈 반환(부분 수집 폐기 — 중간 "
                  "스캔 결과 유출 0)");
            check(calls2 >= 2 && calls2 <= 3,
                  "2p-② 호출 상한 = 경계 3곳(루트 진입+256 보조+sub 진입) "
                  "이내(300 엔트리 트리 — 무경계 폭주 0·래치 해소 후 재청구 0)");

            // ③) 루트 선삭제 — 스캔 개시 전에 루트가 사라진 트리: dir 열기
            // 실패 = 독립 실패 원문(ec 중립형) 그대로 빈 반환·crash 0.
            const fs::path qdir =
                fs::temp_directory_path() / "jk_music_cancel_gone";
            fs::create_directories(qdir / "d");
            std::ofstream(qdir / "d" / "x.mp3").put('x');
            fs::remove_all(qdir);  // 선삭제 — 스캔 시작 전 소멸(레이스 수형)
            int calls3 = 0;
            const std::vector<jk::music::Track> c3 = jk::music::ListAudioFiles(
                qdir.string(), [&calls3] { ++calls3; return false; });
            check(c3.empty() && calls3 == 1,
                  "2p-③ 루트 선삭제 = 빈 반환(ec 중립 — crash 0, 콜백은 진입 "
                  "1회 후 dir 열기 실패 복귀)");

            // ④) 무인자 오버로드 계보 불변 — 항상-false 위임 수형: 본 트리
            // 원문 수치(재귀 2건 수취+txt 열외+mtime desc — 2m-c 수형 재용).
            const fs::path tdir = fs::temp_directory_path() / "jk_music_plain";
            fs::remove_all(tdir);
            {
                fs::create_directories(tdir / "sub");
                std::ofstream(tdir / "a.mp3").put('x');
                std::ofstream(tdir / "c.txt").put('x');
                std::ofstream(tdir / "sub" / "w.wav").put('x');
                fs::last_write_time(
                    tdir / "a.mp3",
                    fs::file_time_type::clock::now() + std::chrono::hours(1));
                fs::last_write_time(
                    tdir / "sub" / "w.wav",
                    fs::file_time_type::clock::now() - std::chrono::hours(1));
                const std::vector<jk::music::Track> c4 =
                    jk::music::ListAudioFiles(tdir.string());
                check(c4.size() == 2 && c4[0].name == "a.mp3" &&
                          c4[1].name == "w.wav" &&
                          std::find_if(c4.begin(), c4.end(),
                                       [](const jk::music::Track& t) {
                                           return t.name == "c.txt";
                                       }) == c4.end(),
                      "2p-④ 무인자 오버로드 = 기존 수치 불변(2m-c 원문 수치 — "
                      "항상-false 위임, 원존 시맨틱 0 변조)");
                fs::remove_all(tdir);
            }

            // REMNANT 0 — 사후 소각(무잔산 — 2m-e 정리 수형 재용).
            std::error_code pec;
            fs::remove_all(pdir, pec);
            check(!pec && !fs::exists(pdir, pec) &&
                      !fs::exists(qdir, pec) && !fs::exists(tdir, pec),
                  "2p-⑤ 사후 소각 = 잔산 0(REMNANT 0 — cancel·선삭제·무인자 "
                  "3트리 전부)");
        }

        // 2q) 경로 인코딩 사슬 (#94): 규약 — **와이어/저장/표기 = 항상
        // UTF-8, 파일 터치점 = 네이티브 변환**. 스캔 leg(MusicModel fs —
        // MinGW libstdc++ narrow 바이트는 UTF-8 strict, T2 원장 실측)가
        // 만드는 Track.full은 소비 측에서도 UTF-8 원문이지만, 파일을 여는
        // 쪽 둘레가 그 바이트를 Windows ACP로 해석해 왔다: ①vplayer 존재
        // 게이트(원 narrow CRT fopen — 한글 경로를 못 찾았다 = #94 사용자
        // 보고 "파일을 찾을 수 없습니다" — fs status 수형으로 수리) ②
        // spatial 디코더(dr_wav/dr_mp3/dr_flac·stb_vorbis 내부 narrow fopen
        // — Decoder::open에 네이티브 바이트 전달로 수리, spatial-player
        // 무변경 계약). 2m/2n/2o/2p 원존 무변조 — 추가만.
        {
            // 더미 — 한글 폴더+한글 파일명(mp3 확장자더미): 생성은 fs::path
            // ofstream(네이티브 wide 개방)이라 인코딩에서 자유, 소비는 아래
            // 두 수형(게이트·디코더)이 같은 트리를 건든다. 소각은 말미.
            const fs::path udir = fs::temp_directory_path() / "jk_music_utf8";
            fs::remove_all(udir);  // 선제거 — 이전 런 잔산(2m-c 원문 수형)
            fs::create_directories(udir / "가요 폴더");
            const fs::path dummy = udir / "가요 폴더" / "가나다 테스트 곡.mp3";
            std::ofstream(dummy, std::ios::binary).put('x');
            check(fs::exists(dummy) && fs::file_size(dummy) == 1,
                  "2q-a 한글 경로 더미 구성 = 개방 성립(사후 소각 전제)");

            // ①) 존재 검사 수형 — fs status gate(vplayer OpenStage 수리
            // 원문 수형): UTF-8 bytes narrow 원문이 네이티브로 대조돼 통과
            // 한다(스캔 leg와 같은 인코딩 — 게이트가 한글 경로를 다시
            // 못 찾지 않는다).
            std::error_code uec;
            const std::filesystem::file_status ust =
                std::filesystem::status(dummy.generic_string(), uec);
            check(!uec && std::filesystem::is_regular_file(ust),
                  "2q-a 한글 파일명 존재 검사 = fs status 수형 통과(UTF-8 "
                  "bytes 원문 — ClientVPlayerApp OpenStage 게이트 수리 수형; "
                  "원 narrow fopen은 ACP 해석이라 이 경로를 못 찾았다)");

            const std::string uFull = dummy.generic_string();
            const std::string uNative = jk::text::Utf8ToAnsi(uFull);
            check(!uNative.empty() && jk::text::AnsiToUtf8(uNative) == uFull,
                  "2q-a Utf8ToAnsi→AnsiToUtf8 = 한글 경로 UTF-8 원문 왕복 "
                  "보전(Win CP949 왕복·posix 무변환 항등 — 동일 단정, "
                  "JKTextConv 2함수 계약 — 2q 케이스는 POSIX에서도 통과)");

            // 디코더 내부 narrow fopen 수형 — 네이티브 바이트로 실제 개방
            // 까지 단정(변환 바이트가 ACP로 원 wide 이름에 다시 만난다 —
            // spatial Decoder::open 전달 계약의 봉합; posix는 항등이라 같은
            // 수형이 UTF-8 bytes 개방이 된다).
            std::FILE* uf = std::fopen(uNative.c_str(), "rb");
            const bool uOpened = uf != nullptr;
            if (uf) std::fclose(uf);
            check(uOpened,
                  "2q-a 네이티브 바이트 narrow fopen = 더미 개방(spatial "
                  "디코더 전달 수형 — 원문 UTF-8 bytes는 ACP 해석과 불일치, "
                  "변환 후 성립)");

            // ②) 한글 폴더 루트 스캔 — ListAudioFiles 수취와 Track.full의
            // 유효 UTF-8 원문 단정(스캔 leg 인코딩 규약 — 소비 측 표기·
            // 위계가 그대로라는 진실원).
            const std::string uRoot = (udir / "가요 폴더").generic_string();
            const std::vector<jk::music::Track> q2 =
                jk::music::ListAudioFiles(uRoot);
            check(q2.size() == 1 &&
                      q2[0].full == uRoot + "/가나다 테스트 곡.mp3" &&
                      q2[0].rel == "가나다 테스트 곡.mp3",
                  "2q-b 한글 폴더 루트 스캔 = 트랙 1건 수취+Track.full·rel이 "
                  "UTF-8 원문(슬래시 generic 표기) 그대로(스캔 leg 규약)");
            check(!jk::text::Utf8ToUtf16(q2[0].full).empty() &&
                      jk::text::Utf16ToUtf8(
                          jk::text::Utf8ToUtf16(q2[0].full)) == q2[0].full,
                  "2q-b Track.full 유효 UTF-8 단정(Utf8ToUtf16 fail-closed "
                  "계약 재용+정규 왕복 보전 — 변환 소비 측의 진실원)");

            std::error_code uec2;
            fs::remove_all(udir, uec2);
            check(!uec2 && !fs::exists(udir, uec2),
                  "2q-b 사후 소각 = 잔산 0(REMNANT 0 — 2m-c/2p-⑤ 정리 수형)");
        }

        // 2r) 폴더 브라우저 순수 부품 (#97 T1 — 플랜
        // 2026-10-11-music-dir-browser): 내장 미니 브라우저(폴더 전용 — 플랜
        // 결정 ②)의 모델 leg — browse::ListSubdirs/JoinDir/Parent 3부품.
        // fs 수형 계약 승계: ec 중립형(throwing 금지)·'/' 규약(#94·Slashize
        // 재용)·이름 오름차순 결정론·숨김 .도트 스킵. 2m/2n/2o/2p/2q 원존
        // 무변조 — 추가만. 임시 leg = 2p-④ 재용(temp_directory_path+사후
        // 소각 — engine/tmp 미사용).
        {
            // 임시 트리 — 폴더 3+파일 2+숨김 폴더 1(브리프 ① 수형). 파일명은
            // 전부 ASCII — 오름차순 판정이 인코딩 축(Win/WSL) 무관 결정
            // (2q 규약 — 한글 트리는 인코딩 사슬 전용).
            const fs::path bdir = fs::temp_directory_path() / "jk_music_browse";
            fs::remove_all(bdir);  // 선제거 — 이전 런 잔산(2m-c 원문 수형)
            fs::create_directories(bdir / "alpha");
            fs::create_directories(bdir / "beta");
            fs::create_directories(bdir / "gamma");
            fs::create_directories(bdir / ".hidden_dir");  // 숨김 — 스킵 대상
            std::ofstream(bdir / "t1.mp3", std::ios::binary).put('x');  // 열외
            std::ofstream(bdir / "t2.txt", std::ios::binary).put('x');  // 열외
            check(fs::exists(bdir / "alpha") && fs::exists(bdir / ".hidden_dir") &&
                      fs::file_size(bdir / "t1.mp3") == 1,
                  "2r-a 임시 트리 구성(폴더 3+파일 2+숨김 1) = 성립(사후 소각 전제)");

            // ①) ListSubdirs — 폴더 3건·이름 오름차순·파일/숨김 0건(브리프 ①).
            const jk::music::browse::DirPage page =
                jk::music::browse::ListSubdirs(bdir.generic_string());
            check(page.dirs.size() == 3 && page.dirs[0] == "alpha" &&
                      page.dirs[1] == "beta" && page.dirs[2] == "gamma",
                  "2r-① ListSubdirs = 폴더 3건 이름 오름차순 결정론(파일 2건·"
                  "숨김 .hidden_dir·상위 행 0건 — 폴더 전용·.도트 스킵)");

            // ②) JoinDir — '/' 규약(브리프 ②): 기존 슬래시 cwd·끝 슬래시 cwd가
            // 등치 1슬래시 결합(이중 슬래시 0).
            const std::string broot = bdir.generic_string();
            const std::string joined = jk::music::browse::JoinDir(broot, "sub");
            const std::string joinedTrail =
                jk::music::browse::JoinDir(broot + "/", "sub");
            check(joined == broot + "/sub" && joinedTrail == joined,
                  "2r-② JoinDir = 기존/끝 슬래시 cwd 등치·1슬래시 결합(슬래시 "
                  "규약 #94 — Slashize 재용·이중 구분자 0)");

            // 루트 폼 회귀 봉인 2건(T1 fix r1 — 리뷰 M-1·M-2): Win32가
            // "I://"를 접어(directory_iterator ec=0 실측) 셀프테스트가
            // 이 폼을 영원히 미검출하던 이중 슬래시 + posix 절대 루트가 상대
            // 조립에 흡수되던 폼. 문자 폼 단정 — 축(Win/posix) 무관.
            check(jk::music::browse::JoinDir(std::string("C:/"), "sub") ==
                      std::string("C:/sub"),
                  "2r-b JoinDir 드라이브 루트 = C:/sub(이중 슬래시 0의 실증 "
                  "폼 — I:// 회귀 봉인 — T1 fix r1 M-1)");
            check(jk::music::browse::JoinDir(std::string("/"), "sub") ==
                      std::string("/sub"),
                  "2r-c JoinDir posix 절대 루트 = /sub(루트가 상대 조립에 흡수"
                  "되던 폼 회귀 봉인 — 루트는 반드시 열리는 cwd — T1 fix r1 "
                  "M-2)");

            // ③) Parent — 브리프 ③ 3케이스.
            // 중간: JoinDir 왕복이 cwd 원문 승계(".." 행의 실조작 수형).
            check(jk::music::browse::Parent(joined) == broot,
                  "2r-③ Parent 중간 = 1단 제거(JoinDir 왕복 == cwd 원문 — 상위 "
                  "행 승계 수형)");
            // 루트 표기: 드라이브 루트 유지+posix 절대 루트(스캔이 뒤에서도
            // 성립하는 표기 — "I:" 드라이브-상대 함정 회피 원문).
            check(jk::music::browse::Parent(std::string("I:/music")) ==
                          std::string("I:/") &&
                      jk::music::browse::Parent(std::string("/a")) ==
                          std::string("/"),
                  "2r-④ Parent 루트 표기 = 드라이브 루트 유지(I:/music→I:/ — "
                  "드라이브-상대 함정 회피)+posix 절대 루트(/a→/)");
            // 루트의 부모 — 부자기 금지(어샥션): 전부 빈 문자열.
            check(jk::music::browse::Parent(std::string("alpha")).empty() &&
                      jk::music::browse::Parent(std::string("/")).empty() &&
                      jk::music::browse::Parent(std::string("C:/")).empty(),
                  "2r-⑤ Parent 루트의 부모 = 빈 문자열(부자기 금지 — 단일 성분/"
                  "/문자 루트/C:/ 3폼 전부 — fs::path::parent_path 자기 함정 "
                  "회피)");

            // ④) 존재하지 않는 cwd = 빈 페이지(브리프 ④ — ec 중립형).
            const jk::music::browse::DirPage missing = jk::music::browse::
                ListSubdirs((bdir / "no_such_dir").generic_string());
            check(missing.dirs.empty(),
                  "2r-⑥ 존재하지 않는 cwd = 빈 페이지(ec 중립형·crash 0 — "
                  "독립 실패 원문 계약)");

            // REMNANT 0 — 사후 소각(무잔산 — 2m-e/2p-⑤/2q-b 정리 수형 재용).
            std::error_code bec;
            fs::remove_all(bdir, bec);
            check(!bec && !fs::exists(bdir, bec),
                  "2r-⑦ 사후 소각 = 잔산 0(REMNANT 0 — 정리 수형 재용)");
        }

        // 2s) 태그 파서 (#99 T1 — 스펙 2026-10-11-music-tags-design):
        // jk::music::tags 순수 부품 — ID3v2.3/2.4(TIT2/TPE1/TALB·인코딩
        // 0/1/2/3)·ID3v1 폴백·vorbis 코멘트(FLAC 블록/Ogg 페이지 — own 부품,
        // dr_flac/stb_vorbis 메타 API는 헤더 재용 불가 실측 — 리포트 §1)·
        // WAV RIFF "id3 " 청크·ParseTag 파일 접점(D2 필요 범위만 read).
        // 합성 원문 블롭 = inline hex byte 배열(리터럴 — FS 접촉 최소 계약;
        // 블롭 원문은 리포트에 16진 표기로 보존). 2m/2n/2o/2p/2q/2r 원존
        // 무변조 — 추가만. FS 접점 leg = 2r 수형(temp_directory_path+사후
        // 소각 REMNANT 0 — 임시 파일 3건).
        {
            // ---- 합성 원문 블롭 (inline hex 배열 — 리포트 원문) ----
            // v2.4 UTF-8: TIT2"가을 노래"+TPE1"아티스트 김"+TALB"앨범 모음"
            // +TRCK(스코프 밖 프레임 — 스킵 증명용), len 98.
            static const unsigned char kBlobV24[] = {
                0x49, 0x44, 0x33, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58,
                0x54, 0x49, 0x54, 0x32, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00,
                0x03, 0xEA, 0xB0, 0x80, 0xEC, 0x9D, 0x84, 0x20, 0xEB, 0x85,
                0xB8, 0xEB, 0x9E, 0x98, 0x54, 0x50, 0x45, 0x31, 0x00, 0x00,
                0x00, 0x11, 0x00, 0x00, 0x03, 0xEC, 0x95, 0x84, 0xED, 0x8B,
                0xB0, 0xEC, 0x8A, 0xA4, 0xED, 0x8A, 0xB8, 0x20, 0xEA, 0xB9,
                0x80, 0x54, 0x41, 0x4C, 0x42, 0x00, 0x00, 0x00, 0x0E, 0x00,
                0x00, 0x03, 0xEC, 0x95, 0xA8, 0xEB, 0xB2, 0x94, 0x20, 0xEB,
                0xAA, 0xA8, 0xEC, 0x9D, 0x8C, 0x54, 0x52, 0x43, 0x4B, 0x00,
                0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x30, 0x35,
            };
            // v2.3 인코딩 0: TIT2 "Éÿ"(E9 FF) — Latin-1 승격 원문, len 23.
            static const unsigned char kBlobV23Latin[] = {
                0x49, 0x44, 0x33, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D,
                0x54, 0x49, 0x54, 0x32, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
                0x00, 0xE9, 0xFF,
            };
            // v2.4 UTF-16LE(BOM): TIT2 enc 01 "Hi", len 29.
            static const unsigned char kBlobV24U16le[] = {
                0x49, 0x44, 0x33, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13,
                0x54, 0x49, 0x54, 0x32, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00,
                0x01, 0xFF, 0xFE, 0x48, 0x00, 0x69, 0x00, 0x00, 0x00,
            };
            // v2.4 UTF-16BE(enc 02, 무BOM): TIT2 "Hi", len 25.
            static const unsigned char kBlobV24U16be[] = {
                0x49, 0x44, 0x33, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F,
                0x54, 0x49, 0x54, 0x32, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00,
                0x02, 0x00, 0x48, 0x00, 0x69,
            };
            // v2.4 footer flag 0x10: TIT2"Pad Test"+12바이트 패딩+footer
            // "3DI…"(태그 크기 밖 — 자연 스킵), len 54.
            static const unsigned char kBlobV24PadFooter[] = {
                0x49, 0x44, 0x33, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00, 0x1F,
                0x54, 0x49, 0x54, 0x32, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00,
                0x00, 0x50, 0x61, 0x64, 0x20, 0x54, 0x65, 0x73, 0x74, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x33, 0x44, 0x49, 0x49, 0x44, 0x33, 0x04, 0x00, 0x10, 0x00,
                0x00, 0x00, 0x1F,
            };
            // v2.4 파산 블롭: declared size = syncsafe 최대 7F 7F 7F 7F
            // (2^28-1) + 실제 24바이트 — 경계 클램프 원문, len 24.
            static const unsigned char kBlobV24Huge[] = {
                0x49, 0x44, 0x33, 0x04, 0x00, 0x00, 0x7F, 0x7F, 0x7F, 0x7F,
                0x54, 0x49, 0x54, 0x32, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
                0x00, 0x42, 0x69, 0x67,
            };
            // FLAC 합성: "fLaC"+STREAMINFO(34B·마지막 아님)+PADDING(8B —
            // 사슬 절차 스킵 증명)+VORBIS_COMMENT(last=1·vendor"JKEN"+3
            // 엔트리), len 132. (블록 크기 = 24BE — FLAC 메타 블록 규약)
            static const unsigned char kBlobFlac[] = {
                0x66, 0x4C, 0x61, 0x43, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x84, 0x00, 0x00, 0x4A, 0x04, 0x00,
                0x00, 0x00, 0x4A, 0x4B, 0x45, 0x4E, 0x03, 0x00, 0x00, 0x00,
                0x10, 0x00, 0x00, 0x00, 0x54, 0x49, 0x54, 0x4C, 0x45, 0x3D,
                0x46, 0x6C, 0x61, 0x63, 0x20, 0x54, 0x69, 0x74, 0x6C, 0x65,
                0x12, 0x00, 0x00, 0x00, 0x41, 0x52, 0x54, 0x49, 0x53, 0x54,
                0x3D, 0x46, 0x6C, 0x61, 0x63, 0x20, 0x41, 0x72, 0x74, 0x69,
                0x73, 0x74, 0x10, 0x00, 0x00, 0x00, 0x41, 0x4C, 0x42, 0x55,
                0x4D, 0x3D, 0x46, 0x6C, 0x61, 0x63, 0x20, 0x41, 0x6C, 0x62,
                0x75, 0x6D,
            };
            // Ogg 합성: 2 페이지(①"\x01vorbis" 헤더 패킷 ②"\x03vorbis"
            // 코멘트 패킷 — vendor"jkog"+2 엔트리), len 129.
            static const unsigned char kBlobOgg[] = {
                0x4F, 0x67, 0x67, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0E, 0x01, 0x76,
                0x6F, 0x72, 0x62, 0x69, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x4F, 0x67, 0x67, 0x53, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04,
                0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3B,
                0x03, 0x76, 0x6F, 0x72, 0x62, 0x69, 0x73, 0x04, 0x00, 0x00,
                0x00, 0x6A, 0x6B, 0x6F, 0x67, 0x02, 0x00, 0x00, 0x00, 0x0F,
                0x00, 0x00, 0x00, 0x54, 0x49, 0x54, 0x4C, 0x45, 0x3D, 0x4F,
                0x67, 0x67, 0x20, 0x54, 0x69, 0x74, 0x6C, 0x65, 0x11, 0x00,
                0x00, 0x00, 0x41, 0x52, 0x54, 0x49, 0x53, 0x54, 0x3D, 0x4F,
                0x67, 0x67, 0x20, 0x41, 0x72, 0x74, 0x69, 0x73, 0x74,
            };
            // WAV 합성: RIFF+WAVE+"id3 " 청크(payload = v2.3 TIT2"Wav
            // Song" — ParseId3v2 재용), len 50.
            static const unsigned char kBlobWavId3[] = {
                0x52, 0x49, 0x46, 0x46, 0x2A, 0x00, 0x00, 0x00, 0x57, 0x41,
                0x56, 0x45, 0x69, 0x64, 0x33, 0x20, 0x1D, 0x00, 0x00, 0x00,
                0x49, 0x44, 0x33, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13,
                0x54, 0x49, 0x54, 0x32, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00,
                0x00, 0x57, 0x61, 0x76, 0x20, 0x53, 0x6F, 0x6E, 0x67, 0x00,
            };
            namespace mt = jk::music::tags;

            // ①) v2.4 UTF-8 — 3필드 원문+TRCK프레임 스킵(싱크 부재·태그
            // 오염 0).
            const mt::TagSet t24 =
                mt::ParseId3v2(kBlobV24, sizeof(kBlobV24));
            check(t24.title == "가을 노래" && t24.artist == "아티스트 김" &&
                      t24.album == "앨범 모음",
                  "2s-① ID3v2.4 UTF-8 = TIT2/TPE1/TALB 3필드 원문(TRCK는 "
                  "싱크 부재 스킵 — 오염 0)");

            // ②) Latin-1 승격 — 원장 명시 규약(바이트<0x80 등가·0x80-0xFF =
            // 2바이트 UTF-8, 0xC2/0xC3 범위 — CP949 아님).
            check(mt::Latin1ToUtf8(reinterpret_cast<const unsigned char*>(
                                       "\xE9\xFF"),
                                   2) ==
                          std::string("\xC3\xA9\xC3\xBF") &&
                      mt::Latin1ToUtf8(
                          reinterpret_cast<const unsigned char*>("a\x7F"),
                          2) == std::string("a\x7F"),
                  "2s-② Latin-1 승격 = 0x80-0xFF는 2바이트 UTF-8(E9→C3A9·"
                  "FF→C3BF), <0x80은 바이트 등가(원장 규약 — CP949 아님)");
            const mt::TagSet t23 =
                mt::ParseId3v2(kBlobV23Latin, sizeof(kBlobV23Latin));
            check(t23.title == std::string("\xC3\xA9\xC3\xBF"),
                  "2s-② ID3v2.3 인코딩 0 = éÿ 승격 원문(E9 FF → C3A9 C3BF)");

            // ③) UTF-16(BOM) — enc 1 = BOM 절취(무BOM = LE 계약)·enc 2 = BE.
            const mt::TagSet t16le =
                mt::ParseId3v2(kBlobV24U16le, sizeof(kBlobV24U16le));
            check(t16le.title == "Hi",
                  "2s-③ v2.4 UTF-16LE(BOM) = BOM 절취+LE 디코드(Hi · 무BOM은 "
                  "LE — v1 확장 계약)");
            const mt::TagSet t16be =
                mt::ParseId3v2(kBlobV24U16be, sizeof(kBlobV24U16be));
            check(t16be.title == "Hi",
                  "2s-③ v2.4 UTF-16BE(enc 2·무BOM) = BE 디코드 원문(Hi)");

            // ④) ID3v1 폴백 — 말미 128B("TAG"+30/30/30, 스페이스 패딩 러스트립)
            // + 더 긴 버퍼의 말미 지목.
            std::vector<unsigned char> v1buf(128, ' ');
            std::memcpy(v1buf.data(), "TAG", 3);
            std::memcpy(v1buf.data() + 3, "Legacy Title", 12);
            std::memcpy(v1buf.data() + 33, "Old Artist", 10);
            std::memcpy(v1buf.data() + 63, "Old Album", 9);
            std::memcpy(v1buf.data() + 93, "1999", 4);
            const mt::TagSet tv1 =
                mt::ParseId3v1(v1buf.data(), v1buf.size());
            check(tv1.title == "Legacy Title" && tv1.artist == "Old Artist" &&
                      tv1.album == "Old Album",
                  "2s-④ ID3v1 = 말미 128B 3필드(스페이스 패딩 러스트립·Latin-1 "
                  "승격 — 폴백 계약)");
            std::vector<unsigned char> junk(40, 0x00);
            junk.insert(junk.end(), v1buf.begin(), v1buf.end());
            const mt::TagSet tv1b =
                mt::ParseId3v1(junk.data(), junk.size());
            check(tv1b.title == "Legacy Title" && tv1b.artist == "Old Artist",
                  "2s-④ ID3v1 말미 지목 = 앞 쓰레기 40B 뒤에도 말미 128B 원문 "
                  "해독(부재 <128B·매직 불일치 = 빈 TagSet)");

            // ⑤) syncsafe 경계 — 0x7F 7F 7F 7F = 0x0FFFFFFF(2^28-1 — syncsafe32
            // 최대)·선언이 버퍼를 넘는 파산 블롭 = 클램프(선언을 믿지 않음).
            const unsigned char ss4[4] = { 0x7F, 0x7F, 0x7F, 0x7F };
            check(mt::SyncSafe32(ss4) == 268435455ULL,
                  "2s-⑤ syncsafe 경계 = 0x7F 7F 7F 7F → 268435455(2^28-1 — "
                  "syncsafe32 최대·상위 비트 마스크)");
            const mt::TagSet thuge =
                mt::ParseId3v2(kBlobV24Huge, sizeof(kBlobV24Huge));
            check(thuge.title == "Big" && mt::TagEmpty(thuge) == false,
                  "2s-⑤ 파산 블롭(선언 2^28-1, 실체 24B) = title Big(경계 "
                  "클램프 — 접점 오버플로 0·선언을 믿지 않음)");

            // ⑥) 패딩+푸터 — 프레임 사슬은 선단 0x00(패딩)에서 종료, 푸터("3DI")
            // 는 태그 크기 밖 자연 스킵.
            const mt::TagSet tpd =
                mt::ParseId3v2(kBlobV24PadFooter, sizeof(kBlobV24PadFooter));
            check(tpd.title == "Pad Test",
                  "2s-⑥ 패딩+푸터 = title Pad Test(프레임 사슬 패딩 종료+"
                  "footer 3DI 스킵 — v2.4 footer flag 0x10)");

            // ⑦) FLAC 합성 1건 — own vorbis 코멘트 부품(dr_flac 메타 API는
            // 헤더 재용 불가 실측 — 리포트 §1). PADDING 블록 사슬 절차 증명
            // +VORBIS_COMMENT(last=1).
            const mt::TagSet tfl =
                mt::ParseFlacTags(kBlobFlac, sizeof(kBlobFlac));
            check(tfl.title == "Flac Title" && tfl.artist == "Flac Artist" &&
                      tfl.album == "Flac Album",
                  "2s-⑦ FLAC 합성 = vorbis 코멘트 3필드(own 부품 — "
                  "STREAMINFO/PADDING 사슬 절차 스킵·VORBIS_COMMENT last=1)");

            // ⑧) Ogg 합성 — "\x03vorbis" 코멘트 패킷(page 2) 지목+ album 부재
            // = 빈(필드별 부재 계약).
            const mt::TagSet tog = mt::ParseOggTags(kBlobOgg, sizeof(kBlobOgg));
            check(tog.title == "Ogg Title" && tog.artist == "Ogg Artist" &&
                      tog.album.empty(),
                  "2s-⑧ Ogg 합성 = 코멘트 패킷 2건 필드(album 부재 = 빈 — "
                  "페이지 연쇄 페이로드 수형)");

            // ⑨-⑩) ParseTag 파일 접점 — FS 접촉 최소(3건 — 2r 수형 재용).
            const fs::path tdir = fs::temp_directory_path() / "jk_music_tags";
            fs::remove_all(tdir);  // 선제거 — 이전 런 잔산(2m-c 원문 수형)
            fs::create_directories(tdir);
            {
                // v1 원존 mp3(매직 없음 — 확장자 보조 분기) + 쓰레기 바이트.
                std::vector<unsigned char> body(90, 0x11);
                body.insert(body.end(), v1buf.begin(), v1buf.end());
                std::ofstream(tdir / "t_v1.mp3", std::ios::binary)
                    .write(reinterpret_cast<const char*>(body.data()),
                           static_cast<std::streamsize>(body.size()));
                std::ofstream(tdir / "t_flac.flac", std::ios::binary)
                    .write(reinterpret_cast<const char*>(kBlobFlac),
                           static_cast<std::streamsize>(sizeof(kBlobFlac)));
                std::ofstream(tdir / "t_unk.bin", std::ios::binary).put('x');
                check(fs::file_size(tdir / "t_v1.mp3") == body.size() &&
                          fs::file_size(tdir / "t_flac.flac") ==
                              sizeof(kBlobFlac),
                      "2s-a 파스 파일 3건 구성 = 성립(사후 소각 전제 — v1 "
                      "mp3·flac·미지 bin)");
            }
            // ⑩) v1 폴백 — v2 부재 mp3 = 말미 128B만 read(필요 범위 규약).
            std::string err1;
            const mt::TagSet pf1 = mt::ParseTag(
                (tdir / "t_v1.mp3").generic_string(), err1);
            check(pf1.title == "Legacy Title" && pf1.artist == "Old Artist" &&
                      err1.empty(),
                  "2s-⑩ ParseTag v1 폴백 = 매직 없는 mp3의 말미 128B read+"
                  "필드 원문(err 빈 — 정상 경로)");
            // ⑩) FLAC 매직 분기 — 파일 접점이 블롭판과 같은 원문 수취.
            std::string err2;
            const mt::TagSet pf2 = mt::ParseTag(
                (tdir / "t_flac.flac").generic_string(), err2);
            check(pf2.title == "Flac Title" && pf2.artist == "Flac Artist" &&
                      pf2.album == "Flac Album" && err2.empty(),
                  "2s-⑩ ParseTag FLAC 매직 분기 = 블롭판 동일 3필드(fLaC "
                  "수형 — dr_flac 재용 불가 실측의 대체 leg)");
            // ⑩) 미지 매직+비 mp3 확장자 = 빈 TagSet+err 빈(파일명 폴백 —
            // D2: 오류 표기 없이 정상 경로).
            std::string err3;
            const mt::TagSet pf3 =
                mt::ParseTag((tdir / "t_unk.bin").generic_string(), err3);
            check(mt::TagEmpty(pf3) && err3.empty(),
                  "2s-⑩ ParseTag 미지 매직 = 빈 TagSet+err 빈(파일명 폴백 = "
                  "정상 경로 — D2 오류 표기 없음)");
            // ⑩) 부재 경로 = 빈 TagSet+err "open"(I/O 실패만 진단 표기).
            std::string err4;
            const mt::TagSet pf4 = mt::ParseTag(
                (tdir / "no_such.mp3").generic_string(), err4);
            check(mt::TagEmpty(pf4) && !err4.empty(),
                  "2s-⑩ ParseTag 부재 경로 = 빈 TagSet+err 표기(개방 실패 — "
                  "폴백 계약·표기 없이 정상 복귀)");

            // ⑨) 필터 확장 — MatchFilter가 곡명/아티스트에도 붙는다(D5 —
            // 표기 데이터 자리는 T2 UI가 조립). ASCII fold(2m-d 수형 재용)
            // +한글 이진 비교 부분일치.
            const mt::TagSet tflt = tfl;
            check(jk::music::MatchFilter(tflt.title, "flac") &&
                      jk::music::MatchFilter(tflt.artist, "artist") &&
                      jk::music::MatchFilter(t24.title, "노래"),
                  "2s-⑨ 필터 확장 = 곡명/아티스트 매치(ASCII fold 대소문자 "
                  "무시+한글 이진 비교 — MatchFilter 재용·D5)");

            // ⑩) WAV — RIFF "id3 " 청크(payload = ID3v2.3 재용).
            const mt::TagSet twav =
                mt::ParseWavTags(kBlobWavId3, sizeof(kBlobWavId3));
            check(twav.title == "Wav Song",
                  "2s-⑩ WAV id3 청크 = title Wav Song(RIFF 사슬 — dr_wav "
                  "LIST INFO 밖 스코프 실측의 대체 leg)");

            // REMNANT 0 — 사후 소각(무잔산 — 2p-⑤/2r-⑦ 정리 수형 재용).
            std::error_code sec;
            fs::remove_all(tdir, sec);
            check(!sec && !fs::exists(tdir, sec),
                  "2s-⑩ 사후 소각 = 잔산 0(REMNANT 0 — 파스 파일 3건)");
        }

        // 2u) 태그 쓰기 + 커버아트 (#111 T1 ①-⑦·T2 ⑧-⑭·T4 ⑮ 도구 경계 — 스펙 2026-10-25-music-tagwrite):
        // 읽기 사슬(2s)의 역방향 — WriteId3v2.3 정규화(D5·v2.3 신설/정규화
        // 재작성·v1 유지)·APIC 커버 삽입/보존(cover null = 기존 APIC 유지)·
        // D6 원자적 쓰기(.pid.tagtmp→fsync→rename)·실패 정직 거부(원본 등호);
        // T2 = FLAC 블록 재조립(D8 PICTURE/D9 보존)·Ogg 코멘트 페이지 재조립
        // (D10 — lacing 원문·granule 불변)·WAV id3 청크 교체/신설+RIFF 재계산
        // ·ReadCover 3형식·독립 판정판(CRC/lacing/RIFF 산술 — 구현 재용 금지).
        // 합성 블롭 = inline byte 배열(2s 원문 수형 — 테스트 음성/이미지는
        // 합성 블롭만 계약), FS 접점 leg = 2s/2r 수형(temp_directory_path+
        // 사후 소각 REMNANT 0). 기존 계보(2a..2s) 원문 무변조 — 추가만.
        {
            namespace mt = jk::music::tags;

            const fs::path tdir =
                fs::temp_directory_path() / "jk_music_tagwrite";
            fs::remove_all(tdir);  // 선제거 — 이전 런 잔산(2s/2r 원문 수형)
            fs::create_directories(tdir);

            // ---- 합성 헬퍼 (테스트 전용 — 2s/2s-④ 수형 재용) ----
            auto putFile = [](const fs::path& p,
                              const std::vector<unsigned char>& bytes) {
                std::ofstream f(p, std::ios::binary);
                f.write(reinterpret_cast<const char*>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size()));
                f.close();
                return fs::file_size(p) == bytes.size();
            };
            auto readFile = [](const fs::path& p, std::string& out) {
                out.clear();
                std::ifstream f(p, std::ios::binary);
                char buf[4096];
                while (f.read(buf, sizeof(buf)) || f.gcount() > 0)
                    out.append(buf, static_cast<size_t>(f.gcount()));
                return !f.bad();
            };
            auto hasBytes = [](const std::string& hay,
                               const unsigned char* needle, size_t n) {
                return hay.find(reinterpret_cast<const char*>(needle), 0, n)
                       != std::string::npos;
            };
            auto pushBE32 = [](std::vector<unsigned char>& v, unsigned int x) {
                for (int sh = 24; sh >= 0; sh -= 8)
                    v.push_back(static_cast<unsigned char>((x >> sh) & 0xFF));
            };
            auto pushSyncsafe = [](std::vector<unsigned char>& v,
                                   unsigned int x) {
                for (int sh = 21; sh >= 0; sh -= 7)
                    v.push_back(static_cast<unsigned char>((x >> sh) & 0x7F));
            };
            // 프레임 합성 — syncsafeSize = v2.4(syncsafe 크기 필드), 아니면
            // v2.3(BE32). 본문은 10B 헤더 아래 그대로.
            auto mkFrame = [&](const char* id, bool syncsafeSize,
                               const std::vector<unsigned char>& body) {
                std::vector<unsigned char> f;
                f.insert(f.end(), id, id + 4);
                if (syncsafeSize)
                    pushSyncsafe(f, static_cast<unsigned int>(body.size()));
                else
                    pushBE32(f, static_cast<unsigned int>(body.size()));
                f.push_back(0);  // 플래그
                f.push_back(0);  // 포맷
                f.insert(f.end(), body.begin(), body.end());
                return f;
            };
            auto mkV24Tag = [&](const std::vector<std::vector<unsigned char>>&
                                    frames) {
                std::vector<unsigned char> b;
                for (const auto& one : frames)
                    b.insert(b.end(), one.begin(), one.end());
                std::vector<unsigned char> t = {'I', 'D', '3', 0x04, 0x00,
                                                0x00};
                pushSyncsafe(t, static_cast<unsigned int>(b.size()));
                t.insert(t.end(), b.begin(), b.end());
                return t;
            };
            std::vector<unsigned char> audio(96);
            for (size_t i = 0; i < audio.size(); ++i)
                audio[i] = static_cast<unsigned char>(i ^ 0xA5);
            // 합성 jpeg 블롭 512B(SOI 마커 원문 형식+결정론 패턴 — inline).
            std::vector<unsigned char> jpg(512);
            jpg[0] = 0xFF; jpg[1] = 0xD8; jpg[2] = 0xFF; jpg[3] = 0xE0;
            for (size_t i = 4; i < jpg.size(); ++i)
                jpg[i] = static_cast<unsigned char>(i * 7 + 3);

            // ①) 합성 mp3(무태그 — 매agic 없는 raw leg) v2.3 신설 —
            // TIT2/TPE1/TALB+패딩 1KB → ParseTag round-trip(D5 신설 leg).
            {
                const fs::path p = tdir / "2u_a_create.mp3";
                mt::TagSet t1;
                t1.title = "New Title";
                t1.artist = "New Artist";
                t1.album = "New Album";
                check(putFile(p, audio),
                      "2u-① 합성 mp3(무태그) 구성 = 96B 성립(신설 대상 원본)");
                std::string err1;
                check(mt::WriteTag(p.generic_string(), t1, nullptr, err1) &&
                          err1.empty(),
                      "2u-① v2.3 신설 쓰기 = ok+err 빈(v2 부재 mp3 — D5 신설 "
                      "leg·WriteTag 계약 시그니처)");
                std::string bytes1;
                check(readFile(p, bytes1), "2u-① 재독 = 파일 수취(구조 단정)");
                check(bytes1.size() >= 10 && bytes1[0] == 'I' &&
                          bytes1[1] == 'D' && bytes1[2] == '3' &&
                          static_cast<unsigned char>(bytes1[3]) == 0x03 &&
                          static_cast<unsigned char>(bytes1[5]) == 0x00,
                      "2u-① 태그 헤더 = ID3+v2.3+전역 플래그 0(D5 정규화 출력 "
                      "— 신설)");
                // 패딩 1KB+오디오 원문 — 말미 96B 등호, 그 앞 1024B 전부 0x00.
                const size_t sz1 = bytes1.size();
                bool padOk = sz1 > 1024 + 96 + 10;
                for (size_t i = sz1 - 96 - 1024; padOk && i < sz1 - 96; ++i)
                    padOk = static_cast<unsigned char>(bytes1[i]) == 0;
                check(padOk && sz1 > 96 &&
                          std::memcmp(bytes1.data() + sz1 - 96, audio.data(),
                                      96) == 0,
                      "2u-① 패딩 1KB+오디오 원문 = 말미 96B 등호+그 앞 1024B "
                      "0x00(신설 패딩 계약)");
                std::string err1b;
                const mt::TagSet back1 =
                    mt::ParseTag(p.generic_string(), err1b);
                check(back1.title == "New Title" &&
                          back1.artist == "New Artist" &&
                          back1.album == "New Album" && err1b.empty(),
                      "2u-① ParseTag round-trip = 3필드 등호(신설 쓰기 → 읽기 "
                      "사슬 재용 — SyncSafe32 태그 크기 원존)");
            }

            // ②) 기존 v2.4 파일 → v2.3 정규화 재작성 → 재독(D5) — TCON 복제와
            // 프레임 원문 보존(크기 필드 BE32 역변환 — 201B 본문 TXXX 증명).
            {
                const fs::path p = tdir / "2u_b_v24.mp3";
                std::vector<unsigned char> audio2(48);
                for (size_t i = 0; i < audio2.size(); ++i)
                    audio2[i] = static_cast<unsigned char>(0x30 + i);
                std::vector<unsigned char> bTIT2;
                bTIT2.push_back(0x03);  // enc 3 — UTF-8(v2.4 규약)
                bTIT2.insert(bTIT2.end(), "Old Title", "Old Title" + 9);
                std::vector<unsigned char> bTPE1;
                bTPE1.push_back(0x00);  // enc 0 — Latin-1
                bTPE1.insert(bTPE1.end(), "Old Artist", "Old Artist" + 10);
                std::vector<unsigned char> bTALB;
                bTALB.push_back(0x03);
                bTALB.insert(bTALB.end(), "Old Album", "Old Album" + 9);
                std::vector<unsigned char> bTCON;
                bTCON.push_back(0x00);
                bTCON.insert(bTCON.end(), "Rock", "Rock" + 4);  // 장르(복제 대상)
                std::vector<unsigned char> bTXXX(201, 'A');
                bTXXX[0] = 0x00;  // 본문 201B — 크기 필드 인코딩 구별 증명용
                std::vector<std::vector<unsigned char>> frames2;
                frames2.push_back(mkFrame("TIT2", true, bTIT2));
                frames2.push_back(mkFrame("TPE1", true, bTPE1));
                frames2.push_back(mkFrame("TALB", true, bTALB));
                frames2.push_back(mkFrame("TCON", true, bTCON));
                frames2.push_back(mkFrame("TXXX", true, bTXXX));
                std::vector<unsigned char> file2 = mkV24Tag(frames2);
                file2.insert(file2.end(), audio2.begin(), audio2.end());
                check(putFile(p, file2),
                      "2u-② v2.4 원본 구성 = 성립(5프레임+오디오 48B)");
                std::string err2pre;
                const mt::TagSet pre2 =
                    mt::ParseTag(p.generic_string(), err2pre);
                check(pre2.title == "Old Title" &&
                          pre2.artist == "Old Artist" &&
                          pre2.album == "Old Album" && err2pre.empty(),
                      "2u-② v2.4 원본 재독 = 3필드(읽기 사슬 원문 — 정규화 전제)");
                mt::TagSet t2;
                t2.title = "Rewritten Title";
                t2.artist = "Rewritten Artist";
                t2.album = "Rewritten Album";
                std::string err2;
                check(mt::WriteTag(p.generic_string(), t2, nullptr, err2) &&
                          err2.empty(),
                      "2u-② v2.3 정규화 재작성 = ok(D5 — 기존 v2 유지 계약)");
                std::string bytes2;
                check(readFile(p, bytes2), "2u-② 재작성 파일 수취(구조 단정)");
                check(bytes2.size() > 10 &&
                          static_cast<unsigned char>(bytes2[3]) == 0x03 &&
                          static_cast<unsigned char>(bytes2[5]) == 0x00,
                      "2u-② 재작성 버전/플래그 = v2.3+평문(D5 — v2.4 syncsafe "
                      "재작성 금지)");
                std::string err2b;
                const mt::TagSet back2 =
                    mt::ParseTag(p.generic_string(), err2b);
                check(back2.title == "Rewritten Title" &&
                          back2.artist == "Rewritten Artist" &&
                          back2.album == "Rewritten Album" && err2b.empty(),
                      "2u-② 재독 = 새 3필드 등호(정규화 round-trip)");
                // TCON 복제(장르 보존) — 원본 프레임(10B+본문) 바이트 등호 생존:
                // 복제는 본문+헤더 재기입(BE32 5 = 원본 syncsafe 5와 동일 바이트).
                std::vector<unsigned char> tconExp("TCON", "TCON" + 4);
                pushBE32(tconExp, 5);
                tconExp.push_back(0);  // 플래그
                tconExp.push_back(0);  // 포맷
                tconExp.push_back(0x00);
                tconExp.insert(tconExp.end(), "Rock", "Rock" + 4);
                check(hasBytes(bytes2, tconExp.data(), tconExp.size()),
                      "2u-② TCON 복제 = 원본 프레임 바이트 등호 생존(장르 보존"
                      " — D5 프레임 원문 계약)");
                // TXXX 보존 — v2.4 syncsafe(00 00 03 09) 201B → v2.3 BE32
                // (00 00 00 C9) 역변환+본문 원문 생존(drop 금지 계약).
                std::vector<unsigned char> txxx("TXXX", "TXXX" + 4);
                pushBE32(txxx, 201);
                txxx.push_back(0);
                txxx.push_back(0);
                check(hasBytes(bytes2, txxx.data(), txxx.size()),
                      "2u-② TXXX 보존 헤더 = 크기 필드 BE32 역변환(00 00 00 C9 "
                      "— 32bit BE·syncsafe 아님 — 모호성 해소 원문)");
                check(hasBytes(bytes2, bTXXX.data(), bTXXX.size()),
                      "2u-② TXXX 본문 원문 = 201B 등호 생존(해석 불가 프레임 "
                      "drop 금지 — D5)");
                check(bytes2.size() > audio2.size() &&
                          std::memcmp(
                              bytes2.data() + bytes2.size() - audio2.size(),
                              audio2.data(), audio2.size()) == 0,
                      "2u-② 오디오 원문 = 말미 48B 등호(사슬 재구성 — 재배치 "
                      "무손상)");
            }

            // ③) v1-원존 파일 → v2 신설+v1 유지 — 재독 = v2 우선 규약(원존).
            {
                const fs::path p = tdir / "2u_c_v1only.mp3";
                std::vector<unsigned char> v1buf(128, ' ');
                std::memcpy(v1buf.data(), "TAG", 3);
                std::memcpy(v1buf.data() + 3, "Legacy Title", 12);
                std::memcpy(v1buf.data() + 33, "Old Artist", 10);
                std::memcpy(v1buf.data() + 63, "Old Album", 9);
                std::memcpy(v1buf.data() + 93, "1999", 4);
                std::vector<unsigned char> file3(40, 0x22);
                file3.insert(file3.end(), v1buf.begin(), v1buf.end());
                check(putFile(p, file3),
                      "2u-③ v1-원존 mp3 구성 = 성립(junk 40B+v1 128B)");
                mt::TagSet t3;
                t3.title = "V2 Title";
                t3.artist = "V2 Artist";
                t3.album = "V2 Album";
                std::string err3;
                check(mt::WriteTag(p.generic_string(), t3, nullptr, err3) &&
                          err3.empty(),
                      "2u-③ v2 신설 쓰기 = ok(v1 존재 파일 — 매agic 없음 leg)");
                std::string err3b;
                const mt::TagSet back3 =
                    mt::ParseTag(p.generic_string(), err3b);
                check(back3.title == "V2 Title" &&
                          back3.artist == "V2 Artist" &&
                          back3.album == "V2 Album" && err3b.empty(),
                      "2u-③ 재독 = v2 우선(v1 존재에도 v2 값 — ParseTag 규약 "
                      "원존)");
                std::string bytes3;
                check(readFile(p, bytes3) && bytes3.size() >= 128 &&
                          std::memcmp(bytes3.data() + bytes3.size() - 128,
                                      v1buf.data(), 128) == 0,
                      "2u-③ v1 유지 = 말미 128B 등호(D5 — ID3v1 원문 보존)");
            }

            // ④) 한글 UTF-8 입력 → v2.3 인코딩 1(UTF-16LE+0xFF 0xFE BOM)
            // round-trip — #94 파일 콘텐츠 UTF-8 규약(모호성 해소 원문).
            {
                const fs::path p = tdir / "2u_d_k.mp3";
                std::vector<unsigned char> audio4(64, 0x55);
                check(putFile(p, audio4), "2u-④ 합성 mp3 구성 = 성립");
                mt::TagSet t4;
                t4.title = "가을 노래";
                t4.artist = "아티스트 김";
                t4.album = "앨범 모음";
                std::string err4;
                check(mt::WriteTag(p.generic_string(), t4, nullptr, err4) &&
                          err4.empty(),
                      "2u-④ 한글 쓰기 = ok(UTF-8 규약 입력 — #94)");
                std::string err4b;
                const mt::TagSet back4 =
                    mt::ParseTag(p.generic_string(), err4b);
                check(back4.title == "가을 노래" &&
                          back4.artist == "아티스트 김" &&
                          back4.album == "앨범 모음" && err4b.empty(),
                      "2u-④ 한글 round-trip = 3필드 등호(UTF-16LE 인코딩 1 "
                      "재독 — DecodeId3Text BOM 계약)");
                std::string bytes4;
                check(readFile(p, bytes4), "2u-④ 파일 수취(바이트 단정)");
                const size_t pos = bytes4.find("TIT2");
                const unsigned char expSize[4] = {0x00, 0x00, 0x00, 0x0F};
                // "가을 노래" = 5자 → UTF-16 유닛 10B+BOM 2+enc 1+NUL 2 = 본문 15
                check(pos != std::string::npos &&
                          bytes4.size() >= pos + 13 &&
                          std::memcmp(bytes4.data() + pos + 4, expSize, 4) ==
                              0 &&
                          static_cast<unsigned char>(bytes4[pos + 10]) == 0x01 &&
                          static_cast<unsigned char>(bytes4[pos + 11]) == 0xFF &&
                          static_cast<unsigned char>(bytes4[pos + 12]) == 0xFE,
                      "2u-④ TIT2 인코딩 바이트 = enc 1+0xFF 0xFE BOM+크기 15BE("
                      "프레임 헤더 10B+32bit BE — 모호성 해소 원문)");
            }

            // ⑤) APIC — 합성 jpeg 블롭 512B("image/jpeg") 삽입 → ReadCover
            // round-trip(mime 등호)·부재 = 정직 거부("no_cover")·인코딩 1
            // (UTF-16 mime) 재독 — 브리프 "인코딩 0/1 모두 지원".
            {
                const fs::path p = tdir / "2u_e_apic.mp3";
                check(putFile(p, audio), "2u-⑤ 합성 mp3(무태그) 구성 = 성립");
                mt::CoverArt none;
                std::string errn;
                check(!mt::ReadCover(p.generic_string(), none, errn) &&
                          errn == "no_cover" && none.mime.empty() &&
                          none.bytes.empty(),
                      "2u-⑤ ReadCover 부재 = 정직 거부(no_cover·빈 out — "
                      "브리프 실패 계약)");
                mt::TagSet t5;
                t5.title = "Cover Song";
                t5.artist = "Cover Artist";
                t5.album = "Cover Album";
                mt::CoverArt c5;
                c5.mime = "JPEG";  // 정규화 입력 — "image/jpeg" 정규화 계약
                c5.bytes = jpg;
                std::string err5;
                check(mt::WriteTag(p.generic_string(), t5, &c5, err5) &&
                          err5.empty(),
                      "2u-⑤ APIC 삽입 쓰기 = ok(D5 APIC 교체·인코딩 0 mime)");
                mt::CoverArt got;
                std::string errg;
                check(mt::ReadCover(p.generic_string(), got, errg) &&
                          got.mime == "image/jpeg" && got.bytes == jpg &&
                          errg.empty(),
                      "2u-⑤ ReadCover round-trip = mime 정규화 등호+그림 512B "
                      "등호(mime 정규화 계약)");
                std::string err5b;
                const mt::TagSet back5 =
                    mt::ParseTag(p.generic_string(), err5b);
                check(back5.title == "Cover Song" &&
                          back5.artist == "Cover Artist" &&
                          back5.album == "Cover Album" && err5b.empty(),
                      "2u-⑤ APIC 삽입 후 재독 = 3필드 등호(APIC와 3필드 무접촉)");
                // 인코딩 1(UTF-16 mime) 수독 — 부품 재용 합성(2u-⑤b).
                const fs::path p1 = tdir / "2u_e_apic_enc1.mp3";
                std::vector<unsigned char> ab;
                ab.push_back(0x01);  // enc 1
                ab.push_back(0xFF);
                ab.push_back(0xFE);
                mt::Utf8ToUtf16Le("image/jpeg", ab);
                ab.push_back(0);
                ab.push_back(0);  // mime 종결(UTF-16)
                ab.push_back(0);
                ab.push_back(0);  // desc = 빈+종결
                ab.insert(ab.end(), jpg.begin(), jpg.end());
                const std::vector<unsigned char> fr = mkFrame("APIC", false, ab);
                std::vector<unsigned char> tag1 = {'I', 'D', '3', 0x03, 0x00,
                                                   0x00};
                pushSyncsafe(tag1, static_cast<unsigned int>(fr.size()));
                tag1.insert(tag1.end(), fr.begin(), fr.end());
                tag1.insert(tag1.end(), audio.begin(), audio.end());
                check(putFile(p1, tag1),
                      "2u-⑤ enc1 APIC 합성 구성 = 성립(UTF-16 mime)");
                mt::CoverArt got1;
                std::string errg1;
                check(mt::ReadCover(p1.generic_string(), got1, errg1) &&
                          got1.mime == "image/jpeg" && got1.bytes == jpg &&
                          errg1.empty(),
                      "2u-⑤ enc1 APIC 재독 = UTF-16 mime 해독+그림 등호"
                      "(인코딩 0/1 모두 지원 — 브리프 계약)");
            }

            // ⑥) cover=null 재작성 — 기존 APIC 유지(D5/브리프 계약 — 2u-⑤
            // 파일 재용).
            {
                const fs::path p = tdir / "2u_e_apic.mp3";
                mt::TagSet t6;
                t6.title = "Second Title";
                t6.artist = "Second Artist";
                t6.album = "Second Album";
                std::string err6;
                check(mt::WriteTag(p.generic_string(), t6, nullptr, err6) &&
                          err6.empty(),
                      "2u-⑥ cover=null 재작성 = ok(기존 APIC 유지 계약)");
                mt::CoverArt kept;
                std::string errk;
                check(mt::ReadCover(p.generic_string(), kept, errk) &&
                          kept.mime == "image/jpeg" && kept.bytes == jpg,
                      "2u-⑥ 기존 APIC 생존 = mime 등호+그림 512B 등호(원문 "
                      "바이트 보존 leg — ReadCover 재독)");
                std::string err6b;
                const mt::TagSet back6 =
                    mt::ParseTag(p.generic_string(), err6b);
                check(back6.title == "Second Title" &&
                          back6.artist == "Second Artist" &&
                          back6.album == "Second Album" && err6b.empty(),
                      "2u-⑥ 재작성 후 재독 = 새 3필드 등호(보존 APIC와 무접촉)");
                std::string bytes6;
                check(readFile(p, bytes6) && hasBytes(bytes6, jpg.data(), jpg.size()) &&
                          bytes6.size() > 10 &&
                          static_cast<unsigned char>(bytes6[3]) == 0x03,
                      "2u-⑥ 재작성 파일 = APIC 그림 원문+버전 v2.3(정규화 재"
                      "작성 무손상)");
            }

            // ⑦) 실패 계약 — 정직 거부+원본 등호(무손상)+tmp 잔산 0.
            {
                // (a) 0바이트.
                const fs::path pe = tdir / "2u_g_zero.mp3";
                mt::TagSet t7;
                t7.title = "X";
                check(putFile(pe, {}),
                      "2u-⑦ 0바이트 파일 구성 = 성립(실패 대상)");
                std::string err7;
                check(!mt::WriteTag(pe.generic_string(), t7, nullptr, err7) &&
                          !err7.empty() && fs::file_size(pe) == 0,
                      "2u-⑦ 0바이트 = 정직 거부+원본 등호(브리프 실패 계약)");
                // (b) FLAC 매agic — 매직은 있으나 STREAMINFO 없는 파산 사슬 =
                // 정직 거부(T2에서 FLAC leg가 채워졌으므로 T1의 임시 거부 어설
                // 션 unsupported_format → 구조 파산 사유로 승계 갱신 — 원본
                // 등호 계약 원문 유지).
                const fs::path pf = tdir / "2u_g_flac.flac";
                std::vector<unsigned char> flac(64, 0);
                flac[0] = 'f';
                flac[1] = 'L';
                flac[2] = 'a';
                flac[3] = 'C';
                check(putFile(pf, flac), "2u-⑦ fLaC 파일 구성 = 성립");
                std::string err7b;
                check(!mt::WriteTag(pf.generic_string(), t7, nullptr, err7b) &&
                          err7b == "flac streaminfo missing" &&
                          fs::file_size(pf) == flac.size(),
                      "2u-⑦ FLAC 파산 사슬 = 정직 거부(flac streaminfo missing "
                      "— T2 스니프 진실원)+원본 등호");
                // (c) 잘린 ID3 선언 — 선언 5010B vs 실체 25B.
                const fs::path pt = tdir / "2u_g_trunc.mp3";
                std::vector<unsigned char> trunc = {'I', 'D', '3', 0x04,
                                                    0x00, 0x00};
                pushSyncsafe(trunc, 5000);
                for (int i = 0; i < 15; ++i) trunc.push_back(0x77);
                check(putFile(pt, trunc) && trunc.size() == 25,
                      "2u-⑦ 잘린 ID3 구성 = 25B(선언 5010B vs 실체)");
                std::string err7c;
                // 사전 스냅샷 1행(T1 deferred ① — park-5): 등호 판진의 "before"
                // 원존은 WriteTag 호출 전에 수취한다 — 호출 후 재독 2회 비교는
                // 쓰기 사고를 감시하지 못하는 무력 어셜션. 케이스 수 불변
                // (사전 수취를 기존 거부 어셜션에 동봉 — 신설 check 없음).
                std::string before, after;
                check(readFile(pt, before) && before.size() == 25 &&
                          !mt::WriteTag(pt.generic_string(), t7, nullptr, err7c) &&
                          !err7c.empty(),
                      "2u-⑦ 잘린 ID3 선언 = 정직 거부(선언 > 실체 — 무리한 "
                      "재동기 금지)+사전 스냅샷");
                check(readFile(pt, after) && before == after &&
                          after.size() == 25,
                      "2u-⑦ 잘린 원본 등호 = 25B 무손상(사전 스냅샷 등호 — 원본 "
                      "무변경 계약)");
                // (d) 비지원 확장자(매agic 없음) — unsupported_format.
                const fs::path po = tdir / "2u_g_raw.xyz";
                check(putFile(po, audio), "2u-⑦ 비지원 파일 구성 = 성립");
                std::string err7d;
                check(!mt::WriteTag(po.generic_string(), t7, nullptr, err7d) &&
                          err7d == "unsupported_format",
                      "2u-⑦ 비지원 확장자 = 정직 거부(unsupported_format — "
                      "스니프 분기)");
                // 잔산 — 실패 경로가 .tagtmp/.bak를 남기지 않는다(D6).
                int junk = 0;
                for (const auto& e : fs::directory_iterator(tdir)) {
                    const std::string n = e.path().filename().string();
                    if (n.find(".tagtmp") != std::string::npos ||
                        n.find(".bak") != std::string::npos)
                        ++junk;
                }
                check(junk == 0,
                      "2u-⑦ 잔산 0 = tagtmp/bak 소각(D6 실패 잔산 방지)");
                // REMNANT 0 — 사후 소각(2s/2r 정리 수형 재용).
                std::error_code sec7;
                fs::remove_all(tdir, sec7);
                check(!sec7 && !fs::exists(tdir, sec7),
                      "2u-⑦ 사후 소각 = 잔산 0(REMNANT 0 — 2s/2r 정리 수형 "
                      "재용)");
            }

            // ---- ⑧-⑭ (T2 — FLAC/Ogg/WAV leg) — ⑦ 사후 소각 직후 tdir 재개
            // 장(계보 연속성 — 2u 동일 통로). 합성 픽스처 = inline(테스트
            // 음성/이미지는 합성 블롭만 계약)·판정 독립판(구현 재용 금지).
            fs::create_directories(tdir);
            // FLAC 블록 1건 합성([1 last|7 type][24BE size]+payload — 독립판).
            auto flacBlock = [](unsigned type, bool last,
                                const std::vector<unsigned char>& body) {
                std::vector<unsigned char> b;
                b.push_back(static_cast<unsigned char>(
                    (last ? 0x80 : 0) | (type & 0x7F)));
                for (int sh = 16; sh >= 0; sh -= 8)
                    b.push_back(static_cast<unsigned char>(
                        (body.size() >> sh) & 0xFF));
                b.insert(b.end(), body.begin(), body.end());
                return b;
            };
            // FLAC PICTURE 구조 합성(독립판 — [32BE 그림형식 3][32LE 아님
            // 주의 BE mime len][mime][32BE desc len][w h depth colors][32BE
            // data len][data]).
            auto flacPicStruct =
                [&](const std::vector<unsigned char>& mime,
                    const std::vector<unsigned char>& img) {
                    std::vector<unsigned char> b;
                    pushBE32(b, 3);  // 그림형식 3 = front cover
                    pushBE32(b, static_cast<unsigned int>(mime.size()));
                    b.insert(b.end(), mime.begin(), mime.end());
                    pushBE32(b, 0);                       // desc 빈
                    for (int i = 0; i < 4; ++i) pushBE32(b, 0);  // w h d c
                    pushBE32(b, static_cast<unsigned int>(img.size()));
                    b.insert(b.end(), img.begin(), img.end());
                    return b;
                };
            // vorbis comment 본문 합성(독립판 — [32LE vlen][vendor][32LE
            // count][count × [32LE]"KEY=value"]).
            auto pushLe32t = [](std::vector<unsigned char>& v, unsigned int x) {
                v.push_back(static_cast<unsigned char>(x & 0xFF));
                v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
                v.push_back(static_cast<unsigned char>((x >> 16) & 0xFF));
                v.push_back(static_cast<unsigned char>((x >> 24) & 0xFF));
            };
            auto vorbisBody =
                [&](const std::vector<unsigned char>& vendor,
                    const std::vector<std::pair<std::string, std::string>>&
                        es) {
                    std::vector<unsigned char> b;
                    pushLe32t(b, static_cast<unsigned int>(vendor.size()));
                    b.insert(b.end(), vendor.begin(), vendor.end());
                    pushLe32t(b, static_cast<unsigned int>(es.size()));
                    for (const auto& e : es) {
                        pushLe32t(b, static_cast<unsigned int>(
                                         e.first.size() + 1 + e.second.size()));
                        b.insert(b.end(), e.first.begin(), e.first.end());
                        b.push_back('=');
                        b.insert(b.end(), e.second.begin(), e.second.end());
                    }
                    return b;
                };
            // Ogg CRC(독립판 — libogg 규약: poly 0x04C11DB7·MSB-first·init 0.
            // 표를 매 호출 재완성 — 구현과 무접촉의 독립 실측).
            auto oggCrcT = [](const std::vector<unsigned char>& b) {
                unsigned int tbl[256];
                for (unsigned int i = 0; i < 256; ++i) {
                    unsigned int r = i << 24;
                    for (int k = 0; k < 8; ++k)
                        r = (r & 0x80000000u) ? ((r << 1) ^ 0x04C11DB7u)
                                              : (r << 1);
                    tbl[i] = r;
                }
                unsigned int crc = 0;
                for (size_t i = 0; i < b.size(); ++i)
                    crc = (crc << 8) ^ tbl[((crc >> 24) & 0xFF) ^ b[i]];
                return crc;
            };
            // Ogg 페이지 합성(독립판 — 27B 헤더+표+페이로드+CRC 기입).
            auto oggPage =
                [&](unsigned char htype, unsigned long long granule,
                    unsigned int serial, unsigned int seq,
                    const std::vector<unsigned char>& lace,
                    const std::vector<unsigned char>& payload) {
                    std::vector<unsigned char> pg;
                    pg.insert(pg.end(), "OggS", "OggS" + 4);
                    pg.push_back(0);   // version
                    pg.push_back(htype);
                    for (int b = 0; b < 8; ++b)
                        pg.push_back(static_cast<unsigned char>(
                            (granule >> (8 * b)) & 0xFF));
                    for (int b = 0; b < 4; ++b)
                        pg.push_back(static_cast<unsigned char>(
                            (serial >> (8 * b)) & 0xFF));
                    for (int b = 0; b < 4; ++b)
                        pg.push_back(static_cast<unsigned char>(
                            (seq >> (8 * b)) & 0xFF));
                    for (int b = 0; b < 4; ++b) pg.push_back(0);  // CRC 자리
                    pg.push_back(static_cast<unsigned char>(lace.size()));
                    pg.insert(pg.end(), lace.begin(), lace.end());
                    pg.insert(pg.end(), payload.begin(), payload.end());
                    const unsigned int crc = oggCrcT(pg);
                    for (int b = 0; b < 4; ++b)
                        pg[22 + b] = static_cast<unsigned char>(
                            (crc >> (8 * b)) & 0xFF);
                    return pg;
                };
            // Ogg 페이지 보행(독립판 — 시퀀스/granule/htype 단정 재료).
            auto oggPages = [](const std::string& bytes,
                               std::vector<std::vector<unsigned char>>& out) {
                out.clear();
                size_t off = 0;
                while (off + 27 <= bytes.size()) {
                    if (std::memcmp(bytes.data() + off, "OggS", 4) != 0) break;
                    const size_t segs =
                        static_cast<unsigned char>(bytes[off + 26]);
                    size_t plen = 0;
                    for (size_t s = 0; s < segs; ++s)
                        plen += static_cast<unsigned char>(bytes[off + 27 + s]);
                    const size_t pgN = 27 + segs + plen;
                    if (off + pgN > bytes.size()) break;
                    out.emplace_back(bytes.data() + off,
                                     bytes.data() + off + pgN);
                    off += pgN;
                }
            };
            // lacing 합성(255 런+종결 — %255==0이면 빈 종결 세그먼트).
            auto laceFor = [](size_t n) {
                std::vector<unsigned char> l;
                const size_t full = n / 255;
                for (size_t k = 0; k < full; ++k) l.push_back(255);
                if (n % 255 != 0 || n == 0)
                    l.push_back(static_cast<unsigned char>(n % 255));
                else
                    l.push_back(0);  // %255 == 0 — 빈 종결 세그먼트
                return l;
            };
            auto rdLe32t = [](const std::vector<unsigned char>& p,
                              size_t off) -> unsigned int {
                return static_cast<unsigned int>(
                           static_cast<unsigned char>(p[off])) |
                       (static_cast<unsigned int>(
                            static_cast<unsigned char>(p[off + 1]))
                        << 8) |
                       (static_cast<unsigned int>(
                            static_cast<unsigned char>(p[off + 2]))
                        << 16) |
                       (static_cast<unsigned int>(
                            static_cast<unsigned char>(p[off + 3]))
                        << 24);
            };
            auto rdLe64t = [](const std::vector<unsigned char>& p,
                              size_t off) -> unsigned long long {
                unsigned long long v = 0;
                for (int b = 7; b >= 0; --b)
                    v = (v << 8) | static_cast<unsigned char>(p[off + b]);
                return v;
            };

            // ⑧) 합성 FLAC(STREAMINFO+무음 오디오 프레임 스니프) → VORBIS_
            // COMMENT 신설 → ParseTag round-trip + STREAMINFO/오디오 보존.
            {
                const fs::path p = tdir / "2u_h_create.flac";
                std::vector<unsigned char> si(34);
                for (size_t i = 0; i < si.size(); ++i)
                    si[i] = static_cast<unsigned char>(0x11 + i);
                std::vector<unsigned char> flac;
                flac.insert(flac.end(), "fLaC", "fLaC" + 4);
                {
                    // is-last 표지 필수(브리프 원문 "STREAMINFO(last=false)"의
                    // 리터럴은 사슬이 끝나지 않는 구조 — 쓰기 leg의 D9 종결
                    // 계약이 유일한 메타 블록의 is-last를 요구한다 → 갱신:
                    // last=true. 오디오 스니프 프레임 원문 계약은 유지).
                    auto b = flacBlock(0, true, si);  // STREAMINFO(34B)
                    flac.insert(flac.end(), b.begin(), b.end());
                }
                std::vector<unsigned char> au(60);
                for (size_t i = 0; i < au.size(); ++i)
                    au[i] = static_cast<unsigned char>(0x40 + i);
                flac.insert(flac.end(), au.begin(), au.end());
                check(putFile(p, flac),
                      "2u-⑧ 합성 FLAC 구성 = 성립(STREAMINFO is-last+오디오 "
                      "스니프)");
                std::string epre;
                check(mt::TagEmpty(mt::ParseTag(p.generic_string(), epre)) &&
                          epre.empty(),
                      "2u-⑧ 무태그 FLAC 재독 = 빈 TagSet(신설 전제 — 읽기 "
                      "사슬 원문)");
                mt::TagSet t8;
                t8.title = "Flac Title";
                t8.artist = "Flac Artist";
                t8.album = "Flac Album";
                std::string err8;
                check(mt::WriteTag(p.generic_string(), t8, nullptr, err8) &&
                          err8.empty(),
                      "2u-⑧ VORBIS_COMMENT 신설 쓰기 = ok(D9 재조립 leg)");
                std::string bytes8;
                check(readFile(p, bytes8), "2u-⑧ 재독 = 파일 수취");
                // STREAMINFO 보존(D9 — 첫 블록: [8..42) 원문 등호).
                check(bytes8.size() > 46 &&
                          bytes8.compare(8, 4, "fLaC", 4) != 0 &&
                          std::memcmp(bytes8.data() + 8, si.data(), 34) == 0,
                      "2u-⑧ STREAMINFO 보존 = [8..42) 원문 등호(D9 — 첫 블록 "
                      "필수·34B)");
                // 블록 표지 — STREAMINFO 비표지+VORBIS_COMMENT is-last
                // (재마킹 leg — 마지막 블록만 0x80).
                check(static_cast<unsigned char>(bytes8[4]) == 0x00 &&
                          bytes8.size() > 46 &&
                          static_cast<unsigned char>(bytes8[42]) == 0x84,
                      "2u-⑧ is-last 재마킹 = STREAMINFO 0x00+VORBIS_COMMENT "
                      "0x84(type 4·마지막 블록 — [42] 두 번째 헤더)");
                // 오디오 원문 = 말미 60B 등호(D9 — 사슬 밖 원문 보존).
                check(std::memcmp(bytes8.data() + bytes8.size() - 60,
                                  au.data(), 60) == 0,
                      "2u-⑧ 오디오 원문 = 말미 60B 등호([chainEnd, EOF) 스트"
                      "림 복사)");
                std::string err8b;
                const mt::TagSet back8 =
                    mt::ParseTag(p.generic_string(), err8b);
                check(back8.title == "Flac Title" &&
                          back8.artist == "Flac Artist" &&
                          back8.album == "Flac Album" && err8b.empty(),
                      "2u-⑧ ParseTag round-trip = 3필드 등호(신설 코멘트 → 읽"
                      "기 ParseFlacTags 재용)");
                // (추가) SEEKTABLE 파산 = 정직 거부(D9 판정 — 18B 정합).
                {
                    std::vector<unsigned char> st(20, 0x7A);  // 1+18*1=19 아님
                    std::vector<unsigned char> bad;
                    bad.insert(bad.end(), "fLaC", "fLaC" + 4);
                    auto b0 = flacBlock(0, false, si);
                    bad.insert(bad.end(), b0.begin(), b0.end());
                    auto b1 = flacBlock(3, true, st);  // SEEKTABLE 20B = 파산
                    bad.insert(bad.end(), b1.begin(), b1.end());
                    bad.push_back(0x0F);
                    const fs::path q = tdir / "2u_h_seek.flac";
                    std::string before, after;
                    check(putFile(q, bad) && readFile(q, before),
                          "2u-⑧ 파산 SEEKTABLE 식성 구성 = 성립(1+18 정합 "
                          "이탈)");
                    std::string errS;
                    check(!mt::WriteTag(q.generic_string(), t8, nullptr,
                                        errS) &&
                              !errS.empty() && readFile(q, after) &&
                              after == before,
                          "2u-⑧ SEEKTABLE 파산 = 정직 거부+원본 등호(D9 판정 "
                          "실패 — 원문 보존 우선)");
                }
            }

            // ⑨) FLAC PICTURE 신설 → ReadCover(mime) round-trip(D8 신설 leg).
            {
                const fs::path p = tdir / "2u_i_pic.flac";
                std::vector<unsigned char> si(34, 0x22);
                std::vector<unsigned char> flac;
                flac.insert(flac.end(), "fLaC", "fLaC" + 4);
                {
                    auto b = flacBlock(0, true, si);
                    flac.insert(flac.end(), b.begin(), b.end());
                }
                flac.push_back(0x0F);  // (그림 없음 — 신설 대상)
                check(putFile(p, flac), "2u-⑨ 무태그 FLAC 구성 = 성립");
                mt::TagSet t9;
                t9.title = "Pic Title";
                mt::CoverArt c9;
                c9.mime = "jpeg";  // 정규화 전 표기(파일 기입 = image/jpeg)
                std::vector<unsigned char> art9(300);
                for (size_t i = 0; i < art9.size(); ++i)
                    art9[i] = static_cast<unsigned char>(0x21 + i);
                c9.bytes = art9;
                std::string err9;
                check(mt::WriteTag(p.generic_string(), t9, &c9, err9) &&
                          err9.empty(),
                      "2u-⑨ PICTURE 신설 쓰기 = ok(D8 신설 leg·mime 정규화)");
                mt::CoverArt rc9;
                std::string err9b;
                check(mt::ReadCover(p.generic_string(), rc9, err9b) &&
                          rc9.mime == "image/jpeg" && rc9.bytes == art9,
                      "2u-⑨ ReadCover round-trip = mime 정규화+그림 300B 등호"
                      "(PICTURE 블록 파싱 — 형식 해소 구조 재용)");
            }

            // ⑩) FLAC 기존 PICTURE 교체(D8 — 복수 PICTURE 소각·cover=null =
            // 기존 유지) + 2차 아트 등호.
            {
                const fs::path p = tdir / "2u_j_rep.flac";
                std::vector<unsigned char> si(34, 0x33);
                std::vector<unsigned char> oldPic(128);
                for (size_t i = 0; i < oldPic.size(); ++i)
                    oldPic[i] = static_cast<unsigned char>(0x5A + i);
                std::vector<unsigned char> flac;
                flac.insert(flac.end(), "fLaC", "fLaC" + 4);
                {
                    auto b = flacBlock(0, false, si);  // STREAMINFO
                    flac.insert(flac.end(), b.begin(), b.end());
                }
                {
                    // 기존 PICTURE 2건(복수 — D8 소각 대상) + 코멘트.
                    auto pA =
                        flacPicStruct({'i','m','a','g','e','/','p','n','g'},
                                      oldPic);
                    auto b1 = flacBlock(6, false, pA);
                    flac.insert(flac.end(), b1.begin(), b1.end());
                    flac.insert(flac.end(), b1.begin(), b1.end());  // 복수
                    std::vector<unsigned char> vnd(0);
                    auto body = vorbisBody(
                        vnd, {{"TITLE", "Old F Title"},
                              {"ARTIST", "Old F Artist"}});
                    auto b2 = flacBlock(4, true, body);
                    flac.insert(flac.end(), b2.begin(), b2.end());
                }
                flac.insert(flac.end(), 8, 0x0F);
                check(putFile(p, flac),
                      "2u-⑩ 기존 PICTURE+코멘트 FLAC 구성 = 성립(복수 "
                      "PICTURE 2건 — D8 소각 대상)");
                // cover=null = 기존 유지(D8 역 — 첫 유효 아트 등호).
                {
                    mt::TagSet tn;
                    tn.title = "Keep Title";
                    std::string er;
                    check(mt::WriteTag(p.generic_string(), tn, nullptr, er),
                          "2u-⑩ cover=null 재작성 = ok(기존 PICTURE 유지 "
                          "계약)");
                    mt::CoverArt rc;
                    std::string er2;
                    check(mt::ReadCover(p.generic_string(), rc, er2) &&
                              rc.mime == "image/png" &&
                              rc.bytes == oldPic,
                          "2u-⑩ cover=null PICTURE 생존 = mime+128B 등호(D8 "
                          "역 — 기존 유지)");
                }
                // cover 지정 = 완전 교체 — 2차 아트 등호+구 PICTURE 소각.
                mt::TagSet t10;
                t10.title = "New F Title";
                t10.artist = "New F Artist";
                t10.album = "New F Album";
                mt::CoverArt c10;
                c10.mime = "image/jpeg";
                std::vector<unsigned char> art10(240);
                for (size_t i = 0; i < art10.size(); ++i)
                    art10[i] = static_cast<unsigned char>(0xA0 + i);
                c10.bytes = art10;
                std::string err10;
                check(mt::WriteTag(p.generic_string(), t10, &c10, err10) &&
                          err10.empty(),
                      "2u-⑩ PICTURE 완전 교체 쓰기 = ok(D8 — 2차 아트)");
                std::string bytes10;
                check(readFile(p, bytes10) &&
                          !hasBytes(bytes10, oldPic.data(), oldPic.size()),
                      "2u-⑩ 구 PICTURE 소각 = 원문 바이트 부재(복수 PICTURE "
                      "소각 leg — 2건 전부)");
                mt::CoverArt rc10;
                std::string err10b;
                check(mt::ReadCover(p.generic_string(), rc10, err10b) &&
                          rc10.mime == "image/jpeg" && rc10.bytes == art10,
                      "2u-⑩ 2차 아트 등호 = ReadCover 재독(완전 교체 round-"
                      "trip)");
                std::string err10c;
                const mt::TagSet back10 =
                    mt::ParseTag(p.generic_string(), err10c);
                check(back10.title == "New F Title" &&
                          back10.artist == "New F Artist" &&
                          back10.album == "New F Album" && err10c.empty(),
                      "2u-⑩ 코멘트 round-trip = 3필드 등호(PICTURE 교체와 무"
                      "접촉)");
                // PICTURE 블록 수 = 1(독립 보행 — D8 1건 계약).
                size_t off10 = 4;
                int picCount = 0, cmCount = 0;
                while (off10 + 4 <= bytes10.size()) {
                    const bool last =
                        (static_cast<unsigned char>(bytes10[off10]) & 0x80) != 0;
                    const unsigned bt =
                        static_cast<unsigned>(bytes10[off10]) & 0x7F;
                    const size_t bs =
                        (static_cast<size_t>(
                             static_cast<unsigned char>(bytes10[off10 + 1]))
                         << 16) |
                        (static_cast<size_t>(
                             static_cast<unsigned char>(bytes10[off10 + 2]))
                         << 8) |
                        static_cast<size_t>(bytes10[off10 + 3]);
                    if (bt == 6) ++picCount;
                    if (bt == 4) ++cmCount;
                    if (off10 + 4 + bs > bytes10.size()) break;
                    off10 += 4 + bs;
                    if (last) break;
                }
                check(picCount == 1 && cmCount == 1,
                      "2u-⑩ 독립 블록 보행 = PICTURE 1건+VORBIS_COMMENT 1건"
                      "(D8 — 신설 1건만 기입)");
            }

            // ⑪) 합성 Ogg(코멘트 페이지+오디오 페이지 2건) → 코멘트 교체 →
            // ParseTag round-trip + 페이지 시퀀스 연속성·granule 불변(D10).
            {
                const fs::path p = tdir / "2u_k_ogg.ogg";
                const unsigned int serial = 0x1234;
                std::vector<unsigned char> idPkt;
                idPkt.push_back(0x01);
                idPkt.insert(idPkt.end(), "vorbis", "vorbis" + 6);
                for (int i = 0; i < 23; ++i) idPkt.push_back(0x2A);  // 총 30B
                std::vector<unsigned char> cmPkt;
                cmPkt.push_back(0x03);
                cmPkt.insert(cmPkt.end(), "vorbis", "vorbis" + 6);
                std::vector<unsigned char> vnd(0);
                {
                    auto b = vorbisBody(vnd, {{"TITLE", "Orig T"},
                                              {"ARTIST", "Orig A"}});
                    cmPkt.insert(cmPkt.end(), b.begin(), b.end());
                }
                std::vector<unsigned char> stPkt;
                stPkt.push_back(0x05);
                stPkt.insert(stPkt.end(), "vorbis", "vorbis" + 6);
                for (int i = 0; i < 40; ++i) stPkt.push_back(0x5B);
                std::vector<unsigned char> auA(30), auB(30);
                for (int i = 0; i < 30; ++i) {
                    auA[i] = static_cast<unsigned char>(0x70 + i);
                    auB[i] = static_cast<unsigned char>(0x90 + i);
                }
                // lacing — 패킷당 [크기](단일 세그먼트 — 소형 합성).
                auto one = [](size_t n) {
                    return std::vector<unsigned char>{
                        static_cast<unsigned char>(n)};
                };
                std::vector<unsigned char> fl;
                { auto g = oggPage(0x02, 0, serial, 0, one(idPkt.size()), idPkt);
                  fl.insert(fl.end(), g.begin(), g.end()); }           // p0
                { auto g = oggPage(0x0, 0, serial, 1, one(cmPkt.size()), cmPkt);
                  fl.insert(fl.end(), g.begin(), g.end()); }           // p1
                { auto g = oggPage(0x0, 0, serial, 2, one(stPkt.size()), stPkt);
                  fl.insert(fl.end(), g.begin(), g.end()); }           // p2
                { auto g = oggPage(0x0, 100, serial, 3, one(auA.size()), auA);
                  fl.insert(fl.end(), g.begin(), g.end()); }           // p3
                { auto g = oggPage(0x0, 300, serial, 4, one(auB.size()), auB);
                  fl.insert(fl.end(), g.begin(), g.end()); }           // p4
                check(putFile(p, fl),
                      "2u-⑪ 합성 Ogg 구성 = 성립(5페이지 — 코멘트+오디오 2개"
                      " 이상)");
                std::string epre;
                const mt::TagSet pre11 =
                    mt::ParseTag(p.generic_string(), epre);
                check(pre11.title == "Orig T" && pre11.artist == "Orig A",
                      "2u-⑪ 원본 재독 = 2필드(읽기 사슬 — 교체 전제)");
                mt::TagSet t11;
                t11.title = "Newk T";
                t11.artist = "Newk A";
                t11.album = "Newk L";
                std::string err11;
                check(mt::WriteTag(p.generic_string(), t11, nullptr, err11) &&
                          err11.empty(),
                      "2u-⑪ 코멘트 교체 쓰기 = ok(D10 — 페이지 재조립)");
                std::string bytes11;
                check(readFile(p, bytes11), "2u-⑪ 재독 = 파일 수취");
                std::string err11b;
                const mt::TagSet back11 =
                    mt::ParseTag(p.generic_string(), err11b);
                check(back11.title == "Newk T" && back11.artist == "Newk A" &&
                          back11.album == "Newk L" && err11b.empty(),
                      "2u-⑪ ParseTag round-trip = 3필드 등호(코멘트 패킷 교체"
                      " → 읽기 사슬 재용)");
                // 페이지 시퀀스 연속성+granule 불변(독립 보행 — D10 계약).
                std::vector<std::vector<unsigned char>> pgs;
                oggPages(bytes11, pgs);
                bool seqOk = pgs.size() >= 5;
                bool grOk = seqOk;
                for (size_t i = 0; seqOk && i < pgs.size(); ++i)
                    seqOk = rdLe32t(pgs[i], 18) == static_cast<unsigned int>(i);
                for (size_t i = 0; grOk && i < pgs.size(); ++i) {
                    const unsigned long long gran =
                        rdLe64t(pgs[i], 6);
                    if (gran != 0 && gran != 0xFFFF'FFFF'FFFF'FFFFull &&
                        !(gran == 100 || gran == 300))
                        grOk = false;
                }
                check(seqOk,
                      "2u-⑪ 페이지 시퀀스 연속 = seq 0..N(재기입 leg — 코멘트"
                      " 페이지 교체 후 연속)");
                check(grOk && pgs.size() >= 5 &&
                          rdLe64t(pgs[3], 6) == 100ull &&
                          rdLe64t(pgs[4], 6) == 300ull,
                      "2u-⑪ granule 불변 = 오디오 페이지 100/300 등호(D10)");
                // CRC 재계산 실측 — 오디오 페이지(시퀀스 재기입)의 CRC 필드 =
                // 독립 판정판 등호.
                {
                    auto pg3 = pgs[3];
                    pg3[22] = 0; pg3[23] = 0; pg3[24] = 0; pg3[25] = 0;
                    check(oggCrcT(pg3) == rdLe32t(pgs[3], 22),
                          "2u-⑪ CRC 재계산 = 독립 판정판 등호(libogg 규약 — "
                          "시퀀스 재기입 페이지)");
                }
                // 오디오/셋업 패킷 원문 보존(granule·payload — 재조립 무손상).
                check(hasBytes(bytes11, auA.data(), auA.size()) &&
                          hasBytes(bytes11, auB.data(), auB.size()) &&
                          hasBytes(bytes11, stPkt.data(), stPkt.size()),
                      "2u-⑪ 셋업·오디오 패킷 원문 = 바이트 등호 생존(패킷 경계"
                      "불변 — 형식 해소 (d))");
                // (b) 트레일 변형 — 코멘트 페이지에 셋업 선두가 같이 심긴 배치
                // (개시 패킷 255 런 — lacing 원문 재사용 + 다음 페이지
                // continuation 정합 실측).
                const fs::path pb = tdir / "2u_k_trail.ogg";
                std::vector<unsigned char> stBig;
                stBig.push_back(0x05);
                stBig.insert(stBig.end(), "vorbis", "vorbis" + 6);
                for (int i = 0; i < 293; ++i) stBig.push_back(0x77);  // 300B
                std::vector<unsigned char> flb;
                { auto g = oggPage(0x02, 0, serial, 0, one(idPkt.size()), idPkt);
                  flb.insert(flb.end(), g.begin(), g.end()); }
                {
                    // p1 = 코멘트(lacing 마감)+셋업 선두(255 런 — 개시 패킷)
                    std::vector<unsigned char> lace;
                    lace.push_back(static_cast<unsigned char>(cmPkt.size()));
                    lace.push_back(255);
                    std::vector<unsigned char> payload = cmPkt;
                    payload.insert(payload.end(), stBig.begin(),
                                   stBig.begin() + 255);
                    auto g = oggPage(0x0, 0, serial, 1, lace, payload);
                    flb.insert(flb.end(), g.begin(), g.end());
                }
                {
                    // p2 = 셋업 마감(45 — 255+45 = 300)+오디오 A(granule 100)
                    std::vector<unsigned char> lace = {45,
                                                       static_cast<unsigned char>(
                                                           auA.size())};
                    std::vector<unsigned char> payload;
                    payload.assign(stBig.begin() + 255, stBig.end());
                    payload.insert(payload.end(), auA.begin(), auA.end());
                    auto g = oggPage(0x01, 100, serial, 2, lace, payload);
                    flb.insert(flb.end(), g.begin(), g.end());
                }
                { auto g = oggPage(0x0, 300, serial, 3, one(auB.size()), auB);
                  flb.insert(flb.end(), g.begin(), g.end()); }
                check(putFile(pb, flb),
                      "2u-⑪(b) 트레일 변형 Ogg 구성 = 성립(코멘트 페이지에 셋"
                      "업 개시·p2 continuation 0x01)");
                std::string err11c;
                check(mt::WriteTag(pb.generic_string(), t11, nullptr, err11c) &&
                          err11c.empty(),
                      "2u-⑪(b) 트레일 변형 교체 = ok(개시 패킷 lacing 원문 재"
                      "사용 leg)");
                std::string err11d;
                const mt::TagSet back11b =
                    mt::ParseTag(pb.generic_string(), err11d);
                check(back11b.title == "Newk T" && back11b.artist == "Newk A" &&
                          back11b.album == "Newk L" && err11d.empty(),
                      "2u-⑪(b) round-trip = 3필드 등호(트레일 재조립 후 코멘트"
                      " 판독)");
                std::string bytes11b;
                std::vector<std::vector<unsigned char>> pgsb;
                check(readFile(pb, bytes11b) &&
                          (oggPages(bytes11b, pgsb), pgsb.size() >= 4),
                      "2u-⑪(b) 페이지 보행 = 4페이지 이상(재조립·뒤페이지 재기"
                      "입 결과)");
                {
                    bool seqb = true;
                    const unsigned int s0 = rdLe32t(pgsb[0], 18);
                    for (size_t i = 0; seqb && i < pgsb.size(); ++i)
                        seqb = rdLe32t(pgsb[i], 18) ==
                               static_cast<unsigned int>(s0 + i);
                    // 개시 패킷 경계 — 신설 말미 페이지의 마지막 lacing = 255
                    // (셋업 개시가 다음 원본 페이지로 이어진다 — continuation
                    // 정합)·granule 불변(100/300).
                    const unsigned char h2 =
                        static_cast<unsigned char>(pgsb[2][5]);
                    check(seqb && rdLe64t(pgsb[2], 6) == 100ull &&
                              rdLe64t(pgsb[3], 6) == 300ull &&
                              (h2 & 0x01) != 0,
                          "2u-⑪(b) 경계 정합 = seq 연속+granule 100/300 등호"
                          "+p2 continuation 0x01(개시 패킷 lacing 원문)");
                }
            }

            // ⑫) Ogg METADATA_BLOCK_PICTURE round-trip(D8 — 그림 필드 교체).
            {
                const fs::path p = tdir / "2u_l_pic.ogg";
                const unsigned int serial = 0x77;
                std::vector<unsigned char> idPkt;
                idPkt.push_back(0x01);
                idPkt.insert(idPkt.end(), "vorbis", "vorbis" + 6);
                for (int i = 0; i < 23; ++i) idPkt.push_back(0x2A);
                std::vector<unsigned char> stPkt;
                stPkt.push_back(0x05);
                stPkt.insert(stPkt.end(), "vorbis", "vorbis" + 6);
                for (int i = 0; i < 40; ++i) stPkt.push_back(0x5B);
                // 독립 base64(구현 재용 금지 — 판정 독립 계약).
                auto b64enc = [](const std::vector<unsigned char>& in) {
                    static const char* tb =
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz0123456789+/";
                    std::string out;
                    unsigned int buf = 0;
                    int bits = 0;
                    for (size_t i = 0; i < in.size(); ++i) {
                        buf = ((buf << 8) | in[i]) & 0xFFFFu;
                        bits += 8;
                        while (bits >= 6) {
                            bits -= 6;
                            out.push_back(tb[(buf >> bits) & 0x3F]);
                        }
                    }
                    if (bits == 2) { out.push_back(tb[(buf << 4) & 0x3F]); out += "=="; }
                    else if (bits == 4) { out.push_back(tb[(buf << 2) & 0x3F]); out += "="; }
                    return out;
                };
                auto one = [](size_t n) {
                    return std::vector<unsigned char>{
                        static_cast<unsigned char>(n)};
                };
                // 기존 아트(160B 독립 패턴) — 기존 MBP 엔트리 심기.
                std::vector<unsigned char> oldArt(160);
                for (size_t i = 0; i < oldArt.size(); ++i)
                    oldArt[i] = static_cast<unsigned char>(0x30 + i);
                std::vector<unsigned char> pngMime;
                pngMime.insert(pngMime.end(), "image/png", "image/png" + 9);
                auto oldPicStruct = flacPicStruct(pngMime, oldArt);
                const std::string oldB64 = b64enc(oldPicStruct);
                std::vector<unsigned char> cmPkt;
                cmPkt.push_back(0x03);
                cmPkt.insert(cmPkt.end(), "vorbis", "vorbis" + 6);
                std::vector<unsigned char> vnd(0);
                {
                    auto b = vorbisBody(
                        vnd, {{"TITLE", "Ogg P"},
                              {"METADATA_BLOCK_PICTURE", oldB64}});
                    cmPkt.insert(cmPkt.end(), b.begin(), b.end());
                }
                std::vector<unsigned char> auA(30), auB(30);
                for (int i = 0; i < 30; ++i) {
                    auA[i] = static_cast<unsigned char>(0x66 + i);
                    auB[i] = static_cast<unsigned char>(0x99 + i);
                }
                std::vector<unsigned char> fl;
                { auto g = oggPage(0x02, 0, serial, 0, one(idPkt.size()), idPkt);
                  fl.insert(fl.end(), g.begin(), g.end()); }
                { auto g =
                      oggPage(0x0, 0, serial, 1, laceFor(cmPkt.size()), cmPkt);
                  fl.insert(fl.end(), g.begin(), g.end()); }  // MBP 코멘트 = 255 런
                { auto g = oggPage(0x0, 0, serial, 2, one(stPkt.size()), stPkt);
                  fl.insert(fl.end(), g.begin(), g.end()); }
                { auto g = oggPage(0x0, 500, serial, 3, one(auA.size()), auA);
                  fl.insert(fl.end(), g.begin(), g.end()); }
                { auto g = oggPage(0x0, 700, serial, 4, one(auB.size()), auB);
                  fl.insert(fl.end(), g.begin(), g.end()); }
                check(putFile(p, fl),
                      "2u-⑫ 합성 Ogg(MBP 코멘트) 구성 = 성립(기존 그림 필드 — "
                      "255 런 lacing)");
                // cover=null = 기존 MBP 유지 → ReadCover 등호.
                {
                    mt::TagSet tn;
                    tn.title = "Keepk T";
                    std::string er;
                    check(mt::WriteTag(p.generic_string(), tn, nullptr, er) &&
                              er.empty(),
                          "2u-⑫ cover=null 재작성 = ok(기존 MBP 유지 계약)");
                    mt::CoverArt rc;
                    std::string er2;
                    check(mt::ReadCover(p.generic_string(), rc, er2) &&
                              rc.mime == "image/png" && rc.bytes == oldArt,
                          "2u-⑫ 기존 MBP 생존 = base64 복호+구조 파싱 등호(필드 "
                          "원문 유지)");
                    std::string er3;
                    const mt::TagSet back =
                        mt::ParseTag(p.generic_string(), er3);
                    check(back.title == "Keepk T" && back.artist.empty() &&
                              back.album.empty() && er3.empty(),
                          "2u-⑫ 코멘트 round-trip = title 등호+빈 필드 생략");
                }
                // cover 지정 = 완전 교체(D8) — 구 b64 소각+신설 등호.
                mt::TagSet t12;
                t12.title = "Picl T";
                mt::CoverArt c12;
                c12.mime = "image/jpeg";
                c12.bytes = jpg;
                std::string err12;
                check(mt::WriteTag(p.generic_string(), t12, &c12, err12) &&
                          err12.empty(),
                      "2u-⑫ MBP 교체 쓰기 = ok(D8 — 그림 필드 완전 교체)");
                std::string bytes12;
                mt::CoverArt rc12;
                std::string err12b;
                check(readFile(p, bytes12) &&
                          bytes12.find(oldB64.c_str(), 0, oldB64.size()) ==
                              std::string::npos &&
                          mt::ReadCover(p.generic_string(), rc12, err12b) &&
                          rc12.mime == "image/jpeg" && rc12.bytes == jpg,
                      "2u-⑫ 구 b64 소각+신설 round-trip = 등호(기존 MBP 소각"
                      "·첫 유효만 기입)");
            }

            // ⑫(f) FLAC MBP 코멘트 소각(F1 — cover 지정 = 기존
            // METADATA_BLOCK_PICTURE 엔트리 소각·PICTURE 블록 = 진실원) +
            // 복수 VORBIS_COMMENT 소각(F2 — FLAC 규약 스트림당 1건). 합성
            // FLAC에 MBP 코멘트+제2 코멘트 블록 사전 심어 → cover 지정 교체 →
            // 재독 = 구 MBP b64 부재+PICTURE/코멘트 블록 각 1건. 2u-⑫ 픽스처
            // 재용(b64·flacPicStruct·vorbisBody — 독립판 재합성).
            {
                const fs::path p = tdir / "2u_l_f_flac.flac";
                auto b64encF = [](const std::vector<unsigned char>& in) {
                    static const char* tb =
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz0123456789+/";
                    std::string out;
                    unsigned int buf = 0;
                    int bits = 0;
                    for (size_t i = 0; i < in.size(); ++i) {
                        buf = ((buf << 8) | in[i]) & 0xFFFFu;
                        bits += 8;
                        while (bits >= 6) {
                            bits -= 6;
                            out.push_back(tb[(buf >> bits) & 0x3F]);
                        }
                    }
                    if (bits == 2) { out.push_back(tb[(buf << 4) & 0x3F]); out += "=="; }
                    else if (bits == 4) { out.push_back(tb[(buf << 2) & 0x3F]); out += "="; }
                    return out;
                };
                std::vector<unsigned char> siF(34, 0x44);
                std::vector<unsigned char> oldArtF(120);
                for (size_t i = 0; i < oldArtF.size(); ++i)
                    oldArtF[i] = static_cast<unsigned char>(0x41 + i);
                std::vector<unsigned char> pngMimeF;
                pngMimeF.insert(pngMimeF.end(), "image/png",
                                "image/png" + 9);
                const std::string oldB64F =
                    b64encF(flacPicStruct(pngMimeF, oldArtF));
                std::vector<unsigned char> flac;
                flac.insert(flac.end(), "fLaC", "fLaC" + 4);
                { auto b = flacBlock(0, false, siF);
                  flac.insert(flac.end(), b.begin(), b.end()); }  // STREAMINFO
                { auto b = flacBlock(6, false,
                                     flacPicStruct(pngMimeF, oldArtF));
                  flac.insert(flac.end(), b.begin(), b.end()); }  // 구 PICTURE
                std::vector<unsigned char> vndF(0);
                { auto body = vorbisBody(
                      vndF, {{"TITLE", "Flac Pic"},
                             {"METADATA_BLOCK_PICTURE", oldB64F}});
                  auto b = flacBlock(4, false, body);
                  flac.insert(flac.end(), b.begin(), b.end()); }  // 코멘트 1
                { auto body = vorbisBody(
                      vndF, {{"TITLE", "Dup Cmnt"}});
                  auto b = flacBlock(4, true, body);
                  flac.insert(flac.end(), b.begin(), b.end()); }  // 코멘트 2
                flac.insert(flac.end(), 8, 0x0F);  // 오디오 원문
                check(putFile(p, flac),
                      "2u-⑫(f) 합성 FLAC(MBP 코멘트+제2 코멘트 블록) 구성 = "
                      "성립(F1·F2 대상)");
                mt::TagSet tF;
                tF.title = "Flac Replace";
                tF.artist = "Flac Artist";
                mt::CoverArt cF;
                cF.mime = "image/jpeg";
                cF.bytes = jpg;
                std::string errF;
                check(mt::WriteTag(p.generic_string(), tF, &cF, errF) &&
                          errF.empty(),
                      "2u-⑫(f) cover 지정 교체 쓰기 = ok(D8 — MBP 코멘트 소각"
                      " leg)");
                std::string bytesF;
                mt::CoverArt rcF;
                std::string errFb;
                check(readFile(p, bytesF) &&
                          bytesF.find(oldB64F.c_str(), 0, oldB64F.size()) ==
                              std::string::npos &&
                          mt::ReadCover(p.generic_string(), rcF, errFb) &&
                          rcF.mime == "image/jpeg" && rcF.bytes == jpg,
                      "2u-⑫(f) 구 MBP b64 부재+PICTURE = 새 아트 등호(F1 — "
                      "블록이 진실원·코멘트 잔존 소각)");
                std::string errFc;
                const mt::TagSet backF =
                    mt::ParseTag(p.generic_string(), errFc);
                check(backF.title == "Flac Replace" &&
                          backF.artist == "Flac Artist" &&
                          backF.album.empty() && errFc.empty(),
                      "2u-⑫(f) 코멘트 round-trip = 필드 등호(교체 사슬 원문)");
                size_t offF = 4;
                int picF = 0, cmF = 0;
                while (offF + 4 <= bytesF.size()) {
                    const bool lastF = (static_cast<unsigned char>(
                                            bytesF[offF]) & 0x80) != 0;
                    const unsigned btF =
                        static_cast<unsigned>(bytesF[offF]) & 0x7F;
                    const size_t bsF = (static_cast<size_t>(
                                            static_cast<unsigned char>(
                                                bytesF[offF + 1])) << 16) |
                                       (static_cast<size_t>(
                                            static_cast<unsigned char>(
                                                bytesF[offF + 2])) << 8) |
                                       static_cast<size_t>(bytesF[offF + 3]);
                    if (btF == 6) ++picF;
                    if (btF == 4) ++cmF;
                    if (offF + 4 + bsF > bytesF.size()) break;
                    offF += 4 + bsF;
                    if (lastF) break;
                }
                check(picF == 1 && cmF == 1,
                      "2u-⑫(f) 독립 블록 보행 = PICTURE 1건+VORBIS_COMMENT "
                      "1건(F2 — 복수 코멘트 블록 소각·규약 스트림당 1건)");
            }

            // ⑬) 합성 WAV → id3 청크 신설 → ParseTag round-trip + RIFF 크기
            // 등호(청크 순서 보존 — 형식 해소 (c)).
            {
                const fs::path p = tdir / "2u_m_wav.wav";
                auto wavChunk = [&](const char* id,
                                   const std::vector<unsigned char>& body) {
                    std::vector<unsigned char> c;
                    c.insert(c.end(), id, id + 4);
                    pushLe32t(c, static_cast<unsigned int>(body.size()));
                    c.insert(c.end(), body.begin(), body.end());
                    if (body.size() & 1) c.push_back(0);  // 홀수 패드
                    return c;
                };
                std::vector<unsigned char> fmtB(16);
                for (size_t i = 0; i < fmtB.size(); ++i)
                    fmtB[i] = static_cast<unsigned char>(0x11 * (i + 1));
                std::vector<unsigned char> dataB(64);
                for (size_t i = 0; i < dataB.size(); ++i)
                    dataB[i] = static_cast<unsigned char>(0xE0 + i);
                std::vector<unsigned char> wav;
                wav.insert(wav.end(), "RIFF", "RIFF" + 4);
                pushLe32t(wav, 0);  // 크기 자리(신설 전 조립 — 말미 재기입)
                wav.insert(wav.end(), "WAVE", "WAVE" + 4);
                { auto c = wavChunk("fmt ", fmtB);
                  wav.insert(wav.end(), c.begin(), c.end()); }
                { auto c = wavChunk("data", dataB);
                  wav.insert(wav.end(), c.begin(), c.end()); }
                const unsigned int riffPre =
                    static_cast<unsigned int>(wav.size() - 8);
                wav[4] = static_cast<unsigned char>(riffPre & 0xFF);
                wav[5] = static_cast<unsigned char>((riffPre >> 8) & 0xFF);
                wav[6] = static_cast<unsigned char>((riffPre >> 16) & 0xFF);
                wav[7] = static_cast<unsigned char>((riffPre >> 24) & 0xFF);
                check(putFile(p, wav),
                      "2u-⑬ 합성 WAV 구성 = 성립(fmt+data — id3 부재)");
                mt::TagSet t13;
                t13.title = "Wav T";
                t13.artist = "Wav A";
                t13.album = "Wav L";
                std::string err13;
                check(mt::WriteTag(p.generic_string(), t13, nullptr, err13) &&
                          err13.empty(),
                      "2u-⑬ id3 청크 신설 쓰기 = ok(말미 신설 leg)");
                std::string bytes13;
                check(readFile(p, bytes13), "2u-⑬ 재독 = 파일 수취");
                // RIFF 크기 등호 — [4..8) LE32 = 파일 크기-8(사슬 재계산).
                {
                    std::vector<unsigned char> b13(bytes13.begin(),
                                                   bytes13.end());
                    check(b13.size() >= 12 &&
                              rdLe32t(b13, 4) ==
                                  static_cast<unsigned int>(b13.size() - 8),
                          "2u-⑬ RIFF 크기 등호 = [4..8) == 파일 크기-8(사슬 재"
                          "계산 계약)");
                }
                std::string err13b;
                const mt::TagSet back13 =
                    mt::ParseTag(p.generic_string(), err13b);
                check(back13.title == "Wav T" && back13.artist == "Wav A" &&
                          back13.album == "Wav L" && err13b.empty(),
                      "2u-⑬ ParseTag round-trip = 3필드 등호(id3 청크 → 읽기 "
                      "ParseWavTags 재용)");
                // 기존 청크 순서 보존 — fmt가 data보다 앞·data payload 원문.
                const size_t fmtAt = bytes13.find("fmt ");
                const size_t dataAt = bytes13.find("data");
                check(fmtAt != std::string::npos &&
                          dataAt != std::string::npos && fmtAt < dataAt &&
                          hasBytes(bytes13, dataB.data(), dataB.size()),
                      "2u-⑬ 청크 순서 보존 = fmt < data+data payload 원문(기존"
                      " 연쇄 무변경)");
                // 신설 id3 청크 = 말미(RIFF 말미 신설 계약).
                {
                    std::vector<unsigned char> b13(bytes13.begin(),
                                                   bytes13.end());
                    // 독립 계산: 보존 청크 총계 = 12+8+16+8+64 = 96(패드 0) →
                    // id3 청크는 [12+96, 파일끝) — 사슬 산출로 위치 단정.
                    const bool id3AtEnd =
                        b13.size() >= 12 + 96 + 12 &&
                        std::memcmp(b13.data() + 12 + 96, "id3 ", 4) == 0;
                    check(id3AtEnd,
                          "2u-⑬ 신설 자리 = RIFF 말미(보존 청크 96B 뒤 — 기존"
                          " 순서 보존의 신설)");
                }
            }

            // ⑭) WAV id3 APIC → ReadCover + 기존 APIC 보존(mp3 APIC 파싱 재용
            // — 브리프 원문).
            {
                const fs::path p = tdir / "2u_n_wav.wav";
                // (a) 신설 — 무태그 WAV → WriteTag(cover) → ReadCover.
                std::vector<unsigned char> dataB(40);
                for (size_t i = 0; i < dataB.size(); ++i)
                    dataB[i] = static_cast<unsigned char>(0xC0 + i);
                std::vector<unsigned char> wav;
                wav.insert(wav.end(), "RIFF", "RIFF" + 4);
                pushLe32t(wav, 0);
                wav.insert(wav.end(), "WAVE", "WAVE" + 4);
                {
                    std::vector<unsigned char> c;
                    c.insert(c.end(), "data", "data" + 4);
                    pushLe32t(c, static_cast<unsigned int>(dataB.size()));
                    c.insert(c.end(), dataB.begin(), dataB.end());
                    wav.insert(wav.end(), c.begin(), c.end());
                }
                const unsigned int rp = static_cast<unsigned int>(wav.size() - 8);
                wav[4] = static_cast<unsigned char>(rp & 0xFF);
                wav[5] = static_cast<unsigned char>((rp >> 8) & 0xFF);
                wav[6] = static_cast<unsigned char>((rp >> 16) & 0xFF);
                wav[7] = static_cast<unsigned char>((rp >> 24) & 0xFF);
                check(putFile(p, wav), "2u-⑭ 합성 WAV 구성 = 성립(id3 부재)");
                mt::TagSet t14;
                t14.title = "Wp T";
                mt::CoverArt c14;
                c14.mime = "png";  // 정규화 전 표기(기입 = image/png)
                c14.bytes = jpg;
                std::string err14;
                check(mt::WriteTag(p.generic_string(), t14, &c14, err14) &&
                          err14.empty(),
                      "2u-⑭ APIC 삽입 쓰기 = ok(id3 청크 신설+APIC leg)");
                mt::CoverArt rc14;
                std::string err14b;
                check(mt::ReadCover(p.generic_string(), rc14, err14b) &&
                          rc14.mime == "image/png" && rc14.bytes == jpg,
                      "2u-⑭ ReadCover round-trip = mime 정규화+그림 512B 등호"
                      "(id3 청크 APIC 파싱 재용)");
                // (b) 기존 id3 청크 파일 — ReadCover 직접 leg.
                const fs::path q = tdir / "2u_n_wav2.wav";
                std::vector<unsigned char> oldArt(96);
                for (size_t i = 0; i < oldArt.size(); ++i)
                    oldArt[i] = static_cast<unsigned char>(0x20 + i);
                std::vector<unsigned char> apicBody;
                apicBody.push_back(0);  // enc 0 — Latin-1 mime
                apicBody.insert(apicBody.end(), "image/jpeg", "image/jpeg" + 10);
                apicBody.push_back(0);  // mime 종결
                apicBody.push_back(0);  // desc 빈 종결
                apicBody.insert(apicBody.end(), oldArt.begin(), oldArt.end());
                // 독립 조립 — v2.3 태그: TIT2(Latin-1)+APIC.
                std::vector<unsigned char> tit2Body;
                tit2Body.push_back(0x00);
                tit2Body.insert(tit2Body.end(), "Wt", "Wt" + 2);
                std::vector<unsigned char> tagBody =
                    mkFrame("TIT2", false, tit2Body);
                {
                    auto f = mkFrame("APIC", false, apicBody);
                    tagBody.insert(tagBody.end(), f.begin(), f.end());
                }
                std::vector<unsigned char> id3Tag;
                std::vector<unsigned char> mk = {'I', 'D', '3', 0x03, 0x00,
                                                 0x00};
                pushSyncsafe(mk,
                             static_cast<unsigned int>(tagBody.size()));
                mk.insert(mk.end(), tagBody.begin(), tagBody.end());
                std::vector<unsigned char> wav2;
                wav2.insert(wav2.end(), "RIFF", "RIFF" + 4);
                pushLe32t(wav2, 0);  // 크기 자리(후산 재기입 — RIFF 계약)
                wav2.insert(wav2.end(), "WAVE", "WAVE" + 4);
                // id3 청크 조립(기존 태그 = 청크 1번째 — RIFF 순서 계약).
                std::vector<unsigned char> id3Chunk;
                id3Chunk.insert(id3Chunk.end(), "id3 ", "id3 " + 4);
                pushLe32t(id3Chunk, static_cast<unsigned int>(mk.size()));
                id3Chunk.insert(id3Chunk.end(), mk.begin(), mk.end());
                {
                    std::vector<unsigned char> c;
                    c.insert(c.end(), "data", "data" + 4);
                    pushLe32t(c, static_cast<unsigned int>(dataB.size()));
                    c.insert(c.end(), dataB.begin(), dataB.end());
                    wav2.insert(wav2.end(), id3Chunk.begin(), id3Chunk.end());
                    wav2.insert(wav2.end(), c.begin(), c.end());
                }
                const unsigned int rp2 =
                    static_cast<unsigned int>(wav2.size() - 8);
                wav2[4] = static_cast<unsigned char>(rp2 & 0xFF);
                wav2[5] = static_cast<unsigned char>((rp2 >> 8) & 0xFF);
                wav2[6] = static_cast<unsigned char>((rp2 >> 16) & 0xFF);
                wav2[7] = static_cast<unsigned char>((rp2 >> 24) & 0xFF);
                check(putFile(q, wav2),
                      "2u-⑭(b) 기존 id3 청크 WAV 구성 = 성립(TIT2+APIC — "
                      "fmt/data 생략 독립 구조)");
                mt::CoverArt rc14b;
                std::string err14c;
                check(mt::ReadCover(q.generic_string(), rc14b, err14c) &&
                          rc14b.mime == "image/jpeg" &&
                          rc14b.bytes == oldArt,
                      "2u-⑭(b) 기존 id3 APIC = ReadCover 직접 leg(mime+96B 등"
                      "호 — RIFF 보행 → ReadId3Tag → ApicFromFrames)");
                // 기존 APIC 보존 — cover=null 재작성(mp3 leg 동형 사슬 재용).
                {
                    mt::TagSet tn;
                    tn.title = "Wk T";
                    std::string er;
                    check(mt::WriteTag(q.generic_string(), tn, nullptr, er) &&
                              er.empty(),
                          "2u-⑭(b) cover=null 재작성 = ok(기존 APIC 유지)");
                    mt::CoverArt rc2;
                    std::string er2;
                    check(mt::ReadCover(q.generic_string(), rc2, er2) &&
                              rc2.mime == "image/jpeg" && rc2.bytes == oldArt,
                          "2u-⑭(b) 기존 APIC 생존 = 등호(D8 역 — id3 청크 갱신"
                          " 시 보존)");
                    std::string er3;
                    const mt::TagSet back =
                        mt::ParseTag(q.generic_string(), er3);
                    check(back.title == "Wk T" && er3.empty(),
                          "2u-⑭(b) 재독 = 새 title 등호(기존 id3 페이로드 갱"
                          "신 round-trip)");
                }
                // REMNANT 0 — 사후 소각(2s/2r/⑦ 정리 수형 재용).
                std::error_code sec14;
                fs::remove_all(tdir, sec14);
                check(!sec14 && !fs::exists(tdir, sec14),
                      "2u-⑭ 사후 소각 = 잔산 0(REMNANT 0 — T2 전체)");
            }

            // ⑮) 도구 경계 (T4 — 스펙 §4): music_tag_write args →
            // ParseTagWriteArgs → TagWriteTool(UI [저장] SaveTagEdit와 동일
            // WriteTag 사슬) — args 5필드 판독(quickjs 원문 회수)·cover
            // 마법수 mime 판정(FFD8=jpeg·8950=png — 브리프 원문)·재독 표기
            // 등호(ParseTag round-trip)+ReadCover round-trip·cover 생략 = 기존
            // 유지(null 계약). ⑭ 사후 소각 뒤의 tdir 재개장(⑧-⑭ 계동형).
            fs::create_directories(tdir);
            // 합성 png 블롭(mime 판정 단정 몫 — 마법수 8B+결정론 패턴).
            std::vector<unsigned char> pngBlob;
            {
                const unsigned char sign[8] = {0x89, 0x50, 0x4E, 0x47,
                                               0x0D, 0x0A, 0x1A, 0x0A};
                pngBlob.assign(sign, sign + 8);
                for (size_t i = 8; i < 200; ++i)
                    pngBlob.push_back(
                        static_cast<unsigned char>(i * 13 + 5));
            }
            auto jsonEsc = [](const std::string& in) {
                std::string out;
                for (char c : in) {
                    if (c == '"' || c == '\\') {
                        out += '\\';
                        out += c;
                    } else {
                        out += c;
                    }
                }
                return out;
            };
            const std::string coverJpg =
                (tdir / "2u_o_cover.jpg").generic_string();
            const std::string coverPng =
                (tdir / "2u_o_cover.png").generic_string();
            const std::string coverJunk =
                (tdir / "2u_o_junk.bin").generic_string();
            const std::string toolMp3 =
                (tdir / "2u_o_tool.mp3").generic_string();
            check(putFile(fs::path(coverJpg), jpg),
                  "2u-⑮ 합성 커버 jpg 구성 = 성립");
            check(putFile(fs::path(coverPng), pngBlob),
                  "2u-⑮ 합성 커버 png 구성 = 성립");
            {
                std::vector<unsigned char> junk(64);
                for (size_t i = 0; i < junk.size(); ++i)
                    junk[i] = static_cast<unsigned char>(i + 1);
                check(putFile(fs::path(coverJunk), junk),
                      "2u-⑮ 합성 커버 junk 구성 = 성립(거부 대상)");
            }
            check(putFile(fs::path(toolMp3), audio),
                  "2u-⑮ 합성 mp3(무태그) 구성 = 성립");
            const std::string args1 =
                "{\"path\":\"" + jsonEsc(toolMp3) +
                "\",\"title\":\"Tool Title\",\"artist\":\"Tool Artist\","
                "\"album\":\"Tool Album\",\"cover\":\"" +
                jsonEsc(coverJpg) + "\"}";
            jk::music::tool::TagWriteArgs parsed;
            check(jk::music::tool::ParseTagWriteArgs(args1, parsed) &&
                      parsed.path == toolMp3 && parsed.title == "Tool Title" &&
                      parsed.artist == "Tool Artist" &&
                      parsed.album == "Tool Album" && parsed.cover == coverJpg,
                  "2u-⑮ args 판독 = 5필드 등호(ParseTagWriteArgs — 도구 허브"
                  " 원존 파서 quickjs 원문 회수)");
            jk::music::tool::TagWriteArgs pOmit, pNull, pNoPath, pBroken;
            check(jk::music::tool::ParseTagWriteArgs(
                      "{\"path\":\"X\",\"title\":\"t\"}", pOmit) &&
                      pOmit.cover.empty() && pOmit.title == "t",
                  "2u-⑮ cover 생략 = 빈값(생략 = null — 기존 유지 계약 취급)");
            check(jk::music::tool::ParseTagWriteArgs(
                      "{\"path\":\"X\",\"cover\":null}", pNull) &&
                      pNull.cover.empty(),
                  "2u-⑮ cover null = 판독 실패 = 빈값(null = 합법 — 도구 계약 "
                  "args 계약 cover(path|null))");
            check(!jk::music::tool::ParseTagWriteArgs("{\"title\":\"t\"}",
                                                      pNoPath),
                  "2u-⑮ path 누락 = 판독 거부(path 필수 — M-3 원문 가드 수형)");
            check(!jk::music::tool::ParseTagWriteArgs("{", pBroken),
                  "2u-⑮ args 파손 = 판독 거부(정직)");
            {
                jk::music::tags::CoverArt art;
                std::string cerr;
                check(jk::music::tool::LoadCoverByMagic(coverJpg, art,
                                                        cerr) &&
                          art.mime == "image/jpeg" && art.bytes == jpg,
                      "2u-⑮ 커버 마법수 = image/jpeg+bytes 등호(FFD8 SOI — "
                      "브리프 원문 판정 leg)");
            }
            {
                jk::music::tags::CoverArt art;
                std::string cerr;
                check(jk::music::tool::LoadCoverByMagic(coverPng, art,
                                                        cerr) &&
                          art.mime == "image/png" && art.bytes == pngBlob,
                      "2u-⑮ 커버 마법수 = image/png(89 50 4E 47 — mime 판정 "
                      "2형)");
            }
            {
                jk::music::tags::CoverArt art;
                std::string cerr;
                check(!jk::music::tool::LoadCoverByMagic(coverJunk, art,
                                                         cerr) &&
                          cerr == "cover magic unsupported" && art.mime.empty(),
                      "2u-⑮ 마법수 불명 = 정직 거부(cover magic unsupported — "
                      "판정 불가 mime)");
            }
            std::string wErr;
            check(jk::music::tool::TagWriteTool(parsed, wErr) && wErr.empty(),
                  "2u-⑮ 도구 쓰기 = ok(WriteTag 사슬 — UI [저장]과 동일)");
            {
                std::string terr;
                const mt::TagSet back = mt::ParseTag(toolMp3, terr);
                check(back.title == "Tool Title" &&
                          back.artist == "Tool Artist" &&
                          back.album == "Tool Album" && terr.empty(),
                      "2u-⑮ 재독 표기 등호 = 3필드 round-trip(ParseTag 재독 — "
                      "도구 경계 쓰기)");
                jk::music::tags::CoverArt art;
                std::string cerr;
                check(jk::music::tags::ReadCover(toolMp3, art, cerr) &&
                          art.mime == "image/jpeg" && art.bytes == jpg,
                      "2u-⑮ ReadCover round-trip = mime+그림 등호(APIC 삽입)");
            }
            // 둘째 쓰기 — cover 생략(빈값) = 기존 아트 유지(null 계약 수형).
            jk::music::tool::TagWriteArgs p2nd;
            check(jk::music::tool::ParseTagWriteArgs(
                      "{\"path\":\"" + jsonEsc(toolMp3) +
                          "\",\"title\":\"Tool 2\"}",
                      p2nd) && p2nd.cover.empty(),
                  "2u-⑮ 둘째 args = cover 빈값 파독(성립)");
            std::string wErr2;
            check(jk::music::tool::TagWriteTool(p2nd, wErr2) && wErr2.empty(),
                  "2u-⑮ 도구 쓰기(cover 생략) = ok(빈값 = null — 유지 leg)");
            {
                jk::music::tags::CoverArt art;
                std::string cerr;
                std::string terr2;
                const mt::TagSet back2 = mt::ParseTag(toolMp3, terr2);
                check(jk::music::tags::ReadCover(toolMp3, art, cerr) &&
                          art.bytes == jpg && back2.title == "Tool 2",
                      "2u-⑮ APIC 생존+재독 = 유지 등호(cover 미지정 수형 — "
                      "2u-⑥ 도구 경계 수형)");
            }
            // 마법수 불명 커버 지정 쓰기 = 정직 거부(원본 무손상 — 사슬 진입 전).
            jk::music::tool::TagWriteArgs pJunk;
            check(jk::music::tool::ParseTagWriteArgs(
                      "{\"path\":\"" + jsonEsc(toolMp3) + "\",\"cover\":\"" +
                          jsonEsc(coverJunk) + "\"}",
                      pJunk),
                  "2u-⑮ junk 커버 args 파독 = 성립");
            std::string junkErr;
            std::string preJunk;
            check(readFile(fs::path(toolMp3), preJunk) &&
                      !jk::music::tool::TagWriteTool(pJunk, junkErr) &&
                      junkErr == "cover magic unsupported",
                  "2u-⑮ 마법수 불명 커버 쓰기 = 정직 거부(사슬 진입 전 중단 — "
                  "err 원문)");
            // 부재 경로 = 정직 거부(err 원문 전파 — 도구 결과 1행의 몫).
            jk::music::tool::TagWriteArgs pGone;
            const std::string gonePath =
                (tdir / "2u_o_gone.mp3").generic_string();
            check(jk::music::tool::ParseTagWriteArgs(
                      "{\"path\":\"" + jsonEsc(gonePath) + "\"}", pGone),
                  "2u-⑮ 부재 경로 args 파독 = 성립");
            std::string goneErr;
            check(!jk::music::tool::TagWriteTool(pGone, goneErr) &&
                      !goneErr.empty(),
                  "2u-⑮ 부재 경로 쓰기 = 정직 거부+err 원문(WriteTag 계약 전파)");
            // REMNANT 0 — 사후 소각(2s/2r/⑦/⑭ 정리 수형 재용).
            std::error_code sec15;
            fs::remove_all(tdir, sec15);
            check(!sec15 && !fs::exists(tdir, sec15),
                  "2u-⑮ 사후 소각 = 잔산 0(REMNANT 0 — T4 도구)");
        }
    }

    // 2v) 구두 leg fail-closed 판정부 (#112 T1 — 스펙 2026-10-25-stt-voice-design
    // §3 ② fail-closed 계약·⑦ 2v 계보·R1 WHISPER_ROOT): 순수 판정부 4건.
    // 전사 비결정 원칙(스펙 §2.4) — selftest는 실제 whisper-cli 스폰을 하지
    // 않고 판정 로직만 단정한다(전사 본문 단정 금지). 폰(Termux aarch64) 축도
    // 동일 4건을 달린다 — 판정부에 축 분기 없음(aarch64 소속 차이는 T5 빌드).
    {
        // ① env 미설정 = 구두 끔+정직 표기(WHISPER_ROOT fail-closed 원존 계약 —
        //   미설정 빌드에서 나머지 시스템 전부 무영향).
        const jk::stt::VoiceVerdict v1 =
            jk::stt::VoiceLegVerdict(false, true, true);
        check(!v1.enabled &&
                  std::strcmp(v1.reason, "구두 끔 (WHISPER_ROOT 미설정)") == 0,
              "2v-① env 미설정 = 구두 끔+정직 표기 등호(fail-closed — 미설정 "
              "빌드 무영향)");
        // ② env 설정+whisper-cli 부재 = 동일 오프(스폰 계약 원존 — 별도
        //   프로세스 바이너리가 조건의 뿌리).
        const jk::stt::VoiceVerdict v2 =
            jk::stt::VoiceLegVerdict(true, false, true);
        check(!v2.enabled &&
                  std::strcmp(v2.reason, "구두 끔 (whisper-cli 부재)") == 0,
              "2v-② env 설정+바이너리 부재 = 구두 끔+표기 등호(fail-closed)");
        // ③ env 설정+whisper-cli 성립+모델 부재 = 동일 오프(ggml 모델은
        //   무추적 조달기 산출 — 부재 = 정직 거부, 암묵 다운그레이드 금지).
        const jk::stt::VoiceVerdict v3 =
            jk::stt::VoiceLegVerdict(true, true, false);
        check(!v3.enabled &&
                  std::strcmp(v3.reason, "구두 끔 (whisper 모델 부재)") == 0,
              "2v-③ env 설정+모델 부재 = 구두 끔+정직 표기 등호(fail-closed)");
        // ④ 전원 성립 = 구두 켬.
        const jk::stt::VoiceVerdict v4 =
            jk::stt::VoiceLegVerdict(true, true, true);
        check(v4.enabled &&
                  std::strcmp(v4.reason, "구두 켬 (whisper-cli)") == 0,
              "2v-④ 전원 성립 = 구두 켬(스폰 계약 성립 — 판정부 유일 통과 수형)");
    }

    // 2v-b) wav — 16 kHz 모노 PCM WAV 헤더 판정부 (#112 T2 — 스펙 2026-10-25-
    // stt-voice-design §3 ②·⑦ 계보·§2.4). 합성 RIFF/WAVE 블롭 — 파일 쓰기 0
    // (순수 헤더 파싱 단정). ffmpeg 정규화 결과(-ar 16000 -ac 1 -f wav) 입력
    // 전제 규약 — 스폰 0·전사 비결정 원칙 무접촉.
    {
        auto le16 = [](unsigned v, std::vector<unsigned char>& b) {
            b.push_back(static_cast<unsigned char>(v & 0xFF));
            b.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
        };
        auto le32 = [](unsigned long v, std::vector<unsigned char>& b) {
            for (unsigned long s = 0; s < 32; s += 8)
                b.push_back(static_cast<unsigned char>((v >> s) & 0xFF));
        };
        auto putTag = [](const char (&tag)[5], std::vector<unsigned char>& b) {
            b.insert(b.end(), tag, tag + 4);
        };
        // riffBlob(모노/스테레오·레이트·비트심도·데이터 바이트) — 캐논 형태
        // (RIFF+fmt 16+data)의 확장 파라미터 형 — 합격/거부 케이스 공용.
        auto riffBlob = [&](unsigned channels, unsigned rate, unsigned bits,
                            unsigned dataBytes) {
            std::vector<unsigned char> b;
            putTag("RIFF", b);
            le32(0, b);  // riff size — 조립 뒤 패치
            putTag("WAVE", b);
            putTag("fmt ", b);
            le32(16, b);  // fmt 청크 크기 (PCM 캐논 16)
            le16(1, b);   // wFormatTag = PCM
            le16(channels, b);
            le32(rate, b);
            le32(rate * channels * bits / 8, b);  // nAvgBytesPerSec
            le16(channels * bits / 8, b);         // nBlockAlign
            le16(bits, b);
            putTag("data", b);
            le32(dataBytes, b);
            b.insert(b.end(), dataBytes,
                     static_cast<unsigned char>('p'));  // 데이터 몸통 (값 무관)
            const unsigned long riffSize =
                static_cast<unsigned long>(b.size() - 8);
            for (int i = 0; i < 4; ++i)
                b[4 + i] = static_cast<unsigned char>((riffSize >> (8 * i)) &
                                                      0xFF);
            return b;
        };
        // ⑤ ffmpeg 정규화 결과와 동일 형태 = 합격.
        {
            const std::vector<unsigned char> okBlob =
                riffBlob(1, 16000, 16, 64);
            check(jk::voice::wav::Validate16kMono(okBlob.data(), okBlob.size()),
                  "2v-⑤ 16k 모노 PCM 합성 = 합격(ffmpeg -ar 16000 -ac 1 정규화 "
                  "전제)");
        }
        // ⑥ 비-16k (44.1kHz — 마이크 원시 출력 그대로) = 정직 거부 —
        //   리샘플은 ffmpeg 몫, 리-인코드 재합성 금지(정규화 사전 계약).
        {
            const std::vector<unsigned char> rateBlob =
                riffBlob(1, 44100, 16, 64);
            check(!jk::voice::wav::Validate16kMono(rateBlob.data(),
                                                   rateBlob.size()),
                  "2v-⑥ 비-16k(44100) = 정직 거부(정규화 사전 계약 — 리샘플 재합성 "
                  "금지)");
        }
        // ⑦ 스테레오 (termux-microphone-record 기본 축 수준) = 정직 거부 —
        //   모노 축소도 정규화(ffmpeg -ac 1) 몫이다.
        {
            const std::vector<unsigned char> stBlob = riffBlob(2, 16000, 16, 64);
            check(!jk::voice::wav::Validate16kMono(stBlob.data(),
                                                   stBlob.size()),
                  "2v-⑦ 스테레오 = 정직 거부(모노 규약 — 축소 재합성 금지)");
        }
    }

    // 2v-c) transcript — whisper-cli stdout 최후 텍스트 절단 (#112 T2 — 스펙
    // §3 ② 응답 형식·⑦ 계보). 고정 입력 블롭 — cli.cpp 원문 구조 실측 규약:
    //   시스템 로그/usage/진행률은 전부 stderr — stdout은 세그먼트 행만
    //   (첫 printf("\n") — s0==0 원문 + `[ts --> ts]  텍스트\n` 반복).
    // 방어 원료 규약(ChatLlmActionParse 동형): 빈/이상 출력 = 빈 문자열
    // 반환·err 없이 정직 — 본문 단정은 전사 비결정 원칙(§2.4) 유지.
    {
        // ⑧ 세그먼트 2행 + 선행 개행(원문 모양) = 최후 텍스트 절단 등호.
        const std::string twoSegs =
            "\n"
            "[00:00:00.000 --> 00:00:01.560]  "
            "\xec\xa7\x80\xeb\xa2\xb0\xec\xb0\xbe\xea\xb8\xb0\n"
            "[00:00:01.560 --> 00:00:02.430]  \xec\xbc\x9c\xec\xa4\x98\n";
        check(jk::voice::transcript::ParseLastText(twoSegs) ==
                  "\xec\xbc\x9c\xec\xa4\x98",
              "2v-⑧ 고정 cli 출력 2세그먼트 = 최후 텍스트 절단 등호(원문 구조)");
        // ⑨ 빈 출력 = 빈 문자열(오류 없음 — 전사 결실 0의 모양).
        check(jk::voice::transcript::ParseLastText("").empty(),
              "2v-⑨ 빈 출력 = 빈 문자열 반환(오류 없음 — 방어 원료 규약)");
        // ⑩ malformed (끊긴 타임스탬프+가짜 행) = 빈 문자열(구조 비적합 —
        //   절단 불가를 조용한 쓰레기로 수용하지 않는다).
        check(jk::voice::transcript::ParseLastText(
                  "[00:00:00 --> 00:00\n\xea\xb0\x80\xec\xa7\x9c\n")
                  .empty(),
              "2v-⑩ malformed 출력 = 빈 문자열(세그먼트 구조 비적합 — 정직)");
    }

    // 2v-d) STT 전사 문자 → ChatRouteTurn 접점 (#112 T3 — 스펙 2026-10-25-
    // stt-voice-design §3 ③ 안 A·⑦ 계보). 1n-f 계열의 **입력 소스 치환
    // 동형** — 발화가 타이핑이 아니라 구두(whisper 전사 문자)에서 들어온
    // 것뿐이다. 어휘 표기 3건 + 질의 조립 — 어설션은 **라우터 즉발 경로만**
    // 단정(cfg 미구성 — LLM 턴 비결정 단정 금지(§2.4)·스폰 0·스포른 0).
    // 전사 비결정 원칙상 whisper 단정 없음 — 여기 들어가는 "전사 문자"는
    // 고정 입력(EYES ①-⑤ 어휘).
    {
        using jk::agent::ChatConfig;
        // 미구성 cfg — 1n-f16 계열(engine="stub" + 표지 — 즉발 경로 고정).
        // (chat.json 파일 접촉 없음 — LoadChatConfig을 쓰지 않는다 — 라우터
        // 즉발 경로의 인자 진실원은 이 판정 게이트의 값 관습(1n-f29 NR4-1)
        // 그대로.) usedLlm는 오염 증거로 시작 — 즉발이 지우지 못하면 FAIL.
        ChatConfig vc;         // 미구성 — 스폰 자체를 안 한다
        vc.engine = "stub";
        vc.fileKnown = false;  // 파일 존재 표지는 세우지 않는다(1n-f30 규약)
        {
            // ⑪ "지뢰찾기 켜줘" — Launch + 앱 별명 해소. 1n-f14/15가 타이핑
            //   입력으로 단정한 도표의 구두 입력 판 — guide는 기존 라우터
            //   guide 원문 그대로(1n-f15 등호).
            jk::ChatAction a;
            bool used = true;  // 오염 증거 — 배선이 지우지 못하면 실패
            const std::string guide = jk::ChatRouteTurn(
                "\xec\xa7\x80\xeb\xa2\xb0\xec\xb0\xbe\xea\xb8\xb0 "
                "\xec\xbc\x9c\xec\xa4\x98",  // 지뢰찾기 켜줘
                a, vc, &used);
            jk::ChatAction ar;
            check(!used && a.kind == jk::ChatAction::Launch &&
                      a.app == "minesweeper" &&
                      guide ==
                          jk::ChatRouterRoute(
                              "\xec\xa7\x80\xeb\xa2\xb0\xec\xb0\xbe"
                              "\xea\xb8\xb0 \xec\xbc\x9c\xec\xa4\x98",
                              ar),
                  "2v-⑪ 구두 전사 문자 \"지뢰찾기 켜줘\" = Launch(minesweeper) "
                  "즉발(1n-f14 입력 소스 치환 동형 — usedLlm=false)");
            // ⑫ 질의 조립 — ChatActionQuery(기존 도구 경로 재용 —
            //   ClientChatApp SendQuery 양식 그대로 — 새 파서 0).
            std::string tool, args;
            check(jk::voice::turn::ChatActionQuery(a, tool, args) &&
                      tool == "launch_app" && args == "{\"app\":\"minesweeper\"}",
                  "2v-⑫ launch_app 질의 조립 = args 등호(도구 경로 재용)");
        }
        {
            // ⑬ "창 목록" — ListWindows + list_windows 질의.
            jk::ChatAction b;
            bool used = true;
            (void)jk::ChatRouteTurn("\xec\xb0\xbd "
                                    "\xeb\xaa\xa9\xeb\xa1\x9d",  // 창 목록
                                    b, vc, &used);
            std::string tool2, args2;
            check(!used && b.kind == jk::ChatAction::ListWindows &&
                      jk::voice::turn::ChatActionQuery(b, tool2, args2) &&
                      tool2 == "list_windows" && args2 == "{}",
                  "2v-⑬ 구두 전사 문자 \"창 목록\" = list_windows 질의(즉발)");
        }
        {
            // ⑭ "닫아줘" — Close 무앱 → argless 질의(포커스 창 위임 —
            //   폰 "닫아줘" 계약 원존).
            jk::ChatAction c;
            bool used = true;
            (void)jk::ChatRouteTurn("\xeb\x8b\xab\xec\x95\x84\xec\xa4\x98",  // 닫아줘
                                    c, vc, &used);
            std::string tool3, args3;
            check(!used && c.kind == jk::ChatAction::Close && c.app.empty() &&
                      jk::voice::turn::ChatActionQuery(c, tool3, args3) &&
                      tool3 == "close_window" && args3 == "{}",
                  "2v-⑭ 구두 전사 문자 \"닫아줘\" = argless close_window "
                  "(포커스 창 위임)");
        }
    }

    // 2y) 구두 어휘 확장 v1.5 — 재생+알림 도메인 (#115b T1 — 스펙
    //   2026-10-27-voice-vocab-design D1-D5). 1n·2v-d 계열의 어휘표 확장 판 —
    //   ①신설 어휘 → ChatAction 등호(원문 대조·usedLlm=0 — 즉발 경로 고정)
    //   ②기존 어휘 회귀(정확매치) ③별명 우선순위(경쟁 어휘 — 1표 1의미) ④
    //   무인자 강제(D1 — 인자 포함 발화 = AppTool 미매치)+D3 배제 어휘 stub
    //   원장행 어설션. **접점 실측(2y의 성립 근거 — 리포트 접점 표)**:
    //   play_pause = vplayer 등록 도구(ClientVPlayerApp.cpp OnInit
    //   SendAgentToolRegister 6종 — 무인자 "{}")·notify 읽기 도구 = 부재
    //   (ClientNotifyApp 등록 0·publish_event = 서버 쓰기 전용) → "알림 뭐
    //   왔어"류 = 배제(stub). 스폰 0 — 순수 라우터+질의 조립 단정.
    {
        using jk::agent::ChatConfig;
        // 미구성 cfg — 1n-f16/2v-d 계열(engine="stub"+무표지 — 즉발 경로
        // 고정). usedLlm는 오염 증거로 시작 — 즉발이 지우지 못하면 FAIL.
        ChatConfig vc;
        vc.engine = "stub";
        vc.fileKnown = false;

        // ① 신설 재생 어휘 → AppTool 등호.
        jk::ChatAction a;
        bool used = true;
        const std::string g1 = jk::ChatRouteTurn("멈춰줘", a, vc, &used);
        check(!used && a.kind == jk::ChatAction::AppTool &&
                  a.app == "vplayer" && a.tool == "play_pause",
              "2y-① \"멈춰줘\" = AppTool(vplayer·play_pause) 즉발(usedLlm 0)");
        check(g1.find("토글") != std::string::npos,
              "2y-② play_pause 토글 안내문(양방향 정직 표기 — stop 도구 부재 "
              "토글 수용 원장행)");
        std::string tool, args;
        check(jk::voice::turn::ChatActionQuery(a, tool, args) &&
                  tool == "app_tool" &&
                  args == "{\"app\":\"vplayer\",\"tool\":\"play_pause\","
                          "\"args\":{}}",
              "2y-③ app_tool 질의 조립 등호(릴레이 양식 — probe app_tool 원문)");
        // fix r1 — 타이핑·웹 경로의 AppTool 소비 = 전송 없이 공유 정직 문언
        // (Submit/jkweb/jktalk 3소비자가 같은 1본을 먹는다 — ChatRouter.h
        // 계약). 라우터 즉발 확인문("보냅니다 — g1")과 별개 문언임을 등호 부일
        // 로 단정한다(⑩ 방어 행과 별개 — 2y-⑰).
        const std::string gt = jk::ChatAppToolTypingGuide(a);
        check(!gt.empty() && gt.find("구두") != std::string::npos &&
                  gt.find("타이핑") != std::string::npos && gt != g1,
              "2y-⑰ 타이핑·웹 경로 AppTool = 전송 없이 공유 정직 안내문(구두 "
              "표면 전용 — 즉발 확인문과 별개 문언)");

        jk::ChatAction b;
        bool usedB = true;
        const std::string g2 = jk::ChatRouteTurn("재생해줘", b, vc, &usedB);
        check(!usedB && b.kind == jk::ChatAction::AppTool &&
                  b.app == "vplayer" && b.tool == "play_pause" &&
                  g2 == g1,
              "2y-④ \"재생해줘\" = 같은 접점 도구 등호(7행 1릴레이 — 안내문도 "
              "등호)");
        jk::ChatAction q;
        bool usedQ = true;
        (void)jk::ChatRouteTurn("일시정지", q, vc, &usedQ);
        check(!usedQ && q.kind == jk::ChatAction::AppTool &&
                  q.app == "vplayer" && q.tool == "play_pause",
              "2y-⑤ \"일시정지\"가 접두 공유 짧은 행 \"정지\"에 잡아먹히지 "
              "않는다(표 순서+exact 게이트)");

        // ① 알림 — Launch 재용(신설 별명 1행 + 신설 트리거 "보여줘").
        jk::ChatAction n;
        bool usedN = true;
        (void)jk::ChatRouteTurn("알림 보여줘", n, vc, &usedN);
        check(!usedN && n.kind == jk::ChatAction::Launch && n.app == "notify",
              "2y-⑥ \"알림 보여줘\" = Launch(notify) 즉발(앱어=별명 해소 — "
              "기존 Launch 경로 재용)");
        std::string toolN, argsN;
        check(jk::voice::turn::ChatActionQuery(n, toolN, argsN) &&
                  toolN == "launch_app" && argsN == "{\"app\":\"notify\"}",
              "2y-⑦ launch_app(notify) 질의 조립 등호");

        // ② 기존 어휘 회귀 — 신설 행(표 끝 배치)이 기존 판정을 바꾸지 않는다.
        jk::ChatAction r1;
        (void)jk::ChatRouterRoute("지뢰찾기 켜줘", r1);
        check(r1.kind == jk::ChatAction::Launch && r1.app == "minesweeper",
              "2y-⑧ 기존 회귀 — \"지뢰찾기 켜줘\"(1n-1 등호)");
        jk::ChatAction r2;
        (void)jk::ChatRouterRoute("꺼줘", r2);
        check(r2.kind == jk::ChatAction::Close && r2.app.empty(),
              "2y-⑨ 기존 회귀 — \"꺼줘\" argless Close(1n-7 등호)");

        // ③ 별명 우선순위(경쟁 어휘 — 1표 1의미·이중 해석 봉쇄).
        jk::ChatAction r3;
        (void)jk::ChatRouterRoute("창 목록 보여줘", r3);
        check(r3.kind == jk::ChatAction::ListWindows,
              "2y-⑩ 표 순서 — \"창 목록\" 행이 신설 뒤접미 트리거 \"보여줘\"에 "
              "잡아먹히지 않는다(ListWindows 우선)");
        jk::ChatAction r4;
        (void)jk::ChatRouterRoute("알림 꺼줘", r4);
        check(r4.kind == jk::ChatAction::Close && r4.app == "notify",
              "2y-⑪ Close 경에서도 별명 해소(알림→notify — 별명은 표 1행)");

        // ④ 무인자 강제(D1) — 인자·접사 포함 발화 = AppTool 행 미매치.
        jk::ChatAction x1;
        (void)jk::ChatRouterRoute("볼륨 50으로 멈춰줘", x1);
        check(x1.kind == jk::ChatAction::Info && x1.app.empty() &&
                  x1.tool.empty(),
              "2y-⑫ 무인자 강제 — \"볼륨 50으로 멈춰줘\" = 미매치(AppTool은 "
              "발화 전체 등호만)");

        // ④ D3 배제 어휘 — 도구 접점 부재 = 행 없음(stub 유지 — 원장행 어설션
        // 으로 흔적 보존).
        jk::ChatAction x2;
        (void)jk::ChatRouterRoute("뮤비 좀 멈춰줘", x2);
        check(x2.kind == jk::ChatAction::Info,
              "2y-⑬ 무인자 강제 — 접사 발화(\"뮤비 좀 멈춰줘\")도 미매치");
        jk::ChatAction x3;
        const std::string gx3 = jk::ChatRouterRoute("다음 곡", x3);
        check(x3.kind == jk::ChatAction::Info &&
                  gx3.find("명령") != std::string::npos,
              "2y-⑭ D3 배제 — \"다음 곡\" 미매치 stub(vplayer·music next/prev "
              "도구 부재 — 신설 도구 금지)");
        jk::ChatAction x4;
        (void)jk::ChatRouterRoute("알림 뭐 왔어", x4);
        check(x4.kind == jk::ChatAction::Info,
              "2y-⑮ D3 배제 — \"알림 뭐 왔어\" 미매치(notify 읽기 도구 부재 — "
              "publish_event는 쓰기 전용)");
        // stub 안내문이 신설 어휘를 함께 훑는다(InfoGuide — 표 동일 진실원).
        jk::ChatAction x5;
        const std::string gx5 =
            jk::ChatRouterRoute("세상엔 채팅이 이렇게 어려웠나", x5);
        check(x5.kind == jk::ChatAction::Info &&
                  gx5.find("멈춰줘") != std::string::npos &&
                  gx5.find("보여줘") != std::string::npos,
              "2y-⑯ Info 안내문이 신설 어휘를 함께 훑는다(kVoicePrompt와 같은 "
              "어휘 원문 동기)");

        // v1.6 — play_stop(vplayer 도구 7종째 — #115b 원장행 "stop 도구
        // 부재" 승계 소각): "완전히 정지해줘" = 방향 고정 정지(토글
        // 아님). (a) 어휘→AppTool 등호  (b) app_tool 질의 조립 등호
        // (ClientChatApp 등 소비자 스위치는 AppTool 일반 — 무변경 계약)
        // (c) Info 훑기(⑯ 원존 계약 계승)  (d) 기존 토글 행 회귀 —
        // 신설 행(표 끝)이 "정지해줘" 접두 판정을 건드리지 않는다.
        jk::ChatAction s6;
        bool usedS6 = true;
        const std::string gs6 =
            jk::ChatRouteTurn("완전히 정지해줘", s6, vc, &usedS6);
        check(!usedS6 && s6.kind == jk::ChatAction::AppTool &&
                  s6.app == "vplayer" && s6.tool == "play_stop",
              "2y-⑱ \"완전히 정지해줘\" = AppTool(vplayer·play_stop) 등호"
              "(도구 7종째 — 방향 고정 정지·토글 구별)");
        std::string toolS6, argsS6;
        check(jk::voice::turn::ChatActionQuery(s6, toolS6, argsS6) &&
                  toolS6 == "app_tool" &&
                  argsS6 == "{\"app\":\"vplayer\",\"tool\":\"play_stop\","
                            "\"args\":{}}",
              "2y-⑱(b) app_tool 질의 조립 등호(릴레이 양식 — 2y-③ 동형)");
        check(gs6.find("정지") != std::string::npos &&
                  gx5.find("완전히 정지해줘") != std::string::npos,
              "2y-⑱(c) 안내문 — 즉발 확인문 방향 고정 문어+Info 훑기 어휘 "
              "포합(kVoicePrompt와 같은 어휘 원문 동기)");
        jk::ChatAction s7;
        (void)jk::ChatRouterRoute("정지해줘", s7);
        check(s7.kind == jk::ChatAction::AppTool &&
                  s7.app == "vplayer" && s7.tool == "play_pause",
              "2y-⑱(d) 기존 회귀 — \"정지해줘\"는 토글 행 원존(신설 행 표 "
              "끝 배치 — 접두 판정 무접촉)");
    }

    // 2z) 구두 응답 TTS leg — 결정 부품 (#117 T1 — 스펙
    //   2026-10-27-tts-response-design D1-D6). ①답신 절단(D4 — 120자 상한+
    //   말미 "…" — UTF-8 코드포인트 경계 보전) ②구두 출발 판정(D3 — voice
    //   origin 표지 턴의 답신만 TTS 대상 — 타이핑 턴·빈 답신 부정) ③opt-out
    //   env 판정(D2 — VOICE_TTS="0" 문자 그대로만 끔 — 기본 켬). **스폰
    //   어셜션 금지(D6 — 비결정 원존)** — 스폰 본체는 cpp의 __ANDROID__ TU
    //   (Win/WSL 2축 바이너리에 leg 미포함 — 컴파일 무영향)·런타임 env 원본
    //   (VoiceTtsOptOutRuntime — getenv 1뒤)의 실측은 폰 E2E probe 몫(T2).
    //   2y 뒤 계보 이어붙임.
    {
        using jk::tts::TtsReplyClip;
        using jk::tts::TtsSpeakWanted;
        using jk::tts::VoiceTtsEnabled;

        // ① D4 — 답신 절단 부품. ASCII 121자 = 121바이트(절단 산치 단순).
        const std::string long121(121, 'a');
        check(TtsReplyClip(long121) == std::string(120, 'a') + "…",
              "2z-① 답신 절단 121자→120+\"…\" 등호(D4 — 1행 규약)");
        // ① UTF-8 경계 — 한글 121자(각 3바이트) → 문자 121자 기저 절단
        // (바이트 절단 0 — 중간 글리프 파괴 없음 — 369바이트 입력).
        std::string ko121;
        std::string koWant;
        for (int i = 0; i < 121; ++i) ko121 += "가";  // 가
        for (int i = 0; i < 120; ++i) koWant += "가";
        koWant += "…";  // …
        check(TtsReplyClip(ko121) == koWant,
              "2z-①b 한글 121자 절단 = 120자+\"…\" 등호(UTF-8 문자 경계 보전 "
              "— 중간 글리프 절단 없음)");
        // ① 상한 등호·이내 = 원문 보존(120자 그대로·짧은 답신 그대로).
        const std::string exact120(120, 'b');
        check(TtsReplyClip(exact120) == exact120 &&
                  TtsReplyClip("정지했습니다") == "정지했습니다",
              "2z-①c 상한 등호·이내 = 원문 그대로(120자 보존 — 접미 무첨가)");

        // ② D3 — 구두 출발 판정 부품. 표지 턴만 TTS 대상.
        check(TtsSpeakWanted(true, false, false) &&
                  !TtsSpeakWanted(false, false, false),
              "2z-② voice origin 표지 턴 = TTS 대상·타이핑 턴 = 부정(D3 — "
              "구두 출발 발화의 답신만)");
        check(!TtsSpeakWanted(true, true, false),
              "2z-②b 빈 답신 = 스킵(말할 원문 없음 — 기록 치환 전송침과 별개 "
              "— 스폰 결정의 가드 1검)");
        check(!TtsSpeakWanted(true, false, true),
              "2z-②c opt-out 표기 상태 = 부정(D2 승계 — 판정부 3진실원 합산)");

        // ③ D2 — opt-out env 판정 부품(표 전부 — 기본 켬·"0" 문자 그대로만
        // 끔). 런타임 getenv 원본 폰 E2E probe 몫(원장행 위 주석).
        check(VoiceTtsEnabled(false, nullptr) &&
                  VoiceTtsEnabled(false, "0"),
              "2z-③ VOICE_TTS 미설정 = 켬(D2 기본 켬 — opt-out 변형의 얼굴)");
        check(!VoiceTtsEnabled(true, "0"),
              "2z-③b VOICE_TTS=\"0\" = 끔(문자 그대로 1바이트 등호 — "
              "fail-closed 원존의 opt-out 변형)");
        check(VoiceTtsEnabled(true, "1") && VoiceTtsEnabled(true, ""),
              "2z-③c VOICE_TTS=\"1\"·\"\" = 켬(0 이외 값은 켬 — 값 판정의 "
              "정직 문자 규약)");
    }

    // 2w) 서버 도구 호출 저널 leg (#113 T1 — 스펙 2026-10-26-use-review-cycle-
    // design D1·D2·D5): 순수 writer 4건 — ①append 형식(JSONL 파싱 등호 —
    // ts/tool/arg/ok/err) ②쓰기 실패 무시(도구 응답 무영향) ③args 절단
    // 128자 ④2연속 콜 = 2행 append. 실서버 스폰 0(writer는 자유 함수 —
    // ResolveAgentWindowTarget 1n-14 선례, 서버 프로세스 불요). 파일 접촉은
    // temp dir 전용 — 사용자 <exeDir>/state/(use_journal.jsonl 포함)
    // 무접촉. 2v 뒤 계보 이어붙임. 저널 행 취득 = 파일 직독(T2 수확 probe와
    // 같은 계약 — 저널 쪽에 읽기 경로 신설 없음 D5).
    {
        namespace fs = std::filesystem;
        using jk::server::UseJournalAppend;
        // 마지막 행 취득 — 끝 개행 소각 후 역방향 '\n' 스캔.
        const auto lastRow = [](const std::string& p) {
            std::string text;
            if (std::FILE* rf = std::fopen(p.c_str(), "rb")) {
                char chunk[1024];
                size_t rn;
                while ((rn = std::fread(chunk, 1, sizeof(chunk), rf)) > 0)
                    text.append(chunk, rn);
                std::fclose(rf);
            }
            if (!text.empty() && text.back() == '\n') text.pop_back();
            const size_t nl = text.rfind('\n');
            return (nl == std::string::npos) ? text : text.substr(nl + 1);
        };
        const auto rowCount = [](const std::string& p) {
            size_t n = 0;
            bool emptyLine = true;
            std::FILE* rf = std::fopen(p.c_str(), "rb");
            char chunk[1024];
            size_t rn;
            while (rf && (rn = std::fread(chunk, 1, sizeof(chunk), rf)) > 0) {
                for (size_t i = 0; i < rn; ++i) {
                    if (chunk[i] == '\n') {
                        ++n;
                        emptyLine = true;
                    } else {
                        emptyLine = false;
                    }
                }
            }
            if (rf) std::fclose(rf);
            return n + (emptyLine ? 0 : 1);
        };
        const fs::path wdir =
            fs::temp_directory_path() / "jk_usejournal_selftest";
        std::error_code wEc;
        fs::remove_all(wdir, wEc);
        fs::create_directories(wdir, wEc);
        const std::string dir = wdir.string();
        const std::string path = dir + "/use_journal.jsonl";
        // ① append 형식 — 합성 1콜 → 마지막 행 JSONL 파싱 등호. ts는 64비트
        //    리더(AgentJson GetInt64 — int32 랩어웃 우회 계약(doc 57 §13.3)이
        //    씌는 관용구)로 고정값 등호 — 밀리초 정형(D2)의 산치. ok = raw
        //    스캔(files_audit/read_receipts의 "ok":true 규약 동형).
        UseJournalAppend(dir.c_str(), 1700000000000LL, "launch_app",
                         "{\"app\":\"minesweeper\"}", true, "");
        {
            const std::string row1 = lastRow(path);
            jk::agent::AgentJson row(row1.c_str());
            std::string jTool, jArg, jErr;
            int64_t jTs = 0;
            check(row.ok() && row.GetStr("tool", jTool) &&
                      jTool == "launch_app" && row.GetInt64("ts", jTs) &&
                      jTs == 1700000000000LL &&
                      row.GetStr("arg", jArg) &&
                      jArg == "{\"app\":\"minesweeper\"}" &&
                      row1.find("\"ok\":true,") != std::string::npos &&
                      row.GetStr("err", jErr) && jErr.empty(),
                  "2w-① 합성 1콜 = 마지막 행 JSONL 형식 등호"
                  "(ts(ms)/tool/arg/ok/err)");
        }
        // ② 쓰기 실패 무시 — use_journal.jsonl 자리가 디렉터리면 fopen("ab")
        //    실패가 보장되는 결정론 장애(Win/posix 공통 — chmod 계열 축 분기
        //    회피). 조용한 스킵 = 무승인 반환·무 예외·장애 자리 무변조 —
        //    훅이 writer를 void로 소비하므로 도구 응답은 무영향(D1).
        {
            const fs::path failDir =
                fs::temp_directory_path() / "jk_usejournal_selftest_fail";
            fs::remove_all(failDir, wEc);
            fs::create_directories(failDir / "use_journal.jsonl", wEc);
            const std::string fdir = failDir.string();
            UseJournalAppend(fdir.c_str(), 1700000000001LL, "ping", "",
                             true, "");
            size_t entries = 0;
            for (fs::directory_iterator it(failDir, wEc), end;
                 it != end && !wEc; ++it)
                ++entries;
            check(fs::is_directory(failDir / "use_journal.jsonl", wEc) &&
                      entries == 1,
                  "2w-② 저널 경로 장애 = append 조용한 스킵(무예외·무손상 — "
                  "도구 응답 무영향)");
            fs::remove_all(failDir, wEc);
        }
        // ③ args 절단 — 129자 입력 → 128자 등호(스펙 D2 상한).
        {
            UseJournalAppend(dir.c_str(), 1700000000002LL, "send_input",
                             std::string(129, 'x'), false, "bad_target");
            jk::agent::AgentJson row(lastRow(path).c_str());
            std::string jArg, jErr3;
            check(row.ok() && row.GetStr("arg", jArg) && jArg.size() == 128 &&
                      jArg == std::string(128, 'x') &&
                      row.GetStr("err", jErr3) && jErr3 == "bad_target",
                  "2w-③ args 129자 = 128자 절단 등호(소스 시점 상한)");
        }
        // ④ 2연속 콜 = 2행 append — 저널은 이벤트 스트림이라 1콜 1행 계약
        //    (선행 ①③ 호출 누적 4행의 마지막 2행 = ④ 콜 — 부가 등호).
        {
            UseJournalAppend(dir.c_str(), 1700000000003LL, "list_windows", "",
                             true, "");
            UseJournalAppend(dir.c_str(), 1700000000004LL, "close_window", "",
                             false, "window_not_found");
            check(rowCount(path) == 4,
                  "2w-④ 2연속 콜 = 2행 append(1콜 1행 — 스트림 누적 등호)");
        }
        fs::remove_all(wdir, wEc);
    }

    // 2x) m4a 태그 읽기 own 파서 — MP4 박스 보행 (#115 T1 — 스펙
    // 2026-10-26-opus-m4a-design D1): ftyp→moov→udta→meta(hdlr 'mdir')→ilst
    // →©nam/©ART/©alb(data atom — 타입 코드 1=UTF-8 통과·2=UTF-16(BOM 우선
    // — DecodeId3Text 원문 leg 승계)). 합성 box 트리(2u 합성 블롭 원문 수
    // 형 — FS 접점 = temp dir 전용·사후 소각 REMNANT 0). 재생 디코드는
    // 비-목표(D6 — 순수 메타 leg만, 스폰 0). 3건(①무태그 폴백 ②태그 등호
    // ③잘린 box 정직 거부 — 잘린/배신적 size = 부분 필드 소각).
    {
        namespace fs = std::filesystem;
        namespace mt = jk::music::tags;
        const fs::path tdir = fs::temp_directory_path() / "jk_music_m4a";
        fs::remove_all(tdir);  // 선제거 — 이전 런 잔산(2s/2u 원문 수형)
        fs::create_directories(tdir);

        auto putFile = [](const fs::path& p,
                          const std::vector<unsigned char>& bytes) {
            std::ofstream f(p, std::ios::binary);
            f.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
            f.close();
            return fs::file_size(p) == bytes.size();
        };
        auto pushBE32 = [](std::vector<unsigned char>& v, unsigned int x) {
            for (int sh = 24; sh >= 0; sh -= 8)
                v.push_back(static_cast<unsigned char>((x >> sh) & 0xFF));
        };
        auto concat = [](std::vector<std::vector<unsigned char>> parts) {
            std::vector<unsigned char> b;
            for (const auto& one : parts)
                b.insert(b.end(), one.begin(), one.end());
            return b;
        };
        // 박스 합성(2u pushBE32/mkFrame 수형) — size 필드는 body+8 실측;
        // 위조 선언(잘림 시험용) = mkBoxDecl.
        auto mkBoxDecl = [&](const char* type4, unsigned int declared,
                             const std::vector<unsigned char>& body) {
            std::vector<unsigned char> b;
            pushBE32(b, declared);
            b.insert(b.end(), type4, type4 + 4);
            b.insert(b.end(), body.begin(), body.end());
            return b;
        };
        auto mkBox = [&](const char* type4,
                         const std::vector<unsigned char>& body) {
            return mkBoxDecl(type4,
                             static_cast<unsigned int>(body.size() + 8),
                             body);
        };
        // ilst 항목의 data atom = [version 1B+flags 3B][locale 4B][값] 박스
        // (파서 접점 p[3] = 타입 코드 — pushBE32 하나로 version 0+flags).
        auto mkData = [&](unsigned int code,
                          const std::vector<unsigned char>& value) {
            std::vector<unsigned char> b;
            pushBE32(b, code);   // version(상위 바이트 0)+flags(타입 코드)
            pushBE32(b, 0);      // locale reserved
            b.insert(b.end(), value.begin(), value.end());
            return mkBox("data", b);
        };
        auto mkItem = [&](const char* type4,
                          const std::vector<unsigned char>& dataBox) {
            return mkBox(type4, dataBox);
        };
        // hdlr = [ver/flags][pre_defined]['mdir'][reserved+name 17B] — 파서
        // 접점은 [8..12) 'mdir'뿐, 뒤는 값 무관.
        auto mkHdlr = [&pushBE32]() {
            std::vector<unsigned char> b;
            pushBE32(b, 0);
            pushBE32(b, 0);
            const unsigned char mdir[4] = {'m', 'd', 'i', 'r'};
            b.insert(b.end(), mdir, mdir + 4);
            b.insert(b.end(), 17, 0);
            return b;
        };
        // meta = full box(payload 머리 4B version/flags)+hdlr+ilst.
        auto mkMeta = [&](const std::vector<unsigned char>& ilstBody) {
            std::vector<unsigned char> meta;
            pushBE32(meta, 0);  // version/flags
            const std::vector<unsigned char> children =
                concat({mkBox("hdlr", mkHdlr()), mkBox("ilst", ilstBody)});
            meta.insert(meta.end(), children.begin(), children.end());
            return mkBox("meta", meta);
        };
        auto mkFile = [&](const std::vector<unsigned char>& moovBody) {
            return concat({mkBox("ftyp", {'M', '4', 'A', ' ', 0, 0, 0, 0,
                                          'i', 's', 'o', 'm'}),
                           mkBox("moov", moovBody),
                           mkBox("mdat", std::vector<unsigned char>(64,
                                                                    0xAB))});
        };

        // ①) 합성 무태그 m4a — ilst 빈(항목 0) → ParseTag = 빈 TagSet 폴백
        //    +err 빈(파일명 폴백 정상 경로 — D2 오류 표기 없음 수형).
        {
            const fs::path p = tdir / "2x_a_notag.m4a";
            const std::vector<unsigned char> blob =
                mkFile(mkBox("udta", concat({mkMeta({})})));
            std::string e;
            const bool built = putFile(p, blob);
            const mt::TagSet t1 =
                built ? mt::ParseTag(p.generic_string(), e) : mt::TagSet{};
            check(built && mt::TagEmpty(t1) && e.empty(),
                  "2x-① 합성 무태그 m4a(ftyp+moov/udta/meta/ilst 빈+mdat) "
                  "ParseTag = 빈 TagSet 폴백+err 빈(파일명 폴백 정상 경로)");
        }

        // ②) 태그 m4a — ©nam/©ART/©alb data atom: UTF-8(타입 1 — 통과, 한글
        //    앨범 포함) 3필드 등호+UTF-16(타입 2 — BOM FE FF/FF FE 양보행)
        //    동등.
        {
            const auto u16be = [](const std::string& s) {
                std::vector<unsigned char> v = {0xFE, 0xFF};  // BOM BE
                for (char c : s) {
                    v.push_back(0);
                    v.push_back(static_cast<unsigned char>(c));
                }
                return v;
            };
            const auto u16le = [](const std::string& s) {
                std::vector<unsigned char> v = {0xFF, 0xFE};  // BOM LE
                for (char c : s) {
                    v.push_back(static_cast<unsigned char>(c));
                    v.push_back(0);
                }
                return v;
            };
            const fs::path p8 = tdir / "2x_b_utf8.m4a";
            const fs::path p16 = tdir / "2x_c_utf16.m4a";
            const std::vector<unsigned char> ilst8 = concat(
                {mkItem("\xA9" "nam",
                        mkData(1, {'M', '4', 'A', ' ', 'T', 'i', 't', 'l',
                                   'e'})),
                 mkItem("\xA9" "ART",
                        mkData(1, {'M', '4', 'A', ' ', 'A', 'r', 't', 'i',
                                   's', 't'})),
                 mkItem("\xA9" "alb",
                        mkData(1, {0xED, 0x95, 0x9C, 0xEA, 0xB8, 0x80, ' ',
                                   0xEC, 0x95, 0xA8, 0xEB, 0xB2,
                                   0x94}) /* "한글 앨범" UTF-8 */)});
            const std::vector<unsigned char> ilst16 = concat(
                {mkItem("\xA9" "nam",
                        mkData(2, u16be("U16 Title"))),
                 mkItem("\xA9" "ART",
                        mkData(2, u16le("U16 Artist"))),
                 mkItem("\xA9" "alb",
                        mkData(2, u16le("U16 Album")))});
            // 무BOM BE(T2 라인 리뷰 r1 소각 — Apple 타입 2 = UTF-16BE 명시
            // 규약: 무BOM = BE, BOM 존재 = BOM이 이긴다).
            const auto u16beRaw = [](const std::string& s) {
                std::vector<unsigned char> v;
                for (char c : s) {
                    v.push_back(0);
                    v.push_back(static_cast<unsigned char>(c));
                }
                return v;
            };
            const fs::path p16be = tdir / "2x_c2_utf16be.m4a";
            const std::vector<unsigned char> ilst16be = concat(
                {mkItem("\xA9" "nam", mkData(2, u16beRaw("BE Title"))),
                 mkItem("\xA9" "ART", mkData(2, u16beRaw("BE Artist")))});
            const std::vector<unsigned char> blob8 =
                mkFile(mkBox("udta", concat({mkMeta(ilst8)})));
            const std::vector<unsigned char> blob16 =
                mkFile(mkBox("udta", concat({mkMeta(ilst16)})));
            const std::vector<unsigned char> blob16be =
                mkFile(mkBox("udta", concat({mkMeta(ilst16be)})));
            std::string e8, e16, e16be;
            const bool built8 = putFile(p8, blob8);
            const bool built16 = putFile(p16, blob16);
            const bool built16be = putFile(p16be, blob16be);
            const mt::TagSet t8 =
                built8 ? mt::ParseTag(p8.generic_string(), e8) : mt::TagSet{};
            const mt::TagSet t16 =
                built16 ? mt::ParseTag(p16.generic_string(), e16)
                        : mt::TagSet{};
            const mt::TagSet t16be =
                built16be ? mt::ParseTag(p16be.generic_string(), e16be)
                          : mt::TagSet{};
            check(built8 && built16 && built16be && e8.empty() && e16.empty() &&
                      e16be.empty() &&
                      t8.title == "M4A Title" &&
                      t8.artist == "M4A Artist" &&
                      t8.album == std::string(
                          "\xED\x95\x9C\xEA\xB8\x80 \xEC\x95\xA8\xEB\xB2\x94") &&
                      t16.title == "U16 Title" &&
                      t16.artist == "U16 Artist" && t16.album == "U16 Album" &&
                      t16be.title == "BE Title" &&
                      t16be.artist == "BE Artist",
                  "2x-② 태그 m4a 재독 = ©nam/©ART/©alb 3필드 등호(UTF-8 data "
                  "통과+한글)+UTF-16(BOM BE/LE) 동등+무BOM = BE(타입 코드 1/2 "
                  "— DecodeId3Text BOM 계약 승계·Apple 타입 2 = BE 명시 — "
                  "T2 리뷰 r1 수형)");
        }

        // ③) 잘린 box(size > 남은 바이트) — (a) 최상위 moov·(b) 항목 data:
        //    ParseTag = 빈 TagSet(정직 거부 — 부분 필드 소각 — ©nam 성립에
        //    도 전부 빈)+err 빈. REMNANT 0 — 사후 소각 동봉(2s/2u 수형).
        {
            const fs::path pa = tdir / "2x_d_moov_trunc.m4a";
            const fs::path pb = tdir / "2x_e_data_trunc.m4a";
            const std::vector<unsigned char> blobTruncA = concat(
                {mkBox("ftyp", {'M', '4', 'A', ' ', 0, 0, 0, 0, 'i', 's',
                                'o', 'm'}),
                 mkBoxDecl("moov", 400,
                           mkBox("udta", concat({mkMeta({})}))),
                 mkBox("mdat", std::vector<unsigned char>(64, 0xAB))});
            const std::vector<unsigned char> ilstBad = concat(
                {mkItem("\xA9" "nam",
                        mkData(1, {'G', 'o', 'o', 'd'})),
                 mkItem("\xA9" "ART",
                        mkBoxDecl("data", 200000, {0, 0, 0, 1, 0, 0, 0, 0,
                                                   'B', 'A', 'D'}) )});
            const std::vector<unsigned char> blobTruncB =
                mkFile(mkBox("udta", concat({mkMeta(ilstBad)})));
            std::string ea, eb;
            const bool builtA = putFile(pa, blobTruncA);
            const bool builtB = putFile(pb, blobTruncB);
            const mt::TagSet ta =
                builtA ? mt::ParseTag(pa.generic_string(), ea) : mt::TagSet{};
            const mt::TagSet tb =
                builtB ? mt::ParseTag(pb.generic_string(), eb) : mt::TagSet{};
            std::error_code sx;
            fs::remove_all(tdir, sx);
            check(builtA && builtB && ea.empty() && eb.empty() &&
                      mt::TagEmpty(ta) && mt::TagEmpty(tb) &&
                      tb.title.empty() && tb.artist.empty() && !sx &&
                      !fs::exists(tdir, sx),
                  "2x-③ 잘린 box(size > 남은 바이트) = 정직 거부(moov 잘림"
                  "+©nam 성립에도 부분 소각 — err 빈·REMNANT 0)");
        }
    }

    // 2x) opus 태그 읽기+m4a/opus 커버아트 — #115 T2 (스펙 D3/D4): opus =
    // OggS 연쇄의 OpusTags 패킷(vorbis 코멘트 동형 구조 재용 — 패킷 1
    // "OpusHead"는 매agic 미일치로 자연 스킵)·covr = ilst 항목 data atom
    // (코드 13=JPEG·14=PNG·0=스니프). 합성 OggS 페이지(쓰기 leg의
    // BuildOggPage/PushPacketLacing/OggCrc 재용 — CRC 규약 등호)+합성 box
    // 트리(2x T1 수형 재용 — FS 접점 = temp dir 전용·사후 소각 REMNANT 0).
    // 재생 디코드 비-목표(D6 — 스폰 0). 5건(④opus 무태그 ⑤opus 태그 ⑥covr
    // round-trip ⑦opus MBP round-trip ⑧잘린 covr/MBP 정직 거부 2건).
    {
        namespace fs = std::filesystem;
        namespace mt = jk::music::tags;
        const fs::path tdir = fs::temp_directory_path() / "jk_music_t2";
        fs::remove_all(tdir);  // 선제거 — 이전 런 잔산(2x 원문 수형)
        fs::create_directories(tdir);

        auto putFile = [](const fs::path& p,
                          const std::vector<unsigned char>& bytes) {
            std::ofstream f(p, std::ios::binary);
            f.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
            f.close();
            return fs::file_size(p) == bytes.size();
        };
        auto pushBE32 = [](std::vector<unsigned char>& v, unsigned int x) {
            for (int sh = 24; sh >= 0; sh -= 8)
                v.push_back(static_cast<unsigned char>((x >> sh) & 0xFF));
        };
        auto pushLE32 = [](std::vector<unsigned char>& v, unsigned int x) {
            for (int sh = 0; sh < 32; sh += 8)
                v.push_back(static_cast<unsigned char>((x >> sh) & 0xFF));
        };
        auto concat = [](std::vector<std::vector<unsigned char>> parts) {
            std::vector<unsigned char> b;
            for (const auto& one : parts)
                b.insert(b.end(), one.begin(), one.end());
            return b;
        };
        // Ogg 페이지 1건(패킷 1개 — 쓰기 leg 부품 재용: PushPacketLacing
        // 세그 런+BuildOggPage CRC 규약 — 원문 판정).
        auto mkPage = [](unsigned char htype,
                         const std::vector<unsigned char>& pkt) {
            std::vector<unsigned char> lace, pg;
            mt::PushPacketLacing(lace, pkt.size());
            mt::BuildOggPage(pg, 0, htype, 0ull, 77u, 0u, lace.data(),
                             lace.size(), pkt.data(), pkt.size());
            return pg;
        };
        auto mkOpusHead = [&pushLE32]() {
            std::vector<unsigned char> pkt;
            const unsigned char magic[8] = {'O', 'p', 'u', 's', 'H', 'e',
                                            'a', 'd'};
            pkt.insert(pkt.end(), magic, magic + 8);
            pkt.push_back(1);   // version
            pkt.push_back(2);   // channels
            pkt.push_back(312 & 0xFF);            // pre-skip LE16
            pkt.push_back((312 >> 8) & 0xFF);
            pushLE32(pkt, 48000);                 // input sample rate LE32
            pkt.push_back(0);   // output gain LE16
            pkt.push_back(0);
            pkt.push_back(0);   // mapping family
            return pkt;
        };
        auto mkOpusTags = [&pushLE32](
            const std::string& vendor,
            const std::vector<std::pair<std::string, std::string>>& es) {
            std::vector<unsigned char> pkt;
            const unsigned char magic[8] = {'O', 'p', 'u', 's', 'T', 'a',
                                            'g', 's'};
            pkt.insert(pkt.end(), magic, magic + 8);
            pushLE32(pkt, static_cast<unsigned int>(vendor.size()));
            pkt.insert(pkt.end(), vendor.begin(), vendor.end());
            pushLE32(pkt, static_cast<unsigned int>(es.size()));
            for (const auto& kv : es) {
                pushLE32(pkt, static_cast<unsigned int>(kv.first.size() + 1 +
                                                        kv.second.size()));
                pkt.insert(pkt.end(), kv.first.begin(), kv.first.end());
                pkt.push_back('=');
                pkt.insert(pkt.end(), kv.second.begin(), kv.second.end());
            }
            return pkt;
        };
        // 그림 블롭(정직 합성 — JPEG/JFIF SOI 마커·PNG 시그니처 8B 규약 원문).
        const std::vector<unsigned char> jpgBlob = {
            0xFF, 0xD8, 0xFF, 0xE0, 'J', 'P', 'J', '+', '!', '-', '.',
            'J', 'P', 'E', 'G'};
        const std::vector<unsigned char> pngBlob = {
            0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 1, 2, 3, 4};

        // ④) 합성 opus(무태그) — OpusHead+OpusTags(엔트리 0) → OggPacket1
        //    IsOpus 스니프 성립+ParseTag = 빈 TagSet 폴백+err 빈(vorbis
        //    구별 — OggS 매agic은 같지만 코멘트 패킷 매agic이 다르다).
        {
            const fs::path p = tdir / "2x_f_notag.opus";
            const std::vector<unsigned char> blob = concat(
                {mkPage(0x02, mkOpusHead()),
                 mkPage(0x00, mkOpusTags("synth-vendor", {}))});
            std::string e;
            const bool built = putFile(p, blob);
            const bool sniff =
                mt::OggPacket1IsOpus(blob.data(), blob.size());
            const mt::TagSet t1 =
                built ? mt::ParseTag(p.generic_string(), e) : mt::TagSet{};
            check(built && sniff && mt::TagEmpty(t1) && e.empty(),
                  "2x-④ 합성 opus(무태그 — OpusHead+OpusTags 빈) = "
                  "OggPacket1IsOpus 스니프 성립(opus/vorbis 구별)+ParseTag "
                  "빈 TagSet 폴백+err 빈");
        }

        // ⑤) opus 태그 읽기 등호 — OpusTags 엔트리 TITLE/ARTIST/ALBUM
        //    (한글 앨범) — vorbis 코멘트 해석 재용(ParseVorbisComments).
        {
            const fs::path p = tdir / "2x_g_tags.opus";
            const std::vector<unsigned char> blob = concat(
                {mkPage(0x02, mkOpusHead()),
                 mkPage(0x00, mkOpusTags(
                                  "synth-vendor",
                                  {{"TITLE", "Opus Title"},
                                   {"ARTIST", "Opus Artist"},
                                   {"ALBUM",
                                    "\xED\x95\x9C\xEA\xB8\x80 \xEC\x95\xA8"
                                    "\xEB\xB2\x94"}}))});  // "한글 앨범" UTF-8
            std::string e;
            const bool built = putFile(p, blob);
            const mt::TagSet t1 =
                built ? mt::ParseTag(p.generic_string(), e) : mt::TagSet{};
            check(built && e.empty() && t1.title == "Opus Title" &&
                      t1.artist == "Opus Artist" &&
                      t1.album == std::string(
                          "\xED\x95\x9C\xEA\xB8\x80 \xEC\x95\xA8\xEB\xB2\x94"),
                  "2x-⑤ opus 태그 읽기 = OpusTags 엔트리 3필드 등호(한글 앨범"
                  " 포함 — ParseVorbisComments 재용·OpusHead 패킷 스킵)");
        }

        // ⑥) m4a covr round-trip — data 타입 14(PNG)+13(JPEG) mime 등호+
        //    바이트 원문 등호+스니프 0(binary 암규약) JPEG 동등.
        {
            const fs::path pPng = tdir / "2x_h_cvr_png.m4a";
            const fs::path pJpg = tdir / "2x_i_cvr_jpg.m4a";
            const auto mkFileCvr = [&](unsigned int code,
                                       const std::vector<unsigned char>& img,
                                       const fs::path& p) {
                auto mkBox = [&](const char* type4,
                                 const std::vector<unsigned char>& body) {
                    std::vector<unsigned char> b;
                    pushBE32(b, static_cast<unsigned int>(body.size() + 8));
                    b.insert(b.end(), type4, type4 + 4);
                    b.insert(b.end(), body.begin(), body.end());
                    return b;
                };
                std::vector<unsigned char> dataBody;
                pushBE32(dataBody, code);
                pushBE32(dataBody, 0);  // locale
                dataBody.insert(dataBody.end(), img.begin(), img.end());
                const std::vector<unsigned char> covrItem =
                    mkBox("covr", mkBox("data", dataBody));
                const std::vector<unsigned char> ilst = mkBox("ilst", covrItem);
                std::vector<unsigned char> meta;
                pushBE32(meta, 0);  // version/flags
                const std::vector<unsigned char> hdlrBody = [&] {
                    std::vector<unsigned char> b;
                    pushBE32(b, 0);
                    pushBE32(b, 0);
                    const unsigned char mdir[4] = {'m', 'd', 'i', 'r'};
                    b.insert(b.end(), mdir, mdir + 4);
                    b.insert(b.end(), 17, 0);
                    return b;
                }();
                const std::vector<unsigned char> children =
                    concat({mkBox("hdlr", hdlrBody), ilst});
                meta.insert(meta.end(), children.begin(), children.end());
                const std::vector<unsigned char> udta =
                    mkBox("udta", concat({mkBox("meta", meta)}));
                const std::vector<unsigned char> moov =
                    mkBox("moov", concat({udta}));
                const std::vector<unsigned char> head =
                    mkBox("ftyp", {'M', '4', 'A', ' ', 0, 0, 0, 0, 'i', 's',
                                   'o', 'm'});
                const std::vector<unsigned char> mdat =
                    mkBox("mdat", std::vector<unsigned char>(64, 0xAB));
                return putFile(p, concat({head, moov, mdat}));
            };
            const bool b1 = mkFileCvr(14, pngBlob, pPng);
            const bool b2 = mkFileCvr(13, jpgBlob, pJpg);
            mt::CoverArt cPng, cJpg;
            std::string ePng, eJpg;
            const bool r1 = b1 ? mt::ReadCover(pPng.generic_string(), cPng,
                                               ePng)
                               : false;
            const bool r2 = b2 ? mt::ReadCover(pJpg.generic_string(), cJpg,
                                               eJpg)
                               : false;
            check(b1 && b2 && r1 && r2 && ePng.empty() && eJpg.empty() &&
                      cPng.mime == "image/png" &&
                      cJpg.mime == "image/jpeg" &&
                      cPng.bytes.size() == pngBlob.size() &&
                      cJpg.bytes.size() == jpgBlob.size() &&
                      std::memcmp(cPng.bytes.data(), pngBlob.data(),
                                  pngBlob.size()) == 0 &&
                      std::memcmp(cJpg.bytes.data(), jpgBlob.data(),
                                  jpgBlob.size()) == 0,
                  "2x-⑥ m4a covr round-trip = mime 등호(image/png 14·image/"
                  "jpeg 13 — iTunes 규약 코드)+그림 바이트 원문 등호");
        }

        // ⑦) opus MBP round-trip — OpusTags METADATA_BLOCK_PICTURE(베이스64
        //    FLAC PICTURE 구조 — 쓰기 leg BuildFlacPictureStruct/Base64Encode
        //    재용) → ReadCover mime+바이트 등호(2u Ogg MBP 원문 사슬 재용).
        {
            const fs::path p = tdir / "2x_j_mbp.opus";
            const std::vector<unsigned char> pic = mt::BuildFlacPictureStruct(
                "image/jpeg", {jpgBlob.begin(), jpgBlob.end()});
            std::string b64;
            mt::Base64Encode(pic.data(), pic.size(), b64);
            const std::vector<unsigned char> blob = concat(
                {mkPage(0x02, mkOpusHead()),
                 mkPage(0x00, mkOpusTags(
                                  "synth-vendor",
                                  {{"METADATA_BLOCK_PICTURE", b64}}))});
            std::string e;
            const bool built = putFile(p, blob);
            mt::CoverArt c;
            const bool r = built
                               ? mt::ReadCover(p.generic_string(), c, e)
                               : false;
            check(built && r && e.empty() && c.mime == "image/jpeg" &&
                      c.bytes.size() == jpgBlob.size() &&
                      std::memcmp(c.bytes.data(), jpgBlob.data(),
                                  jpgBlob.size()) == 0,
                  "2x-⑦ opus METADATA_BLOCK_PICTURE round-trip = mime image/"
                  "jpeg+그림 바이트 등호(베이스64 FLAC PICTURE 구조 — vorbis "
                  "MBP 사슬 재용·OpusTags 지목 leg 신설)");
        }

        // ⑧) 잘린 covr/MBP = 정직 거부 2건 — (a) m4a covr data 선언 200000
        //    잘림·(b) opus MBP의 PICTURE data 선언 100000 잘림 — ReadCover
        //    false+"no_cover"(부분 그림 0 — 잘린 그림 전시 금지 원문).
        {
            const fs::path pa = tdir / "2x_k_covr_trunc.m4a";
            const fs::path pb = tdir / "2x_l_mbp_trunc.opus";
            auto mkBoxDecl = [&](const char* type4, unsigned int declared,
                                 const std::vector<unsigned char>& body) {
                std::vector<unsigned char> b;
                pushBE32(b, declared);
                b.insert(b.end(), type4, type4 + 4);
                b.insert(b.end(), body.begin(), body.end());
                return b;
            };
            auto mkBox = [&](const char* type4,
                             const std::vector<unsigned char>& body) {
                return mkBoxDecl(
                    type4, static_cast<unsigned int>(body.size() + 8), body);
            };
            const std::vector<unsigned char> covrItemTrunc =
                mkBoxDecl("covr", 8 + 200000,
                          mkBoxDecl("data", 200000, {0, 0, 0, 14, 0, 0, 0, 0,
                                                     0x89, 'P'}));
            const std::vector<unsigned char> ilst =
                mkBox("ilst", concat({covrItemTrunc}));
            const std::vector<unsigned char> hdlrBody = [&] {
                std::vector<unsigned char> b;
                pushBE32(b, 0);
                pushBE32(b, 0);
                const unsigned char mdir[4] = {'m', 'd', 'i', 'r'};
                b.insert(b.end(), mdir, mdir + 4);
                b.insert(b.end(), 17, 0);
                return b;
            }();
            const std::vector<unsigned char> meta = [&] {
                std::vector<unsigned char> b;
                pushBE32(b, 0);  // version/flags
                const std::vector<unsigned char> children =
                    concat({mkBox("hdlr", hdlrBody), ilst});
                b.insert(b.end(), children.begin(), children.end());
                return mkBox("meta", b);
            }();
            const std::vector<unsigned char> blobA = concat(
                {mkBox("ftyp", {'M', '4', 'A', ' ', 0, 0, 0, 0, 'i', 's',
                                'o', 'm'}),
                 mkBox("moov", concat({mkBox("udta", concat({meta}))})),
                 mkBox("mdat", std::vector<unsigned char>(64, 0xAB))});
            // (b) MBP 잘림 — PICTURE data 선언 100000, 실체 8B(파산 —
            // ParseFlacPictureStruct 정직 거부 원문).
            std::vector<unsigned char> picTrunc;
            pushBE32(picTrunc, 3);
            pushBE32(picTrunc, 10);
            picTrunc.insert(picTrunc.end(), {'i', 'm', 'a', 'g', 'e', '/', 'p',
                                             'n', 'g', '!'});
            pushBE32(picTrunc, 0);   // desc
            for (int i = 0; i < 4; ++i) pushBE32(picTrunc, 0);  // w·h·d·c
            pushBE32(picTrunc, 100000);  // data 선언 — 과대(파산)
            picTrunc.insert(picTrunc.end(), {0x89, 'P', 'N', 'G',
                                             0x0D, 0x0A, 0x1A, 0x0A});
            std::string b64;
            mt::Base64Encode(picTrunc.data(), picTrunc.size(), b64);
            const std::vector<unsigned char> blobB = concat(
                {mkPage(0x02, mkOpusHead()),
                 mkPage(0x00, mkOpusTags(
                                  "synth-vendor",
                                  {{"METADATA_BLOCK_PICTURE", b64}}))});
            const bool builtA = putFile(pa, blobA);
            const bool builtB = putFile(pb, blobB);
            mt::CoverArt ca, cb;
            std::string eca, ecb;
            const bool ra = builtA ? mt::ReadCover(pa.generic_string(), ca,
                                                   eca)
                                   : false;
            const bool rb = builtB ? mt::ReadCover(pb.generic_string(), cb,
                                                   ecb)
                                   : false;
            std::error_code sx;
            fs::remove_all(tdir, sx);
            check(builtA && builtB && !ra && !rb && eca == "no_cover" &&
                      ecb == "no_cover" && ca.mime.empty() &&
                      cb.mime.empty() && ca.bytes.empty() &&
                      cb.bytes.empty() && !sx && !fs::exists(tdir, sx),
                  "2x-⑧ 잘린 covr/MBP = 정직 거부(m4a covr data 선언 잘림+opus"
                  " MBP PICTURE data 과대 선언 — ReadCover false·no_cover·"
                  "out 소각·REMNANT 0)");
        }
    }

    // 2x) opus/m4a 태그 쓰기 — WriteTag 사슬 병합 (#115 T3 — 스펙 D2/D3/D4):
    // m4a = moov/udta/meta/ilst 재작성+box 사이즈 전파(mdat 원문 바이트 무이동
    // — D2·신설 폴백 = 부재 부모 box 순차 신설)·opus = OpusTags 패킷 교체(D10
    // 재조립 사슬 재용 — D3)·covr/MBP 커버 교체(D4). 합성 = T1 box 트리+T2
    // Ogg 페이지 수형 재용(정직 합성 — 파서 접점 계약). 4건(⑨m4a 쓰기
    // round-trip+mdat 등호 ⑩covr 교체(null 유지/지정 교체/opus MBP) ⑪opus
    // 쓰기 round-trip+페이지 시퀀스/CRC ⑫한글 2형식 round-trip).
    {
        namespace fs = std::filesystem;
        namespace mt = jk::music::tags;
        const fs::path tdir = fs::temp_directory_path() / "jk_music_t3";
        fs::remove_all(tdir);  // 선제거 — 이전 런 잔산(2s/2u/2x 원문 수형)
        fs::create_directories(tdir);

        auto putFile = [](const fs::path& p,
                          const std::vector<unsigned char>& bytes) {
            std::ofstream f(p, std::ios::binary);
            f.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
            f.close();
            return fs::file_size(p) == bytes.size();
        };
        auto readFile = [](const fs::path& p,
                           std::vector<unsigned char>& out) {
            std::ifstream g(p, std::ios::binary);
            g.seekg(0, std::ios::end);
            const std::streamoff n = g.tellg();
            if (n < 0) return false;
            out.resize(static_cast<size_t>(n));
            g.seekg(0, std::ios::beg);
            g.read(reinterpret_cast<char*>(out.data()), n);
            return static_cast<size_t>(g.gcount()) == out.size();
        };
        auto hasBytes = [](const std::vector<unsigned char>& hay,
                           const std::vector<unsigned char>& nd) {
            if (nd.empty() || hay.size() < nd.size()) return false;
            for (size_t i = 0; i + nd.size() <= hay.size(); ++i)
                if (std::memcmp(hay.data() + i, nd.data(), nd.size()) == 0)
                    return true;
            return false;
        };
        auto pushBE32 = [](std::vector<unsigned char>& v, unsigned int x) {
            for (int sh = 24; sh >= 0; sh -= 8)
                v.push_back(static_cast<unsigned char>((x >> sh) & 0xFF));
        };
        auto pushLE32 = [](std::vector<unsigned char>& v, unsigned int x) {
            for (int sh = 0; sh < 32; sh += 8)
                v.push_back(static_cast<unsigned char>((x >> sh) & 0xFF));
        };
        auto asBytes = [](const std::string& s) {
            return std::vector<unsigned char>(s.begin(), s.end());
        };
        auto concat = [](std::vector<std::vector<unsigned char>> parts) {
            std::vector<unsigned char> b;
            for (const auto& one : parts)
                b.insert(b.end(), one.begin(), one.end());
            return b;
        };
        // ---- m4a box 트리 합성(T1 2x 수형 재용 — size 필드 = body+8 실측) ----
        auto mkBox = [&](const char* type4,
                         const std::vector<unsigned char>& body) {
            std::vector<unsigned char> b;
            pushBE32(b, static_cast<unsigned int>(body.size() + 8));
            b.insert(b.end(), type4, type4 + 4);
            b.insert(b.end(), body.begin(), body.end());
            return b;
        };
        auto mkData = [&](unsigned int code,
                          const std::vector<unsigned char>& value) {
            std::vector<unsigned char> b;
            pushBE32(b, code);   // version(0)+flags(타입 코드 — 파서 접점 p[3])
            pushBE32(b, 0);      // locale reserved
            b.insert(b.end(), value.begin(), value.end());
            return mkBox("data", b);
        };
        auto mkItem = [&](const char* type4,
                          const std::vector<unsigned char>& dataBox) {
            return mkBox(type4, dataBox);
        };
        auto mkHdlr = [&pushBE32]() {
            std::vector<unsigned char> b;
            pushBE32(b, 0);
            pushBE32(b, 0);
            const unsigned char mdir[4] = {'m', 'd', 'i', 'r'};
            b.insert(b.end(), mdir, mdir + 4);
            b.insert(b.end(), 17, 0);
            return b;
        };
        auto mkMeta = [&](const std::vector<unsigned char>& ilstBody) {
            std::vector<unsigned char> meta;
            pushBE32(meta, 0);  // version/flags
            const std::vector<unsigned char> children =
                concat({mkBox("hdlr", mkHdlr()), mkBox("ilst", ilstBody)});
            meta.insert(meta.end(), children.begin(), children.end());
            return mkBox("meta", meta);
        };
        auto mkFtyp = [&]() {
            return mkBox("ftyp", {'M', '4', 'A', ' ', 0, 0, 0, 0, 'i', 's',
                                  'o', 'm'});
        };
        // ---- opus 페이지 합성(T2 2x 수형 재용 — PushPacketLacing 세그 런+
        // BuildOggPage CRC 규약) — granule/seq 가변(⑪ 시퀀스/GRC 계약) ----
        auto mkPage = [](unsigned char htype, unsigned long long gran,
                         unsigned int seq,
                         const std::vector<unsigned char>& pkt) {
            std::vector<unsigned char> lace, pg;
            mt::PushPacketLacing(lace, pkt.size());
            mt::BuildOggPage(pg, 0, htype, gran, 77u, seq, lace.data(),
                             lace.size(), pkt.data(), pkt.size());
            return pg;
        };
        auto mkOpusHead = [&pushLE32]() {
            std::vector<unsigned char> pkt;
            const unsigned char magic[8] = {'O', 'p', 'u', 's', 'H', 'e',
                                            'a', 'd'};
            pkt.insert(pkt.end(), magic, magic + 8);
            pkt.push_back(1);   // version
            pkt.push_back(2);   // channels
            pkt.push_back(312 & 0xFF);            // pre-skip LE16
            pkt.push_back((312 >> 8) & 0xFF);
            pushLE32(pkt, 48000);                 // input sample rate LE32
            pkt.push_back(0);   // output gain LE16
            pkt.push_back(0);
            pkt.push_back(0);   // mapping family
            return pkt;
        };
        auto mkOpusTags = [&pushLE32](
            const std::string& vendor,
            const std::vector<std::pair<std::string, std::string>>& es) {
            std::vector<unsigned char> pkt;
            const unsigned char magic[8] = {'O', 'p', 'u', 's', 'T', 'a',
                                            'g', 's'};
            pkt.insert(pkt.end(), magic, magic + 8);
            pushLE32(pkt, static_cast<unsigned int>(vendor.size()));
            pkt.insert(pkt.end(), vendor.begin(), vendor.end());
            pushLE32(pkt, static_cast<unsigned int>(es.size()));
            for (const auto& kv : es) {
                pushLE32(pkt, static_cast<unsigned int>(kv.first.size() + 1 +
                                                        kv.second.size()));
                pkt.insert(pkt.end(), kv.first.begin(), kv.first.end());
                pkt.push_back('=');
                pkt.insert(pkt.end(), kv.second.begin(), kv.second.end());
            }
            return pkt;
        };
        // 그림 블롭(정직 합성 — T2 2x 수형 재용: JPEG/JFIF SOI 마커·PNG
        // 시그니처 8B 규약).
        const std::vector<unsigned char> jpgBlob = {
            0xFF, 0xD8, 0xFF, 0xE0, 'J', 'P', 'J', '+', '!', '-', '.',
            'J', 'P', 'E', 'G'};
        const std::vector<unsigned char> pngBlob = {
            0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 1, 2, 3, 4};

        // ⑨) m4a 쓰기 round-trip+mdat 원문 무이동 — (a) faststart(moov가 mdat
        //    앞 — moov 재조립 = mdat 자리 앞당겨짐·데이터 원문 바이트 무이동)
        //    (b) 비 faststart(mdat 선두·moov 말미). 미관리 항목(cprt) 원문
        //    보존+구 관리 항목(©nam) 소각 교체 동봉.
        {
            const fs::path pa = tdir / "2x_m_wtag_fs.m4a";
            const fs::path pb = tdir / "2x_n_wtag_nofs.m4a";
            const std::vector<unsigned char> ilstA = concat(
                {mkItem("\xA9" "nam",
                        mkData(1, {'O', 'l', 'd', ' ', 'T', 'i', 't', 'l',
                                   'e'})),
                 mkItem("cprt",
                        mkData(1, {'C', 'P', ' ', 'R', 'i', 'g', 'h', 't'}))});
            // (a) ftyp+moov+mdat — faststart.
            const std::vector<unsigned char> blobA = concat(
                {mkFtyp(),
                 mkBox("moov", mkBox("udta", concat({mkMeta(ilstA)}))),
                 mkBox("mdat", std::vector<unsigned char>(64, 0xAB))});
            // (b) ftyp+mdat+moov — mdat 선두(비 faststart).
            const std::vector<unsigned char> blobB = concat(
                {mkFtyp(),
                 mkBox("mdat", std::vector<unsigned char>(32, 0xCD)),
                 mkBox("moov", mkBox("udta", concat({mkMeta(ilstA)})))});
            const bool builtA = putFile(pa, blobA);
            const bool builtB = putFile(pb, blobB);
            std::string e;
            const mt::TagSet w{"M4A W Title", "W Artist", "W Album"};
            const bool rA = builtA ? mt::WriteTag(pa.generic_string(), w,
                                                  nullptr, e)
                                   : false;
            const bool rB = rA ? mt::WriteTag(pb.generic_string(), w, nullptr,
                                              e)
                               : false;
            std::vector<unsigned char> bufA, bufB;
            const bool readA = rB ? readFile(pa, bufA) : false;
            const bool readB = rB ? readFile(pb, bufB) : false;
            // 재독 — 3필드 등호(구 ©nam 교체 확정).
            std::string err;
            const mt::TagSet t =
                rB ? mt::ParseTag(pa.generic_string(), err) : mt::TagSet{};
            // (b) mdat 원문 — ftyp(20B)+mdat 헤더(8B size+type) 뒤 32B 0xCD 그대로(무이동).
            const bool mdatB =
                readB && std::all_of(bufB.begin() + 28, bufB.begin() + 56,
                                     [](unsigned char c) {
                                         return c == 0xCD;
                                     });
            check(builtA && builtB && rA && rB && readA && readB &&
                      err.empty() && t.title == "M4A W Title" &&
                      t.artist == "W Artist" && t.album == "W Album" &&
                      bufA.size() > 64 &&
                      std::all_of(bufA.end() - 64, bufA.end(),
                                  [](unsigned char c) {
                                      return c == 0xAB;
                                  }) /* (a) mdat payload = 말미 64B 원문 */ &&
                      std::memcmp(bufA.data(), blobA.data(), 20) ==
                          0 /* ftyp 원존 */ &&
                      hasBytes(bufA, asBytes("M4A W Title")) &&
                      hasBytes(bufA, asBytes(
                                         "CP Right")) /* cprt 원문 보존 */ &&
                      !hasBytes(bufA,
                                asBytes("Old Title")) /* 구 ©nam 소각 */ &&
                      mdatB,
                  "2x-⑨ m4a 쓰기 round-trip = 3필드 등호(구 ©nam 소각 재쓰기)"
                  "+cprt 원문 보존+mdat 원문 바이트 무이동(2배치 — faststart "
                  "말미 64B 0xAB·비faststart 0xCD 32B — D2)+ftyp 원존");
        }

        // ⑨b) m4a 신설 폴백 — (c) udta 부재(moov children = mvhd 더미) → moov
        //    말미 신설 udta{meta{hdlr+ilst}} (d) ilst 부재(meta = hdlr만) →
        //    meta children 말미 신설 — 부재 부모 box 순차 신설 규약(D2) 재독.
        {
            const fs::path pc = tdir / "2x_w_noudta.m4a";
            const fs::path pd = tdir / "2x_x_noilst.m4a";
            const std::vector<unsigned char> mvhdDummy = mkBox(
                "mvhd", std::vector<unsigned char>(32, 0x1A));
            const std::vector<unsigned char> hdrlOnly = [&]() {
                std::vector<unsigned char> meta;
                pushBE32(meta, 0);  // version/flags
                const std::vector<unsigned char> h = mkBox("hdlr", mkHdlr());
                meta.insert(meta.end(), h.begin(), h.end());
                return mkBox("meta", meta);
            }();
            const std::vector<unsigned char> blobC = concat(
                {mkFtyp(),
                 mkBox("moov", concat({mvhdDummy})),
                 mkBox("mdat", std::vector<unsigned char>(64, 0x55))});
            const std::vector<unsigned char> blobD = concat(
                {mkFtyp(),
                 mkBox("moov", mkBox("udta", concat({hdrlOnly}))),
                 mkBox("mdat", std::vector<unsigned char>(64, 0x55))});
            const bool builtC = putFile(pc, blobC);
            const bool builtD = putFile(pd, blobD);
            const mt::TagSet w{"No Udta", "No Ilst A", "No Ilst Al"};
            std::string e2;
            const bool rC = builtC ? mt::WriteTag(pc.generic_string(), w,
                                                  nullptr, e2)
                                   : false;
            const bool rD = rC ? mt::WriteTag(pd.generic_string(), w, nullptr,
                                              e2)
                               : false;
            std::string e3;
            const mt::TagSet tc =
                rD ? mt::ParseTag(pc.generic_string(), e3) : mt::TagSet{};
            const mt::TagSet td =
                rD ? mt::ParseTag(pd.generic_string(), e3) : mt::TagSet{};
            std::vector<unsigned char> bufC, bufD;
            const bool readC = rD ? readFile(pc, bufC) : false;
            const bool readD = rD ? readFile(pd, bufD) : false;
            // 신설 후 재독 = 트리 성립(mdat 원문 0x55 유지 — mdat 뒤 moov 신설
            // leg = 무이동).
            const bool mdatC = readC && std::all_of(bufC.end() - 64, bufC.end(),
                                                    [](unsigned char c) {
                                                        return c == 0x55;
                                                    });
            const bool mdatD = readD && std::all_of(bufD.end() - 64, bufD.end(),
                                                    [](unsigned char c) {
                                                        return c == 0x55;
                                                    });
            check(builtC && builtD && rC && rD && readC && readD &&
                      e2.empty() && e3.empty() && tc.title == "No Udta" &&
                      tc.artist == "No Ilst A" && tc.album == "No Ilst Al" &&
                      td.title == "No Udta" && td.artist == "No Ilst A" &&
                      td.album == "No Ilst Al" && mdatC && mdatD &&
                      hasBytes(bufC, asBytes("No Udta")),
                  "2x-⑨ m4a 신설 폴백 = udta 부재(moov 말미 신설)+ilst 부재"
                  "(meta 말미 신설) 쓰기 → 재독 3필드 등호+mdat 원문 무이동"
                  "(부재 부모 box 순차 신설 규약 — D2)");
        }

        // ⑩) m4a covr 교체 round-trip — (a) cover null = 기존 covr 유지
        //    (b) cover 지정 = 기존 covr 소각+신설(ReadCover 재독 = 신설
        //    그림)·(c) opus MBP 교체(엔트리 부재 → 신설 — D8 완전 교체).
        {
            const fs::path pa = tdir / "2x_o_cvr_null.m4a";
            const fs::path pb = tdir / "2x_p_cvr_repl.m4a";
            const fs::path pc = tdir / "2x_q_cvr_opus.opus";
            auto mkCvrItem = [&](const std::vector<unsigned char>& img) {
                return mkItem("covr", mkData(14, img));  // iTunes 코드 14=PNG
            };
            const std::vector<unsigned char> ilstC = concat(
                {mkItem("\xA9" "nam",
                        mkData(1, {'K', 'e', 'e', 'p', ' ', 'T', 'i', 't',
                                   'l', 'e'})),
                 mkCvrItem(pngBlob)});
            const std::vector<unsigned char> blob =
                concat({mkFtyp(),
                        mkBox("moov", mkBox("udta", concat({mkMeta(ilstC)}))),
                        mkBox("mdat", std::vector<unsigned char>(64, 0xAB))});
            const bool builtA = putFile(pa, blob);
            const bool builtB = putFile(pb, blob);
            // (a) cover null — covr 유지.
            const mt::TagSet w1{"Null Covr", "Null Artist", "Null Album"};
            std::string ea;
            const bool rA = builtA ? mt::WriteTag(pa.generic_string(), w1,
                                                  nullptr, ea)
                                   : false;
            mt::CoverArt cA;
            const bool vA = rA ? mt::ReadCover(pa.generic_string(), cA, ea)
                               : false;
            // (b) cover 지정 — png → jpeg 교체.
            const mt::TagSet w2{"Jpg Covr", "Jpg Artist", "Jpg Album"};
            std::string eb;
            mt::CoverArt jpgC;
            jpgC.mime = "image/jpeg";
            jpgC.bytes = jpgBlob;
            const bool rB = builtB ? mt::WriteTag(pb.generic_string(), w2,
                                                  &jpgC, eb)
                                   : false;
            mt::CoverArt cB;
            const bool vB = rB ? mt::ReadCover(pb.generic_string(), cB, eb)
                               : false;
            // (c) opus MBP — 엔트리 부재 opus → MBP 신설.
            const std::vector<unsigned char> opusBlob = concat(
                {mkPage(0x02, 0ull, 0u, mkOpusHead()),
                 mkPage(0x04, 0ull, 1u,
                        mkOpusTags("synth-vendor", {}))});
            const bool builtC = putFile(pc, opusBlob);
            const mt::TagSet w3{"Opus Covr", "Opus A", "Opus Al"};
            std::string ec;
            const bool rC = builtC ? mt::WriteTag(pc.generic_string(), w3,
                                                  &jpgC, ec)
                                   : false;
            std::string ve;
            mt::CoverArt cC;
            const bool vC = rC ? mt::ReadCover(pc.generic_string(), cC, ve)
                               : false;
            check(builtA && builtB && builtC && rA && rB && rC &&
                      ea.empty() && eb.empty() && ec.empty() && vA && vB &&
                      vC &&
                      cA.mime == "image/png" &&
                      cA.bytes.size() == pngBlob.size() &&
                      std::memcmp(cA.bytes.data(), pngBlob.data(),
                                  pngBlob.size()) == 0 &&
                      cB.mime == "image/jpeg" &&
                      cB.bytes.size() == jpgBlob.size() &&
                      std::memcmp(cB.bytes.data(), jpgBlob.data(),
                                  jpgBlob.size()) == 0 &&
                      cC.mime == "image/jpeg" &&
                      cC.bytes.size() == jpgBlob.size() &&
                      std::memcmp(cC.bytes.data(), jpgBlob.data(),
                                  jpgBlob.size()) == 0,
                  "2x-⑩ covr 교체 round-trip = (a) cover null = 기존 png covr "
                  "유지·(b) 지정 = jpeg 소각+신설 재독 등호(D4/D8)+opus MBP "
                  "신설 재독 등호(vorbis 코멘트 사슬 재용 — 쓰기 leg 병합)");
        }

        // ⑪) opus 쓰기 round-trip+페이지 시퀀스 — 3필드 등호+벤더 유지+미관리
        //    엔트리(GENRE) 보존+페이지 사슬(CRC 전부 유효·시퀀스 0,1,2 연속·
        //    granule 불변(D10 — 오디오 페이지 원문 재기입)·코멘트 페이지 EOS
        //    표지(RFC 7845 — c0Htype 원존 유지).
        {
            const fs::path p = tdir / "2x_r_wtag.opus";
            const std::vector<unsigned char> blob = concat(
                {mkPage(0x02, 0ull, 0u, mkOpusHead()),
                 mkPage(0x04, 0ull, 1u,
                        mkOpusTags("synth-vendor",
                                   {{"GENRE", "Wave"},
                                    {"TITLE", "Old Opus"}})),
                 mkPage(0x00, 1000ull, 2u, {1, 2, 3, 4, 5})});
            const bool built = putFile(p, blob);
            std::string e;
            const mt::TagSet w{"Opus W Title", "W Opus Artist", "W Opus Album"};
            const bool r = built ? mt::WriteTag(p.generic_string(), w, nullptr,
                                                e)
                                 : false;
            std::string err;
            const mt::TagSet t =
                r ? mt::ParseTag(p.generic_string(), err) : mt::TagSet{};
            std::vector<unsigned char> buf;
            const bool read = r ? readFile(p, buf) : false;
            // 페이지 사슬 보행 — CRC 유효(자리 0 재계산 등호)·seq 연속·granule
            // 불변·코멘트 페이지 EOS 표지 유지.
            size_t off = 0;
            unsigned int seqs[3] = {0, 0, 0};
            unsigned long long gres[3] = {0, 0, 0};
            unsigned char htypes[3] = {0, 0, 0};
            bool crcOk[3] = {false, false, false};
            int pgNo = 0;
            while (off + 27 <= buf.size() && pgNo < 3 &&
                   std::memcmp(buf.data() + off, "OggS", 4) == 0) {
                const size_t segs = buf[off + 26];
                size_t plen = 0;
                for (size_t s = 0; s < segs; ++s) plen += buf[off + 27 + s];
                if (off + 27 + segs + plen > buf.size()) break;
                seqs[pgNo] = mt::RdLE32(buf.data() + off + 18);
                gres[pgNo] = mt::RdLE64(buf.data() + off + 6);
                htypes[pgNo] = buf[off + 5];
                std::vector<unsigned char> pg(
                    buf.begin() + static_cast<long>(off),
                    buf.begin() + static_cast<long>(off + 27 + segs + plen));
                const unsigned int stored = mt::RdLE32(pg.data() + 22);
                pg[22] = pg[23] = pg[24] = pg[25] = 0;  // CRC 자리 소각(규약)
                crcOk[pgNo] = mt::OggCrc(pg.data(), pg.size()) == stored;
                ++pgNo;
                off += 27 + segs + plen;
            }
            // 벤더 유지+미관리 엔트리(GENRE) 보존 — 쓰기 코어(RebuildVorbis
            // Entries 재용 — D9/D8) 재독.
            std::vector<unsigned char> comments;
            std::string vendor;
            std::vector<mt::VorbisEntry> entries;
            bool vok = read ? mt::OpusTagsPacket(buf.data(), buf.size(),
                                                 comments) &&
                                  mt::ParseVorbisCommentsRaw(
                                      comments.data(), comments.size(),
                                      vendor, entries)
                            : false;
            bool genreOk = false;
            for (const mt::VorbisEntry& en : entries)
                if (en.key == "GENRE" && en.value == "Wave") genreOk = true;
            check(built && r && err.empty() && read && t.title == "Opus W "
                      "Title" && t.artist == "W Opus Artist" &&
                      t.album == "W Opus Album" && vok &&
                      vendor == "synth-vendor" && genreOk && pgNo == 3 &&
                      seqs[0] == 0 && seqs[1] == 1 && seqs[2] == 2 &&
                      gres[0] == 0 && gres[1] == 0 && gres[2] == 1000 &&
                      htypes[0] == 0x02 && htypes[1] == 0x04 &&
                      htypes[2] == 0x00 && crcOk[0] && crcOk[1] && crcOk[2],
                  "2x-⑪ opus 쓰기 round-trip = 3필드 등호+벤더 유지+GENRE 보존"
                  "+페이지 시퀀스 0,1,2 연속(재번호 — D10)+CRC 전부 유효+"
                  "granule 불변+코멘트 페이지 EOS 표지 유지(RFC 7845)");
        }

        // ⑫) 한글 2형식 round-trip — m4a(covr 데이터 타입 1 UTF-8 통과)+opus
        //    (vorbis 코멘트 사슬) 쓰기 → 재독 = 3필드 UTF-8 바이트 등호.
        {
            const fs::path pm = tdir / "2x_s_kr.m4a";
            const fs::path po = tdir / "2x_t_kr.opus";
            const std::string krTitle = "\xED\x95\x9C\xEA\xB8\x80 "
                                        "\xEC\xA0\x9C\xEB\xAA\xA9";
            const std::string krArtist = "\xED\x95\x9C\xEA\xB8\x80 "
                                         "\xEC\x95\x84\xED\x8B\xB0\xEC\x8A"
                                         "\xA4\xED\x8A\xB8";
            const std::string krAlbum = "\xED\x95\x9C\xEA\xB8\x80 \xEC\x95\xA8"
                                        "\xEB\xB2\x94";
            const std::vector<unsigned char> m4aBlob = concat(
                {mkFtyp(),
                 mkBox("moov", mkBox("udta", concat({mkMeta({})}))),
                 mkBox("mdat", std::vector<unsigned char>(64, 0xAB))});
            const std::vector<unsigned char> opusBlob = concat(
                {mkPage(0x02, 0ull, 0u, mkOpusHead()),
                 mkPage(0x04, 0ull, 1u, mkOpusTags("synth-vendor", {}))});
            const bool builtM = putFile(pm, m4aBlob);
            const bool builtO = putFile(po, opusBlob);
            std::string em;
            const mt::TagSet w{krTitle, krArtist, krAlbum};
            const bool rM = builtM ? mt::WriteTag(pm.generic_string(), w,
                                                  nullptr, em)
                                   : false;
            const bool rO = rM ? mt::WriteTag(po.generic_string(), w, nullptr,
                                              em)
                               : false;
            std::string em2, eo;
            const mt::TagSet tm =
                rO ? mt::ParseTag(pm.generic_string(), em2) : mt::TagSet{};
            const mt::TagSet to =
                rO ? mt::ParseTag(po.generic_string(), eo) : mt::TagSet{};
            check(builtM && builtO && rM && rO && em.empty() && em2.empty() &&
                      eo.empty() && tm.title == krTitle &&
                      tm.artist == krArtist && tm.album == krAlbum &&
                      to.title == krTitle && to.artist == krArtist &&
                      to.album == krAlbum,
                  "2x-⑫ 한글 2형식 round-trip = m4a(covr 타입 1 UTF-8)+opus"
                  "(OpusTags 코멘트 사슬) 쓰기→재독 3필드 UTF-8 바이트 등호"
                  "(REMNANT 0 — 말미 ⑫b로 이관)");
        }

        // ⑫b) stco/co64 청크 오프셋 패치 — moov Δ 이동 계약(#115 T3 fix r1
        //    — C1: 외부 플레이어 재생 인덱스 — 태그 자체 재독의 블라인드
        //    스팟). faststart(moov가 mdat 앞) 태그 쓰기(사이즈 변화 유발):
        //    (a) 무free = stco 항목 Δ 가산 패치(재독 항목 = 실측 신mdat
        //    payload 자리 등호 — 원문+Δ 동치·trak/tkhd/mdia/minf/stbl 표준
        //    사슬 fixture) (b) free 흡수 = mdat 자리 고정+stco 원존(무패치)
        //    (c) 무free+패치 넘침(32bit 상한 근접 합성) = "free 부족" 정직
        //    거부(원본 무변경 — ③ 계약). REMNANT 0(사후 소각 — ⑫에서 이관).
        {
            auto mkStco = [&](const std::vector<unsigned int>& es) {
                std::vector<unsigned char> b;
                pushBE32(b, 0);  // version/flags
                pushBE32(b, static_cast<unsigned int>(es.size()));
                for (unsigned int v : es) pushBE32(b, v);
                return mkBox("stco", b);
            };
            auto mkTrak = [&](const std::vector<unsigned char>& stco) {
                return mkBox(
                    "trak",
                    concat({mkBox("tkhd",
                                  std::vector<unsigned char>(32, 0x2A)),
                            mkBox("mdia",
                                  mkBox("minf", mkBox("stbl", stco)))}));
            };
            // 2패스 합성 — stco 절대 오프셋 = 실측 mdat payload 선두(T1
            // mkData 12B 함정 원상 봉인 — fixture 정직성 계약: 항목 값 크기와
            // 무관한 사이즈 고정성 실측 — 1패스 probe0로 자리 산출).
            const std::vector<unsigned char> mdatBox =
                mkBox("mdat", std::vector<unsigned char>(48, 0x77));
            const auto buildM4a = [&](const std::vector<unsigned int>& es,
                                      bool withFree,
                                      const std::vector<unsigned char>& mdat) {
                const std::vector<unsigned char> ilstFix = concat(
                    {mkItem("\xA9" "nam", mkData(1, {'T', 'i'}))});
                const std::vector<unsigned char> moov = mkBox(
                    "moov",
                    concat({mkBox("udta", concat({mkMeta(ilstFix)})),
                            mkTrak(mkStco(es))}));
                if (withFree)
                    return concat(
                        {mkFtyp(), moov,
                         mkBox("free",
                               std::vector<unsigned char>(1024, 0x11)),
                         mdat});
                return concat({mkFtyp(), moov, mdat});
            };
            auto stcoEntry = [](const std::vector<unsigned char>& buf,
                                unsigned int& cnt, unsigned int& e0) {
                size_t sp = 0, sn = 0;
                if (!mt::Mp4FindBox(buf.data(), 0, buf.size(), "moov", sp,
                                    sn) ||
                    !mt::Mp4FindBox(buf.data(), sp, sn, "trak", sp, sn) ||
                    !mt::Mp4FindBox(buf.data(), sp, sn, "mdia", sp, sn) ||
                    !mt::Mp4FindBox(buf.data(), sp, sn, "minf", sp, sn) ||
                    !mt::Mp4FindBox(buf.data(), sp, sn, "stbl", sp, sn) ||
                    !mt::Mp4FindBox(buf.data(), sp, sn, "stco", sp, sn) ||
                    sn < 8)
                    return false;
                cnt = mt::RdBE32(buf.data() + sp + 4);
                e0 = cnt > 0 ? mt::RdBE32(buf.data() + sp + 8) : 0;
                return true;
            };
            auto mdatStart = [](const std::vector<unsigned char>& buf,
                                size_t& at) {
                size_t sp = 0, sn = 0;
                if (!mt::Mp4FindBox(buf.data(), 0, buf.size(), "mdat", sp,
                                    sn))
                    return false;
                at = sp;
                return true;
            };
            // 2패스 = 배치별 실측(free 배치 = 자리가 다르다 — probe0 단일
            // 오프셋 오용 원장 — fixture 형태별 절대 오프셋 산출).
            const std::vector<unsigned char> probe0 =
                buildM4a({0u, 0u}, false, mdatBox);
            size_t pAt = 0;
            const bool probeOk = mdatStart(probe0, pAt);
            const size_t pAtA = pAt;  // (a) 무free 배치의 mdat payload 자리
            const std::vector<unsigned char> probe1 =
                buildM4a({0u, 0u}, true, mdatBox);
            size_t tB = 0;
            const bool probeOkB = mdatStart(probe1, tB);
            const size_t pAtB = tB;  // (b) free 배치의 mdat payload 자리
            // (a) 무free — stco = 실측 절대 오프셋 fixture.
            const std::vector<unsigned char> blobA =
                buildM4a({static_cast<unsigned int>(pAtA),
                           static_cast<unsigned int>(pAtA)},
                          false, mdatBox);
            const fs::path pA = tdir / "2x_y_stco_nofree.m4a";
            // (b) free 흡수 — moov 뒤 free(1024B payload)가 Δ를 삼킨다.
            const std::vector<unsigned char> blobB =
                buildM4a({static_cast<unsigned int>(pAtB),
                           static_cast<unsigned int>(pAtB)},
                          true, mdatBox);
            const fs::path pB = tdir / "2x_z_stco_free.m4a";
            // (c) 거부 — 32bit stco 상한 근접(0xFFFFFFFB — 규약 상한 합성
            // 시험 — Δ≥5면 전부 넘친다 — 무free → 패치 leg → 거부).
            const std::vector<unsigned char> blobC =
                buildM4a({0xFFFFFFFBu, 0xFFFFFFFBu}, false, mdatBox);
            const fs::path pC = tdir / "2x_w_stco_max.m4a";
            const bool builtA = putFile(pA, blobA);
            const bool builtB = putFile(pB, blobB);
            const bool builtC = putFile(pC, blobC);
            // Δ>0 유발 — 구 ©nam("Ti")보다 긴 신규 값.
            const mt::TagSet w{"Long Title grows the moov tree", "Stco A",
                               "Stco Al"};
            std::string eA, eB, eC;
            const bool rA = builtA ? mt::WriteTag(pA.generic_string(), w,
                                                  nullptr, eA)
                                   : false;
            const bool rB = rA ? mt::WriteTag(pB.generic_string(), w, nullptr,
                                              eB)
                               : false;
            const bool rC = rB ? mt::WriteTag(pC.generic_string(), w, nullptr,
                                              eC)
                               : false;
            std::vector<unsigned char> bufA, bufB, bufC;
            const bool readA = rB ? readFile(pA, bufA) : false;
            const bool readB = rB ? readFile(pB, bufB) : false;
            const bool readC = rB ? readFile(pC, bufC) : false;
            // 재독 — (a) 패치 = stco 항목 == 신mdat payload 자리(절대 위치
            // 불변량 — 원문+Δ 동치 진실원)·mdat payload 원문. (b) 흡수 =
            // mdat 자리 고정+stco 원존. (c) 거부 = 원본 무변경(바이트 등호).
            size_t aAt = 0, bAt = 0;
            unsigned int cntA = 0, e0A = 0, cntB = 0, e0B = 0;
            const bool walkA =
                readA && mdatStart(bufA, aAt) && stcoEntry(bufA, cntA, e0A);
            const bool walkB =
                readB && mdatStart(bufB, bAt) && stcoEntry(bufB, cntB, e0B);
            std::string e2;
            const mt::TagSet tA =
                rA ? mt::ParseTag(pA.generic_string(), e2) : mt::TagSet{};
            check(builtA && builtB && builtC && rA && rB && !rC &&
                      eA.empty() && eB.empty() && eC == "free 부족" &&
                      e2.empty() && probeOk && walkA && walkB &&
                      cntA == 2 && e0A == aAt &&
                      bufA.size() > 56 &&
                      std::all_of(bufA.end() - 48, bufA.end(),
                                  [](unsigned char c) { return c == 0x77; })
                          /* mdat payload 원문(패치 leg 이동·데이터 원존) */ &&
                      tA.title == "Long Title grows the moov tree" &&
                      cntB == 2 && bAt == pAtB && e0B == pAtB && bufC == blobC,
                  "2x-⑫b stco/co64 패치 = (a) 무free Δ 패치(재독 stco = 실측"
                  " 신mdat payload 자리 등호+payload 원문)+ParseTag 등호"
                  "(b) free 흡수(mdat 자리 고정·stco 원존)(c) 상한 근접+무"
                  "free = free 부족 정직 거부(원본 무변경 — C1 수형)");
            std::error_code sx;
            fs::remove_all(tdir, sx);  // REMNANT 0 — 사후 소각 동봉(2x 수형)
            check(!sx && !fs::exists(tdir, sx), "2x-⑫b REMNANT 0(사후 소각)");
        }

        // ⑬) moov 후행 m4a(#115 원장행 — 2026-10-27 batch2 실환 소각):
        //    ffmpeg 기본 mux m4a = mdat 선두+moov 말미 — moov가 머리
        //    kTagHeadCap(1MB) 밖 = 구 ParseTag "-" 폴백. 수형 = 첫 mdat 끝
        //    오프(선언 size 진실원 — Mp4MdatNext)부터 kMoovLocateMax(2MB)
        //    창 seek+read → 창 보행 재용. (a) mdat 1.5MB 더미+moov(udta/
        //    meta/ilst 3항) 합성 → ParseTag 3필드 등호(② UTF-8 원문과
        //    동일 표 — ParseM4aIlstItems 재용 계보 등호) (b) 창 밖 사전
        //    확인(mdat 끝 = 머리 cap 밖 — 수형 leg가 실제로 쓰인다) (c) err
        //    빈(정상 경로) ③ REMNANT 0.
        {
            std::error_code s13;
            fs::remove_all(tdir, s13);  // 선제거+재개방(⑫b가 소각한 tdir)
            fs::create_directories(tdir, s13);
            const fs::path p = tdir / "2x_m_moov_trailing.m4a";
            const std::vector<unsigned char> ilst13 = concat(
                {mkItem("\xA9" "nam", mkData(1, asBytes("M4A Title"))),
                 mkItem("\xA9" "ART", mkData(1, asBytes("M4A Artist"))),
                 mkItem("\xA9" "alb",
                        mkData(1, {0xED, 0x95, 0x9C, 0xEA, 0xB8, 0x80, ' ',
                                   0xEC, 0x95, 0xA8, 0xEB, 0xB2,
                                   0x94}) /* "한글 앨범" UTF-8 */)});

            const std::vector<unsigned char> blob13 = concat(
                {mkFtyp(), mkBox("mdat",
                                 std::vector<unsigned char>(1500 * 1024,
                                                            0xCD)),
                 mkBox("moov", mkBox("udta", mkMeta(ilst13)))});
            const bool built13 = putFile(p, blob13);
            // (b) 창 밖 사전 확인 — 머리 1MB 보행의 첫 mdat 끝 오프가
            // 머리 cap 밖(=moov는 머리에 성립 불가 — NoMoov 경로 동형).
            std::vector<unsigned char> head13;
            size_t mdatNext = 0;
            const bool r13 = readFile(p, head13);
            const size_t headCap =
                head13.size() < 1024 * 1024 ? head13.size() : 1024 * 1024;
            const bool headNoMoov =
                r13 && mt::Mp4MdatNext(head13.data(), headCap, mdatNext) &&
                mdatNext > headCap;
            std::string e13;
            const mt::TagSet t13 =
                built13 ? mt::ParseTag(p.generic_string(), e13)
                        : mt::TagSet{};
            check(built13 && headNoMoov && e13.empty() &&
                      t13.title == "M4A Title" &&
                      t13.artist == "M4A Artist" &&
                      t13.album ==
                          std::string("\xED\x95\x9C\xEA\xB8\x80 \xEC\x95"
                                      "\xA8\xEB\xB2\x94"),
                  "2x-⑬ moov 후행 m4a = mdat 1.5MB 더미(머리 kTagHeadCap 밖"
                  " — 사전 확인 mdatNext>cap)+moov 창(kMoovLocateMax) 2차 "
                  "read → 3필드 등호(② UTF-8 원문 동일 표 — ParseM4aIlst"
                  "Items 재용)+err 빈(폴백 아닌 정상 경로)");
            fs::remove_all(tdir, s13);  // REMNANT 0 — 사후 소각 동봉(2x 수형)
            check(!s13 && !fs::exists(tdir, s13),
                  "2x-⑬ REMNANT 0(사후 소각)");
        }

        // ⑭) moov 후행 covr 2차 read(ReadCover leg — ⑬ 태그 leg와 같은
        //    계통 계승): ⑬의 mdat 1.5MB 더미는 ReadCover 머리 창
        //    (kTagWriteMax 16MiB) 안쪽이라 커버는 머리 보행으로 이미
        //    성립 — 실제 2차 read를 강제하려면 mdat가 머리 창을 넘어야
        //    한다: mdat 더미 = kTagWriteMax+2KB → 머리 read 절단(mdat
        //    중간) → ParseMp4Covr 미발견+Mp4LocateIlst kMp4TagNoMoov →
        //    Mp4MdatNext(선언 size 진실원) → kMoovLocateMax 창 seek+read →
        //    창 보행(moov→Mp4LocateTagTree→ParseMp4CovrItems 재용) → covr
        //    등호. (a) 사전 확인(mdat 끝 오프 > 머리 창 = 수형 leg 실사용
        //    ·파일 전체도 창 초과) (b) mime image/jpeg(코드 13)+바이트 원문
        //    등호 (c) err 빈 (d) REMNANT 0.
        {
            std::error_code s14;
            fs::remove_all(tdir, s14);
            fs::create_directories(tdir, s14);
            const fs::path p = tdir / "2x_n_covr_trailing.m4a";
            // 그림 블롭(⑥ jpgBlob 동일 표 — 정직 합성 JPEG SOI 매agic).
            const std::vector<unsigned char> img14 = {
                0xFF, 0xD8, 0xFF, 0xE0, 'J', 'P', 'J', '+', '!', '-', '.',
                'J', 'P', 'E', 'G'};
            std::vector<unsigned char> dataBody14;
            pushBE32(dataBody14, 13);  // iTunes 규약 코드 13 (JPEG)
            pushBE32(dataBody14, 0);   // locale
            dataBody14.insert(dataBody14.end(), img14.begin(), img14.end());
            const std::vector<unsigned char> ilst14 = concat(
                {mkItem("covr", mkBox("data", dataBody14))});
            const std::size_t mdatPay14 =
                mt::kTagWriteMax + 2048;  // 머리 창 밖으로 moov를 밀어낸다
            const std::vector<unsigned char> blob14 =
                concat({mkFtyp(),
                        mkBox("mdat", std::vector<unsigned char>(mdatPay14,
                                                                 0xCD)),
                        mkBox("moov", mkBox("udta", mkMeta(ilst14)))});
            const bool built14 = putFile(p, blob14);
            // (a) 사전 확인 — 머리 창(kTagWriteMax) 슬라이스의 Mp4MdatNext가
            //     내는 mdat 끝 오프 > 창(moov는 머리 창에 성립 불가 — 수형
            //     leg 실사용 보증).
            std::vector<unsigned char> head14(
                blob14.size() < mt::kTagWriteMax ? blob14.size()
                                                 : mt::kTagWriteMax);
            std::copy(blob14.begin(), blob14.begin() + head14.size(),
                      head14.begin());
            size_t mdatNext14 = 0;
            const bool beyondHead14 =
                mt::Mp4MdatNext(head14.data(), head14.size(), mdatNext14) &&
                mdatNext14 > mt::kTagWriteMax;
            mt::CoverArt c14;
            std::string e14;
            const bool r14 =
                built14 ? mt::ReadCover(p.generic_string(), c14, e14) : false;
            check(built14 && beyondHead14 && r14 && e14.empty() &&
                      c14.mime == "image/jpeg" &&
                      c14.bytes.size() == img14.size() &&
                      std::memcmp(c14.bytes.data(), img14.data(),
                                  img14.size()) == 0,
                  "2x-⑭ moov 후행 covr = 머리 창(kTagWriteMax) 절단 사전 확"
                  "인(mdatNext>창)+2차 read(mdat 끝 오프→kMoovLocateMax 창)"
                  " → mime image/jpeg(코드 13)+그림 바이트 원문 등호+err "
                  "빈(ParseMp4CovrItems 재용 — ⑥ 등호 계승)");
            fs::remove_all(tdir, s14);  // REMNANT 0 — 사후 소각 동봉(2x 수형)
            check(!s14 && !fs::exists(tdir, s14),
                  "2x-⑭ REMNANT 0(사후 소각)");
        }
    }
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
        std::printf("  library-list [BASE]  Scan the app library (.jkx/console/builtin), 1 line per app\n");
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
        // posix leg(앱 커버리지 1단): 본체 공유, usage 접미 문구만 분기 —
        // win32 원문 문구는 바이트 그대로(회귀 무수정 계약).
#ifdef _WIN32
        if (argc < 3) {
            std::fprintf(stderr, "Usage: jkx-pack <app>  (bundles jkapp_<app>.dll + icons + manifest into apps/<app>.jkx)\n");
            return 1;
        }
#else
        if (argc < 3) {
            std::fprintf(stderr, "Usage: jkx-pack <app>  (bundles jkapp_<app>.so + icons + manifest into apps/<app>.jkx)\n");
            return 1;
        }
#endif
        return RunJkxPack(argv[2]);
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

    if (argc > 1 && std::strcmp(argv[1], "library-list") == 0) {
        // 순수 stdio 라우트 — win32 게이트 없음: TX6 개방 선례(docs/78)로 posix
        // 본체에서도 동일 동작(스펙 2026-10-06-app-library §4).
        return RunLibraryList(argc, argv);
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
