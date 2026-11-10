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
#include <agent/JKLlmTurnHttp.h>  // selftest 1n-g — T7 LLM HTTP 어댑터(스턬 transport 원존)
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
#include <apps/ChatSessionLog.h>  // selftest 3c — 대화 세션 기록(스펙 2026-11-01-chat-session-log-design)
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

// jkweb 본체 in-process 재용 (tools/jkweb/main.cpp — JK_WEB_INPROCESS leg)
extern "C" int JKWebStartInProcess();

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
#if defined(__ANDROID__)
    // jkdesktop-apk: 실행물이 nativeLibraryDir의 lib*.so로 포장되어 exec되므로
    // 모듈도 lib 접두(libjkapp_<app>.so)로 판정한다 — jniLibs 규약이 lib 접두
    // 만 nativeLibraryDir에 실린다(W^X 우회 경유 — docs/101 §3).
    return prefix + "libjkapp_" + appName + jk::server::AppModuleSuffix();
#else
    return prefix + "jkapp_" + appName + jk::server::AppModuleSuffix();
#endif
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


int RunAppSelfTest();  // [P1] src/tests/JKSelfTest.cpp — 분리

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

    if (argc > 1 && (std::strcmp(argv[1], "--version") == 0 ||
                     std::strcmp(argv[1], "version") == 0)) {
        // [P0 횡 — docs/106 §1-8] 빌드 지문: 실행본이 어느 커밋인지 응답.
        // JK_BUILD_SHA는 CMake 주입(미주입 빌드 = "미상" 정직 표기).
#ifdef JK_BUILD_SHA
        std::printf("jkdesktop build %s", JK_BUILD_SHA);
#else
        std::printf("jkdesktop build unknown (no JK_BUILD_SHA)");
#endif
        return 0;
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
#if defined(__ANDROID__)
        // jkdesktop-apk — docs/103 실측 승계(docs/101 방향 B): 클라 호스트 = exec 전용 PIE 실행파일
        // libjkc.so(r12 실측 원문 — ET_DYN 공유 객체는 exec로 SIGILL 즉사:
        // PT_INTERP 부재라 커널이 e_entry로 점프. W^X 우회는 실행파일 포장이
        // 정석). 서버 본체(libjkdesk.so)는 SDLActivity의 dlopen 물리로 분리.
        server.SetClientHostExe("libjkc.so");
#endif
        // 단일 인스턴스 가드(docs/59 §10) — Init 전에 봉쇄: 가드가 Init 뒤에
        // 있으면 거부 인스턴스가 앱 설치+아이콘 로드를 전부 수행한 뒤 죽는다
        // (2026-09-20 사용자 붙여넣기 로그 실측 — 낭비+로그 혼란).
#if defined(__ANDROID__)
        // jkdesktop-apk: 같은 프로세스 SDL_main 재진입(닫았다 다시 열기 — DeX
        /// 폰 전환)에서 선행 부트의 leaked fd가 flock을 계속 보유해 무성
        // return 1(원문 — Fatal signal 0건)로 사망한다. 재진입 판(SDL_main 심이
        // JK_SPIKE_NO_GUARD를 심음)에서는 가드를 우회한다.
        if (!getenv("JK_SPIKE_NO_GUARD") &&
            !server.TryAcquireSingleInstanceGuard(jk::ipc::kWindowServerPipeName)) {
            return 1;
        }
        // 선행 부트의 zombie acceptor(동일 프로세스 leak)가 소켓 경로를 계속
        // 소유한다 — 바인딩 전에 경로 파일을 떼어 낸다(unlink — 살아 있는
        // 리스너의 이노디드는 그대로 고아, 신규 바인드가 경로를 소유).
        {
            std::error_code rmEc;
            std::filesystem::remove(
                std::filesystem::path(jk::fs::TempDir() +
                    std::string(jk::ipc::kWindowServerPipeName) + ".sock"), rmEc);
            std::fprintf(stderr, "[jkdesk] endpoint unlink rmEc=%d (rebind 전)\n",
                         (int)rmEc.value());
        }
#else
        if (!server.TryAcquireSingleInstanceGuard(jk::ipc::kWindowServerPipeName)) {
            return 1;
        }
#endif
        if (!server.Init("JKENGINE Window Server", 1280, 720)) {
            return 1;
        }
        if (!server.StartAcceptor(jk::ipc::kWindowServerPipeName)) {
            return 1;
        }
#if defined(__ANDROID__)
        // 스펙 §5 — jkweb 본체를 in-process 스레드로(별도 바이너리 폐기):
        // /talk POST(localhost:8090) → ChatRouteTurn → JKAgentClient(같은
        // 서버 IPC 루프백) → spawn/close/focus 위임. jkweb 단독 빌드와 동일
        // 진실원(tools/jkweb/main.cpp의 JK_WEB_INPROCESS leg).
#if defined(JK_WEB_INPROCESS)
        std::thread([] { ::JKWebStartInProcess(); }).detach();
#endif
#endif
#if defined(__ANDROID__)
        // jkdesktop-apk 자동화(docs/101 §5 재시험 루프 절감): JK_SPIKE_AUTOSPAWN
        // env의 앱을 부팅 1.5s 후에 스폰 — 재시험마다 수동 아이콘 탭을 대신한다.
        // 스폰 물리는 서버 spawn leg와 동형(libjkc.so --client <app> — exe-dir
        // = JK_EXE_PATH(nativeLibraryDir)). Run 뒤 인가를 받지 않는 분리 스레드.
        if (const char* autoSpawn = ::getenv("JK_SPIKE_AUTOSPAWN");
            autoSpawn && autoSpawn[0]) {
            std::thread([autoApp = std::string(autoSpawn)]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                const std::string exePath = jk::fs::GetExecutablePath();
                if (exePath.empty()) return;
                const size_t cut = exePath.find_last_of('/');
                if (cut == std::string::npos) return;
                const std::string dir = exePath.substr(0, cut);
                jk::process::SpawnOptions opt;
                opt.commandLineUtf8 =
                    "'" + dir + "/libjkc.so' --client " + autoApp;
                opt.workingDir = dir;
                const jk::process::SpawnResult sp = jk::process::Spawn(opt);
                std::fprintf(stderr,
                             "[jkdesk] autospawn %s ok=%d err=%u\n",
                             autoApp.c_str(), (int)sp.ok, sp.errorCode);
                std::fflush(stderr);
            }).detach();
        }
#endif
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
#if defined(__ANDROID__)
    // jkdesktop-apk — docs/103 실측 승계(docs/101 방향 B): exec'd 클라(libjkc.so — PIE 실행파일
    // 포장)는 SDLActivity의 SDL_Android_Init 사다리가 없다 — SDL_Init의
    // main-ready 게이트(r14 원문 "did you include SDL_main.h in the file
    // containing your main()?")를 SDL_SetMainReady() 1회로 통과. 비디오
    // 드라이버는 클라 힌트 leg(offscreen+SW)가 이어받는다.
    SDL_SetMainReady();
#endif
    return RunMain(argc, argv);
}
#endif // _WIN32
