#include <server/JKWindowServer.h>
#include <agent/JKAgentJson.h>

#include <apps/AppLauncherItem.h>
#include <desktop/JKDesktopShell.h>
#include <JKAudioCommand.h>
#include <JKAudioThread.h>
#include <JKDC.h>
#include <JKHangulManager.h>
#include <JKHangulUtil.h>
#include <JKImageLoader.h>
#include <JKImeHook.h>
#include <JKMessageBus.h>
#include <JKSDLAudioBackend.h>
#include <JKSDLRenderBackend.h>
#include <JKResourceCache.h>
#include <JKSoundManager.h>
#include <JKTextAtlas.h>
#include <JKPlatform.h>
#include <theme/JKTheme.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <thread>
#include <vector>

// stb_image_write (docs/35): single-TU implementation — STBIW_STATIC keeps
// the symbols file-local so other TUs (imgui) are unaffected.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

#ifdef _WIN32
// Minimal Windows API declarations for spawning client processes without
// pulling in the full Windows headers (which conflict with legacy JKENGINE
// typedefs in other translation units).
struct LauncherStartupInfoA {
    unsigned long cb = 0;
    char* lpReserved = nullptr;
    char* lpDesktop = nullptr;
    char* lpTitle = nullptr;
    unsigned long dwX = 0;
    unsigned long dwY = 0;
    unsigned long dwXSize = 0;
    unsigned long dwYSize = 0;
    unsigned long dwXCountChars = 0;
    unsigned long dwYCountChars = 0;
    unsigned long dwFillAttribute = 0;
    unsigned long dwFlags = 0;
    unsigned short wShowWindow = 0;
    unsigned short cbReserved2 = 0;
    unsigned char* lpReserved2 = nullptr;
    void* hStdInput = nullptr;
    void* hStdOutput = nullptr;
    void* hStdError = nullptr;
};

struct LauncherProcessInformation {
    void* hProcess = nullptr;
    void* hThread = nullptr;
    unsigned long dwProcessId = 0;
    unsigned long dwThreadId = 0;
};

// W variant (docs/48 후속 CP949 레저): the A variants round-trip the command
// line through CP_ACP (CP949 on Korean Windows), mangling UTF-8 args — the
// filedlg json filter/title Korean labels arrived corrupted. Spawn wide:
// convert UTF-8 args to UTF-16 here and let the child's wmain entry
// (main.cpp) convert back with CP_UTF8.
struct LauncherStartupInfoW {
    unsigned long cb = 0;
    wchar_t* lpReserved = nullptr;
    wchar_t* lpDesktop = nullptr;
    wchar_t* lpTitle = nullptr;
    unsigned long dwX = 0;
    unsigned long dwY = 0;
    unsigned long dwXSize = 0;
    unsigned long dwYSize = 0;
    unsigned long dwXCountChars = 0;
    unsigned long dwYCountChars = 0;
    unsigned long dwFillAttribute = 0;
    unsigned long dwFlags = 0;
    unsigned short wShowWindow = 0;
    unsigned short cbReserved2 = 0;
    unsigned char* lpReserved2 = nullptr;
    void* hStdInput = nullptr;
    void* hStdOutput = nullptr;
    void* hStdError = nullptr;
};

extern "C" __declspec(dllimport) int __stdcall CreateProcessW(
    const wchar_t* lpApplicationName,
    wchar_t* lpCommandLine,
    void* lpProcessAttributes,
    void* lpThreadAttributes,
    int bInheritHandles,
    unsigned long dwCreationFlags,
    void* lpEnvironment,
    const wchar_t* lpCurrentDirectory,
    LauncherStartupInfoW* lpStartupInfo,
    LauncherProcessInformation* lpProcessInformation);

extern "C" __declspec(dllimport) int __stdcall MultiByteToWideChar(
    unsigned int codePage, unsigned long dwFlags, const char* lpMultiByteStr,
    int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar);

extern "C" __declspec(dllimport) int __stdcall CloseHandle(void* hObject);
extern "C" __declspec(dllimport) int __stdcall GetExitCodeProcess(
    void* hProcess, unsigned long* lpExitCode);
static const unsigned long kStillActiveExit = 259;  // STILL_ACTIVE

extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(
    void* hModule, char* lpFilename, unsigned long nSize);

extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(
    void* hModule, wchar_t* lpFilename, unsigned long nSize);

extern "C" __declspec(dllimport) int __stdcall CreateDirectoryA(
    const char* lpPathName, void* lpSecurityAttributes);

extern "C" __declspec(dllimport) int __stdcall WaitNamedPipeA(
    const char* lpNamedPipeName, unsigned long nTimeOut);

// 단일 인스턴스 가드 (StartAcceptor) — 수기 선언 관례 동일.
extern "C" __declspec(dllimport) void* __stdcall CreateMutexA(
    void* lpMutexAttributes, int bInitialOwner, const char* lpName);
extern "C" __declspec(dllimport) int __stdcall ReleaseMutex(void* hMutex);
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
constexpr unsigned long kErrorAlreadyExists = 183;   // winbase.h
constexpr unsigned long kErrorPipeBusy = 231;        // winbase.h

extern "C" __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);

extern "C" __declspec(dllimport) unsigned long __stdcall GetFileAttributesA(
    const char* lpFileName);
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentProcessId();
extern "C" __declspec(dllimport) void* __stdcall OpenProcess(
    unsigned long dwDesiredAccess, int bInheritHandle, unsigned long dwProcessId);
extern "C" __declspec(dllimport) int __stdcall TerminateProcess(
    void* hProcess, unsigned int uExitCode);

constexpr unsigned long kInvalidFileAttributes = 0xFFFFFFFF;

// 가드 보유자 표시 (2026-09-24 사용자 보고 "자주 반복되는데"): 거부 메시지가
// "close it first"만 하고 무엇을 닫을지 알려주지 않아 매번 프로세스 탐색이
// 필요했다 — 라이브 스택 서버는 jkwinserver.exe인데 사용자가 띄우는 건
// jkdesktop.exe --server라 이름도 달라 더 헷갈린다. Toolhelp 스냅샷 수기
// 선언(이 TU는 windows.h를 끌지 않는 관례 유지).
extern "C" __declspec(dllimport) void* __stdcall CreateToolhelp32Snapshot(
    unsigned long dwFlags, unsigned long th32ProcessID);
extern "C" __declspec(dllimport) int __stdcall Process32FirstW(
    void* hSnapshot, void* lppe);
extern "C" __declspec(dllimport) int __stdcall Process32NextW(
    void* hSnapshot, void* lppe);
constexpr unsigned long kTh32CsSnapProcess = 0x2;  // winutil.h

// PROCESSENTRY32W — 기본 정렬(8) 레이아웃(ULONG_PTR 멤버가 8바이트 정렬):
// dwSize 0 / cntUsage 4 / th32ProcessID 8 / th32DefaultHeap 16 / th32ModuleID 24
// / cntThreads 28 / th32ParentProcessID 32 / pcPriClassBase 36 / dwFlags 40
// / szExeFile 44. FindFileDataA와 달리 pack(4)이 아니라 자연 정렬이 정답.
struct ProcEntry32W {
    unsigned long dwSize = 0;
    unsigned long cntUsage = 0;
    unsigned long th32ProcessID = 0;
    unsigned long long th32DefaultHeap = 0;
    unsigned long th32ModuleID = 0;
    unsigned long cntThreads = 0;
    unsigned long th32ParentProcessID = 0;
    long pcPriClassBase = 0;
    unsigned long dwFlags = 0;
    wchar_t szExeFile[260] = {};
};

// settings_read의 layout_*.json 열거 (설정 허브 스펙 §2.2) — 이 TU는
// windows.h를 끌지 않으므로(JKENGINE 레거시 typedef 충돌) 수기 선언.
// WIN32_FIND_DATAA는 4바이트 팩(FILETIME 멤버가 8아니라 DWORD 정렬 —
// cFileName 오프셋 44) — pack 없으면 패딩이 4 들어가 이름이 4바이트 밀린다
// (파일 허브 files_list에서 발견 — settings_read 열거도 같은 결함).
#pragma pack(push, 4)
struct FindFileDataA {
    unsigned long dwFileAttributes = 0;
    unsigned long long ftCreationTime = 0;
    unsigned long long ftLastAccessTime = 0;
    unsigned long long ftLastWriteTime = 0;
    unsigned long nFileSizeHigh = 0;
    unsigned long nFileSizeLow = 0;
    unsigned long dwReserved0 = 0;
    unsigned long dwReserved1 = 0;
    char cFileName[260] = {};
    char cAlternateFileName[14] = {};
};
#pragma pack(pop)

extern "C" __declspec(dllimport) void* __stdcall FindFirstFileA(
    const char* lpFileName, FindFileDataA* lpFindFileData);
extern "C" __declspec(dllimport) int __stdcall FindNextFileA(
    void* hFindFile, FindFileDataA* lpFindFileData);
extern "C" __declspec(dllimport) int __stdcall FindClose(void* hFindFile);
// INVALID_HANDLE_VALUE (-1) — constexpr reinterpret_cast는 상수식이 아니라 함수로.
static inline void* kInvalidFindHandle() { return reinterpret_cast<void*>(-1); }
#endif // _WIN32

// 설정 허브 KV 헬퍼 — 본문은 WritePermissionsEntry 뒤(§2.2). Init의 부팅
// 로드가 쓴다(정의가 뒤에 있으므로 네임스페이스 내 전방선언).
namespace jk {
namespace server {

// 설정 허브 KV 헬퍼 — 본문은 WritePermissionsEntry 뒤(§2.2). Init의 부팅
// 로드가 쓴다(정의가 뒤에 있으므로 네임스페이스 내 전방선언).
static void LoadSettingsKv(bool& mute, int& volume, int& retention,
                           std::string& fontPath, std::string& fontFallback,
                           std::string& fontScale);

JKWindowServer::JKWindowServer() = default;

JKWindowServer::~JKWindowServer() {
    Stop();
#ifdef _WIN32
    if (serverGuardMutex_) {
        CloseHandle(serverGuardMutex_);
        serverGuardMutex_ = nullptr;
    }
#endif
}

bool JKWindowServer::Init(const std::string& title, int width, int height) {
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
    // A click on an unfocused window must BOTH activate it and act (grab the
    // title bar, press a button...). SDL's default drops the activation click,
    // which breaks "click title bar of a background surface to move it".
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::fprintf(stderr, "JKWindowServer::Init: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI |
        SDL_WINDOW_RESIZABLE);
    if (!window_) {
        std::fprintf(stderr, "JKWindowServer::Init: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::fprintf(stderr, "JKWindowServer::Init: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    // Client apps need SDL_TEXTINPUT (Char events) for text fields/IME. In
    // client/single-process modes the app starts text input itself; the
    // server window must too, or no client ever receives a Char event
    // (vplayer path field stayed empty under synthetic typing).
    SDL_StartTextInput();

    // 배너 벡터 글리프 (docs/63 Task 6): 서버에는 JKApplication의 렌더 스레드
    // 백엔드 같은 영구 백엔드가 없다 — renderer_ 위의 영구 백엔드를 만들어
    // bannerCache_의 등록(CreateImageFromRGBA)/플러시 소유자로 삼는다.
    bannerBackend_ = std::make_unique<JKSDLRenderBackend>(renderer_);

    compositor_ = std::make_unique<JKCompositor>(renderer_);
    // 승인 대상 시각화 (스펙 2026-09-19-app-tool-hub §5 1단): 컴포지트 패스의
    // 최상위 드로잉 단계를 서버 쪽 멤버 함수로 연결한다(의존성 역전 — 컴포지터는
    // pendingApprovals_를 모른다). outputScale 인자로 Composite의 스케일을 전달.
    compositor_->SetOverlayHook([this](float outputScale) {
        // 의미 커서 셀 (스펙 2026-09-22-semantic-cursor §5, Task 3) — 커서
        // 계층은 승인 파킹과 무관하게 매 프레임 그려지므로 별개 함수로 먼저
        // 그린다(승인 링/배너가 최상단을 유지하는 기존 정책 유지).
        DrawSemanticCursorCells(outputScale);
        DrawApprovalHighlights(outputScale);
    });
    UpdateOutputBounds();

    // P1 ③: the launcher is the in-process privileged shell (spec D7) — the
    // shell owns the grid + background; the server only supplies host
    // services through ShellHost (renderer, scale, texture factory, spawn).
    jk::desktop::JKDesktopShell::ShellHost shellHost;
    shellHost.renderer = renderer_;
    shellHost.outputScale = [this]() {
        return compositor_ ? compositor_->OutputScale() : 1.0f;
    };
    shellHost.makeTexture = [this](const jk::LoadedImage& img, const char* label) {
        return TextureFromRGBA(img, label);
    };
    shellHost.launch = [this](const char* app, bool fromJkx) {
        SpawnClient(app, fromJkx);
    };
    // 콘솔 앱 스폰 (P4 SDK §3): 터미널 위에 cmd — cwd는 앱 폴더. SpawnProcess가
    // 인용을 만들므로 cmd/cwd에 뒤따르는 백슬래시가 없어야 한다(453a327 레슨) —
    // 매니페스트는 상대경로 규칙으로 이를 보장한다.
    shellHost.spawnConsole = [this](const std::string& cmd, const std::string& cwd,
                                    const std::string& name) {
        SpawnConsoleApp(cmd, cwd, name);
    };
    shell_ = std::make_unique<jk::desktop::JKDesktopShell>();
    shell_->Init(shellHost);

    // Directional cursors for chrome resize hotspots (hover feedback).
    chromeCursors_[static_cast<int>(CursorShape::Arrow)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    chromeCursors_[static_cast<int>(CursorShape::SizeWE)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    chromeCursors_[static_cast<int>(CursorShape::SizeNS)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    chromeCursors_[static_cast<int>(CursorShape::SizeNWSE)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    chromeCursors_[static_cast<int>(CursorShape::SizeNESW)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);

#ifdef _WIN32
    // The server forwards raw keys to client surfaces and does no text
    // composition of its own. With an IME attached, Enter/Esc/letters arrive
    // as VK_PROCESSKEY + committed text only, so clients would never see
    // those keydowns — detach the IME context from the SDL window.
    JKPlatform::DetachIme(window_);
#endif

    // 설정 허브 KV (스펙 2026-09-18-settings-hub §2.2): 재시작 복원 —
    // audio.master/retention은 이 값이 진실원(파일 없으면 기본값 유지).
    LoadSettingsKv(audioMasterMute_, audioMasterVolume_, receiptRetentionDays_,
                   textFontPath_, textFontFallback_, textFontScale_);

    return true;
}

// 서버 프로세스 후보 스캔 — 서버 이미지명은 둘뿐이다: 라이브 스택 서버
// jkwinserver.exe(진짜 보유자, 종료 겨냥 가능)와 구형/개발 경로 jkdesktop.exe
// --server(명령행은 Toolhelp로 읽을 수 없고 태스크바 클라·단일 프로세스 앱과
// 이미지명이 같아 종료 겨냥 불가 — 표기만).
struct ServerCandidateScan {
    std::vector<unsigned long> wserver;
    std::vector<unsigned long> desktop;
};

static ServerCandidateScan ScanServerCandidates(unsigned long excludePid) {
    ServerCandidateScan s;
    void* snap = CreateToolhelp32Snapshot(kTh32CsSnapProcess, 0);
    if (!snap || snap == (void*)(long long)-1 /*INVALID_HANDLE_VALUE*/) return s;
    ProcEntry32W e;
    e.dwSize = sizeof(ProcEntry32W);
    if (Process32FirstW(snap, &e)) {
        do {
            // wchar→ascii 소문자 이미지명 (ASCII만 비교 — 이미지명은 ASCII)
            std::string exe;
            for (const wchar_t* p = e.szExeFile; *p; ++p) {
                char c = (*p >= 0 && *p < 128) ? static_cast<char>(*p) : '?';
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                exe.push_back(c);
            }
            if (e.th32ProcessID == excludePid) continue;  // 자기 자신 제외
            if (exe == "jkwinserver.exe") s.wserver.push_back(e.th32ProcessID);
            else if (exe == "jkdesktop.exe") s.desktop.push_back(e.th32ProcessID);
        } while (Process32NextW(snap, &e));
    }
    CloseHandle(snap);
    return s;
}

static std::string JoinPids(const std::vector<unsigned long>& pids) {
    std::string r;
    for (unsigned long pid : pids) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%lu", pid);
        if (!r.empty()) r += ", ";
        r += buf;
    }
    return r;
}

static std::string FindGuardHolderHint(const ServerCandidateScan& scan) {
    if (!scan.wserver.empty()) {
        std::string hint =
            "holder candidates: jkwinserver.exe PID " + JoinPids(scan.wserver) +
            " (the live-stack server) — close it first, e.g. `taskkill /F /PID " +
            std::to_string(scan.wserver.front()) + "`";
        if (!scan.desktop.empty()) {
            hint += "; jkdesktop.exe PID " + JoinPids(scan.desktop) +
                    " (a --server holder or the --client taskbar shell)";
        }
        return hint;
    }
    if (!scan.desktop.empty()) {
        return "holder candidates (image name only): jkdesktop.exe PID " +
               JoinPids(scan.desktop) + " — the '--server' one owns the guard "
               "('--client taskbar' is its shell client)";
    }
    return "";
}

// 접 처리 (2026-09-24 사용자 지시 "직접"): 보유자 서버를 직접 종료한다 —
// 보유자는 눈에 보이지 않아(숨김 기동) "닫으라"고만 하면 매번 프로세스
// 탐색이 필요했다(사용자 보고). jkwinserver.exe만 겨냥 — jkdesktop.exe는
// 클라/앱과 이미지명이 같아 겨냥 금지.
static bool KillServerHolders(const ServerCandidateScan& scan) {
    bool any = false;
    for (unsigned long pid : scan.wserver) {
        void* h = OpenProcess(0x0001 /*PROCESS_TERMINATE*/, 0, pid);
        if (!h) continue;
        if (TerminateProcess(h, 1)) any = true;
        CloseHandle(h);
    }
    return any;
}

bool JKWindowServer::TryAcquireSingleInstanceGuard(const std::string& pipeName,
                                                   bool takeover) {
#ifdef _WIN32
    // 단일 인스턴스 가드 (2026-09-20, docs/59 §10 유보 ②): 파이프 인스턴스는
    // PIPE_UNLIMITED_INSTANCES라 두 서버가 같은 이름을 열면 클라이언트가
    // 인스턴스에 갈라져 절반은 빈 서버(list_windows:[])를 보게 된다 — 폰
    // 세션 실측(list_windows:[] 오판 → 프로세스 kill 에스컬레이션)의 근원.
    // ①세션 로컬 명명 뮤텍스(뮤텍스 보유 = 서버 생존, 프로세스 사망 시 커널
    // 해제라 크래시 후 재기동 자유) ②파이프 프로브 벨트(가드 없는 구
    // 바이너리가 파이프를 이미 점유 중이어도 WaitNamedPipeA로 잡아낸다 —
    // 자기 인스턴스 생성 전 검사라 오탐 없음; ERROR_FILE_NOT_FOUND만 통과).
    // Init 이전 봉쇄(2026-09-20 실측): 가드가 Init 뒤에 있으면 거부 인스턴스가
    // 앱 설치+아이콘 로드를 전부 수행한 뒤 죽는다 — 낭비+로그 혼란.
    //
    // 접 처리(2026-09-24 사용자 지시 "직접"): 보유자가 jkwinserver.exe면
    // 거부 대신 직접 종료하고 인수한다(takeover 기본 ON — 서버 기동은 곧
    // "새 서버를 원한다"는 뜻). 단일 인스턴스 원칙은 유지: 죽이고 나서
    // 취득하므로 갈림은 생기지 않는다. 고아 클라(태스크바)는 자가 종료
    // (407af96)하므로 인수 후 새 태스크바와 일시 중복만 있고 정리된다.
    {
        std::string guard = pipeName;
        const size_t slash = guard.find_last_of("\\/");
        if (slash != std::string::npos) guard = guard.substr(slash + 1);
        guard = "Local\\jkdesktop-server-" + guard;

        auto refuse = [&](const char* why) {
            std::fprintf(stderr, "JKWindowServer: %s '%s' — close it first\n",
                         why, pipeName.c_str());
            const std::string hint =
                FindGuardHolderHint(ScanServerCandidates(GetCurrentProcessId()));
            if (!hint.empty()) std::fprintf(stderr, "  %s\n", hint.c_str());
        };

        void* m = CreateMutexA(nullptr, 1 /* TRUE: initial owner */, guard.c_str());
        if (!m) {
            std::fprintf(stderr,
                         "JKWindowServer: single-instance guard CreateMutex failed (%lu)\n",
                         GetLastError());
            return false;
        }
        if (GetLastError() == kErrorAlreadyExists) {
            CloseHandle(m);
            m = nullptr;
            ServerCandidateScan scan = ScanServerCandidates(GetCurrentProcessId());
            if (takeover && !scan.wserver.empty() && KillServerHolders(scan)) {
                std::fprintf(stderr,
                             "JKWindowServer: takeover — killed holder "
                             "jkwinserver.exe PID %s, re-acquiring guard\n",
                             JoinPids(scan.wserver).c_str());
                std::fflush(stderr);
                for (int i = 0; i < 12; ++i) {  // 커널 뮤텍스 해제 대기 최대 ~3s
                    Sleep(250);
                    m = CreateMutexA(nullptr, 1, guard.c_str());
                    if (m && GetLastError() != kErrorAlreadyExists) break;
                    if (m) { CloseHandle(m); m = nullptr; }
                }
            }
            if (!m) {
                refuse("another window server already holds the single-instance guard for");
                return false;
            }
        }
        if (WaitNamedPipeA(pipeName.c_str(), 50) ||
            GetLastError() == kErrorPipeBusy) {
            // 뮤텍스는 우리가 갖지만 파이프에 살아있는 인스턴스가 응답 —
            // 가드 이전 바이너리의 서버다. 두 인스턴스 갈림을 막기 위해 거부.
            ReleaseMutex(m);
            CloseHandle(m);
            m = nullptr;
            ServerCandidateScan scan = ScanServerCandidates(GetCurrentProcessId());
            if (takeover && !scan.wserver.empty() && KillServerHolders(scan)) {
                std::fprintf(stderr,
                             "JKWindowServer: takeover — killed pre-guard holder "
                             "jkwinserver.exe PID %s, waiting for pipe to clear\n",
                             JoinPids(scan.wserver).c_str());
                std::fflush(stderr);
                bool cleared = false;
                for (int i = 0; i < 12; ++i) {  // 파이프 인스턴스 소멸 대기 최대 ~3s
                    Sleep(250);
                    if (WaitNamedPipeA(pipeName.c_str(), 50)) continue;
                    if (GetLastError() == kErrorPipeBusy) continue;
                    cleared = true;  // ERROR_FILE_NOT_FOUND — 파이프 소멸
                    break;
                }
                if (cleared) {
                    m = CreateMutexA(nullptr, 1, guard.c_str());
                    if (m && GetLastError() == kErrorAlreadyExists) {
                        CloseHandle(m);
                        m = nullptr;
                    }
                }
            }
            if (!m) {
                refuse("a live pipe instance is already serving (single-instance guard) on");
                return false;
            }
        }
        serverGuardMutex_ = static_cast<void*>(m);
    }
#else
    (void)pipeName;
    (void)takeover;
#endif
    return true;
}

bool JKWindowServer::StartAcceptor(const std::string& pipeName) {
#ifdef _WIN32
    // 가드는 통상 TryAcquireSingleInstanceGuard가 Init 전에 취득 — 여기서는
    // 미취득 시에만 (직접 호출자 방어선) 취득을 시도한다.
    if (!serverGuardMutex_ && !TryAcquireSingleInstanceGuard(pipeName)) {
        return false;
    }
#endif
    pipeName_ = pipeName;
    InitAudio();
    running_ = true;
    acceptorThread_ = std::thread([this] { AcceptorLoop(); });

#ifdef _WIN32
    // Auto-spawn the shell (docs/28): the taskbar is a privileged client, not
    // an app — the server boots it itself when its module is installed next
    // to the exe. Clients have no connect-retry, so wait for the acceptor's
    // first pipe instance before spawning (bounded ~200 ms).
    for (int i = 0; i < 20; ++i) {
        if (WaitNamedPipeA(pipeName_.c_str(), 20)) break;
        Sleep(10);
    }
    char modulePath[1024] = {};
    const unsigned long len = GetModuleFileNameA(nullptr, modulePath, sizeof(modulePath));
    if (len > 0 && len < sizeof(modulePath)) {
        char* lastSlash = modulePath;
        for (char* p = modulePath; *p; ++p) {
            if (*p == '\\' || *p == '/') lastSlash = p;
        }
        *lastSlash = '\0';
        std::string dllPath = std::string(modulePath[0] ? modulePath : ".") + "\\jkapp_taskbar.dll";
        if (GetFileAttributesA(dllPath.c_str()) != kInvalidFileAttributes) {
            SpawnClient("taskbar");
        } else {
            std::fprintf(stderr, "JKWindowServer: no jkapp_taskbar.dll — desktop runs without a shell\n");
        }
    }
#endif
    return true;
}

void JKWindowServer::AcceptorLoop() {
    while (running_) {
        auto transport = ipc::JKPipeTransport::CreateServer(pipeName_);
        if (!transport) {
            if (!running_) break;
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: accept failed\n");
            continue;
        }
        if (!running_) break;

        // Expect Hello. Protocol v2 carries the client's OS pid; a v1 Hello
        // (4-byte payload) is accepted with pid = 0.
        ipc::Message hello;
        if (!ipc::ReadMessage(*transport, hello) || hello.type != ipc::MsgType::Hello) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: expected Hello, got type=%u\n",
                         static_cast<uint32_t>(hello.type));
            continue;
        }
        uint32_t helloPid = 0;
        if (hello.payload.size() >= sizeof(ipc::HelloPayload)) {
            ipc::HelloPayload helloPayload{};
            std::memcpy(&helloPayload, hello.payload.data(), sizeof(helloPayload));
            helloPid = helloPayload.pid;
        }

        // Second message: CreateSurface for a regular window client, or
        // AgentEventSubscribe for a control-only agent connection.
        ipc::Message second;
        if (!ipc::ReadMessage(*transport, second)) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: second message read failed\n");
            continue;
        }

        uint32_t id = nextSurfaceId_++;
        auto client = std::make_unique<JKClientConnection>(id, std::move(transport));
        client->SetPid(helloPid);

        if (second.type == ipc::MsgType::AgentEventSubscribe) {
            // Control-only agent connection (Desktop Agent API, spec §3):
            // skip the surface/shm handshake entirely — pipe-only. Queued
            // through pendingClients_ so registration happens on the main
            // thread like every other client.
            ipc::AgentEventSubscribePayload sub{};
            if (second.payload.size() >= sizeof(sub)) {
                std::memcpy(&sub, second.payload.data(), sizeof(sub));
                client->SetAgentEventSubscriber(sub.subscribe != 0);
            }
            client->SetControlOnly(true);
            client->StartReadThread();
            {
                std::lock_guard<std::mutex> lock(pendingClientsMutex_);
                pendingClients_.push_back(std::move(client));
            }
            std::fprintf(stderr, "JKWindowServer: control-only client %u connected\n", id);
            continue;
        }

        if (second.type != ipc::MsgType::CreateSurface ||
            second.payload.size() < sizeof(ipc::SurfaceCreatePayload)) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: expected CreateSurface\n");
            continue;
        }

        ipc::SurfaceCreatePayload create{};
        std::memcpy(&create, second.payload.data(), sizeof(create));

        if (!client->CreateSurface(create.width, create.height, create.title)) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: failed to create surface\n");
            continue;
        }

        ipc::SurfaceCreatedPayload created{};
        created.surfaceId = id;
        std::string shmName = std::string("Local\\JKSurfaceShm_") + std::to_string(id);
        std::strncpy(created.shmName, shmName.c_str(), sizeof(created.shmName) - 1);
        if (!client->Send(ipc::MsgType::SurfaceCreated, &created, sizeof(created))) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: failed to send SurfaceCreated\n");
            continue;
        }

        client->StartReadThread();

        {
            std::lock_guard<std::mutex> lock(pendingClientsMutex_);
            pendingClients_.push_back(std::move(client));
        }

        std::fprintf(stderr, "JKWindowServer: client surface %u created (%dx%d)\n",
                     id, create.width, create.height);
    }
}

void JKWindowServer::ProcessPendingClients() {
    std::vector<std::unique_ptr<JKClientConnection>> newClients;
    {
        std::lock_guard<std::mutex> lock(pendingClientsMutex_);
        newClients = std::move(pendingClients_);
        pendingClients_.clear();
    }

    int existingCount = 0;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        existingCount = static_cast<int>(clients_.size());
    }

    for (auto& client : newClients) {
        if (!client) continue;

        // Control-only clients have no layer: no placement, no focus, no
        // window-list entry — just join the client table.
        if (client->IsControlOnly()) {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.push_back(std::move(client));
            continue;
        }

        int ww = 0, wh = 0;
        SDL_GetWindowSize(window_, &ww, &wh);
        // Work-area reserve (docs/28): the shell's docked bar height keeps new
        // windows out of the taskbar zone. Computed per batch — the shell is
        // not yet layered when it is placed itself.
        const int reserve = compositor_ ? compositor_->ShellReserveHeight() : 0;
        // Surfaces larger than the desktop (apps designed for 1920x1080) are
        // displayed scaled down to fit; the client keeps rendering at its
        // designed surface size. Chrome zones are proportional to the layer
        // size, so title-drag, the close overlay and resize hotspots keep
        // working under a fit scale.
        const float fit = std::min(1.0f,
            std::min(ww / static_cast<float>(client->Width()),
                     (wh - reserve) / static_cast<float>(client->Height())));
        const int dispW = static_cast<int>(client->Width() * fit);
        const int dispH = static_cast<int>(client->Height() * fit);
        int x = std::max(0, (ww - dispW) / 2) + existingCount * 20;
        int y = std::max(0, (wh - reserve - dispH) / 2) + existingCount * 20;
        // A full-desktop fit layer (dispW == ww) would push its close-button
        // corner past the window edge with the cascade offset — clamp so the
        // whole layer, chrome included, stays inside the work area.
        x = std::min(x, std::max(0, ww - dispW));
        y = std::min(y, std::max(0, wh - reserve - dispH));
        client->SetPosition(x, y);
        ++existingCount;

        // Register the client surface with the compositor.
        auto* layer = compositor_->AddLayer(
            client->Id(),
            client->Width(),
            client->Height(),
            client->Title(),
            client->SurfaceData());
        if (!layer) {
            std::fprintf(stderr, "JKWindowServer: failed to add layer for surface %u\n",
                         client->Id());
            client->StopReadThread();
            continue;
        }
        compositor_->SetLayerPosition(client->Id(), x, y);
        if (fit < 1.0f) {
            compositor_->SetLayerScale(client->Id(), fit, fit);
        }
        // A newly spawned client takes keyboard focus unconditionally, like a
        // new desktop window: focusedClientId_ drives key/text/wheel routing
        // while FocusLayer only fixes z-order. Only calling FocusLayer here
        // left focusedClientId_ at 0, so keys were silently dropped until the
        // first click on the surface (tetris arrows appeared dead at spawn).
        // The shell never takes focus (docs/28) — keys stay with app windows.
        if (client->IsShell()) {
            // ShellRegister can beat this intake (client sends it right after
            // the CreateSurface handshake) — SetLayerShell was a no-op then,
            // so (re)apply the role here and dock for real; the centered
            // placement above is overwritten by the bottom-edge dock.
            compositor_->SetLayerShell(client->Id(), true);
            DockShellClient(client.get());
        } else if (client->Title() == kCaptureOverlayTitle) {
            // Consume the pending launch_app requester — this overlay now
            // knows which client to hide during its own capture_region.
            if (pendingSnapSpawnerConnId_ != 0) {
                overlaySpawner_[client->Id()] = pendingSnapSpawnerConnId_;
                pendingSnapSpawnerConnId_ = 0;
            }
            // docs/35: the rubber-band capture overlay always covers the
            // whole desktop, taskbar included — it is momentary (dismissed
            // by mouse-up or ESC) so it neither reserves work area nor keeps
            // its meta size. The meta size is a placeholder: the same
            // ResizeSurface round-trip DockShellClient uses dictates the
            // real (output) size, and the layer sits at (0,0) with scale 1
            // so the client's drag coords are desktop logical coords.
            CommitChromeResize(*client, client->Id(), ww, wh, ww, wh);
            compositor_->SetLayerPosition(client->Id(), 0, 0);
            client->SetPosition(0, 0);
            FocusClient(client->Id());
        } else {
            FocusClient(client->Id());
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            const std::string createdTitle = client->Title();
            const uint32_t createdPid = client->Pid();
            const uint32_t createdId = client->Id();
            clients_.push_back(std::move(client));
            PushAgentEvent("window.created", createdId, createdTitle, createdPid);
        }

        // Shell protocol: the new window shows up in the taskbar.
        PushWindowList();
    }
}

void JKWindowServer::Run() {
    if (!renderer_) return;
    running_ = true;

    // 한/영 토글키 저수준 훅(docs/61 §16.1): OS IME가 VK_HANGUL을 삼켜 앱에
    // 키 이벤트가 도달하지 않고 신식 IME는 IMM 변환 플래그도 갱신하지 않는다
    // — WH_KEYBOARD_LL만이 IME 이전의 원시 키를 본다. 훅은 이 스레드의 메시지
    // 펌프(SDL_PollEvent)에서 발화해 SDL 사용자 이벤트로 되돌아온다.
    JkInstallImeKeyHook(window_);

    while (running_) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running_ = false;
                break;
            }
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                running_ = false;
                break;
            }
            if (ev.type == JkImeToggleEventType()) {
                JKClientConnection* client = FindClientById(focusedClientId_);
                // 라이브 진단 증거(docs/61 §19): 훅 발화+전달 대상을 로그에 남긴다.
                std::fprintf(stderr, "[ime] toggle -> client %u (%s)\n",
                             focusedClientId_, client ? "sent" : "no-focus-client");
                if (client) {
                    ipc::InputEventPayload payload{};
                    payload.surfaceId = client->Id();
                    payload.type      = ipc::InputEventType::ImeToggle;
                    SendInputEvent(*client, payload);
                }
                continue;
            }
            HandleSDLEvent(ev);
        }
        if (!running_) break;

        // 한/영 전환키는 OS IME가 삼켜 SDL에 도달하지 않는다(docs/61 §16
        // 실측 — VK_HANGUL에 KEYDOWN/KEYUP이 없다). 대신 변환 상태를 주기
        // 폴링해 변화를 포커스 클라에 브로드캐스트한다. 첫 관측은 기준값
        // 으로만 쓴다(기동 시점 노이즈 브로드캐스트 방지).
        {
            uint32_t now = SDL_GetTicks();
            if (now - lastImePoll_ >= 300) {
                lastImePoll_ = now;
                int mode = static_cast<int>(jk::JKPlatform::GetCurrentConversionMode(window_));
                if (mode != static_cast<int>(jk::JKPlatform::ImeMode::Unknown)) {
                    if (lastImeMode_ != -1 && mode != lastImeMode_) {
                        JKClientConnection* client = FindClientById(focusedClientId_);
                        if (client) {
                            ipc::InputEventPayload payload{};
                            payload.surfaceId = client->Id();
                            payload.type      = ipc::InputEventType::ImeChanged;
                            payload.option    = static_cast<uint32_t>(mode);
                            SendInputEvent(*client, payload);
                        }
                    }
                    lastImeMode_ = mode;
                }
            }
        }

        ProcessPendingClients();
        ProcessPendingMessages();
        Composite();
        CleanupDisconnectedClients();

        SDL_Delay(1);
    }
}

void JKWindowServer::Stop() {
    running_ = false;

    JkUninstallImeKeyHook();

    UnblockAcceptor();

    if (acceptorThread_.joinable()) {
        acceptorThread_.join();
    }

    if (audioThread_) {
        AudioCommand quitCmd{};
        quitCmd.type = AudioCommand::Type::Quit;
        PostAudioCommand(quitCmd);
        (*audioThread_).Stop();
        audioThread_.reset();
    }
    messageBus_.reset();

    // Move clients out of the locked vectors before joining their read threads
    // to avoid holding clientsMutex_/pendingClientsMutex_ during a potentially
    // blocking join and to prevent deadlocks if a read thread tries to queue a
    // message during shutdown.
    {
        std::vector<std::unique_ptr<JKClientConnection>> clientsToStop;
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clientsToStop = std::move(clients_);
            clients_.clear();
        }
        for (auto& client : clientsToStop) {
            if (client) client->StopReadThread();
        }
    }

    {
        std::vector<std::unique_ptr<JKClientConnection>> pendingToStop;
        {
            std::lock_guard<std::mutex> lock(pendingClientsMutex_);
            pendingToStop = std::move(pendingClients_);
            pendingClients_.clear();
        }
        for (auto& client : pendingToStop) {
            if (client) client->StopReadThread();
        }
    }

    pendingCleanup_.clear();

    if (shell_) {
        shell_->Destroy();
        shell_.reset();
    }
    // 승인 배너 텍스처 캐시 폐기 (스펙 2026-09-19-app-tool-hub §5 1단) —
    // renderer_가 살아 있을 때 SDL_DestroyTexture해야 한다(소유 순서: 컴포지터
    // 텍스처와 동일 — renderer 소멸 전).
    for (auto& kv : approvalBannerTexs_) {
        if (kv.second.tex) {
            SDL_DestroyTexture(kv.second.tex);
        }
    }
    approvalBannerTexs_.clear();
    // 배너 벡터 글리프 캐시 (docs/63 Task 6): 승인 배너 텍스처와 동일 소유
    // 순서 — renderer_가 살아 있을 때 글리프 텍스처를 회수한다(영구
    // bannerBackend_가 플러시 주체 — 캐시 소멸은 그 뒤).
    if (bannerCache_ && bannerBackend_) {
        bannerCache_->UnloadAllImages();
        bannerCache_->FlushUploads(bannerBackend_.get());
    }
    bannerAtlas_.reset();
    bannerCache_.reset();
    bannerBackend_.reset();
    approvalFont_.reset();
    compositor_.reset();

    for (SDL_Cursor* cursor : chromeCursors_) {
        if (cursor) SDL_FreeCursor(cursor);
    }

    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void JKWindowServer::InitAudio() {
    messageBus_ = std::make_unique<JKMessageBus>();
    audioThread_ = std::make_unique<JKAudioThread>();
    (*audioThread_).Start(messageBus_.get(), std::make_unique<SDLAudioBackend>());

    AudioCommand initCmd{};
    initCmd.type = AudioCommand::Type::Init;
    PostAudioCommand(initCmd);
}

namespace {

std::string ResolveAudioPath(const char* id, AudioCommand::Type type) {
    const char* ext = (type == AudioCommand::Type::LoadBGM) ? ".wav" : ".wav";
    return JKSoundManager::AssetPath(std::string(id) + ext);
}

// Minimal JSON string escape for agent replies (quotes, backslash, control
// chars). UTF-8 bytes pass through untouched — titles are KSSM-decoded
// UTF-8 already (taskbar convention).
std::string JsonEsc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    char num[8];
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '{')  out += "\\u007b";   // 행 경계 스캔(노트 허브 등
        else if (c == '}')  out += "\\u007d";   // 원문 수술)이 안전하도록
        else if (c < 0x20)  { std::snprintf(num, sizeof(num), "\\u%04x", c); out += num; }
        else                out += ch;
    }
    return out;
}

// PNG write for screenshots (docs/35). pixels must be RGBA32, w*h*4 bytes —
// the same layout the client surfaces use in their shm mapping.
bool WritePng(const std::string& path, int w, int h, const uint8_t* px) {
    return stbi_write_png(path.c_str(), w, h, 4, px, w * 4) != 0;
}

} // anonymous namespace

// capture_window 본문 추출 (스펙 2026-09-19-app-tool-hub §5 3단): docs/35의
// 레이어 readback→stb_image_write 경로를 app_tool 파킹 썸네일이 재사용한다.
// 레이어 id = 연결 id — 창 클라의 windowId와 같다(HandleToolRegister가
// windowId = client.Id()로 등록). pixels를 복사해 인코딩하는 이유도 원본
// 그대로 — 클라가 인코딩 중에 shm에 커밋할 수 있다.
bool JKWindowServer::CaptureLayerToPng(uint32_t connId, const std::string& path) {
    JKCompositorLayer* layer =
        compositor_ ? compositor_->FindLayerById(connId) : nullptr;
    if (!layer || !layer->Pixels() || layer->Width() <= 0 ||
        layer->Height() <= 0) {
        return false;
    }
    const int w = layer->Width(), h = layer->Height();
    std::vector<uint8_t> px(layer->Pixels(),
                            layer->Pixels() +
                                static_cast<size_t>(w) * h * 4);
    return WritePng(path, w, h, px.data());
}

void JKWindowServer::PostAudioCommand(const AudioCommand& cmd) {
    if (!messageBus_) return;
    std::vector<uint8_t> data(sizeof(AudioCommand));
    std::memcpy(data.data(), &cmd, sizeof(AudioCommand));
    (*messageBus_).Push(JKMessageBus::Channel::Audio,
        JKMessageBus::Payload(static_cast<uint32_t>(cmd.type), std::move(data)));
}

void JKWindowServer::UnblockAcceptor() {
    if (pipeName_.empty()) return;
    // The acceptor thread blocks in ConnectNamedPipe. Open a short-lived
    // client connection so it unblocks and notices running_ == false.
    for (int i = 0; i < 50; ++i) {
        auto poison = ipc::JKPipeTransport::ConnectClient(pipeName_);
        if (poison) {
            poison->Close();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::fprintf(stderr, "JKWindowServer::UnblockAcceptor: failed to unblock acceptor\n");
}

bool JKWindowServer::HandleChromeGrab(const SDL_Event& ev, int mx, int my, float scale) {
    if (chromeGrab_ == ChromeGrab::None) {
        return false;
    }
    JKClientConnection* client = FindClientById(chromeGrabClient_);
    JKCompositorLayer* layer =
        compositor_ ? compositor_->FindLayerById(chromeGrabLayerId_) : nullptr;
    if (!client || !layer) {
        // The grabbed layer vanished (client disconnected during drag).
        chromeGrab_ = ChromeGrab::None;
        chromeGrabClient_ = 0;
        chromeGrabLayerId_ = 0;
        chromeRestorePendingId_ = 0;
        return true;
    }

    const int lmX = static_cast<int>(std::llround(mx / scale));
    const int lmY = static_cast<int>(std::llround(my / scale));

    if (ev.type == SDL_MOUSEBUTTONDOWN) {
        return true;  // other buttons during a drag are consumed
    }

    if (ev.type == SDL_MOUSEMOTION) {
        // Deferred drag-restore (docs/39 fix 1): the first motion of a grab
        // that was started on a maximized layer restores it (single
        // window.restored event), then the grab anchors are re-derived from
        // the restored geometry before the normal logic below runs.
        if (chromeRestorePendingId_ != 0) {
            // Drag threshold (docs/39 final review): restore only once the
            // accumulated motion from grab start exceeds kResizeHotspot (the
            // edge-hotspot 6px). A real hand's double-click has ~1px jitter
            // between its clicks — that must NOT trigger the restore, or the
            // second click (clicks==2) would re-maximize the now-normal
            // window. Below threshold the motion is consumed and ignored.
            const int adx = lmX > chromeGrabStartX_
                                ? lmX - chromeGrabStartX_
                                : chromeGrabStartX_ - lmX;
            const int ady = lmY > chromeGrabStartY_
                                ? lmY - chromeGrabStartY_
                                : chromeGrabStartY_ - lmY;
            if (std::max(adx, ady) <= kResizeHotspot) {
                return true;
            }
            if (chromeRestorePendingId_ == layer->Id() &&
                preMaxRects_.count(chromeRestorePendingId_) != 0) {
                RestoreFromMaximize(*client, *layer);
                chromeRestorePendingId_ = 0;
                const int dispW = static_cast<int>(std::llround(
                    layer->Width() * layer->ScaleX()));
                const int dispH = static_cast<int>(std::llround(
                    layer->Height() * layer->ScaleY()));
                if (chromeGrab_ == ChromeGrab::Move) {
                    // Put the restored window under the cursor: keep the
                    // fractional grab point inside the window.
                    chromeGrabDX_ = static_cast<int>(std::llround(
                        chromeGrabFX_ * dispW));
                    chromeGrabDY_ = static_cast<int>(std::llround(
                        chromeGrabFY_ * dispH));
                } else {  // Resize: re-evaluate the hotspots on the restored
                          // rect — the maximized-geometry edges are stale.
                    const int dx = lmX - layer->X();
                    const int dy = lmY - layer->Y();
                    const bool eLeft = (dx < kResizeHotspot);
                    const bool eRight = (dx >= dispW - kResizeHotspot);
                    const bool eTop = (dy < kResizeHotspot);
                    const bool eBottom = (dy >= dispH - kResizeHotspot);
                    if (!(eLeft || eRight || eTop || eBottom)) {
                        // The click no longer sits on any resize edge after
                        // restore — cancel the grab, window stays restored
                        // and grab-free.
                        chromeGrab_ = ChromeGrab::None;
                        chromeGrabClient_ = 0;
                        chromeGrabLayerId_ = 0;
                        chromeRestorePendingId_ = 0;
                        SetChromeCursor(CursorShape::Arrow);
                        return true;
                    }
                    chromeEdgeLeft_ = eLeft;
                    chromeEdgeRight_ = eRight;
                    chromeEdgeTop_ = eTop;
                    chromeEdgeBottom_ = eBottom;
                    chromeResizeX_ = layer->X();
                    chromeResizeY_ = layer->Y();
                    chromeResizeW_ = dispW;
                    chromeResizeH_ = dispH;
                    SetChromeCursor(ChromeCursorFromEdges(
                        eLeft, eRight, eTop, eBottom));
                }
            } else {
                // Stale pending id (grab state mismatch) — drop it.
                chromeRestorePendingId_ = 0;
            }
        }
        if (chromeGrab_ == ChromeGrab::Move) {
            int winW = 0, winH = 0;
            SDL_GetWindowSize(window_, &winW, &winH);
            // Display size in logical points (a fit-scaled layer is smaller
            // than its surface size).
            const int lw = static_cast<int>(std::llround(
                layer->Width() * layer->ScaleX()));
            const int lh = static_cast<int>(std::llround(
                layer->Height() * layer->ScaleY()));
            int nx = lmX - chromeGrabDX_;
            int ny = lmY - chromeGrabDY_;
            // Keep the dragged window inside the work area: the shell's
            // docked bar stays visible under it (docs/28).
            const int reserve = compositor_ ? compositor_->ShellReserveHeight() : 0;
            nx = std::max(-lw + 40, std::min(nx, std::max(0, winW - 40)));
            ny = std::max(0, std::min(ny, std::max(0, winH - reserve - 40)));
            client->SetPosition(nx, ny);
            compositor_->SetLayerPosition(client->Id(), nx, ny);
        } else {  // Resize: stretch-preview via layer scale.
            int newW = chromeResizeW_;
            int newH = chromeResizeH_;
            int newX = chromeResizeX_;
            int newY = chromeResizeY_;
            if (chromeEdgeRight_) newW = lmX - chromeResizeX_;
            if (chromeEdgeBottom_) newH = lmY - chromeResizeY_;
            if (chromeEdgeLeft_) {
                newW = chromeResizeW_ + (chromeResizeX_ - lmX);
            }
            if (chromeEdgeTop_) {
                newH = chromeResizeH_ + (chromeResizeY_ - lmY);
                // The grab-time bottom edge is fixed — growing past it would
                // push the layer origin above the desktop.
                newH = std::min(newH, chromeResizeY_ + chromeResizeH_);
            }
            newW = std::max(64, newW);
            newH = std::max(48, newH);
            // Absolute display scale = display target / surface width (the
            // surface size does not change until the resize is committed).
            layer->SetScale(newW / static_cast<float>(layer->Width()),
                            newH / static_cast<float>(layer->Height()));
            if (chromeEdgeLeft_ || chromeEdgeTop_) {
                // The fixed (opposite) edge stays put: right edge for a left
                // resize, bottom edge for a top resize.
                if (chromeEdgeLeft_) {
                    newX = chromeResizeX_ + chromeResizeW_ - newW;
                }
                if (chromeEdgeTop_) {
                    newY = chromeResizeY_ + chromeResizeH_ - newH;
                }
                client->SetPosition(newX, newY);
                compositor_->SetLayerPosition(client->Id(), newX, newY);
            }
        }
        return true;
    }

    if (ev.type == SDL_MOUSEBUTTONUP) {
        if (chromeGrab_ == ChromeGrab::Resize) {
            // Recompute the final logical size at the release point.
            int newW = chromeResizeW_;
            int newH = chromeResizeH_;
            int newX = chromeResizeX_;
            int newY = chromeResizeY_;
            if (chromeEdgeRight_) newW = lmX - chromeResizeX_;
            if (chromeEdgeBottom_) newH = lmY - chromeResizeY_;
            if (chromeEdgeLeft_) {
                newW = chromeResizeW_ + (chromeResizeX_ - lmX);
            }
            if (chromeEdgeTop_) {
                newH = chromeResizeH_ + (chromeResizeY_ - lmY);
                newH = std::min(newH, chromeResizeY_ + chromeResizeH_);
            }
            newW = std::max(64, newW);
            newH = std::max(48, newH);
            if (newW != chromeResizeW_ || newH != chromeResizeH_) {
                if (chromeEdgeLeft_ || chromeEdgeTop_) {
                    if (chromeEdgeLeft_) {
                        newX = chromeResizeX_ + chromeResizeW_ - newW;
                    }
                    if (chromeEdgeTop_) {
                        newY = chromeResizeY_ + chromeResizeH_ - newH;
                    }
                    client->SetPosition(newX, newY);
                    compositor_->SetLayerPosition(client->Id(), newX, newY);
                }
                // Keep the grab-time surface:display ratio so a fit-scaled
                // surface resizes without cutting off its layout (1:1 layers
                // get surface == display, unchanged).
                const float fitX = static_cast<float>(layer->Width()) / std::max(1, chromeResizeW_);
                const float fitY = static_cast<float>(layer->Height()) / std::max(1, chromeResizeH_);
                const int commitW = std::max(64,
                    static_cast<int>(std::llround(newW / fitX)));
                const int commitH = std::max(48,
                    static_cast<int>(std::llround(newH / fitY)));
                CommitChromeResize(*client, layer->Id(), commitW, commitH, newW, newH);
            }
        }
        chromeGrab_ = ChromeGrab::None;
        chromeGrabClient_ = 0;
        chromeGrabLayerId_ = 0;
        // A click without motion never restores (Windows parity): drop the
        // armed restore so a later unrelated grab cannot fire it.
        chromeRestorePendingId_ = 0;
        return true;
    }

    return true;
}

bool JKWindowServer::TryChromeGrab(int mx, int my, float scale, int clicks) {
    if (!compositor_) {
        return false;
    }
    JKCompositorLayer* layer = compositor_->HitTest(mx, my);
    if (!layer) {
        return false;
    }
    JKClientConnection* client = FindClientById(layer->Id());
    if (!client) {
        return false;
    }
    // The shell has no window chrome (docs/28): no close X, no title drag,
    // no resize edges — clicks fall through to the shell's own UI. The
    // capture overlay (docs/35) likewise: a title-bar grab over its top
    // strip would swallow the first 24 px of the rubber band.
    if (client->IsShell() || client->Title() == kCaptureOverlayTitle) {
        return false;
    }

    // 전체화면 레이어(vplayer 스펙 §2.1)는 크롬이 없다 — 상단 24pt 포함 모든
    // 클릭이 앱에 도달한다. 클릭 포커스는 1241행 일반 경로라 살아 있다.
    if (layer->IsFullscreen()) {
        return false;
    }

    // Chrome zones are in SURFACE-local px (they shrink proportionally on
    // fit-scaled layers, §7.3), so convert display px → surface px here.
    // For 1:1 layers ScaleX/Y == 1 and this is the plain logical-local map.
    // The deferred drag-restore (zone 1d) keeps maximized geometry here; the
    // grab anchors are re-derived from the restored layer on the first motion.
    const int lx = static_cast<int>(std::llround(
        (mx / scale - layer->X()) / layer->ScaleX()));
    const int ly = static_cast<int>(std::llround(
        (my / scale - layer->Y()) / layer->ScaleY()));
    const int w = layer->Width();
    const int h = layer->Height();

    // 1) Close overlay (top-right of the title bar, server-drawn).
    const bool inCloseX = (lx >= w - kChromeCloseSize - kChromeCloseMargin) &&
                          (lx < w - kChromeCloseMargin);
    const bool inCloseY = (ly >= kChromeCloseMargin) &&
                          (ly < kChromeCloseMargin + kChromeCloseSize);
    if (inCloseX && inCloseY) {
        FocusClient(client->Id());
        PushWindowList();  // active highlight follows click focus
        client->Send(ipc::MsgType::Close, nullptr, 0);
        return true;
    }

    // 1b) Maximize/restore button (left of the close X, server-drawn — docs/39).
    const int maxBtnX0 = w - kChromeCloseMargin - kChromeCloseSize -
                         kChromeMaximizeGap - kChromeMaximizeSize;
    const bool inMaxX = (lx >= maxBtnX0) && (lx < maxBtnX0 + kChromeMaximizeSize);
    if (inMaxX && inCloseY) {
        FocusClient(client->Id());
        PushWindowList();  // active highlight follows click focus
        ToggleMaximize(*client, *layer);
        return true;
    }

    // 1c) Title double-click toggles maximize/restore (docs/39) — checked
    // before the deferred restore is armed in zone 1d, so a maximized
    // window's double-click (second click, still maximized) toggles exactly
    // ONCE, Windows-like. The top resize strip stays a resize zone (a move
    // grab never starts there either).
    if (ly >= kResizeHotspot && ly < kChromeTitleBar && clicks == 2) {
        FocusClient(client->Id());
        PushWindowList();  // active highlight follows click focus
        ToggleMaximize(*client, *layer);
        return true;
    }

    // 1d) Deferred drag-restore (docs/39 fix 1): a Move or Resize grab on a
    // maximized layer only ARMS the restore (chromeRestorePendingId_) — the
    // restore fires on the grab's FIRST motion (HandleChromeGrab), so a plain
    // double-click still reaches zone 1c while maximized and toggles exactly
    // once. Anchors below use the maximized geometry; they are discarded and
    // re-derived from the restored layer on the first motion.
    const bool grabMaximized = (preMaxRects_.count(layer->Id()) != 0);

    // 2) Resize edges: all four sides + corners (6px inset). The top strip's
    //    first 6px are resize; title drag starts below that (Windows-like).
    const bool edgeLeft = (lx < kResizeHotspot);
    const bool edgeRight = (lx >= w - kResizeHotspot);
    const bool edgeBottom = (ly >= h - kResizeHotspot);
    const bool edgeTop = (ly < kResizeHotspot);
    if (edgeLeft || edgeRight || edgeBottom || edgeTop) {
        FocusClient(client->Id());
        PushWindowList();  // active highlight follows click focus
        capturedClientId_ = 0;
        chromeGrab_ = ChromeGrab::Resize;
        chromeGrabClient_ = client->Id();
        chromeGrabLayerId_ = layer->Id();
        // Deferred drag-restore (docs/39 fix 1): re-armed per grab.
        chromeRestorePendingId_ = grabMaximized ? layer->Id() : 0;
        chromeGrabStartX_ = static_cast<int>(std::llround(mx / scale));
        chromeGrabStartY_ = static_cast<int>(std::llround(my / scale));
        chromeResizeX_ = layer->X();
        chromeResizeY_ = layer->Y();
        // Display size at grab time (logical points) — the resize drag and
        // its commit threshold work in display space.
        chromeResizeW_ = static_cast<int>(std::llround(w * layer->ScaleX()));
        chromeResizeH_ = static_cast<int>(std::llround(h * layer->ScaleY()));
        chromeEdgeLeft_ = edgeLeft;
        chromeEdgeRight_ = edgeRight;
        chromeEdgeBottom_ = edgeBottom;
        chromeEdgeTop_ = edgeTop;
        SetChromeCursor(ChromeCursorFromEdges(edgeLeft, edgeRight, edgeTop, edgeBottom));
        return true;
    }

    // 3) Title bar: start a move grab (y ∈ [kResizeHotspot, kChromeTitleBar) —
    //    the top resize strip above returned first).
    if (ly < kChromeTitleBar) {
        FocusClient(client->Id());
        PushWindowList();  // active highlight follows click focus
        capturedClientId_ = 0;
        chromeGrab_ = ChromeGrab::Move;
        chromeGrabClient_ = client->Id();
        chromeGrabLayerId_ = layer->Id();
        // Deferred drag-restore (docs/39 fix 1): re-armed per grab.
        chromeRestorePendingId_ = grabMaximized ? layer->Id() : 0;
        chromeGrabStartX_ = static_cast<int>(std::llround(mx / scale));
        chromeGrabStartY_ = static_cast<int>(std::llround(my / scale));
        // Move works in desktop-logical positions, but lx/ly are surface-local
        // and a fit-scaled layer maps surface px to logical px at ScaleX/Y.
        chromeGrabDX_ = static_cast<int>(std::llround(lx * layer->ScaleX()));
        chromeGrabDY_ = static_cast<int>(std::llround(ly * layer->ScaleY()));
        // Fractional grab point inside the surface — the anchor a deferred
        // drag-restore re-uses to put the restored window under the cursor.
        chromeGrabFX_ = (w > 0) ? (lx / static_cast<float>(w)) : 0.0f;
        chromeGrabFY_ = (h > 0) ? (ly / static_cast<float>(h)) : 0.0f;
        return true;
    }
    return false;
}

void JKWindowServer::CommitChromeResize(JKClientConnection& client, uint32_t layerId,
                                        int width, int height, int dispW, int dispH) {
    // Order matters: (1) create the new shared memory generation and retire
    // the old one, (2) atomically swap the layer texture/pixels to the new
    // size (a layer with the new width and the old pixel buffer would make
    // SDL_UpdateTexture read out of bounds), (3) only then notify the client.
    ipc::SurfaceResizePayload payload{};
    if (!client.BeginResizeSurface(width, height, payload)) {
        return;
    }
    if (!compositor_ || !compositor_->ResizeLayer(layerId, width, height,
                                                  client.SurfaceData())) {
        return;
    }
    // Restore the display size the drag asked for (ResizeLayer resets the
    // layer scale to 1; a fit-scaled surface needs its scale back).
    if (dispW != width || dispH != height) {
        compositor_->SetLayerScale(layerId,
                                   dispW / static_cast<float>(width),
                                   dispH / static_cast<float>(height));
    }
    client.Send(ipc::MsgType::ResizeSurface, &payload, sizeof(payload));
}

// docs/39: window.maximized / window.restored envelope — id/title at top
// level like the PushAgentEvent sites (window.created / window.destroyed),
// minus pid (no process change). Emitted from the chrome path on the server
// loop thread: the same locking regime as FocusClient's PushAgentEvent call
// (clientsMutex_ is NOT held here, matching that existing call site).
void JKWindowServer::PushMaximizeEvent(const char* topic, JKClientConnection& client) {
    char buf[640];
    std::snprintf(buf, sizeof(buf),
                  "{\"topic\":\"%s\",\"id\":%u,\"title\":\"%s\",\"ts\":%lld}",
                  topic, client.Id(), JsonEsc(client.Title()).c_str(),
                  static_cast<long long>(std::time(nullptr)) * 1000);
    PushAgentEventJson(buf);
}

// docs/39: chrome maximize/restore toggle (button click or title
// double-click). The state map preMaxRects_ is the single source of truth:
// presence = maximized; JKCompositorLayer::SetMaximized mirrors it only so
// the compositor draw path can pick the button glyph.
void JKWindowServer::ToggleMaximize(JKClientConnection& client, JKCompositorLayer& layer) {
    if (preMaxRects_.count(layer.Id()) != 0) {
        RestoreFromMaximize(client, layer);
        return;
    }
    if (!compositor_ || !window_) {
        return;
    }
    // Capture the pre-maximize rect BEFORE the resize below replaces the
    // surface: layer origin, surface size, and the on-screen display size
    // (a fit-scaled layer shows a shrunk surface).
    MaxState saved;
    saved.x = layer.X();
    saved.y = layer.Y();
    saved.surfW = layer.Width();
    saved.surfH = layer.Height();
    saved.dispW = static_cast<int>(std::llround(saved.surfW * layer.ScaleX()));
    saved.dispH = static_cast<int>(std::llround(saved.surfH * layer.ScaleY()));

    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    // Work-area reserve (docs/28): a maximized window must not cover the
    // taskbar.
    const int reserve = compositor_->ShellReserveHeight();
    // Grow the surface to the whole work area via the same machinery a
    // resize drag uses (CommitChromeResize): shared-memory remap + layer
    // texture swap. dispW/dispH equal the new surface size, so the layer
    // scale resets to 1 — the window is drawn 1:1 across the work area.
    CommitChromeResize(client, layer.Id(), ww, wh - reserve, ww, wh - reserve);
    preMaxRects_[layer.Id()] = saved;
    compositor_->SetLayerPosition(layer.Id(), 0, 0);
    // Keep the connection-side position in sync: the input mapping reads
    // client->X()/Y() while drawing reads the compositor layer (see
    // DockShellClient for the same pairing).
    client.SetPosition(0, 0);
    layer.SetMaximized(true);
    PushMaximizeEvent("window.maximized", client);
}

// Shared restore core: pre-maximize rect back, map entry erased, flag
// cleared, window.restored published exactly once. Also used as the
// drag-restore step before a Move/Resize grab starts on a maximized layer.
bool JKWindowServer::RestoreFromMaximize(JKClientConnection& client,
                                         JKCompositorLayer& layer) {
    auto it = preMaxRects_.find(layer.Id());
    if (it == preMaxRects_.end()) {
        return false;  // not maximized
    }
    const MaxState saved = it->second;
    preMaxRects_.erase(it);
    // Shrink the surface back to the pre-maximize size (CommitChromeResize
    // restores the saved fit via dispW/dispH) and put the layer back where
    // it was, both on the compositor side and the connection side.
    CommitChromeResize(client, layer.Id(), saved.surfW, saved.surfH,
                       saved.dispW, saved.dispH);
    compositor_->SetLayerPosition(layer.Id(), saved.x, saved.y);
    client.SetPosition(saved.x, saved.y);
    layer.SetMaximized(false);
    PushMaximizeEvent("window.restored", client);
    return true;
}

// vplayer 전체화면(스펙 2026-09-17 vplayer-fullscreen-osd §2.1): maximize
// 형제. 최대화 중이면 먼저 복원에서 출발(Windows 관례)하고, 전체 출력 크기로
// CommitChromeResize — 작업 영역 예약 없음(전체화면은 작업표시줄을 덮는다,
// 표준). 크롬 스킵은 layer.SetFullscreen 거울 + TryChromeGrab / Composite /
// UpdateChromeHoverCursor의 플래그 검사 3곳.
void JKWindowServer::ToggleFullscreen(JKClientConnection& client,
                                      JKCompositorLayer& layer, bool on) {
    if (!on) {
        RestoreFullscreen(client, layer);
        return;
    }
    if (preFsRects_.count(layer.Id()) != 0) {
        return;  // already fullscreen — idempotent, no duplicate event
    }
    if (!compositor_ || !window_) {
        return;
    }
    // 최대화 중이면 먼저 정상 크기로 복원한 뒤 출발(Windows 관례 — 최대화
    // →전체화면 토글은 복원 rect에서 시작). window.restored 이벤트는
    // RestoreFromMaximize가 정직 발행한다.
    if (preMaxRects_.count(layer.Id()) != 0) {
        RestoreFromMaximize(client, layer);
    }
    MaxState saved;
    saved.x = layer.X();
    saved.y = layer.Y();
    saved.surfW = layer.Width();
    saved.surfH = layer.Height();
    saved.dispW = static_cast<int>(std::llround(saved.surfW * layer.ScaleX()));
    saved.dispH = static_cast<int>(std::llround(saved.surfH * layer.ScaleY()));
    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    CommitChromeResize(client, layer.Id(), ww, wh, ww, wh);
    preFsRects_[layer.Id()] = saved;
    compositor_->SetLayerPosition(layer.Id(), 0, 0);
    client.SetPosition(0, 0);
    layer.SetFullscreen(true);
    PushMaximizeEvent("window.fullscreen", client);
}

// 전체화면 복원: RestoreFromMaximize의 대응물 — 저장 rect 복원 + 이벤트 1회.
// 데스크탑 리사이즈 재발행은 이 함수를 경유하지 않는다(재발행은 이벤트
// 없음, 저장 rect는 무효화하지 않음 — maximize와 동일 규약).
bool JKWindowServer::RestoreFullscreen(JKClientConnection& client,
                                       JKCompositorLayer& layer) {
    auto it = preFsRects_.find(layer.Id());
    if (it == preFsRects_.end()) {
        return false;  // not fullscreen
    }
    const MaxState saved = it->second;
    preFsRects_.erase(it);
    CommitChromeResize(client, layer.Id(), saved.surfW, saved.surfH,
                       saved.dispW, saved.dispH);
    compositor_->SetLayerPosition(layer.Id(), saved.x, saved.y);
    client.SetPosition(saved.x, saved.y);
    layer.SetFullscreen(false);
    PushMaximizeEvent("window.fullscreen_exit", client);
    return true;
}

JKWindowServer::CursorShape JKWindowServer::ChromeCursorFromEdges(bool left, bool right,
                                                                  bool top, bool bottom) {
    const bool horiz = left || right;
    const bool vert = top || bottom;
    if (horiz && vert) {
        // TL/BR share one diagonal, TR/BL the other.
        return (left == top) ? CursorShape::SizeNWSE : CursorShape::SizeNESW;
    }
    if (horiz) return CursorShape::SizeWE;
    if (vert) return CursorShape::SizeNS;
    return CursorShape::Arrow;
}

void JKWindowServer::SetChromeCursor(CursorShape shape) {
    if (shape == cursorShape_) {
        return;
    }
    SDL_Cursor* cursor = chromeCursors_[static_cast<int>(shape)];
    if (cursor) {
        SDL_SetCursor(cursor);
        cursorShape_ = shape;
    }
}

void JKWindowServer::UpdateChromeHoverCursor(int mx, int my, float scale) {
    CursorShape shape = CursorShape::Arrow;
    if (compositor_ && chromeGrab_ == ChromeGrab::None) {
        JKCompositorLayer* layer = compositor_->HitTest(mx, my);
        JKClientConnection* client = layer ? FindClientById(layer->Id()) : nullptr;
        // 전체화면 레이어는 리사이즈 핫스팟이 없다(스펙 §2.1) — 화살표 고정.
        if (layer && layer->IsFullscreen()) {
            SetChromeCursor(CursorShape::Arrow);
            return;
        }
        // The shell has no chrome — its own UI keeps the arrow (docs/28).
        if (layer && client && !client->IsShell()) {
            const int lx = static_cast<int>(std::llround(
                (mx / scale - layer->X()) / layer->ScaleX()));
            const int ly = static_cast<int>(std::llround(
                (my / scale - layer->Y()) / layer->ScaleY()));
            const int w = layer->Width();
            const int h = layer->Height();
            // The close overlay and the maximize/restore button (docs/39)
            // stay a plain arrow even though their corner overlaps the top
            // resize strip.
            const bool inCloseX = (lx >= w - kChromeCloseSize - kChromeCloseMargin) &&
                                  (lx < w - kChromeCloseMargin);
            const bool inCloseY = (ly >= kChromeCloseMargin) &&
                                  (ly < kChromeCloseMargin + kChromeCloseSize);
            const int maxBtnX0 = w - kChromeCloseMargin - kChromeCloseSize -
                                 kChromeMaximizeGap - kChromeMaximizeSize;
            const bool inMaxX = (lx >= maxBtnX0) &&
                                (lx < maxBtnX0 + kChromeMaximizeSize);
            if (!(inCloseX && inCloseY) && !(inMaxX && inCloseY)) {
                shape = ChromeCursorFromEdges(lx < kResizeHotspot,
                                              lx >= w - kResizeHotspot,
                                              ly < kResizeHotspot,
                                              ly >= h - kResizeHotspot);
            }
        }
    }
    SetChromeCursor(shape);
}

void JKWindowServer::HandleSDLEvent(const SDL_Event& ev) {
    if (ev.type == SDL_WINDOWEVENT &&
        (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         ev.window.event == SDL_WINDOWEVENT_MOVED ||
         ev.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED)) {
        UpdateOutputBounds();
    }

    if (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN ||
        ev.type == SDL_MOUSEBUTTONUP) {
        // Everything in this server is in physical client pixels: the launcher
        // icons and compositor layers are drawn at logical_pt * outputScale =
        // physical px, and the hit-test compares against those same physical
        // px. So we want the mouse in raw physical client px (no DPI division).
        int mx = 0, my = 0;
        const float outputScale = compositor_ ? compositor_->OutputScale() : 1.0f;
        int physX = 0, physY = 0;
        if (window_ &&
            JKPlatform::GetPhysicalClientMousePos(window_, physX, physY)) {
            mx = physX;
            my = physY;
        } else if (ev.type == SDL_MOUSEMOTION) {
            // SDL coords are logical points; convert to physical to match the
            // rest of the pipeline.
            mx = static_cast<int>(std::llround(ev.motion.x * outputScale));
            my = static_cast<int>(std::llround(ev.motion.y * outputScale));
        } else {
            mx = static_cast<int>(std::llround(ev.button.x * outputScale));
            my = static_cast<int>(std::llround(ev.button.y * outputScale));
        }

        // Server window chrome (title-bar move / close / border resize)
        // intercepts mouse input before anything reaches the client.
        if (HandleChromeGrab(ev, mx, my, outputScale)) {
            return;
        }
        // Hover feedback for chrome hotspots (drag-active cursor was already
        // set at grab start and survives until the next free motion).
        if (ev.type == SDL_MOUSEMOTION) {
            UpdateChromeHoverCursor(mx, my, outputScale);
        }
        if (ev.type == SDL_MOUSEBUTTONDOWN &&
            TryChromeGrab(mx, my, outputScale, ev.button.clicks)) {
            return;
        }

        // Client surfaces are rendered on top of the launcher, so they should
        // receive input first. Only treat a click as a launcher icon click if
        // it did not hit any client surface.
        //
        // While a mouse button is held inside a client surface (captured),
        // keep routing motion/release to that client even when the cursor
        // leaves the surface — the server-side equivalent of Win32
        // SetCapture. The client runs the same capture logic as the
        // single-process path, so out-of-bounds payload coordinates are
        // expected and handled there.
        JKClientConnection* client = nullptr;
        if (capturedClientId_ != 0) {
            client = FindClientById(capturedClientId_);
            if (!client) capturedClientId_ = 0;  // captured client vanished
        }
        if (!client) client = HitTestClient(mx, my);
        if (!client && ev.type == SDL_MOUSEBUTTONDOWN) {
            // Launcher icons live in the desktop shell (P1 ③): LaunchAt
            // hit-tests in physical pixels and dispatches the spawn through
            // the ShellHost launch callback (this server's SpawnClient, so
            // the 500 ms throttle below stays server-side).
            if (shell_ && shell_->LaunchAt(mx, my)) {
                return;
            }
        }
        if (!client) return;

        // mx/my are physical client px. The client surface is client->Width() x
        // client->Height() surface pixels, stretched by outputScale when drawn
        // and possibly shrunk by the fit scale (surface larger than desktop).
        // Convert the physical mouse position back into the client's surface
        // pixel space: ((mx/outputScale) - client->X()) / layerScale.
        float layerScaleX = 1.0f, layerScaleY = 1.0f;
        if (compositor_) {
            if (auto* layer = compositor_->FindLayerById(client->Id())) {
                layerScaleX = layer->ScaleX();
                layerScaleY = layer->ScaleY();
            }
        }
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.x = static_cast<int>(std::llround(
            (mx / outputScale - client->X()) / layerScaleX));
        payload.y = static_cast<int>(std::llround(
            (my / outputScale - client->Y()) / layerScaleY));

        if (ev.type == SDL_MOUSEMOTION) {
            payload.type = ipc::InputEventType::MouseMove;
            payload.dx = static_cast<int32_t>(std::llround(ev.motion.xrel / layerScaleX));
            payload.dy = static_cast<int32_t>(std::llround(ev.motion.yrel / layerScaleY));
            // Live modifier state for mouse reports (docs/26 단계 3) — the
            // mouse structs carry no mods, the server owns the real keyboard
            // state; JKClientSurface copies payload.option through verbatim.
            payload.option = SDL_GetModState();
        } else if (ev.type == SDL_MOUSEBUTTONDOWN) {
            payload.type = ipc::InputEventType::MouseDown;
            payload.keyCode = ev.button.button;
            payload.detail = ev.button.clicks;
            payload.option = SDL_GetModState();
            // Clicking the shell does not steal keyboard focus (docs/28).
            if (!client->IsShell()) {
                FocusClient(client->Id());
                PushWindowList();  // active highlight follows click focus
            }
            capturedClientId_ = client->Id();
        } else if (ev.type == SDL_MOUSEBUTTONUP) {
            payload.type = ipc::InputEventType::MouseUp;
            payload.keyCode = ev.button.button;
            payload.detail = ev.button.clicks;
            payload.option = SDL_GetModState();
            capturedClientId_ = 0;
        }

        SendInputEvent(*client, payload);
    } else if (ev.type == SDL_MOUSEWHEEL) {
        JKClientConnection* client = FindClientById(focusedClientId_);
        if (!client) return;
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.type = ipc::InputEventType::MouseWheel;
        payload.dx = ev.wheel.x;
        payload.dy = ev.wheel.y;
        payload.option = SDL_GetModState();   // mouse-report mods (단계 3)
        SendInputEvent(*client, payload);
    } else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
        // Alt+Space opens (or refocuses) the command palette (M2a, spec §6.2).
        // A shell-level chord: the server interprets it instead of forwarding.
        if (ev.type == SDL_KEYDOWN && !ev.key.repeat &&
            (ev.key.keysym.mod & KMOD_ALT) &&
            ev.key.keysym.sym == SDLK_SPACE) {
            TogglePalette();
            return;
        }
        JKClientConnection* client = FindClientById(focusedClientId_);
        if (!client) return;
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.type = (ev.type == SDL_KEYDOWN) ? ipc::InputEventType::KeyDown
                                                : ipc::InputEventType::KeyUp;
        payload.keyCode = ev.key.keysym.sym;
        payload.detail = ev.key.repeat;
        payload.option = ev.key.keysym.mod;
        SendInputEvent(*client, payload);
    } else if (ev.type == SDL_TEXTINPUT) {
        JKClientConnection* client = FindClientById(focusedClientId_);
        if (!client) return;
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.type = ipc::InputEventType::Char;
        std::strncpy(payload.text, ev.text.text, sizeof(payload.text) - 1);
        SendInputEvent(*client, payload);
    } else if (ev.type == SDL_TEXTEDITING) {
        JKClientConnection* client = FindClientById(focusedClientId_);
        if (!client) return;
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.type = ipc::InputEventType::TextEditing;
        std::strncpy(payload.text, ev.edit.text, sizeof(payload.text) - 1);
        payload.detail = ev.edit.start;
        payload.option = ev.edit.length;
        SendInputEvent(*client, payload);
    }
}

void JKWindowServer::SendInputEvent(JKClientConnection& client, const ipc::InputEventPayload& payload) {
    client.Send(ipc::MsgType::InputEvent, &payload, sizeof(payload));
}

// 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): send_input 실행기.
// 도구 경로와 승인 재실행 경로가 공유한다. 좌표는 논리 데스크톱 좌표 — 실시간
// 경로의 표면 변환식은 ((mx/outputScale) - client->X()) / layerScale (mx는
// 물리 px)인데 send_input은 논리 좌표를 받으므로 물리 전곱이 소거돼
// (논리 - client->X()) / layerScale이 된다. 셸은 대상에서 제외 — 크롬 닫기는
// close_window가 담당한다.
//
// Precondition: clientsMutex_ held — 도구/승인 재실행 호출부가 모두
// HandleAgentQuery 계열(레슨 35: 락 보유 경로에서 FindClientById 같은
// 자체-락 헬퍼를 부르면 std::mutex 비재귀라 데드락)이라 clients_를 직접
// 순회한다(ProcessClientMessage의 2696-2697 선례 주석).
std::string JKWindowServer::ExecuteSendInputOp(const SendInputOp& op) {
    JKClientConnection* client = nullptr;
    for (auto& c : clients_) {
        if (c && c->Id() == op.target && !c->IsDisconnected()) {
            client = c.get();
            break;
        }
    }
    if (!client) return "window_not_found";
    // 셸 + 캡처 오버레이 쌍검사 (최종리뷰 Important 픽스): 캡처 오버레이는
    // 셸과 별개 실존 연결 — 배제 선례는 IsShell() || Title()==kCaptureOverlayTitle
    // 쌍검사(560·1054·5888). 러버밴드 캡처 진행 중 오버레이 id로 합성
    // click/key가 들어오면 러버밴드·ESC 처리가 오염된다.
    if (client->IsShell() || client->Title() == kCaptureOverlayTitle)
        return "bad_target";
    ipc::InputEventPayload p{};
    p.surfaceId = op.target;
    if (op.op == "click") {
        float sx = 1.0f, sy = 1.0f;
        if (compositor_) {
            if (auto* layer = compositor_->FindLayerById(op.target)) {
                sx = layer->ScaleX();
                sy = layer->ScaleY();
            }
        }
        p.x = static_cast<int>(std::llround((op.x - client->X()) / sx));
        p.y = static_cast<int>(std::llround((op.y - client->Y()) / sy));
        p.keyCode = op.button;
        p.detail = op.clicks;
        // 합성 마우스에도 실시간 경로와 동일한 mods 스탬프(최종리뷰 Minor 3) —
        // 마우스 구조체는 mods 필드가 없고 option으로 실린다(단계 3 선례).
        p.option = SDL_GetModState();
        // 선행 MouseMove 필수 — ImGui 백엔드는 MouseMove에서만 MousePos를
        // 갱신한다(imgui_impl_jkwindow.cpp:219, MouseDown은 좌표를 안 실음).
        // 이동 없이 Down/Up만 보내면 합성 클릭이 마지막 MousePos에 착지해
        // ImGui 앱(taskmgr 등)에서 클릭이 무력했다(런그 5 실측 — JKDC
        // 커스텀 렌더링 앱은 이벤트 좌표를 직독해 런그 1이 통과했던 것).
        p.type = ipc::InputEventType::MouseMove;
        SendInputEvent(*client, p);
        p.type = ipc::InputEventType::MouseDown;
        SendInputEvent(*client, p);
        p.type = ipc::InputEventType::MouseUp;
        SendInputEvent(*client, p);
        return "";
    }
    if (op.op == "key") {
        if (op.key == 0) return "bad_key";
        p.keyCode = op.key;
        p.option = op.mods;
        if (op.action != "up") {
            p.type = ipc::InputEventType::KeyDown;
            p.detail = 0;
            SendInputEvent(*client, p);
        }
        if (op.action != "down") {
            p.type = ipc::InputEventType::KeyUp;
            SendInputEvent(*client, p);
        }
        return "";
    }
    if (op.op == "type") {
        if (op.text.empty()) return "bad_text";
        // Char 페이로드는 63B — UTF-8 후속 바이트(0x80-0xBF)를 넘지 않게 분할.
        size_t off = 0;
        while (off < op.text.size()) {
            size_t len = std::min<size_t>(63, op.text.size() - off);
            while (len > 0 && (op.text[off + len] & 0xC0) == 0x80) --len;
            // 경화(최종리뷰 Important 2): 후속 바이트만 이어지면 len이 0에 닿아
            // memcpy 0 + off 진행 0으로 무한 루프한다. GetStr 재인코더는 후속
            // 런 ≤3을 통상 보장하지만 SendInputOp는 미래의 비-QuickJS 생산자를
            // 위한 구조 — 불변식(off는 항상 리드 바이트에서 시작)이 깨진 입력에
            //도 핫 경로가 멈추지 않게 방어선을 둔다.
            if (len == 0) len = 1;
            p.type = ipc::InputEventType::Char;
            std::memcpy(p.text, op.text.c_str() + off, len);
            p.text[len] = '\0';
            SendInputEvent(*client, p);
            off += len;
        }
        return "";
    }
    if (op.op == "wheel") {
        p.type = ipc::InputEventType::MouseWheel;
        p.dx = op.dx;
        p.dy = op.dy;
        // 실시간 휠 경로와 동일한 mods 스탬프(최종리뷰 Minor 3, :1523 선례).
        p.option = SDL_GetModState();
        SendInputEvent(*client, p);
        return "";
    }
    return "bad_op";
}

// 도구 인자(args 오브젝트) → SendInputOp. 빈 문자열=성공, 아니면 error 키.
std::string JKWindowServer::BuildSendInputOp(
    const jk::agent::AgentJson& args, SendInputOp* out) {
    std::string op, text, action;
    int id = 0, x = 0, y = 0, dx = 0, dy = 0, key = 0, mods = 0;
    int button = 1, clicks = 1;
    args.GetStr("op", op);
    args.GetInt("id", id);
    args.GetInt("x", x);
    args.GetInt("y", y);
    args.GetInt("dx", dx);
    args.GetInt("dy", dy);
    args.GetInt("key", key);
    args.GetInt("mods", mods);
    args.GetInt("button", button);
    args.GetInt("clicks", clicks);
    args.GetStr("text", text);
    args.GetStr("action", action);
    if (op != "click" && op != "key" && op != "type" && op != "wheel")
        return "bad_op";
    if (id <= 0) return "bad_target";
    if (action.empty()) action = "tap";
    if (action != "tap" && action != "down" && action != "up") return "bad_action";
    SendInputOp& o = *out;
    o.op = op;
    o.target = static_cast<uint32_t>(id);
    o.x = x;
    o.y = y;
    o.dx = dx;
    o.dy = dy;
    o.key = static_cast<uint32_t>(key);
    o.mods = static_cast<uint32_t>(mods);
    o.button = static_cast<uint32_t>(button > 0 ? button : 1);
    o.clicks = static_cast<uint32_t>(clicks > 0 ? clicks : 1);
    o.text = text;
    o.action = action;
    return "";
}

JKClientConnection* JKWindowServer::HitTestClient(int32_t x, int32_t y) {
    if (!compositor_) return nullptr;
    auto* layer = compositor_->HitTest(x, y);
    if (!layer) return nullptr;
    return FindClientById(layer->Id());
}

JKClientConnection* JKWindowServer::FindClientById(uint32_t surfaceId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& client : clients_) {
        if (client && client->Id() == surfaceId) {
            return client.get();
        }
    }
    return nullptr;
}

void JKWindowServer::FocusClient(uint32_t surfaceId) {
    // 무변화 디바운스 (docs/57 §12.6 ⑦): 비교/갱신 대상은 focusedClientId_가
    // 아니라 lastFocusedPushed_ (마지막으로 실제 push한 id). 스폰 인테이크는
    // 클라 테이블 등록 전이라 push 없이 focusedClientId_만 세팅하므로,
    // focusedClientId_ 기준 비교는 그 창의 첫 명시 포커스 이벤트를 삼켰다 —
    // lastFocusedPushed_ 기준이면 같은 id 재포커스는 무push(폰 WS 초당 수회
    // 스팸 봉쇄) + 스폰 창의 첫 명시 포커스는 1회 push(구 동작 복원).
    focusedClientId_ = surfaceId;
    if (compositor_) {
        compositor_->FocusLayer(surfaceId);
    }
    // Desktop Agent event (spec §4). Resolves nothing when the id is not (yet)
    // in the table (e.g. focus at spawn intake, before push_back) — push가
    // resolve된 경우에만 lastFocusedPushed_를 갱신한다.
    if (surfaceId == lastFocusedPushed_) return;
    for (auto& c : clients_) {
        if (c && c->Id() == surfaceId) {
            PushAgentEvent("window.focused", surfaceId, c->Title(), c->Pid());
            lastFocusedPushed_ = surfaceId;
            break;
        }
    }
}

void JKWindowServer::UpdateOutputBounds() {
    if (!window_ || !compositor_ || !renderer_) return;
    int logW = 0, logH = 0;
    SDL_GetWindowSize(window_, &logW, &logH);
    int physW = 0, physH = 0;
    SDL_GetRendererOutputSize(renderer_, &physW, &physH);

    // Match the single-process render thread: the renderer works in physical
    // pixels, but all layer positions/sizes are stored in SDL logical points.
    // Scale logical points to physical pixels when compositing.
    float scale = 1.0f;
    if (logW > 0 && logH > 0) {
        scale = physW / static_cast<float>(logW);
    }
    compositor_->SetOutput(JKCompositorOutput(0, JKRect{0, 0, logW, logH}, scale));

    // Keep the shell docked across desktop size changes (SIZE_CHANGED /
    // MOVED / DISPLAY_CHANGED all funnel here).
    DockShellClient(nullptr);

    // docs/39 §8: the server window is now RESIZABLE (and maximizable via the
    // OS title button). SIZE_CHANGED funnels here too, but MOVED /
    // DISPLAY_CHANGED must NOT touch the maximized layers — only an actual
    // logical SIZE change re-issues their maximize against the new work area.
    // The first call (Init, before any client exists) only seeds the trackers.
    if (lastDesktopW_ != logW || lastDesktopH_ != logH) {
        const bool firstCall = (lastDesktopW_ < 0);
        lastDesktopW_ = logW;
        lastDesktopH_ = logH;
        if (!firstCall) {
            size_t nMax = 0;
            if (!preMaxRects_.empty()) {
                const int reserve = compositor_->ShellReserveHeight();
                const int workH = logH - reserve;
                for (const auto& kv : preMaxRects_) {
                    // FindClientById locks clientsMutex_; this path never runs
                    // with that lock held (Init / HandleSDLEvent), matching
                    // FocusClient's PushAgentEvent locking regime. Entries
                    // whose client is gone (died or disconnected while the
                    // desktop was resized) are skipped —
                    // CleanupDisconnectedClients erases their map entries
                    // later anyway.
                    JKClientConnection* client = FindClientById(kv.first);
                    if (!client || client->IsDisconnected()) {
                        continue;
                    }
                    ++nMax;
                    CommitChromeResize(*client, kv.first, logW, workH,
                                       logW, workH);
                    compositor_->SetLayerPosition(kv.first, 0, 0);
                    // Connection-side position in sync (DockShellClient
                    // comment: the input mapping reads client->X()/Y(),
                    // drawing reads the compositor layer).
                    client->SetPosition(0, 0);
                }
            }
            // vplayer 전체화면(스펙 §2.1) 재발행: 새 출력 전체(예약 없음).
            // 저장 rect는 무효화하지 않는다 — 복원 대상은 여전히 진짜 원 rect.
            size_t nFs = 0;
            for (const auto& kv : preFsRects_) {
                JKClientConnection* client = FindClientById(kv.first);
                if (!client || client->IsDisconnected()) {
                    continue;
                }
                ++nFs;
                CommitChromeResize(*client, kv.first, logW, logH, logW, logH);
                compositor_->SetLayerPosition(kv.first, 0, 0);
                client->SetPosition(0, 0);
            }
            // Desktop-size adjustment, NOT a toggle: no window.maximized /
            // window.restored event, and the saved pre-max rects stay valid.
            // One log line — stdout is the server log (run_test.sh redirects
            // it, read_log tails it).
            std::printf("[server] desktop size changed to %dx%d "
                        "(re-maximized %zu, re-fullscreened %zu layer(s))\n",
                        logW, logH, nMax, nFs);
            std::fflush(stdout);
        }
    }
}

void JKWindowServer::ProcessPendingMessages() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& client : clients_) {
        if (!client) continue;

        ipc::Message msg;
        while (client->PopMessage(msg)) {
            ProcessClientMessage(*client, msg);
        }
    }

    // M2 chat: expire parked approvals — answer the parked query with
    // approval_timeout and broadcast the resolution (same lock scope).
    const time_t now = std::time(nullptr);
    for (auto it = pendingApprovals_.begin(); it != pendingApprovals_.end();) {
        if (now < it->expiresAt) { ++it; continue; }
        // file_open (filedlg 설계)은 승인이 아니라 대화상자가 해소자 — 같은
        // 만료 기계로 회수하되 오류 문자열만 대화상자에 맞춘다.
        const char* timeoutErr =
            (it->kind == "file_open") ? "dialog_timeout" : "approval_timeout";
        // 다이얼로그가 result 없이 죽으면(크래시/kill/요청자 먼저 종료) 슬롯도
        // 같이 비워야 한다 — 안 그러면 이후 file_open이 서버 재시작까지
        // dialog_busy로 막힌다. 새 무효화 기계 없이 이 스캔 안에서 회수.
        bool eventModeExpired = false;  // waitAsync 만료 — timeout reply 생략
        if (it->kind == "file_open" &&
            pendingFileDialog_.requestId == it->requestId) {
            // 스펙 §6: waitAsync 요청자는 파킹 응답을 이미 받았으므로 만료도
            // file.open_result 이벤트로 통지한다 (reply 모드는 아래의
            // dialog_timeout AgentReply 현행 불변). 슬롯 대조 성공이 만료된
            // 요청이 waitAsync였음을 증명하는 유일한 시점 — 슬롯이 먼저
            // 소진돼 비었으면 waitAsync 판독 불가 (그때는 요청자 연결 소멸
            // 후이라 통지할 곳도 없다). reset 전에 판독.
            if (pendingFileDialog_.waitAsync) {
                PushAgentEventJson("{\"topic\":\"file.open_result\","
                                   "\"ok\":false,\"error\":\"expired\"}");
                // 단일 전달 불변 (스펙 §6): 원래 쿼리는 parked ack로 이미
                // 회답됐다 — 아래의 dialog_timeout AgentReply를 또 보내면
                // 응답받은 queryId에 2통째가 꽂힌다. reply 모드/비 file_open/
                // 슬롯 불일치는 기존대로 timeout reply를 받는다.
                eventModeExpired = true;
            }
            pendingFileDialog_ = PendingFileDialog{};
        }
        if (!eventModeExpired) {
            for (auto& c : clients_) {
                if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
                    ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                        it->queryId, 0,
                                        (std::string("{\"ok\":false,\"error\":\"") +
                                         timeoutErr + "\"}").c_str());
                    break;
                }
            }
        }
        // file_open 만료는 승인 결정이 아니라 대화상자 수명 만료다
        // (final-review NOTE-4) — agent.approval_resolved 승인 이벤트를
        // 브로드캐스트하면 구독자에게 존재하지 않는 승인의 timeout 결정을
        // 날조해 전달하게 된다. 슬롯 회수(위)는 그대로 유지.
        if (it->kind != "file_open") {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "{\"topic\":\"agent.approval_resolved\","
                          "\"request\":%u,\"decision\":\"timeout\"}",
                          it->requestId);
            PushAgentEventJson(buf);
        }
        it = pendingApprovals_.erase(it);
    }

    // 앱 도구 중계 타임아웃 (스펙 2026-09-19-app-tool-hub §9): 10s —
    // expiresAt은 allow 중계 시점(승인 resolve 포함)에 설정되므로 승인 파킹
    // 대기 중엔 오발하지 않는다. 회수 = 요청자 queryId에 tool_timeout 에러
    // reply(§8 표면).
    const time_t toolNow = std::time(nullptr);
    for (auto it = inflightAppTools_.begin(); it != inflightAppTools_.end();) {
        if (toolNow >= it->second.expiresAt) {
            ReplyAppToolError(it->second, "tool_timeout");
            it = inflightAppTools_.erase(it);
        } else {
            ++it;
        }
    }
}

void JKWindowServer::ProcessClientMessage(JKClientConnection& client, const ipc::Message& msg) {
    if (msg.type == ipc::MsgType::CommitSurface) {
        if (msg.payload.size() >= sizeof(ipc::CommitSurfaceHeader)) {
            const auto* header = reinterpret_cast<const ipc::CommitSurfaceHeader*>(
                msg.payload.data());
            const size_t expected = sizeof(ipc::CommitSurfaceHeader) +
                                    header->dirtyCount * sizeof(ipc::DirtyRect);
            if (msg.payload.size() >= expected) {
                client.MarkDirty();
                if (compositor_) {
                    compositor_->MarkDirty(client.Id());
                }
            }
        }
    } else if (msg.type == ipc::MsgType::AudioCommand) {
        if (msg.payload.size() >= sizeof(AudioCommand)) {
            AudioCommand cmd{};
            std::memcpy(&cmd, msg.payload.data(), sizeof(AudioCommand));
            // Resolve asset paths on the server so clients only need the id.
            if ((cmd.type == AudioCommand::Type::LoadSFX ||
                 cmd.type == AudioCommand::Type::LoadBGM) && cmd.path[0] == '\0') {
                std::string path = ResolveAudioPath(cmd.id, cmd.type);
                std::strncpy(cmd.path, path.c_str(), sizeof(cmd.path) - 1);
            }
            PostAudioCommand(cmd);
        }
    } else if (msg.type == ipc::MsgType::Close) {
        // Client explicitly closed.
    } else if (msg.type == ipc::MsgType::ShellRegister) {
        // Shell protocol (docs/28): the FIRST client to register becomes the
        // desktop shell (taskbar). Later registrations are ignored while one
        // is active — the shell is a role, not an app.
        bool alreadyShell = false;
        for (const auto& other : clients_) {
            if (other && other->Id() != client.Id() && other->IsShell()) {
                alreadyShell = true;
                break;
            }
        }
        if (alreadyShell) {
            std::fprintf(stderr, "JKWindowServer: surface %u shell register denied (shell already active)\n",
                         client.Id());
            ipc::ShellRegisterAckPayload ack{};  // accepted = 0
            client.Send(ipc::MsgType::ShellRegisterAck, &ack, sizeof(ack));
        } else {
            client.SetShell(true);
            if (compositor_) {
                compositor_->SetLayerShell(client.Id(), true);
            }
            ipc::ShellRegisterAckPayload ack{};
            ack.accepted = 1;
            client.Send(ipc::MsgType::ShellRegisterAck, &ack, sizeof(ack));
            std::fprintf(stderr, "JKWindowServer: surface %u registered as shell\n",
                         client.Id());
            PushWindowListUnsafe();  // initial snapshot (clientsMutex_ held)
            DockShellClient(&client);  // bottom edge + full desktop width
        }
    } else if (msg.type == ipc::MsgType::WindowActivate) {
        if (msg.payload.size() >= sizeof(ipc::WindowActivatePayload)) {
            ipc::WindowActivatePayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));
            if (payload.surfaceId != 0 && payload.surfaceId != client.Id()) {
                // Restore-on-activate: a minimized window comes back first.
                if (compositor_) {
                    compositor_->SetLayerVisible(payload.surfaceId, true);
                }
                FocusClient(payload.surfaceId);
                PushWindowListUnsafe();  // active highlight follows focus
            }
        }
    } else if (msg.type == ipc::MsgType::WindowMinimizeToggle) {
        if (msg.payload.size() >= sizeof(ipc::WindowActivatePayload)) {
            ipc::WindowActivatePayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));
            if (payload.surfaceId != 0 && payload.surfaceId != client.Id() && compositor_) {
                const bool visible = compositor_->IsLayerVisible(payload.surfaceId);
                compositor_->SetLayerVisible(payload.surfaceId, !visible);
                if (visible) {
                    // Hiding it loses focus — hand the keyboard to the next
                    // topmost app window (same fallback as a disconnect).
                    if (focusedClientId_ == payload.surfaceId) {
                        FocusClient(compositor_->TopmostLayerId());
                    }
                }
                PushWindowListUnsafe();  // minimized flag follows visibility
            }
        }
    } else if (msg.type == ipc::MsgType::WindowListSubscribe) {
        if (msg.payload.size() >= sizeof(ipc::WindowListSubscribePayload)) {
            ipc::WindowListSubscribePayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));
            client.SetWindowListSubscriber(payload.subscribe != 0);
            if (payload.subscribe) {
                PushWindowListUnsafe();  // initial snapshot (clientsMutex_ held)
            }
        }
    } else if (msg.type == ipc::MsgType::AgentEventSubscribe) {
        // Subscribe/unsubscribe to desktop event pushes (agent connections).
        if (msg.payload.size() >= sizeof(ipc::AgentEventSubscribePayload)) {
            ipc::AgentEventSubscribePayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));
            client.SetAgentEventSubscriber(payload.subscribe != 0);
        }
    } else if (msg.type == ipc::MsgType::WindowTitle) {
        // C -> S title update (docs/33): the notification center's unread
        // badge. Raw UTF-8 payload, capped so a rogue client can't bloat the
        // window-list entries.
        if (!msg.payload.empty() && msg.payload.size() <= 96) {
            client.SetTitle(std::string(msg.payload.begin(), msg.payload.end()));
            PushWindowListUnsafe();  // taskbar button text follows (mutex held)
        }
    } else if (msg.type == ipc::MsgType::AgentQuery) {
        uint32_t queryId = 0, ok = 0;
        std::string json;
        if (ipc::ReadAgentJson(msg, queryId, ok, json)) {
            HandleAgentQuery(client, queryId, json);
        }
    } else if (msg.type == ipc::MsgType::AgentToolRegister) {
        std::string json;
        if (ipc::ReadAgentToolRegister(msg, json))
            HandleToolRegister(client, json);
    } else if (msg.type == ipc::MsgType::AgentToolResult) {
        HandleToolResult(client, msg);
    }
}

void JKWindowServer::PushWindowList() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    PushWindowListUnsafe();
}

// Caller must hold clientsMutex_: ProcessPendingMessages and
// CleanupDisconnectedClients iterate under it, and std::mutex is not
// recursive.
void JKWindowServer::PushWindowListUnsafe() {
    JKClientConnection* shell = nullptr;
    ipc::WindowListPayload payload{};
    for (auto& c : clients_) {
        if (!c || c->IsDisconnected()) continue;
        if (c->IsShell()) {
            shell = c.get();          // the shell never lists itself
            continue;
        }
        if (c->IsControlOnly()) {
            continue;                 // agent connections are not windows
        }
        if (payload.count < 32) {
            ipc::ShellWindowEntry& entry = payload.windows[payload.count++];
            entry.surfaceId = c->Id();
            entry.flags = 0;
            if (focusedClientId_ == c->Id()) {
                entry.flags |= ipc::kShellWindowActive;
            }
            // Minimized = server-side layer visibility (docs/25 §C reuse).
            if (compositor_ && !compositor_->IsLayerVisible(c->Id())) {
                entry.flags |= ipc::kShellWindowMinimized;
            }
            std::strncpy(entry.title, c->Title().c_str(), sizeof(entry.title) - 1);
            entry.pid = c->Pid();
        }
    }
    if (shell) {
        shell->Send(ipc::MsgType::WindowList, &payload, sizeof(payload));
    }
    // Non-shell subscribers (taskmgr) get the same snapshot; unlike the
    // shell they are regular windows and appear in it.
    for (auto& c : clients_) {
        if (c && !c->IsDisconnected() && !c->IsShell() && c->WantsWindowList()) {
            c->Send(ipc::MsgType::WindowList, &payload, sizeof(payload));
        }
    }
}

// 권한 매트릭스의 행 — 게이트 소비처별 정직 표기 (스펙 §2.1). 서버는
// AgentToolAllowed를 ask 게이트 도구들에서 검사하고 브로커 bool 맵이 MCP
// 경로만 걸러낸다. "none" 행의 파일값은 서버 경로에서 무력. "server(flip)"
// 행(캡처 쌍, docs/54 §11)은 파일값이 서버에서 강제 — "ask"는 도구 거부,
// 승인 파킹이 "allow"로 뒤집는다.
struct AgentPermRow { const char* tool; const char* gate; const char* deflt; };
static const AgentPermRow kPermMatrix[] = {
    {"close_window", "server", "deny"},
    {"trust_request", "server", "ask"},
    {"run_console_app", "server", "ask"},
    {"trust_revoke", "server", "ask"},
    {"permission_set", "server(fixed)", "ask"},
    {"read_log", "broker", "allow"},
    {"read_events", "broker", "allow"},
    {"terminal_exec", "broker", "allow"},
    {"list_windows", "none", "allow"},
    {"focus_window", "none", "allow"},
    {"window_fullscreen", "none", "allow"},
    // 창 기하 2종 (스펙 2026-09-21-phone-practical-improvements): fullscreen
    // 과 같은 none/allow 분류 — 화면 상태 변경일 뿐 승인 행위가 아니다.
    {"window_move", "none", "allow"},
    {"window_resize", "none", "allow"},
    {"launch_app", "none", "allow"},
    {"save_layout", "none", "allow"},
    {"restore_layout", "none", "allow"},
    {"publish_event", "none", "allow"},
    // 캡처 쌍은 gate "server(flip)" (docs/54 §11 opus M2 픽스): 파일값이
    // 서버에서 강제되고, "ask"는 도구 거부(capture_ask) — 승인 파킹이
    // "allow"로 뒤집을 때까지. 기본값(파일 없음)은 allow 유지.
    {"capture_window", "server(flip)", "allow"},
    {"capture_region", "server(flip)", "allow"},
    {"trigger_toggle", "none", "allow"},
    {"theme_set", "none", "allow"},
    {"open_notify", "none", "allow"},
    {"launch_chat", "none", "allow"},
    {"approve", "none", "allow"},
    {"file_open", "none", "allow"},
    {"file_dialog_params", "none", "allow"},
    {"agent_permissions", "none", "allow"},
    {"installed_list", "none", "allow"},
    // 노트 허브 (스펙 2026-09-18-notes-hub §2.2): 저위험 사용자 데이터 —
    // 스팸 벡터는 receipts 감사 + rate limiter(docs/38).
    {"notes_read", "none", "allow"},
    {"notes_write", "none", "allow"},
    {"read_receipts", "none", "allow"},
    // 설정 허브 (스펙 2026-09-18-settings-hub §2.2): 외관·환경 설정 —
    // theme_set/trigger_toggle 분류. 단 settings_set의 capture_allow 키만
    // 키별 Ask(파킹) — 에이전트가 설정 도구로 권한을 넓히는 경로 봉쇄.
    {"settings_read", "none", "allow"},
    {"settings_set", "none", "allow"},
    // 파일 허브 (스펙 2026-09-18-file-hub §2.2): 최초의 파일 콘텐츠 도구 —
    // 게이트 "server(files)" (소스 분리: window 연결=allow, control-only=ask
    // 파킹 kind files_access; 명시 allow=에이전트 무승인, deny=전 소스 거부).
    // audit은 감사 열람이라 저위험 allow.
    {"files_list", "server(files)", "none"},
    {"files_read", "server(files)", "none"},
    // audit은 감사 열람이라 기본 allow — 단 파일값은 강제한다(opus 리뷰
    // MINOR-3): deny=거부, ask=files 도구와 동일 파킹(kind files_access).
    {"files_audit", "server(audit)", "allow"},
    // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.3/§4.4): app_tool의
    // 실질 게이트는 3단 키(AppToolAllowed) — 이 행은 전역 기본값/매트릭스
    // 표기용. list_app_tools는 카탈로그 조회(none 등급).
    // ⚠ load-bearing(Task 2 리뷰): HandleToolRegister의 namespace_conflict는
    // 이 두 행으로 app="app_tool"/"list_app_tools" 등록을 자동 봉쇄한다.
    {"list_app_tools", "none", "allow"},
    {"app_tool", "server", "allow"},
    // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): 트랙 B 조작
    // 수단 — 대상 창 합성 입력. ask 기본(2026-09-21 사용자 승인): 매 호출이
    // 승인 파킹으로 들어간다. 자동화 편의는 permissions.json에서 allow로.
    {"send_input", "server", "ask"},
    // 의미 커서 (스펙 2026-09-22-semantic-cursor §3): 합성 <app>.move/read/
    // act는 행 없음 — 앱 도구 허브 3단 키(app_tool.*)와 동일 분류. move/read는
    // 플랫폼 구현 무승인(window_move 분류), act는 앱 릴레이(커서 선언 앱 한정
    // 기본 ask — permissions.json의 app_tool.* 키가 이긴다).
};

// permissions.json RMW (스펙 §2.2): 알려진 도구 키 전부 명시 기록 — 없던 키는
// 현재 기본값으로 채워 다음 편집자가 기본값을 온전히 본다. permission_set 행은
// 기록하지 않는다(파일값 무시 게이트). 반환: 빈 문자열 = 성공, 아니면
// write_failed. kPermMatrix는 gate "server" 행의 기본값에도 쓰인다.
static std::string WritePermissionsEntry(const std::string& permTool,
                                         const std::string& decision) {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    const std::string path = dir + "\\permissions.json";

    std::map<std::string, std::string> values;
    if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
        // 전체 읽기 (docs/53 §9 잔여): 4KB 스택 버프는 파일 뒤쪽의 알려진
        // 도구 행을 잘라내 RMW가 기본값으로 되돌렸다. 256KiB 상한 = 이상
        // 파일 메모리 가드 — 초과 시 잘린 JSON이 파싱 실패하면 전재기록이
        // 기본값으로 복원한다(자기 치유, 알려진 키만 기록되는 RMW 원래 의미).
        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        if (sz < 0) {
            // ftell 실패에도 RMW를 진행하면 기존 파일 전체가 기본값으로
            // 되돌려진다(opus 최종리뷰 n3) — 지문 없는 not-found RMW보다
            // 나은 정직 오류로 파킹 전 즉답 경로에 표면화한다.
            std::fclose(f);
            return "permissions_unreadable";
        }
        std::fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            const size_t cap =
                std::min<size_t>(static_cast<size_t>(sz), 256 * 1024);
            std::vector<char> buf(cap + 1, '\0');
            const size_t n = std::fread(buf.data(), 1, cap, f);
            buf[n] = '\0';
            jk::agent::AgentJson json(buf.data());
            std::string v;
            if (json.ok()) {
                for (const AgentPermRow& r : kPermMatrix) {
                    if (json.GetStr(r.tool, v) &&
                        (v == "allow" || v == "ask" || v == "deny")) {
                        values[r.tool] = v;
                    }
                }
            }
        }
        std::fclose(f);
    }
    values[permTool] = decision;
    std::string out = "{";
    bool first = true;
    for (const AgentPermRow& r : kPermMatrix) {
        if (std::string(r.gate) == "server(fixed)") continue;
        if (!first) out += ",";
        first = false;
        const auto it = values.find(r.tool);
        out += "\"" + std::string(r.tool) + "\":\"" +
               (it != values.end() ? it->second : std::string(r.deflt)) + "\"";
    }
    out += "}";
    std::FILE* w = std::fopen(path.c_str(), "wb");
    if (!w) return "write_failed";
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), w);
    std::fclose(w);
    return wrote == out.size() ? std::string() : std::string("write_failed");
}

// ---- 설정 허브 KV (스펙 2026-09-18-settings-hub §2.2) --------------------
// state/settings.json: {"audio":{"mute":0/1,"volume":int},
// "retention":{"days":int},"text":{"font_path":"...","font_fallback":"..."}}.
// StateDir()는 멤버
// 메서드라 static 헬퍼는 exe-dir 인라인 계산(WritePermissionsEntry/
// RevokeTrustRecord 선례). 실패는 조용한 소실 없이 write_failed로 표면화
// (docs/52 리뷰 규약). text.font_path는 데스크탑 벡터 폰트 경로(docs/63 §4),
// font_fallback은 보조 폰트 체인 경로(docs/63 §6 2단계)
// — JKTextAtlas::ResolveDesktopFontPath/ResolveDesktopFallbackPath가 기동 시
// 같은 키를 원독한다.
static std::string SettingsKvPath() {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    return dir + "\\state\\settings.json";
}

// text.font_scale 유효성 (docs/63 §6 Task 3): 문자열 float 전체 소비 + 범위
// [1.0, 3.0]. 부팅 로드(LoadSettingsKv)와 settings_set(정문 게이트)이 같은
// 규약을 쓴다 — "1.5x"·"-2"·"abc"류는 전부 기각(기본 1.0 폴백/bad_value).
static bool ValidFontScale(const std::string& v) {
    if (v.empty()) return false;
    char* end = nullptr;
    const double d = std::strtod(v.c_str(), &end);
    return end != v.c_str() && *end == '\0' && d >= 1.0 && d <= 3.0;
}

static void LoadSettingsKv(bool& mute, int& volume, int& retention,
                           std::string& fontPath, std::string& fontFallback,
                           std::string& fontScale) {
    std::FILE* f = std::fopen(SettingsKvPath().c_str(), "rb");
    if (!f) return;
    std::string text;
    char chunk[2048];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) text.append(chunk, n);
    std::fclose(f);
    jk::agent::AgentJson json(text);
    if (!json.ok()) return;
    // 가드가 읽는 값은 int64 리더로 (docs/57 §13.3 파서 계약 — 축소 캐스트
    // 랩어라웃이 [0,100]/[7,∞) 가드를 우회하는 것을 봉쇄).
    int64_t v = 0;
    if (json.GetObjInt64("audio", "mute", v) && (v == 0 || v == 1)) {
        mute = (v == 1);
    }
    if (json.GetObjInt64("audio", "volume", v) && v >= 0 && v <= 100)
        volume = static_cast<int>(v);
    if (json.GetObjInt64("retention", "days", v) && v >= 7)
        retention = static_cast<int>(v);
    // text.font_path (docs/63 §4): 상한 300 — settings_set 및
    // ResolveDesktopFontPath와 같은 캡(초과치는 폐기, 기본 폰트로 폴백).
    std::string s;
    if (json.GetObjStr("text", "font_path", s) && !s.empty() &&
        s.size() <= 300) {
        fontPath = s;
    }
    // text.font_fallback (docs/63 §6 2단계): 보조 폰트 체인 — 기본값 없음
    // (빈 값 = 미설정 유지). 상한 300 동일.
    if (json.GetObjStr("text", "font_fallback", s) && !s.empty() &&
        s.size() <= 300) {
        fontFallback = s;
    }
    // text.font_scale (docs/63 §6 Task 3): 셀 확대 배율 문자열 float —
    // 범위 밖/파손치는 폐기(멤버 기본 "1.0" 유지 = 비트맵 셀).
    if (json.GetObjStr("text", "font_scale", s) && ValidFontScale(s)) {
        fontScale = s;
    }
}

static bool WriteSettingsKv(bool mute, int volume, int retention,
                            const std::string& fontPath,
                            const std::string& fontFallback,
                            const std::string& fontScale) {
    // 고정 char 버퍼 대신 문자열 조립 — fontPath 300자가 JsonEsc로 제어문자
    // 6배 확장(\\uXXXX)까지 갈 수 있어 512 버퍼는 조용한 절단이 나온다.
    const std::string out =
        std::string("{\"audio\":{\"mute\":") + (mute ? "1" : "0") +
        ",\"volume\":" + std::to_string(volume) + "},\"retention\":{\"days\":" +
        std::to_string(retention) + "},\"text\":{\"font_path\":\"" +
        JsonEsc(fontPath) + "\",\"font_fallback\":\"" +
        JsonEsc(fontFallback) + "\",\"font_scale\":\"" +
        JsonEsc(fontScale) + "\"}}";
    const std::string kvPath = SettingsKvPath();
    // .bak 1세대 (opus 리뷰 MINOR-1): 비원자 쓰기 중간 절단 시 부팅 로더가
    // 기본값으로 조용히 리셋한다 — 직전 KV를 복구 원본으로 남긴다.
    std::remove((kvPath + ".bak").c_str());
    std::rename(kvPath.c_str(), (kvPath + ".bak").c_str());
    std::FILE* f = std::fopen(kvPath.c_str(), "wb");
    if (!f) return false;
    const size_t len = out.size();
    const size_t wrote = std::fwrite(out.c_str(), 1, len, f);
    std::fclose(f);
    return wrote == len;
}

// receipts.jsonl 보존기간 정리 (스펙 §2.5): ts(epoch ms — read_receipts와
// 같은 규약)가 경계 이전인 행 삭제 + 전체 리라이트. 파일은 256KiB 캡 스케일
// (docs/38 rate limiter 파일 전체 읽기 허용 규모) — 전체 리라이트 허용.
// .bak 1세대 보존(bookmarks/trust 관례). 실패 시 .bak 복원 — 부분 상태 방지.
// 정리 대상은 receipts뿐 — trust/permissions는 불변(수술 위험 레슨).
static bool PruneReceipts(int retentionDays) {
    std::string stateDir = SettingsKvPath();
    stateDir = stateDir.substr(0, stateDir.find_last_of("\\/") + 1);
    const std::string path = stateDir + "receipts.jsonl";
    const std::string newPath = path + ".new";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return true;  // 파일 없음 = 정리할 것도 없음 (정상)
    // 스트리밍 프룬 (docs/54 §10 레저): 전체 파일+kept를 메모리에 올리지
    // 않고 행 단위로 .new에 복사한다 — 행당 메모리 = 최대 행 길이. 원본은
    // .new가 완성될 때까지 건드리지 않으므로 실패 시 전 단계 무손상.
    std::FILE* w = std::fopen(newPath.c_str(), "wb");
    if (!w) {
        std::fclose(f);
        return false;
    }
    const long long cutoff =
        static_cast<long long>(std::time(nullptr)) * 1000 -
        static_cast<long long>(retentionDays) * 86400 * 1000;
    auto writeKept = [&](const std::string& line) -> bool {
        if (line.empty()) return true;
        const size_t tp = line.find("\"ts\":");
        const long long ts =
            (tp == std::string::npos) ? 0 : std::atoll(line.c_str() + tp + 5);
        if (ts < cutoff) return true;
        if (std::fwrite(line.data(), 1, line.size(), w) != line.size())
            return false;
        if (std::fputc('\n', w) == EOF) return false;
        return true;
    };
    std::vector<char> buf(65536);
    std::string line;  // 청크 경계에 걸린 미완성 행
    bool wfail = false;
    size_t nread;
    while (!wfail && (nread = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
        size_t pos = 0;
        while (pos < nread) {
            const char* nl =
                static_cast<const char*>(std::memchr(buf.data() + pos, '\n',
                                                     nread - pos));
            if (!nl) {
                line.append(buf.data() + pos, nread - pos);
                break;
            }
            line.append(buf.data() + pos,
                        static_cast<size_t>(nl - (buf.data() + pos)));
            pos += static_cast<size_t>(nl - (buf.data() + pos)) + 1;
            if (!writeKept(line)) {
                wfail = true;
                break;
            }
            line.clear();
        }
    }
    if (!wfail && !line.empty() && !writeKept(line))  // 개행 없는 마지막 행
        wfail = true;
    std::fclose(f);
    std::fclose(w);
    if (wfail) {
        std::remove(newPath.c_str());
        return false;
    }
    // opus 리뷰 M5 (docs/54 §11): rename 실패(예: 브로커가 append용으로
    // 열어둔 공유 위반 창)에도 진행하면 원본 절단 + .bak은 한 세대 전
    // 임파일러가 된다 — 실패 시 중단, 원본은 그대로(다음 set에서 재시도).
    std::remove((path + ".bak").c_str());
    if (std::rename(path.c_str(), (path + ".bak").c_str()) != 0) {
        std::remove(newPath.c_str());
        return false;
    }
    if (std::rename(newPath.c_str(), path.c_str()) != 0) {
        std::rename((path + ".bak").c_str(), path.c_str());  // 복원
        return false;
    }
    // 성공 — .bak은 한 세대 전 원본 보존(1세대 규약).
    return true;
}

// state/notes.json (스펙 2026-09-18-notes-hub §2.3): 노트 허브 저장 — 서버가
// 유일 쓰기자. 행은 AgentJson으로 파싱해 재직렬화한다(ts는 int64라 AgentJson
// 접근자가 없어 docs/38 — 행 원문에서 "ts":<숫자>만 수기 추출). 행 경계는
// 중괄호 스캔이 안전하다(JsonEsc가 { }를 {/}로 이스케이프 — 위).
// 256KiB 캡 초과 쓰기는 거부(docs/53 3d 선례) + .bak 1세대.
struct NoteRow {
    int id = 0;
    std::string text;        // note 본문 / item 제목
    int win = 0;             // note: 타깃 창 id (0=범용)
    int state = 0;           // item: 0대기/1진행/2완료
    std::string src;         // "agent"|"user"
    std::string tsRaw;       // "ts":<원문 숫자> — 재직렬화용
    bool isItem = false;
};
static std::string NotesPath() {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    return dir + "\\state\\notes.json";
}
// 바이트 절단은 UTF-8 후행 시퀀스를 자른다(substr는 바이트 단위 — opus
// MINOR-2: 한국어 3바이트 글자가 경계에 걸리면 mojibake). 마지막 완전한
// 시퀀스 뒤로 물러난다.
static std::string Utf8TrimTo(const std::string& s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    size_t cut = maxBytes;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) {
        --cut;   // 연속 바이트(10xxxxxx)만큼 물러난다
    }
    if (cut > 0) --cut;   // 리드 바이트도 제거(잘린 시퀀스)
    return s.substr(0, cut);
}
// 배열 원문에서 행을 뽑는다 — JsonEsc의 중괄호 이스케이프 덕에 행 경계는
// 중괄호 스캔으로 확정된다(스펙 §2.3).
static void NotesArrayRows(const std::string& body, const char* name,
                           bool isItem, std::vector<NoteRow>& rows) {
    const std::string key = std::string("\"") + name + "\":[";
    const size_t arr = body.find(key);
    if (arr == std::string::npos) return;
    size_t pos = body.find('[', arr);
    if (pos == std::string::npos) return;
    ++pos;
    while (pos < body.size()) {
        // 다음 비공백 문자가 '{'가 아니면 배열 끝 — find('{')로는 ']'를
        // 건너뛰어 다음 배열(역방향 오염: backlog 행이 notes로 흡수됨)까지
        // 긁는다. RMW가 그 오염을 재직렬화로 굳히므로 반드시 여기서 끊는다.
        while (pos < body.size() &&
               std::isspace(static_cast<unsigned char>(body[pos]))) {
            ++pos;
        }
        if (pos >= body.size() || body[pos] != '{') break;
        const size_t obj = pos;
        const size_t close = body.find('}', obj);
        if (close == std::string::npos) break;
        const std::string row = body.substr(obj, close - obj + 1);
        jk::agent::AgentJson j(row);
        NoteRow r;
        r.isItem = isItem;
        if (j.ok()) {
            j.GetInt("id", r.id);
            j.GetStr("src", r.src);
            if (isItem) {
                j.GetStr("title", r.text);
                j.GetInt("state", r.state);
            } else {
                j.GetStr("text", r.text);
                j.GetInt("win", r.win);
            }
            const size_t tp = row.find("\"ts\":");
            if (tp != std::string::npos) {
                size_t d = tp + 5;
                while (d < row.size() &&
                       (row[d] == '-' || (row[d] >= '0' && row[d] <= '9'))) {
                    r.tsRaw += row[d];
                    ++d;
                }
            }
            if (r.id > 0 && !r.tsRaw.empty()) rows.push_back(r);
        }
        pos = close + 1;
        // 다음 행은 쉼표 뒤 — ','/공백을 걷어내고 '{'를 다시 검사한다.
        while (pos < body.size() &&
               (body[pos] == ',' ||
                std::isspace(static_cast<unsigned char>(body[pos])))) {
            ++pos;
        }
    }
}
static bool ReadNotes(std::vector<NoteRow>& notes, std::vector<NoteRow>& items,
                      int& next) {
    std::FILE* f = std::fopen(NotesPath().c_str(), "rb");
    if (!f) return true;   // 파일 없음 = 빈 상태(정상)
    std::string body;
    char chunk[8192];
    size_t n = 0;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) body.append(chunk, n);
    std::fclose(f);
    // 이상 수확 방어(opus NIT-2): 8MiB 초과 파일은 파싱/수확을 거부 —
    // docs/53 state 파일 캡과 같은 상한. 쓰기 캡(256KiB)보다 큰 정상 상태는
    // 있을 수 없으므로(캡 거부) 이 경계는 심어진 파일만 걸러낸다.
    if (body.size() > 8u * 1024 * 1024) return false;
    jk::agent::AgentJson j(body);
    if (!j.ok()) return false;   // 손상 — 호출자가 빈 상태 시작(정직 로그)
    NotesArrayRows(body, "notes", false, notes);
    NotesArrayRows(body, "backlog", true, items);
    // 역오염 방어(opus MINOR-4): JSON은 통과하는데 행 스캔이 행을 놓치면
    // (pretty-print 재포맷, 키 오탈자, drop된 행) RMW가 빈 배열/축소 배열을
    // "정상"으로 재직렬화해 사용자 노트를 소각한다. 읽기 오염 = 쓰기 오염 —
    // 수확 수와 파서 수가 다르면 파산 파일로 판정(RMW 거부, read는 빈 시작).
    {
        int cntN = 0, cntI = 0;
        const bool shapeOk =
            j.GetArraySize("notes", cntN) && j.GetArraySize("backlog", cntI);
        if (!shapeOk || cntN != static_cast<int>(notes.size()) ||
            cntI != static_cast<int>(items.size())) {
            return false;
        }
    }
    if (!j.GetInt("next", next)) {
        // 구형/손상 파일 — 채번기 복구(최대 id + 1).
        for (const NoteRow& r : notes) next = std::max(next, r.id + 1);
        for (const NoteRow& r : items) next = std::max(next, r.id + 1);
    }
    return true;
}
static bool WriteNotesFile(const std::vector<NoteRow>& notes,
                           const std::vector<NoteRow>& items, int next) {
    std::string out = "{\"notes\":[";
    for (size_t i = 0; i < notes.size(); ++i) {
        if (i) out += ",";
        // 행 크기가 가변(text 512바이트 × 이스케이프 확장)이라 스트링 빌더 —
        // 고정 snprintf 버퍼는 잘림 함정(docs/38 레슨 계열).
        out += "{\"id\":" + std::to_string(notes[i].id) +
               ",\"text\":\"" + JsonEsc(notes[i].text) +
               "\",\"win\":" + std::to_string(notes[i].win) +
               ",\"ts\":" + notes[i].tsRaw +
               ",\"src\":\"" + JsonEsc(notes[i].src) + "\"}";
    }
    out += "],\"backlog\":[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += ",";
        out += "{\"id\":" + std::to_string(items[i].id) +
               ",\"title\":\"" + JsonEsc(items[i].text) +
               "\",\"state\":" + std::to_string(items[i].state) +
               ",\"ts\":" + items[i].tsRaw +
               ",\"src\":\"" + JsonEsc(items[i].src) + "\"}";
    }
    out += "],\"next\":" + std::to_string(next) + "}";
    if (out.size() > 262144) return false;   // 256KiB 캡 (docs/53 3d 선례)
    const std::string path = NotesPath();
    std::remove((path + ".bak").c_str());
    // rename 실패(브로커/프로브가 읽기 잠금) → fopen "wb"로 원본이 잘리는
    // receipts M5 선례의 동일 벡터 — 실패 시 절단 없이 중단. 첫 쓰기(원본
    // 부재)는 통과.
    if (std::FILE* probe = std::fopen(path.c_str(), "rb")) {
        std::fclose(probe);
        if (std::rename(path.c_str(), (path + ".bak").c_str()) != 0) {
            return false;
        }
    }
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::rename((path + ".bak").c_str(), path.c_str());
        return false;
    }
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (wrote != out.size()) {
        std::remove(path.c_str());
        std::rename((path + ".bak").c_str(), path.c_str());
        return false;
    }
    return true;
}

// ---- 파일 허브 (스펙 2026-09-18-file-hub §2.2) ---------------------------
// 최초의 파일 콘텐츠 도구 — 읽기 전용 3종 + 감사. 쓰기 도구는 YAGNI(스펙 §3).

// permissions.json 원문값("missing"/"allow"/"ask"/"deny"). AgentToolAllowed는
// 없음과 명시 allow를 구분하지 못한다(없음 = 기본 Allow) — 파일 도구 분기는
// 명시 "allow"만 에이전트 무승인이어야 하므로 원문을 직접 읽는다
// (WritePermissionsEntry 선례의 exe-dir 인라인 — StateDir는 멤버라 static
// 헬퍼 불가). AgentToolAllowed와 같은 4KiB 원문 상한.
static std::string FilesPermRaw(const std::string& tool) {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    const std::string path = dir + "\\permissions.json";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "missing";
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson perm(buf);
    std::string value;
    if (!perm.ok() || !perm.GetStr(tool.c_str(), value)) return "missing";
    if (value == "allow" || value == "ask" || value == "deny") return value;
    return "missing";
}

// 경로 검증기 (스펙 §2.2): 절대 드라이브 형식(X:\...)만 — UNC 접두와 이중
// 구분자(\\, //)/상대/빈 경로 거부, ".." 구성요소 거부, ADS 지점(:) 거부,
// 제어문자 거부(opus 리뷰 MINOR-2 — JsonEsc 6배 확장이 approval_request
// 고정 버퍼를 잘라 무효 JSON을 만든다; Windows 경로에서도 원래 무효),
// 길이 256 캡(opus 리뷰 NIT-7 — list op가 "\\*"를 뒤에 붙여도 MAX_PATH 내).
// 8.3 짧은 이름은 수용 — FindFirstFileA가 실명으로 확장해 목록/읽기 자체는
// 실명으로 이뤄진다(docs/56 한계 기록).
static bool ValidFilePath(const std::string& path) {
    if (path.size() < 3 || path.size() > 256) return false;
    for (const char c : path) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7F) return false;
    }
    const char d = path[0];
    if (!((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z'))) return false;
    if (path[1] != ':' || (path[2] != '\\' && path[2] != '/')) return false;
    if (path.find("\\\\") != std::string::npos ||
        path.find("//") != std::string::npos) return false;
    size_t i = 3;
    while (i <= path.size()) {
        size_t j = i;
        while (j < path.size() && path[j] != '\\' && path[j] != '/') ++j;
        const size_t len = j - i;
        if (len == 2 && path[i] == '.' && path[i + 1] == '.') return false;
        if (std::memchr(path.data() + i, ':', len) != nullptr) return false;
        i = j + 1;
    }
    return true;
}

// files_list op (스펙 §2.2): 절대 경로의 단일 디렉터리 열거 — dir 우선 +
// 이름 asc(바이트 순 — 탐색기와 다르나 결정적), 상한 512행(초과는 capped).
// 와일드카드/리다이렉트 문자는 op에서 재거부(명시 — ValidFilePath가 못
// 걸러낸다). 숨김 파일도 열거(MVP 단순 — 감사 친화).
static std::string FilesListOpJson(const std::string& path) {
    if (path.find_first_of("*?\"<>|") != std::string::npos) {
        return "{\"ok\":false,\"error\":\"bad_path\"}";
    }
    struct Ent {
        std::string name;
        bool dir = false;
        long long size = 0;
        long long mtime = 0;
    };
    std::vector<Ent> rows;
    bool capped = false;
    FindFileDataA fd;
    void* h = FindFirstFileA((path + "\\*").c_str(), &fd);
    if (h == kInvalidFindHandle()) {
        return "{\"ok\":false,\"error\":\"not_found\"}";
    }
    bool more = true;
    while (more && rows.size() < 512) {
        if (std::strcmp(fd.cFileName, ".") != 0 &&
            std::strcmp(fd.cFileName, "..") != 0) {
            Ent e;
            e.name = fd.cFileName;
            e.dir = (fd.dwFileAttributes & 0x10) != 0;   // DIRECTORY
            e.size = (static_cast<long long>(fd.nFileSizeHigh) << 32) |
                     fd.nFileSizeLow;
            // FILETIME(1601 100ns) → epoch 초 — read_receipts의 초 절단 규약.
            e.mtime = fd.ftLastWriteTime > 116444736000000000ULL
                ? static_cast<long long>(
                      (fd.ftLastWriteTime - 116444736000000000ULL) /
                      10000000ULL)
                : 0;
            rows.push_back(e);
        }
        more = FindNextFileA(h, &fd) != 0;
    }
    if (more) capped = true;
    FindClose(h);
    std::sort(rows.begin(), rows.end(),
              [](const Ent& a, const Ent& b) {
                  if (a.dir != b.dir) return a.dir;
                  return a.name < b.name;
              });
    std::string out = "{\"ok\":true,\"entries\":[";
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i) out += ",";
        out += "{\"name\":\"" + JsonEsc(rows[i].name) +
               "\",\"kind\":\"" + (rows[i].dir ? "dir" : "file") +
               "\",\"size\":" + std::to_string(rows[i].size) +
               ",\"mtime\":" + std::to_string(rows[i].mtime) + "}";
    }
    return out + "],\"capped\":" + (capped ? "1" : "0") + "}";
}

// files_read op (스펙 §2.2): 텍스트 미리보기 — 상한 64KiB(기본), maxBytes는
// 요청 상한(≤65536). 첫 4KiB에 NUL → binary:true(text 공란) — UTF-16 텍스트
// 도 NUL을 포함하므로 이진으로 분류된다(스펙 §6(e) 허수를 한계로 기록).
static std::string FilesReadOpJson(const std::string& path, int maxBytes) {
    if (maxBytes <= 0) maxBytes = 65536;
    if (maxBytes > 65536) maxBytes = 65536;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "{\"ok\":false,\"error\":\"not_found\"}";
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    // ftell 실패(−1 — 2GiB 초과 파일)를 size 0으로 흡수(opus 리뷰 NIT-5 —
    // 음수 size가 그대로 응답에 나가지 않게; truncated=0 — 64KiB는 이미
    // 읽힌다). MVP 한계: >2GiB 파일의 size 표기가 0으로 뭉개진다.
    if (size < 0) {
        std::fclose(f);
        return "{\"ok\":true,\"text\":\"\",\"truncated\":0,\"binary\":1,"
               "\"size\":0}";
    }
    const size_t want = static_cast<size_t>(maxBytes);
    const size_t take =
        want < static_cast<size_t>(size) ? want : static_cast<size_t>(size);
    std::vector<char> buf(take);
    const size_t n = buf.empty() ? 0 : std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    // NUL 스니핑 — 실제 읽은 바이트만 (빈 파일은 이진 아님; sniff 0일 때
    // memchr에 buf.data()를 넘기지 않는다 — opus 리뷰 NIT-5).
    const size_t sniff = n < 4096 ? n : 4096;
    if (sniff > 0 && std::memchr(buf.data(), '\0', sniff) != nullptr) {
        return "{\"ok\":true,\"text\":\"\",\"truncated\":0,\"binary\":1,"
               "\"size\":" + std::to_string(size) + "}";
    }
    const bool truncated = static_cast<size_t>(size) > n;
    std::string out = "{\"ok\":true,\"text\":\"";
    out += JsonEsc(std::string(buf.data(), n));
    out += "\",\"truncated\":" + std::string(truncated ? "1" : "0") +
           ",\"binary\":0,\"size\":" + std::to_string(size) + "}";
    return out;
}

// files_audit op (스펙 §2.2): receipts.jsonl 꼬리 256KiB 행 스캔에서
// files_* 도구 행만 필터 — 에이전트 접근 시각화. receipts는 브로커
// (jkagentd)가 쓰는 단일 감사원(GUI 직접 호출은 기록되지 않는다 — 스펙 §4
// 위험의 자연 완화). read_receipts와 동일 행 분해 + args.path 2레벨 추출.
// state dir는 SettingsKvPath 파생(PruneReceipts 선례 — StateDir 멤버라
// static 헬퍼 불가).
static std::string FilesAuditOpJson(int limit) {
    if (limit <= 0) limit = 50;
    if (limit > 200) limit = 200;
    std::string stateDir = SettingsKvPath();
    stateDir = stateDir.substr(0, stateDir.find_last_of("\\/") + 1);
    const std::string path = stateDir + "receipts.jsonl";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "{\"ok\":true,\"rows\":[]}";   // 브로커 미사용 = 정상
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    const long start = size > 262144 ? size - 262144 : 0;
    std::fseek(f, start, SEEK_SET);
    std::vector<char> buf(static_cast<size_t>(size - start) + 1);
    const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < n) {
        const char* begin = buf.data() + pos;
        const char* nl = static_cast<const char*>(
            std::memchr(begin, '\n', n - pos));
        const size_t len = nl ? static_cast<size_t>(nl - begin) : (n - pos);
        if (len > 0) lines.push_back(std::string(begin, len));
        pos += len + (nl ? 1 : 0);
    }
    std::string out = "{\"ok\":true,\"rows\":[";
    int used = 0;
    for (size_t i = lines.size(); i-- > 0 && used < limit;) {
        const std::string& line = lines[i];
        // 행 파서(ts/tool) + ts raw 스캔 + result.ok raw 스캔 —
        // read_receipts의 동일 규약(ok는 "0"/"1" 문자열).
        jk::agent::AgentJson row(line.c_str());
        std::string toolName;
        if (!row.ok() || !row.GetStr("tool", toolName)) continue;
        if (toolName.rfind("files_", 0) != 0) continue;
        long long ts = 0;
        const size_t tp = line.find("\"ts\":");
        if (tp != std::string::npos) {
            ts = std::atoll(line.c_str() + tp + 5);
        }
        std::string ppath;
        row.GetObjStr("args", "path", ppath);
        const bool okFlag =
            line.find("\"result\":") != std::string::npos &&
            line.find("\"ok\":true") != std::string::npos;
        if (used) out += ",";
        out += "{\"ts\":" + std::to_string(ts / 1000) +
               ",\"tool\":\"" + JsonEsc(toolName) +
               "\",\"ok\":\"" + (okFlag ? "1" : "0") +
               "\",\"path\":\"" + JsonEsc(ppath) + "\"}";
        ++used;
    }
    return out + "]}";
}

// 지문 형식: 정확히 "sha256:" + 64 소문자 hex (로더 형식 — docs/37).
static bool ValidFingerprint(const std::string& fp) {
    if (fp.size() != 7 + 64 || fp.compare(0, 7, "sha256:") != 0) return false;
    for (size_t i = 7; i < fp.size(); ++i) {
        const char c = fp[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

// trust.json 원본 텍스트에서 지문 레코드의 { ... } 경계를 찾는다. 레코드는
// 로더 쓰기 형식(name/source/fingerprint/ts — 중첩 객체 없음)이므로 중괄호
// 스캔이 안전하다. AgentJson 재직렬화는 ts(int64)를 잃는다(AgentJson에 int64
// 접근자 없음 — docs/38) — 그래서 원문 수술.
static bool TrustRecordText(const std::string& text,
                            const std::string& fingerprint,
                            std::string& recOut) {
    const std::string needle = "\"fingerprint\":\"" + fingerprint + "\"";
    const size_t hit = text.find(needle);
    if (hit == std::string::npos) return false;
    const size_t begin = text.rfind('{', hit);
    const size_t end = text.find('}', hit);
    if (begin == std::string::npos || end == std::string::npos) return false;
    recOut = text.substr(begin, end - begin + 1);
    return true;
}

// trust.json에서 해당 지문 레코드 제거 + .bak 1회 보존(북마크 선례 — 최초
// 덮어쓰기 시점 원본만). 반환: 빈 문자열 = 성공(제거 1건), 아니면 오류 문자열.
static std::string RevokeTrustRecord(const std::string& fingerprint) {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    CreateDirectoryA((dir + "\\state").c_str(), nullptr);
    const std::string path = dir + "\\state\\trust.json";

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "trust_store_unreadable";
    // 전체 읽기 (docs/53 §9 잔여): 64KB 캡은 장기 설치의 스토어에서 뒤쪽
    // 레코드를 not_found로 미끄러뜨린다. 8MiB 상한 = 이상 파일 가드.
    // 상한 초과/읽기 미달(stale write 중 등)에는 스플라이스를 하지 않는다 —
    // 잘린 접두어를 되돌려 쓰면 상한 너머의 레코드가 파괴되고 .bak도 잘린
    // 원본이라 복구 불가(opus 최종리뷰 m3). 정직한 not_found 반환.
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    if (sz < 0) { std::fclose(f); return "trust_store_unreadable"; }
    std::fseek(f, 0, SEEK_SET);
    const size_t tsz = static_cast<size_t>(sz);
    const bool overCap = tsz > 8u * 1024 * 1024;
    const size_t cap = overCap ? 8u * 1024 * 1024 : tsz;
    std::vector<char> buf(cap + 1, '\0');
    const size_t n = std::fread(buf.data(), 1, cap, f);
    std::fclose(f);
    if (overCap || n < cap) return "not_found";
    buf[n] = '\0';
    const std::string text(buf.data());

    std::string rec;
    if (!TrustRecordText(text, fingerprint, rec)) return "not_found";
    const size_t hit = text.find(rec);
    size_t begin = hit;
    size_t end = hit + rec.size() - 1;
    // 선행 쉼표 흡수 — "},{" 형태에서 앞 레코드의 쉼표를 남기지 않는다.
    size_t cutBegin = begin;
    if (cutBegin > 0 && text[cutBegin - 1] == ',') --cutBegin;
    else if (end + 1 < text.size() && text[end + 1] == ',') ++end;
    const std::string out = text.substr(0, cutBegin) +
                            text.substr(end + 1);

    const std::string bak = path + ".bak";
    if (std::FILE* b = std::fopen(bak.c_str(), "rb")) {
        std::fclose(b);   // .bak 이미 있음 — 1회 보존 규약
    } else if (std::FILE* b = std::fopen(bak.c_str(), "wb")) {
        std::fwrite(text.data(), 1, text.size(), b);
        std::fclose(b);
    }
    std::FILE* w = std::fopen(path.c_str(), "wb");
    if (!w) return "write_failed";
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), w);
    std::fclose(w);
    return wrote == out.size() ? std::string() : std::string("write_failed");
}

// Desktop Agent API (spec §3): normally answers at once — the agent client
// blocks on ReadMessage waiting for the reply with the matching queryId.
// Exception (M2 chat): an "ask"-gated close_window parks its query and replies
// later, when the inline approval resolves.
// Precondition: clientsMutex_ held (called from ProcessClientMessage), so
// iterate clients_ directly — FindClientById/FocusClient-style helpers that
// lock would deadlock on the non-recursive mutex.
// 서버 이벤트 토픽 예약 접두 — 단일 원본. publish_event의 스크립트 토픽
// 봉쇄(docs/54 NIT-3)와 HandleToolRegister의 namespace_conflict 검사(스펙
// 2026-09-19-app-tool-hub §4.1 — app명 정확 일치/도구명 접두 충돌 금지)가
// 이 표를 공유한다. events_list 카탈로그 "server" 행의 접두와 동일 집합
// (window/app/agent/terminal/audio/triggers/file) — 카탈로그에 서버 토픽을
// 추가하면 여기도 손으로 넣는다(두 표가 한 파일 안에 있다).
// 함정(2026-09-23 실측): 표의 규칙은 "server 행 접두"인데 카탈로그에는 예약
// 접두 아래 사는 source:"app" 행이 2개 있다 — terminal.output(터미널 앱 M2b
// 직접 발행)와 agent.notify(jktriggers desktop.notify — 트리거 액션 경로).
// c9ba8bf가 두 접두를 넣으면서 정당 발행자를 reserved_topic으로 죽였고(양쪽
// 발행자 모두 응답을 무시하는 fire-and-forget이라 발견 5일 지연) publish
// 게이트는 이 둘의 정확-토픽 면제로 회복한다(아래 topicReserved).
// 스푸핑 노출 = 트리거 오타동/가짜 알림뿐 — 스크립트 신뢰 등급에서 허용.
static const char* kReservedTopicPrefixes[] = {
    "window.", "agent.", "app.", "terminal.", "audio.", "triggers.", "file.",
};

void JKWindowServer::HandleAgentQuery(JKClientConnection& client,
                                      uint32_t queryId, const std::string& json) {
    jk::agent::AgentJson req(json);
    std::string tool, reply;
    bool replied = true;
    if (!req.ok() || !req.GetStr("tool", tool)) {
        reply = "{\"ok\":false,\"error\":\"bad_request\"}";
    } else if (tool == "ping") {
        reply = "{\"ok\":true,\"pong\":true}";
    } else if (tool == "list_windows") {
        std::string out = "{\"ok\":true,\"windows\":[";
        bool first = true;
        for (auto& c : clients_) {
            if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) continue;
            char item[640];
            std::snprintf(item, sizeof(item),
                "%s{\"id\":%u,\"title\":\"%s\",\"pid\":%u,\"x\":%d,\"y\":%d,"
                "\"w\":%d,\"h\":%d,\"focused\":%s,\"minimized\":%s}",
                first ? "" : ",", c->Id(), JsonEsc(c->Title()).c_str(), c->Pid(),
                c->X(), c->Y(), c->Width(), c->Height(),
                focusedClientId_ == c->Id() ? "true" : "false",
                (compositor_ && !compositor_->IsLayerVisible(c->Id()))
                    ? "true" : "false");
            out += item;
            first = false;
        }
        reply = out + "]}";
    } else if (tool == "focus_window") {
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        }
        if (target && !target->IsControlOnly()) {
            // Same restore-on-activate semantics as WindowActivate (docs/28).
            if (compositor_) {
                compositor_->SetLayerVisible(target->Id(), true);
            }
            FocusClient(target->Id());
            PushWindowListUnsafe();
            reply = "{\"ok\":true}";
        } else {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        }
    } else if (tool == "window_fullscreen") {
        // vplayer 전체화면(스펙 2026-09-17 vplayer-fullscreen-osd §2.2): id
        // 생략 = 호출자 자기 창(창 클라이언트 — vplayer 경로), 명시 id =
        // 임의 창(스크립트/프로브). control-only 호출자의 생략형은 no_window.
        // on 생략 = 현재 상태 반전(AgentJson에 bool 접근자 없음 — GetObjInt
        // 0/1로 읽는다). 화면 상태 변경일 뿐 승인 행위가 아니어서 none gate
        // (focus_window 분류). reply의 fullscreen은 토글 후 실제 플래그 —
        // 클라 플래그의 유일 신뢰원.
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        } else if (!client.IsControlOnly()) {
            target = &client;
        }
        JKCompositorLayer* fsLayer = nullptr;
        if (target && !target->IsControlOnly() && !target->IsShell() && compositor_) {
            // opus 리뷰 MINOR-2: 명시 id로 shell(태스크바) 레이어를 노리면
            // 전체화면 토글이 데스크탑 셸을 덮는다 — shell은 '창'이 아니므로
            // window_not_found로 거절한다(생략형 경로는 애초 shell 불가).
            fsLayer = compositor_->FindLayerById(target->Id());
        }
        if (!fsLayer) {
            reply = (target ? "{\"ok\":false,\"error\":\"window_not_found\"}"
                            : "{\"ok\":false,\"error\":\"no_window\"}");
        } else {
            int onArg = -1;
            bool on = fsLayer->IsFullscreen() ? false : true;
            if (req.GetObjInt("args", "on", onArg) &&
                (onArg == 0 || onArg == 1)) {
                on = (onArg == 1);
            }
            ToggleFullscreen(*target, *fsLayer, on);
            reply = std::string("{\"ok\":true,\"fullscreen\":") +
                    (fsLayer->IsFullscreen() ? "true" : "false") + "}";
        }
    } else if (tool == "window_move" || tool == "window_resize") {
        // 폰 실전 개선 태스크 2 (스펙 2026-09-21-phone-practical-improvements):
        // 창 기하 서버 조작 2종 — 폰 채팅 LLM이 창을 옮기고 크기를 바꾼다.
        // 대상 선정/거절 뼈대는 window_fullscreen 복제: id 생략 = 호출자 자기
        // 창(윈도 클라), control-only 생략형 = no_window. 명시 id로 shell
        // (태스크바)/캡처 오버레이/control-only 연결을 노리면 window_not_found
        // (shell은 '창'이 아니라는 opus MINOR-2 분류 승계).
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        } else if (!client.IsControlOnly()) {
            target = &client;
        }
        // 가드가 읽는 값은 int64 리더로 (docs/57 §13.3 — int32 축소 캐스트는
        // 2^32+100 → 100 랩어라웃으로 ±32768/[80,8192] 가드를 우회시킨다).
        int64_t x = 0, y = 0, w = 0, h = 0;
        const bool hasXY = req.GetObjInt64("args", "x", x) &&
                           req.GetObjInt64("args", "y", y);
        const bool hasWH = req.GetObjInt64("args", "w", w) &&
                           req.GetObjInt64("args", "h", h);
        // 인자 유효성을 대상 판정보다 먼저 — 인자가 잘못된 호출은 대상이
        // 있든 없든 bad_args가 정직한 답.
        if (tool == "window_move" && !hasXY) {
            reply = "{\"ok\":false,\"error\":\"bad_args\"}";
        } else if (tool == "window_resize" && !hasWH) {
            reply = "{\"ok\":false,\"error\":\"bad_args\"}";
        } else if (!target) {
            reply = "{\"ok\":false,\"error\":\"no_window\"}";
        } else if (target->IsControlOnly() || target->IsShell() ||
                   target->Title() == kCaptureOverlayTitle || !compositor_) {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        } else {
            JKCompositorLayer* layer = compositor_->FindLayerById(target->Id());
            if (!layer) {
                reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
            } else if (preMaxRects_.count(layer->Id()) != 0) {
                // 최대화 중 기하 조작은 preMaxRects_ 진실원(저장 rect)과
                // 충돌한다 — 복원 후 조작하라는 거절.
                reply = "{\"ok\":false,\"error\":\"window_maximized\"}";
            } else if (layer->IsFullscreen()) {
                // 전체화면 레이어는 상태 전용(위치 0,0/출력 전체 크기) —
                // 복원 후 조작(preFsRects_ 저장 rect 보호, window_fullscreen
                // 토글로 복원).
                reply = "{\"ok\":false,\"error\":\"window_fullscreen_state\"}";
            } else if (tool == "window_move") {
                // 좌표 클램프 없음(Windows 동작 — 화면 밖 허용), int 범위만.
                if (x < -32768 || x > 32768 || y < -32768 || y > 32768) {
                    reply = "{\"ok\":false,\"error\":\"bad_args\"}";
                } else {
                    // BOTH required (docs/28 lesson): input mapping reads
                    // client->X()/Y() while the draw path reads the layer.
                    compositor_->SetLayerPosition(target->Id(),
                                                  static_cast<int>(x),
                                                  static_cast<int>(y));
                    target->SetPosition(static_cast<int>(x), static_cast<int>(y));
                    PushWindowListUnsafe();  // focus_window 선례 — 태스크바 동기
                    reply = "{\"ok\":true}";
                }
            } else {
                // resize: 인자 클램프가 아니라 범위 검사 — 범위 밖은 bad_args.
                if (w < 80 || w > 8192 || h < 80 || h > 8192) {
                    reply = "{\"ok\":false,\"error\":\"bad_args\"}";
                } else if (w == layer->Width() && h == layer->Height()) {
                    reply = "{\"ok\":true}";  // no-op — 동일 픽셀 크기
                } else {
                    // CommitChromeResize의 ResizeLayer가 layer scale을 1로
                    // 리셋한다(JKWindowServer.cpp 주석 참조). fit-scaled
                    // 레이어(픽셀 크기≠표시 크기)는 기존 ScaleX/Y를 반영한
                    // 표시 크기를 넘겨 scale이 보존되게 한다 — 스케일 1 레이어는
                    // dispW/H=자기(정규 경로와 동일, SetLayerScale 스킵).
                    int dispW = static_cast<int>(w), dispH = static_cast<int>(h);
                    const float sx = layer->ScaleX(), sy = layer->ScaleY();
                    if (sx != 1.0f || sy != 1.0f) {
                        dispW = static_cast<int>(std::llround(w * sx));
                        dispH = static_cast<int>(std::llround(h * sy));
                    }
                    CommitChromeResize(*target, target->Id(), static_cast<int>(w),
                                       static_cast<int>(h), dispW, dispH);
                    PushWindowListUnsafe();  // 태스크바 크기 표기 동기
                    reply = "{\"ok\":true}";
                }
            }
        }
    } else if (tool == "list_app_tools") {
        // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.4): 평면 행 카탈로그
        // (레슨 39 — AgentJson에 중첩 배열 접근이 없어 브로커 tools/list
        // 합성과 agentctl face 양쪽이 평면 행을 먹는다). inputSchema는 등록
        // 원문 그대로 임베드(부재 시 {}), 나머지 필드는 JsonEsc.
        std::string out = "{\"ok\":true,\"tools\":[";
        bool first = true;
        for (const auto& kv : appToolManifests_) {
            const AppToolManifest& m = kv.second;
            for (const AppToolDef& t : m.tools) {
                if (!first) out += ",";
                first = false;
                out += "{\"app\":\"" + JsonEsc(m.app) + "\",\"name\":\"" +
                       JsonEsc(t.name) + "\",\"description\":\"" +
                       JsonEsc(t.description) + "\",\"inputSchema\":" +
                       (t.inputSchema.empty() ? "{}" : t.inputSchema) +
                       ",\"windowId\":" + std::to_string(m.windowId) +
                       ",\"title\":\"" + JsonEsc(m.title) +
                       "\",\"connId\":" + std::to_string(m.connId) +
                       // 스펙 §4: modal은 false도 명시 — 소비자 파싱 단순화.
                       ",\"modal\":" + (m.modal ? "true" : "false") + "}";
            }
        }
        reply = out + "]}";
    } else if (tool == "app_tool") {
        // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.4): 중계. args는
        // 원문 패스스루(서버 스키마 검증 안 함 — 앱 계약). allow/ask 모두
        // 응답이 지연된다 — 기존 파킹 경로의 replied=false 기계를 그대로 쓴다
        // (allow 중계 = 앱의 AgentToolResult 대기, ask 파킹 = 승인 resolve가
        // 중계를 시작). deny만 즉답.
        std::string app, toolName, argsRaw;
        int windowIdArg = 0;
        const bool hasWindowId = req.GetObjInt("args", "windowId", windowIdArg);
        req.GetObjStr("args", "app", app);
        req.GetObjStr("args", "tool", toolName);
        req.GetObjRaw("args", "args", argsRaw);
        // 의미 커서 (스펙 2026-09-22-semantic-cursor §3): 커서 선언 앱의
        // <app>.move/read는 플랫폼 구현 — 릴레이 체인 전에 인터셉트한다
        // (move 동기 에코 / read는 snapshot 중계+커서 헤더 조립 지연응답).
        // act도 인터셉트해 사전 검증(kind enum/인자/격자)만 하고 — 유효
        // 요청은 false를 돌려 기존 릴레이 경로가 그대로 간다(kind/row/col
        // 원문, 게이트 ask 기본). 미선언 앱은 false(기존 릴레이).
        if ((toolName == "move" || toolName == "read" || toolName == "act") &&
            HandleCursorAppTool(client, queryId, app, toolName, argsRaw,
                                reply, replied)) {
            // 인터셉트됨 — 응답 완료(move) 또는 중계 지연(read).
        } else if (argsRaw.size() > 256 * 1024) {
            // 스펙 §4.1 호출 경로 상한 (result 16KiB는 HandleToolResult가).
            // 256KiB로 상향 (docs/60 §5 백로그 소각, docs/57 §12.6): 워크숍
            // set_script가 256KiB 백스톱을 두고 있어 8KiB 앞단이 병목이었다 —
            // 앱 측 백스톱과 정렬. result 16KiB 캡은 별도 백로그로 유지.
            reply = "{\"ok\":false,\"error\":\"args_too_large\"}";
        } else if (app.empty() || toolName.empty()) {
            reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
        } else {
            // 후보 수집 (스펙 §4.2): 등록된 (app, tool) 조합 역매칭 — 접두
            // 추측 금지. 같은 app의 다중 인스턴스(연결간 중복 등록 허용 —
            // HandleToolRegister 러링)가 복수 후보를 낸다: windowId 지정 =
            // 직행, 미지정+복수 = ambiguous+후보 목록(자기교정).
            std::vector<const AppToolManifest*> cands;
            for (const auto& kv : appToolManifests_) {
                for (const AppToolDef& t : kv.second.tools) {
                    if (kv.second.app == app && t.name == toolName) {
                        cands.push_back(&kv.second);
                        break;
                    }
                }
            }
            // 슬롯 리바인드 (docs/59 §13, 2026-09-20 폰 플로우 실측): 요청자
            // (jkagentd)는 CLI가 턴마다 재스폰하므로 연결 id가 턴마다 바뀌고,
            // 턴 종료의 요청자 회수기(final-review MINOR-2)가 슬롯을 전량
            // 비운다. 생존한 다이얼로그는 모달 가드(§5)에 영구 tool_gone —
            // 폰 플로우의 턴 2 내비게이션(filedlg_navigate/list/choose)이
            // 전면 파산했다. 슬롯이 "완전히" 비어 있고(회수/만료 — 소유
            // 다이얼로그도 기록 없음) 목표 모달 후보의 연결이 생존해 있으면,
            // 제어 채널 호출자를 새 요청자로 리바인드한다. 슬롯에 소유
            // 다이얼로그가 기록돼 있는 진짜 모달 경합은 기존 tool_gone 유지.
            // paramsTaken=true — 원 다이얼로그가 이미 소진했으므로 재전달
            // 금지. waitAsync=true — 리바인드 슬롯은 파킹 승인 항목이 없어
            // 해소는 file.open_result 이벤트 방송으로만 통지(아래 해소 경로).
            if (!cands.empty() && client.IsControlOnly() &&
                pendingFileDialog_.requesterConnId == 0 &&
                pendingFileDialog_.dialogConnId == 0 &&
                pendingFileDialog_.requestId == 0) {
                for (const AppToolManifest* c : cands) {
                    if (!c->modal) continue;
                    bool alive = false;
                    for (const auto& cc : clients_) {
                        if (cc && cc->Id() == c->connId &&
                            !cc->IsDisconnected()) {
                            alive = true;
                            break;
                        }
                    }
                    if (alive) {
                        pendingFileDialog_.requesterConnId = client.Id();
                        pendingFileDialog_.dialogConnId = c->connId;
                        pendingFileDialog_.paramsTaken = true;
                        pendingFileDialog_.waitAsync = true;
                        std::fprintf(stderr,
                                     "filedlg slot rebound: requester=%u dialog=%u\n",
                                     client.Id(), c->connId);
                        break;   // 슬롯 1개 — 첫 생존 모달 후보에 바인드
                    }
                }
            }
            // 모달 후보 선별 (스펙 §5 케이스 ③ — 고아+슬롯 재사용): 슬롯
            // 만료/요청자 회수/재사용 후에도 살아 있는 고아 다이얼로그가 후보에
            // 끼면 정상 슬롯 소유 다이얼로그의 호출까지 ambiguous로 봉쇄된다
            // (아래 단일 분기의 가드는 cands.size()==1에만 도달 — 복수 후보
            // 경로는 미커버였음). 수집 직후 모달 후보를 슬롯 소유로 선별해
            // 뿌리에서 걷어낸다. 판정은 앱이 아니라 슬롯 진실원
            // pendingFileDialog_.dialogConnId가 한다 — 단일 분기 가드와 동일
            // 조건(중복 제거해도 무방하나 최소 변경으로 유지).
            bool orphanPurgedAll = false;
            if (!cands.empty()) {
                std::vector<const AppToolManifest*> owned;
                for (const AppToolManifest* c : cands) {
                    if (!c->modal ||
                        pendingFileDialog_.dialogConnId == c->connId) {
                        owned.push_back(c);
                    }
                }
                orphanPurgedAll = owned.empty();
                cands.swap(owned);
            }
            // windowId 지정 = 그 창 직행 (스펙 §4.2 인스턴스 변별).
            if (hasWindowId && windowIdArg > 0) {
                std::vector<const AppToolManifest*> filtered;
                for (const AppToolManifest* c : cands)
                    if (c->windowId == static_cast<uint32_t>(windowIdArg))
                        filtered.push_back(c);
                cands.swap(filtered);
            }
            if (cands.empty()) {
                // 후보가 있었지만 모달 선별에서 전부 걷어졌으면 tool_gone(기존
                // 가드와 동일 페이로드) — 애초 후보가 없었던 경우는 기존
                // unknown_app_tool 유지(windowId 불일치로 비는 경로 포함).
                reply = (orphanPurgedAll
                             ? "{\"ok\":false,\"error\":\"tool_gone\"}"
                             : "{\"ok\":false,\"error\":\"unknown_app_tool\"}");
            } else if (cands.size() > 1) {
                // 스펙 §4.2: 묵시적 추측 라우팅 금지 — 후보 제시(자기교정).
                std::string list = "[";
                for (size_t i = 0; i < cands.size(); ++i) {
                    if (i) list += ",";
                    list += "{\"windowId\":" +
                            std::to_string(cands[i]->windowId) +
                            ",\"title\":\"" + JsonEsc(cands[i]->title) + "\"}";
                }
                reply = "{\"ok\":false,\"error\":\"ambiguous\",\"candidates\":" +
                        list + "]}";
            } else {
                const AppToolManifest* m = cands.front();
                // 모달 가드 (스펙 §5): 모달 도구는 "다이얼로그가 슬롯을
                // 소유한 동안만" 존재한다. 슬롯 만료(600s)/해소/재사용 후에도
                // 살아 있는 고아 다이얼로그의 도구 호출을 중계 전에 차단한다
                // (docs/48 MAJOR-1 교차결함 동류 — 판정은 앱이 아니라 슬롯
                // 진실원 pendingFileDialog_.dialogConnId가 한다).
                if (m->modal && pendingFileDialog_.dialogConnId != m->connId) {
                    reply = "{\"ok\":false,\"error\":\"tool_gone\"}";
                } else {
                    JKClientConnection* conn = nullptr;
                    for (const auto& c : clients_) {
                        if (c && c->Id() == m->connId && !c->IsDisconnected()) {
                            conn = c.get();
                            break;
                        }
                    }
                    if (!conn) {
                        // 매니페스트는 살아 있지만 연결이 끊김(정리 대기) —
                        // tool_gone과 동일 수명 사건 (스펙 §9).
                        reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
                    } else {
                        // 의미 커서 act (스펙 §3): 커서 선언 앱의 act는 게이트
                        // 기본값 ask(파킹) — permissions.json의 app_tool.* 키
                        // (allow/ask/deny)가 있으면 그 값이 이긴다(스펙 §8
                        // "앱별 act allow 선택지는 permissions.json 운용").
                        AgentDecision gate = AppToolAllowed(
                            app, toolName,
                            (m->cursor.valid && toolName == "act")
                                ? AgentDecision::Ask
                                : AgentDecision::Allow);
                        switch (gate) {
                            case AgentDecision::Deny:
                                // 스펙 §9 게이트 표면 — capture_ask 선례의 "denied".
                                reply = "{\"ok\":false,\"error\":\"denied\"}";
                                break;
                            case AgentDecision::Ask: {
                                // 스펙 §4.3: 기존 승인 파이프라인 재사용 —
                                // close_window ask 패턴 그대로 (구독자 체크 →
                                // approval_unavailable, PendingApproval push +
                                // agent.approval_request 방송, replied=false).
                                // kind="app_tool", name=app+"."+tool,
                                // targetId=windowId(Task 8 하이라이트 소비).
                                bool subscriber = false;
                                for (const auto& c : clients_) {
                                    if (c && c->AgentEventSubscriber() &&
                                        !c->IsDisconnected()) {
                                        subscriber = true;
                                        break;
                                    }
                                }
                                if (!subscriber) {
                                    reply = "{\"ok\":false,"
                                            "\"error\":\"approval_unavailable\"}";
                                    break;
                                }
                                if (ApprovalParkingFull(client.Id())) {
                                    reply = "{\"ok\":false,"
                                            "\"error\":\"approval_overflow\"}";
                                    break;
                                }
                                PendingApproval p;
                                p.kind = "app_tool";
                                p.requestId = nextApprovalId_++;
                                p.queryId = queryId;
                                p.requesterId = client.Id();
                                p.targetId = m->windowId;
                                p.expiresAt = std::time(nullptr) + 60;
                                // 배너 표기(스펙 §5 1단)는 p.name 하나를 소스로
                                // 삼는다 — 그리는 쪽에서 app/tool을 다시 조립하지
                                // 않게 app+"."+tool을 파킹 시점에 확정.
                                p.name = app + "." + toolName;
                                // 의미 커서 (스펙 2026-09-22-semantic-cursor
                                // §3 act, Task 2): 파킹 시점에 최신 선언으로
                                // 셀 rect를 고정한다 — 배너 문구
                                // "<app>.<kind> at (r,c)"와 승인 하이라이트의
                                // 진실원. 유효성(kind enum/인자/격자 내)은
                                // HandleCursorAppTool이 앱 도달 전 검증하므로
                                // 이 분기는 항상 성공한다(파싱 실패 시 기존
                                // <app>.<tool> 유지 — 방어적 폴백, rect 고정
                                // 없이는 승인 시점 재검증이 선언 불변으로
                                // 통과시켜 릴레이가 원문을 때리므로 안전).
                                if (m->cursor.valid && toolName == "act") {
                                    jk::agent::AgentJson a(argsRaw.empty()
                                                               ? "{}"
                                                               : argsRaw);
                                    std::string kind;
                                    int row = 0, col = 0;
                                    if (a.GetStr("kind", kind) &&
                                        a.GetInt("row", row) &&
                                        a.GetInt("col", col)) {
                                        // reset은 위치 무의미한 정의 전이
                                        // (스펙 §4 리셋=좌상단) — 앱도 좌표를
                                        // 무시하므로 셀 링/배너 좌표 표기 없음
                                        // (폰 실전 1판: "reset at (0,0)"이
                                        // 오해를 산 실측 — docs/64 §8). LLM 인자
                                        // 스키마는 계약 유지(row/col 필수 —
                                        // 자리 채움값만 배너에 안 나온다).
                                        const bool positionless =
                                            (kind == "reset");
                                        p.semCell = !positionless;
                                        p.semKind = kind;
                                        p.semRow = row;
                                        p.semCol = col;
                                        p.semRectX =
                                            m->cursor.originX +
                                            col * m->cursor.cellW;
                                        p.semRectY =
                                            m->cursor.originY +
                                            row * m->cursor.cellH;
                                        p.semRectW = m->cursor.cellW;
                                        p.semRectH = m->cursor.cellH;
                                        p.name = positionless
                                            ? app + "." + kind
                                            : app + "." + kind + " at (" +
                                                  std::to_string(row) + "," +
                                                  std::to_string(col) + ")";
                                    }
                                }
                                p.appToolApp = app;
                                p.appToolTool = toolName;
                                p.appToolArgs = argsRaw;
                                p.appToolConnId = m->connId;
                                // 스펙 §5 3단: 대상 창 "조작 전" 썸네일 — 캡처
                                // 실패는 비치명(thumb 필드만 생략, 승인 흐름은
                                // 계속). 제어 연결 매니페스트(windowId=0)는 레이어
                                // 부재로 자연 실패한다. 파일명 규약 =
                                // capture_window의 shot_<ts>_<id>.png와 동일 형식.
                                // 서버 루프 스레드에서의 GPU readback은
                                // capture_window가 이미 이 스레드에서 하는 비용과
                                // 같다(스레드 추가 없음).
                                std::string thumb;
                                {
                                    const std::string sdir =
                                        StateDir() + "\\screenshots";
                                    CreateDirectoryA(sdir.c_str(), nullptr);
                                    char tbuf[512];
                                    std::snprintf(tbuf, sizeof(tbuf),
                                                  "%s\\approval_%lld_%u.png",
                                                  sdir.c_str(),
                                                  static_cast<long long>(
                                                      std::time(nullptr)),
                                                  p.requestId);
                                    if (CaptureLayerToPng(m->windowId, tbuf))
                                        thumb = tbuf;
                                }
                                char buf[2048];
                                std::snprintf(buf, sizeof(buf),
                                              "{\"topic\":\"agent.approval_request\","
                                              "\"request\":%u,\"tool\":\"app_tool\","
                                              "\"kind\":\"app_tool\","
                                              "\"name\":\"%s\","
                                              "\"target_id\":%u,\"title\":\"%s\","
                                              "\"target\":{\"app\":\"%s\","
                                              "\"tool\":\"%s\",\"windowId\":%u,"
                                              "\"title\":\"%s\"}"
                                              "%s%s%s,"
                                              "\"ts\":%lld}",
                                              p.requestId, JsonEsc(p.name).c_str(),
                                              m->windowId,
                                              JsonEsc(m->title).c_str(),
                                              JsonEsc(app).c_str(),
                                              JsonEsc(toolName).c_str(),
                                              m->windowId,
                                              JsonEsc(m->title).c_str(),
                                              thumb.empty() ? "" : ",\"thumb\":\"",
                                              thumb.empty()
                                                  ? ""
                                                  : JsonEsc(thumb).c_str(),
                                              thumb.empty() ? "" : "\"",
                                              static_cast<long long>(
                                                  std::time(nullptr)) * 1000);
                                pendingApprovals_.push_back(p);
                                PushAgentEventJson(buf);
                                replied = false;  // 승인 resolve(또는 만료)가 응답
                                break;
                            }
                            case AgentDecision::Allow: {
                                // 스펙 §9 시퀀싱: 즉시 중계 + 타이머 시작. 응답은
                                // 앱의 AgentToolResult(HandleToolResult)가 queryId로
                                // 회송 — replied=false. expiresAt은 여기서 설정하므로
                                // 승인 대기 시간이 타임아웃을 갉지 않는다.
                                const uint32_t reqId = nextToolReqId_++;
                                InflightAppTool inf;
                                inf.reqId = reqId;
                                inf.queryId = queryId;
                                inf.requesterConnId = client.Id();
                                inf.targetConnId = conn->Id();
                                inf.windowId = m->windowId;
                                inf.expiresAt = std::time(nullptr) + 10;
                                // MINOR-3 — reset act 즉시 allow 경로: ok 회송
                                // 시 커서 정의 전이((0,0)) 리셋(스펙 §4).
                                if (m->cursor.valid && toolName == "act") {
                                    jk::agent::AgentJson a(argsRaw.empty()
                                                               ? "{}"
                                                               : argsRaw);
                                    std::string kind;
                                    if (a.GetStr("kind", kind))
                                        inf.resetCursorOnOk =
                                            (kind == "reset");
                                }
                                inflightAppTools_[reqId] = inf;
                                std::string callJson = "{\"app\":\"" + JsonEsc(app) +
                                    "\",\"tool\":\"" + JsonEsc(toolName) +
                                    "\",\"args\":" +
                                    (argsRaw.empty() ? "{}" : argsRaw) + "}";
                                ipc::WriteAgentToolCall(conn->Transport(), reqId,
                                                        callJson);
                                replied = false;  // HandleToolResult가 응답한다
                                break;
                            }
                        }
                    }
                }  // 모달 가드 else
            }
        }
    } else if (tool == "close_window") {
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        }
        if (!target || target->IsControlOnly()) {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        } else {
            switch (AgentToolAllowed("close_window")) {
                case AgentDecision::Allow: {
                    // Server-initiated close: the client's read loop treats
                    // Close as quit; the disconnect cleanup path then removes
                    // the layer and fires window.destroyed.
                    ipc::WriteMessage(target->Transport(), ipc::MsgType::Close,
                                      std::vector<uint8_t>{});
                    reply = "{\"ok\":true}";
                    break;
                }
                case AgentDecision::Ask: {
                    // M2 chat inline approval: park the query and broadcast
                    // the request — the reply goes out only when the approval
                    // resolves (or the expiry scan answers with timeout).
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = target->Id();
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"close_window\","
                                  "\"target_id\":%u,\"title\":\"%s\",\"ts\":%lld}",
                                  p.requestId, p.targetId,
                                  JsonEsc(target->Title()).c_str(),
                                  static_cast<long long>(std::time(nullptr)) * 1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    // M2a: the server-side gate denied it — permissions.json
                    // is the approval act (same file the broker reads).
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "trust_request") {
        // Script trust gate (docs/37 spec): jktriggers asks before the first
        // eval of an unknown fingerprint. Default permission is "ask" — the
        // same inline-approval pipeline as close_window. The server only
        // relays the decision; the loader owns trust.json.
        std::string name, origin, fingerprint;
        req.GetObjStr("args", "name", name);
        req.GetObjStr("args", "origin", origin);
        req.GetObjStr("args", "fingerprint", fingerprint);
        // Trim the name — whitespace-only is still missing (final-review fix:
        // validate before parking an approval, so no face can feed a
        // malformed fingerprint/origin into the broadcast payload).
        {
            const size_t b = name.find_first_not_of(" \t\r\n");
            if (b == std::string::npos) {
                name.clear();
            } else {
                name = name.substr(b, name.find_last_not_of(" \t\r\n") - b + 1);
            }
        }
        if (name.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else if (name.size() > 96) {
            // docs/38: bound the display name before it reaches the approval
            // broadcast payload and the parked PendingApproval (same shape as
            // bad_fingerprint — validated before parking).
            reply = "{\"ok\":false,\"error\":\"bad_name\"}";
        } else if (!ValidFingerprint(fingerprint)) {
            reply = "{\"ok\":false,\"error\":\"bad_fingerprint\"}";
        } else if (origin != "dev" && origin != "package") {
            reply = "{\"ok\":false,\"error\":\"bad_origin\"}";
        } else {
            switch (AgentToolAllowed("trust_request")) {
                case AgentDecision::Allow:
                    // permissions.json says allow — every script is trusted.
                    reply = "{\"ok\":true}";
                    break;
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "trust_request";
                    p.name = name;
                    p.origin = origin;
                    p.fingerprint = fingerprint;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[1024];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"trust_request\","
                                  "\"kind\":\"trust_request\",\"name\":\"%s\","
                                  "\"origin\":\"%s\",\"fingerprint\":\"%s\","
                                  "\"ts\":%lld}",
                                  p.requestId, JsonEsc(name).c_str(),
                                  JsonEsc(origin).c_str(),
                                  JsonEsc(fingerprint).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "permission_set") {
        // 스펙 §2.2: 매트릭스 쓰기 — AgentToolAllowed("permission_set")은
        // 파일값 무시하고 항상 Ask(하드코딩, §2.2 2단 우회 봉쇄). 검증은
        // 파킹 전(trust_request의 bad_* 선례).
        std::string permTool, decision;
        req.GetObjStr("args", "tool", permTool);
        req.GetObjStr("args", "decision", decision);
        bool known = false;
        for (const AgentPermRow& r : kPermMatrix) {
            if (permTool == r.tool) { known = true; break; }
        }
        if (permTool.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_tool\"}";
        } else if (!known) {
            reply = "{\"ok\":false,\"error\":\"unknown_tool\"}";
        } else if (decision != "allow" && decision != "ask" &&
                   decision != "deny") {
            reply = "{\"ok\":false,\"error\":\"bad_decision\"}";
        } else if (permTool == "permission_set") {
            // fixed-ask 행은 WritePermissionsEntry가 스킵하므로 승인해도
            // 파일이 불변 — written:true 거짓 보고 대신 즉답 거절(리뷰 MINOR).
            reply = "{\"ok\":false,\"error\":\"bad_target\"}";
        } else {
            switch (AgentToolAllowed("permission_set")) {
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,"
                                "\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "permission_set";
                    p.permTool = permTool;
                    p.permDecision = decision;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"permission_set\","
                                  "\"kind\":\"permission_set\","
                                  "\"target_tool\":\"%s\","
                                  "\"decision\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(permTool).c_str(),
                                  decision.c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;   // 해소 시 답신
                    break;
                }
                case AgentDecision::Allow:
                case AgentDecision::Deny:
                default:
                    // 도달하지 않는다(고정 Ask) — 방어선으로 deny 유지.
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "trust_revoke") {
        // 스펙 §2.3: 신뢰 해지 — 안전 방향이지만 무게이트는 신뢰 저장소
        // 전면 소각 DoS 통로. 기본 Ask, 파일로 allow/deny 변경 가능(해지는
        // 권한 부여가 아니라 2단 우회 위험이 없다). 파킹 전 검증+실측.
        std::string fingerprint;
        req.GetObjStr("args", "fingerprint", fingerprint);
        std::string rec, name;
        if (fingerprint.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_fingerprint\"}";
        } else if (!ValidFingerprint(fingerprint)) {
            reply = "{\"ok\":false,\"error\":\"bad_fingerprint\"}";
        } else {
            std::string trustText;
            char exePath[1024] = {};
            GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
            std::string dir = exePath;
            const size_t dslash = dir.find_last_of("\\/");
            if (dslash != std::string::npos) dir = dir.substr(0, dslash);
            if (std::FILE* f = std::fopen(
                    (dir + "\\state\\trust.json").c_str(), "rb")) {
                // 전체 읽기 (docs/53 §9 잔여 — RevokeTrustRecord와 동일 근거):
                // 64KB 캡이면 뒤쪽 레코드가 not_found로 미끄러져 해지가
                // RMW까지 못 간다. 8MiB 상한 = 이상 파일 가드.
                std::fseek(f, 0, SEEK_END);
                const long tsz = std::ftell(f);
                std::fseek(f, 0, SEEK_SET);
                if (tsz > 0) {
                    const size_t tcap = std::min<size_t>(
                        static_cast<size_t>(tsz), 8u * 1024 * 1024);
                    std::vector<char> tbuf(tcap + 1, '\0');
                    const size_t tn = std::fread(tbuf.data(), 1, tcap, f);
                    tbuf[tn] = '\0';
                    trustText = tbuf.data();
                }
                std::fclose(f);
            }
            if (!TrustRecordText(trustText, fingerprint, rec)) {
                reply = "{\"ok\":false,\"error\":\"not_found\"}";
            } else {
                jk::agent::AgentJson(rec.c_str()).GetStr("name", name);
                switch (AgentToolAllowed("trust_revoke")) {
                    case AgentDecision::Ask: {
                        bool subscriber = false;
                        for (auto& c : clients_) {
                            if (c && c->AgentEventSubscriber() &&
                                !c->IsDisconnected()) {
                                subscriber = true;
                                break;
                            }
                        }
                        if (!subscriber) {
                            reply = "{\"ok\":false,"
                                    "\"error\":\"approval_unavailable\"}";
                            break;
                        }
                        if (ApprovalParkingFull(client.Id())) {
                            reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                            break;
                        }
                        PendingApproval p;
                        p.kind = "trust_revoke";
                        p.name = name.empty() ? "script" : name;
                        p.fingerprint = fingerprint;
                        p.requestId = nextApprovalId_++;
                        p.queryId = queryId;
                        p.requesterId = client.Id();
                        p.targetId = 0;
                        p.expiresAt = std::time(nullptr) + 60;
                        char buf[1024];
                        std::snprintf(buf, sizeof(buf),
                                      "{\"topic\":\"agent.approval_request\","
                                      "\"request\":%u,\"tool\":\"trust_revoke\","
                                      "\"kind\":\"trust_revoke\",\"name\":\"%s\","
                                      "\"fingerprint\":\"%s\",\"ts\":%lld}",
                                      p.requestId, JsonEsc(p.name).c_str(),
                                      fingerprint.c_str(),
                                      static_cast<long long>(
                                          std::time(nullptr)) * 1000);
                        pendingApprovals_.push_back(p);
                        PushAgentEventJson(buf);
                        replied = false;
                        break;
                    }
                    case AgentDecision::Allow:
                        // 파일이 allow로 명시한 설치 — 즉시 해지(사용자가
                        // 매트릭스에서 그렇게 정한 것).
                        {
                            const std::string err =
                                RevokeTrustRecord(fingerprint);
                            reply = err.empty()
                                ? "{\"ok\":true,\"written\":true,"
                                  "\"restart_needed\":true}"
                                : "{\"ok\":false,\"error\":\"" + err + "\"}";
                        }
                        break;
                    case AgentDecision::Deny:
                    default:
                        reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                        break;
                }
            }
        }
    } else if (tool == "launch_app") {
        std::string app, jkx;
        req.GetObjStr("args", "app", app);
        req.GetObjStr("args", "jkx", jkx);
        // 런치 진실성 (2026-09-24 LLM 실전 발각): 스폰 전 존재 검증. 스폰은
        // 비동기 사망한다 — app은 jkapp_<app>.dll이 없으면 자식이 즉시 exit,
        // jkx는 경로가 없으면 jkx.Open 실패 exit — 그런데도 무조건 ok:true는
        // "accepted 후 침묵"(docs/59 §13 폰 플로우 실측) 동형 결함이었다.
        // LLM이 스키마의 jkx 키를 골라 죽은 스폰을 ok:true로 받고 침묵하는
        // 실측이 정확히 이 구멍이다.
        //   app: jkapp_<app>.dll 존재 검사(런처와 같은 exeDir 기준) — 단
        //        terminal:/filedlg: 접두 관례(스폰 전용 경로)는 검사 면제.
        //   jkx: 주어진 경로 우선, 없으면 exeDir/apps/<bare>.jkx 폴백 해석
        //        (런처의 apps/ 열거와 같은 기준 — bare 이름을 쓰는 LLM을
        //        살리는 쪽). 둘 다 없으면 unknown_jkx.
        char exePath[1024] = {};
        std::string exeDir;
        if (GetModuleFileNameA(nullptr, exePath, sizeof(exePath)) > 0) {
            char* lastSlash = exePath;
            for (char* p = exePath; *p; ++p) {
                if (*p == '\\' || *p == '/') lastSlash = p;
            }
            *lastSlash = '\0';
            exeDir = exePath;
        }
        auto fileExistsFn = [](const std::string& p) {
            return GetFileAttributesA(p.c_str()) != kInvalidFileAttributes;
        };
        if (!app.empty()) {
            const bool prefixed = app.find(':') != std::string::npos;
            const std::string dllPath =
                exeDir + "\\jkapp_" + app + ".dll";
            if (prefixed || exeDir.empty() || fileExistsFn(dllPath)) {
                // docs/35: pair the capture overlay with the client that asked
                // for it (see overlaySpawner_ member comment).
                pendingSnapSpawnerConnId_ = (app == "snap") ? client.Id() : 0;
                SpawnClient(app.c_str(), false);
                reply = "{\"ok\":true}";
            } else {
                // app→jkx 폴백 (2026-09-24 폰 눈확인 발각): 워크숍 등 .jkx
                // 패키지 앱을 스키마의 내장 앱 이름 목록과 혼동해
                // {"app":"workshop"}으로 부르는 실측이 있다 — jkapp_<app>.dll이
                // 없어도 apps/<app>.jkx가 있으면 컨테이너로 스폰해 두 호출
                // 형태를 모두 살린다(설명 드리프트가 LLM을 막히게 하지 않는다).
                const std::string jkxCandidate =
                    exeDir + "\\apps\\" + app + ".jkx";
                if (fileExistsFn(jkxCandidate)) {
                    SpawnClient(jkxCandidate.c_str(), true);
                    reply = "{\"ok\":true,\"via\":\"jkx\"}";
                } else {
                    reply = "{\"ok\":false,\"error\":\"unknown_app\",\"app\":\"" +
                            app +
                            "\",\"hint\":\"not a built-in app and no apps/<app>"
                            ".jkx package exists — use jkx with a path or a "
                            "bare package name\"}";
                }
            }
        } else if (!jkx.empty()) {
            // 경로 표기 변주 수용 (2026-09-24 폰 실전 2차 발각): 같은 패키지를
            // LLM이 "workshop" / "workshop.jkx" / "apps/workshop.jkx"로 번갈아
            // 불렀다 — bare 이름만 해석하던 첫 폴백은 나머지 두 형태를
            // unknown_jkx로 죽였다(원본 트랜스크립트 실측). 정규화:
            // '/'→'\\', 끝 .jkx 탈락, 선두 apps\ 탈락 후 후보 4종 순차 검사.
            std::string norm = jkx;
            for (char& ch : norm) {
                if (ch == '/') ch = '\\';
            }
            const size_t n = norm.size();
            if (n > 4 && norm[n - 4] == '.' &&
                (norm[n - 3] == 'j' || norm[n - 3] == 'J') &&
                (norm[n - 2] == 'k' || norm[n - 2] == 'K') &&
                (norm[n - 1] == 'x' || norm[n - 1] == 'X')) {
                norm = norm.substr(0, n - 4);
            }
            if (norm.rfind("apps\\", 0) == 0) norm = norm.substr(5);
            std::string resolved;
            const std::string candidates[4] = {
                jkx,                                   // 원문 (절대 경로 등)
                exeDir + "\\" + jkx,                   // exeDir 기준 원문
                exeDir + "\\apps\\" + norm + ".jkx",   // 정규화 이름
                exeDir + "\\" + norm + ".jkx",
            };
            for (const auto& c : candidates) {
                if (!c.empty() && fileExistsFn(c)) {
                    resolved = c;
                    break;
                }
            }
            if (resolved.empty()) {
                reply = "{\"ok\":false,\"error\":\"unknown_jkx\",\"jkx\":\"" +
                        jkx + "\"}";
            } else {
                SpawnClient(resolved.c_str(), true);
                reply = "{\"ok\":true}";
            }
        } else {
            reply = "{\"ok\":false,\"error\":\"missing_app\"}";
        }
    } else if (tool == "send_input") {
        // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): 트랙 B 조작
        // 수단 — 대상 창에 합성 입력. ask 기본 게이트(run_console_app와 같은
        // inline-approval 파이프라인, 승인 시점 원 요청 재실행 = files_access
        // 선례). 원문 args는 승인 재실행을 위해 파킹에 함께 저장한다.
        std::string rawArgs;
        req.GetRaw("args", rawArgs);
        jk::agent::AgentJson args(rawArgs);
        SendInputOp op;
        const std::string buildErr =
            args.ok() ? BuildSendInputOp(args, &op) : "bad_args";
        if (!buildErr.empty()) {
            reply = "{\"ok\":false,\"error\":\"" + buildErr + "\"}";
        } else {
            switch (AgentToolAllowed("send_input")) {
                case AgentDecision::Allow: {
                    const std::string ex = ExecuteSendInputOp(op);
                    reply = ex.empty() ? "{\"ok\":true,\"sent\":true}"
                                       : "{\"ok\":false,\"error\":\"" + ex + "\"}";
                    break;
                }
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "send_input";
                    p.name = op.op;           // 승인 스트립 표시용 조작명
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = op.target;
                    p.sendArgs = rawArgs;     // 승인 시점 원 요청 재실행 원문
                    p.expiresAt = std::time(nullptr) + 60;
                    // 대상 식별 (최종리뷰 Important 1): close_window 선례 형태
                    // (:3262) — 승인 스트립이 조작(op)만이 아니라 어느 창에
                    // 가해지는지 보여야 한다. 해소 실패 시 빈 문자열.
                    std::string targetTitle;
                    for (auto& c : clients_) {
                        if (c && c->Id() == op.target &&
                            !c->IsDisconnected()) {
                            targetTitle = c->Title();
                            break;
                        }
                    }
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"send_input\","
                                  "\"name\":\"%s\",\"target_id\":%u,"
                                  "\"title\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(op.op).c_str(),
                                  p.targetId, JsonEsc(targetTitle).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // 승인 해소 시 응답
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "run_console_app") {
        // P4 SDK §5: 에이전트가 콘솔 앱(apps/<name>/manifest.json)을 스폰.
        // ask 기본 게이트 — close_window/trust_request와 같은 inline-approval
        // 파이프라인. target 창이 없으므로 targetId=0, 식별자는 앱 name.
        std::string name;
        req.GetObjStr("args", "name", name);
        std::string cmd, dir, fp;
        if (name.empty() || name.size() > 64) {
            reply = "{\"ok\":false,\"error\":\"bad_name\"}";
        } else if (!shell_ ||
                   !shell_->ConsoleAppInfo(name, cmd, dir, fp)) {
            reply = "{\"ok\":false,\"error\":\"unknown_app\"}";
        } else {
            switch (AgentToolAllowed("run_console_app")) {
                case AgentDecision::Allow:
                    SpawnConsoleApp(cmd, dir, name);
                    reply = "{\"ok\":true}";
                    break;
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "run_console_app";
                    p.name = name;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[512];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"run_console_app\","
                                  "\"name\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(name).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "theme_set") {
        // P3 hot-swap (docs/52): write theme.json (the same truth the boot
        // loader and every client's 500ms mtime poll read) then swap
        // in-process. Paint-time consumers (shell, JKDC default args) follow
        // instantly; ctor-captured widget tokens re-capture via the client
        // poll's ApplyTheme walk. Allow by default — appearance only.
        std::string preset;
        req.GetObjStr("args", "preset", preset);
        const jk::theme::JKTheme* t = nullptr;
        if (preset == "light") t = &jk::theme::kLight;
        else if (preset == "classic") t = &jk::theme::kClassic;
        else if (preset == "dark") t = &jk::theme::kDefault;
        if (!t) {
            reply = "{\"ok\":false,\"error\":\"bad_preset\"}";
        } else {
            jk::theme::setTheme(t);
            if (!jk::theme::WriteThemePresetFile(preset)) {
                // Swap happened in-process but the truth file didn't land —
                // clients' mtime poll would never follow. Fail loudly
                // instead of replying ok (docs/52 review MINOR).
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                char buf[96];
                std::snprintf(buf, sizeof(buf),
                              "{\"ok\":true,\"preset\":\"%s\"}",
                              preset.c_str());
                reply = buf;
            }
        }
    } else if (tool == "settings_read") {
        // 스펙 2026-09-18-settings-hub §2.2: 현재 설정 수집 — B 진화 씨앗
        // 봉투(key/kind/value). bool은 int 0/1로 직렬화(AgentJson 파서
        // 계약), ts는 epoch 초. 수집 원천: theme.json(exe 옆),
        // state/triggers.json, state/idle_minutes, 서버 KV 멤버,
        // state/layout_*.json, receipts.jsonl 꼬리.
        std::string out = "{\"ok\":true,\"settings\":[";
        // 2048: text.font_path 300자가 JsonEsc 제어문자 6배 확장({ 등)까지
        // 갈 수 있다 — 640은 조용한 절단으로 봉투 전체를 파산시킨다.
        char item[2048];
        // theme.current: theme.json의 preset(부트 로더 진실원). 파일은 exe
        // 옆 — jk::theme::DefaultThemePath() (StateDir 아님).
        {
            std::string preset = "dark";
            std::FILE* f =
                std::fopen(jk::theme::DefaultThemePath().c_str(), "rb");
            if (f) {
                std::string text;
                char chunk[4096];
                size_t n;
                while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
                    text.append(chunk, n);
                std::fclose(f);
                jk::agent::AgentJson json(text);
                std::string p;
                if (json.ok() && json.GetStr("preset", p) && !p.empty())
                    preset = p;
            }
            std::snprintf(item, sizeof(item),
                          "{\"key\":\"theme.current\",\"kind\":\"string\","
                          "\"value\":\"%s\"}",
                          JsonEsc(preset).c_str());
            out += item;
        }
        // text.font_path (docs/63 §4): 서버 멤버 — 기동 시 KV+기본값 합성
        // (빈 문자열 = 오버라이드 없음, GUI가 기본 경로를 표시).
        std::snprintf(item, sizeof(item),
                      ",{\"key\":\"text.font_path\",\"kind\":\"string\","
                      "\"value\":\"%s\"}",
                      JsonEsc(textFontPath_).c_str());
        out += item;
        // text.font_fallback (docs/63 §6 2단계): 보조 폰트 체인 — 빈 문자열 =
        // 미설정(체인 없음).
        std::snprintf(item, sizeof(item),
                      ",{\"key\":\"text.font_fallback\",\"kind\":\"string\","
                      "\"value\":\"%s\"}",
                      JsonEsc(textFontFallback_).c_str());
        out += item;
        // text.font_scale (docs/63 §6 Task 3): 셀 확대 배율 문자열 float —
        // 미설정은 기본 "1.0"(멤버 초기값).
        std::snprintf(item, sizeof(item),
                      ",{\"key\":\"text.font_scale\",\"kind\":\"string\","
                      "\"value\":\"%s\"}",
                      JsonEsc(textFontScale_).c_str());
        out += item;
        // triggers: state/triggers.json 플래그(trigger_toggle의 진실원).
        {
            std::FILE* f =
                std::fopen((StateDir() + "\\triggers.json").c_str(), "rb");
            if (f) {
                std::string text;
                char chunk[8192];
                size_t n;
                while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
                    text.append(chunk, n);
                std::fclose(f);
                jk::agent::AgentJson json(text);
                int cnt = 0;
                if (json.ok() && json.GetArraySize("triggers", cnt)) {
                    for (int i = 0; i < cnt && i < 64; ++i) {
                        std::string nm;
                        int en = 1;
                        if (!json.GetArrStr("triggers", i, "name", nm)) continue;
                        json.GetArrInt("triggers", i, "enabled", en);
                        std::snprintf(item, sizeof(item),
                                      ",{\"key\":\"trigger.%s\",\"kind\":"
                                      "\"bool\",\"value\":%d}",
                                      JsonEsc(nm).c_str(), en ? 1 : 0);
                        out += item;
                    }
                }
            }
        }
        // idle_minutes: state/idle_minutes 파일(trig_idle이 읽는 진실원).
        {
            int idle = 30;
            if (std::FILE* f =
                    std::fopen((StateDir() + "\\idle_minutes").c_str(), "rb")) {
                char buf[32] = {};
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                buf[n] = '\0';
                const int v = std::atoi(buf);
                if (v >= 0) idle = v;
            }
            std::snprintf(item, sizeof(item),
                          ",{\"key\":\"idle_minutes\",\"kind\":\"int\","
                          "\"value\":%d}",
                          idle);
            out += item;
        }
        // 서버 KV 멤버(부팅 로드 — Init의 LoadSettingsKv).
        std::snprintf(
            item, sizeof(item),
            ",{\"key\":\"receipt_retention_days\",\"kind\":\"int\","
            "\"value\":%d},{\"key\":\"audio_master_mute\",\"kind\":\"bool\","
            "\"value\":%d},{\"key\":\"audio_master_volume\",\"kind\":\"int\","
            "\"value\":%d}",
            receiptRetentionDays_, audioMasterMute_ ? 1 : 0, audioMasterVolume_);
        out += item;
        // layouts: state/layout_*.json 열거 — key/value = 레이아웃 이름.
        {
            const std::string dir = StateDir();
            FindFileDataA fd;
            void* h = FindFirstFileA((dir + "\\layout_*.json").c_str(), &fd);
            while (h != kInvalidFindHandle()) {
                std::string name = fd.cFileName;
                // "layout_<name>.json" → <name> (7자 접두, 5자 확장자).
                if (name.size() > 12) {
                    name = name.substr(7, name.size() - 12);
                    std::snprintf(item, sizeof(item),
                                  ",{\"key\":\"layout.%s\",\"kind\":\"string\","
                                  "\"value\":\"%s\"}",
                                  JsonEsc(name).c_str(), JsonEsc(name).c_str());
                    out += item;
                }
                if (!FindNextFileA(h, &fd)) {
                    FindClose(h);
                    break;
                }
            }
        }
        // receipts 통계: 총 행수 + 최근 ts(초). 꼬리 256KiB만 읽는다
        // (read_receipts의 상한 스캔과 같은 규모 — 이상 파일 가드).
        {
            long long rows = 0, lastTs = 0;
            const std::string path = StateDir() + "\\receipts.jsonl";
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (f) {
                std::fseek(f, 0, SEEK_END);
                const long size = std::ftell(f);
                const long start = size > 262144 ? size - 262144 : 0;
                std::fseek(f, start, SEEK_SET);
                std::vector<char> buf(static_cast<size_t>(size - start) + 1);
                const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
                std::fclose(f);
                buf[n] = '\0';
                size_t pos = 0;
                while (pos < n) {
                    const char* begin = buf.data() + pos;
                    const char* nl = static_cast<const char*>(
                        std::memchr(begin, '\n', n - pos));
                    const size_t len =
                        nl ? static_cast<size_t>(nl - begin) : (n - pos);
                    if (len > 2) ++rows;
                    const std::string line(begin, len);
                    const size_t tp = line.find("\"ts\":");
                    if (tp != std::string::npos)
                        lastTs = std::atoll(line.c_str() + tp + 5);
                    if (!nl) break;
                    pos += len + 1;
                }
            }
            std::snprintf(item, sizeof(item),
                          "],\"receipts\":{\"rows\":%lld,\"last_ts\":%lld}",
                          rows, lastTs / 1000);
            out += item;
        }
        out += "}";
        reply = out;
    } else if (tool == "settings_set") {
        // 스펙 §2.2: 화이트리스트 KV. 응답에 적용 후 값(applied) — 클라
        // 단일 신뢰원(fullscreen 응답 규약). capture_allow만 키별 Ask:
        // 파킹(kind "capture_allow") — 에이전트/GUI 불문 전 경로, 승인은
        // jkchat 스트립(§2.2 키별 게이트 — 도구별 askCapable과 달리).
        std::string key;
        // 가드가 읽는 값은 int64 리더로 (docs/57 §13.3 파서 계약).
        int64_t val64 = 0;
        const bool hasInt = req.GetObjInt64("args", "value", val64);
        const int valInt = static_cast<int>(val64);
        req.GetObjStr("args", "key", key);
        if (key.empty()) {
            reply = "{\"ok\":false,\"error\":\"bad_key\"}";
        } else if (key == "idle_minutes") {
            if (!hasInt || val64 < 0 || val64 > 1440) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else {
                std::FILE* f = std::fopen(
                    (StateDir() + "\\idle_minutes").c_str(), "wb");
                if (!f) {
                    reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                } else {
                    std::fprintf(f, "%d", valInt);
                    std::fclose(f);
                    reply = "{\"ok\":true,\"applied\":{\"idle_minutes\":" +
                            std::to_string(valInt) + "}}";
                }
            }
        } else if (key == "receipt_retention_days") {
            // 하한 7 (docs/54 §11 opus M4): receipts.jsonl은 에이전트 도구
            // 호출의 감사 증적 — 어느 경로에서든 하한 아래로 절단 불가.
            // (0 = 무기한은 현재값(관행) — set 불가(§2.2 표). GUI 콤보가
            // 제시하는 최소 프리셋도 7이므로 GUI 기능 손실 없음.)
            if (!hasInt || val64 < 7) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (!WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                        valInt, textFontPath_,
                                        textFontFallback_, textFontScale_)) {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                receiptRetentionDays_ = valInt;
                // KV 성공 후 즉시 정리 — 정리 실패는 reply에 표면화(KV는
                // 이미 적용됐지만 다음 set에서 재정리된다 — 정직 응답).
                reply = PruneReceipts(valInt)
                            ? ("{\"ok\":true,\"applied\":{"
                               "\"receipt_retention_days\":" +
                               std::to_string(valInt) + "}}")
                            : "{\"ok\":true,\"applied\":{\"receipt_"
                              "retention_days\":" + std::to_string(valInt) +
                              "},\"warning\":\"prune_failed\"}";
            }
        } else if (key == "audio_master_mute" || key == "audio_master_volume") {
            if (key == "audio_master_mute" && hasInt &&
                (val64 == 0 || val64 == 1)) {
                audioMasterMute_ = (val64 == 1);
            } else if (key == "audio_master_volume" && hasInt &&
                       val64 >= 0 && val64 <= 100) {
                audioMasterVolume_ = static_cast<int>(val64);
                audioMasterMute_ = false;  // 볼륨 조작은 음소거 해제 의미
            } else {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            }
            if (reply.empty()) {
                if (!WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                     receiptRetentionDays_, textFontPath_,
                                     textFontFallback_, textFontScale_)) {
                    reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                } else {
                    char ev[160];
                    std::snprintf(ev, sizeof(ev),
                                  "{\"topic\":\"audio.master\",\"data\":{"
                                  "\"mute\":%d,\"volume\":%d},\"ts\":%lld}",
                                  audioMasterMute_ ? 1 : 0, audioMasterVolume_,
                                  static_cast<long long>(
                                      std::time(nullptr)) * 1000);
                    PushAgentEventJson(ev);
                    reply = "{\"ok\":true,\"applied\":{\"audio_master_mute\":" +
                            std::string(audioMasterMute_ ? "1" : "0") +
                            ",\"audio_master_volume\":" +
                            std::to_string(audioMasterVolume_) + "}}";
                }
            }
        } else if (key == "text_font_path") {
            // docs/63 §4: 데스크탑 벡터 폰트 경로. 값은 문자열 — 재시작 적용
            // (아틀라스는 기동 시 Init이 JKTextAtlas::ResolveDesktopFontPath로
            // 합성한다). 상한 300자 — LoadSettingsKv·ResolveDesktopFontPath와
            // 같은 캡. 빈 값은 기본 폰트로의 폴백이 아니라 기각(오타 방어 —
            // 되돌리려면 파일 수동 편집이 아니라 기본값 명시를 쓴다).
            std::string valStr;
            if (!req.GetObjStr("args", "value", valStr) || valStr.empty() ||
                valStr.size() > 300) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (!WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                        receiptRetentionDays_, valStr,
                                        textFontFallback_, textFontScale_)) {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                textFontPath_ = valStr;
                reply = std::string("{\"ok\":true,\"applied\":{"
                                    "\"text_font_path\":\"") +
                        JsonEsc(valStr) + "\"},\"note\":\"applies_on_restart\"}";
            }
        } else if (key == "text_font_fallback") {
            // docs/63 §6 2단계: 보조 폰트 체인 경로. 재시작 적용(text_font_path와
            // 동일 — 아틀라스는 기동 시 InitFallback이
            // ResolveDesktopFallbackPath로 합성). 상한 300자 동일. **빈 값은
            // 해제 허용**(font_path와 반대 — 체인의 기본 상태가 "없음"이라
            // 빈 값이 유효한 목표 상태다).
            std::string valStr;
            if (!req.GetObjStr("args", "value", valStr) || valStr.size() > 300) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (!WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                        receiptRetentionDays_, textFontPath_,
                                        valStr, textFontScale_)) {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                textFontFallback_ = valStr;
                reply = std::string("{\"ok\":true,\"applied\":{"
                                    "\"text_font_fallback\":\"") +
                        JsonEsc(valStr) + "\"},\"note\":\"applies_on_restart\"}";
            }
        } else if (key == "text_font_scale") {
            // docs/63 §6 Task 3: 셀 확대 배율(옵트인) — 문자열 float. 범위
            // [1.0, 3.0] 밖·숫자 아님·빈 값은 전부 bad_value(하한 미달을
            // "1.0으로 클램프"하지 않는다 — 오타 방어, font_path 선례).
            // 재시작 적용(CellMetrics()는 기동 시 settings.json 직독, 함수
            // 로컬 static — 실행 중 반영 불가).
            std::string valStr;
            if (!req.GetObjStr("args", "value", valStr) ||
                !ValidFontScale(valStr)) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (!WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                        receiptRetentionDays_, textFontPath_,
                                        textFontFallback_, valStr)) {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                textFontScale_ = valStr;
                reply = std::string("{\"ok\":true,\"applied\":{"
                                    "\"text_font_scale\":\"") +
                        JsonEsc(valStr) + "\"},\"note\":\"applies_on_restart\"}";
            }
        } else if (key == "capture_allow") {
            // §2.2 키별 Ask: value 1=allow, 0=ask. 승인 시점에 두 캡처 도구를
            // 함께 쓴다(approve 분기의 kind "capture_allow" 해소).
            if (!hasInt || (valInt != 0 && valInt != 1)) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else {
                bool subscriber = false;
                for (auto& c : clients_) {
                    // opus 리뷰 M3 (docs/54 §11): 코어가 모든 ImGui 클라를
                    // 구독시키므로 "구독자 존재" 검사는 요청자 자신까지
                    // true가 되어 공허하다. 승인 표면(jkchat류)은 제어
                    // 전용 연결 — 그것만 센다.
                    if (c && c->IsControlOnly() &&
                        c->AgentEventSubscriber() && !c->IsDisconnected()) {
                        subscriber = true;
                        break;
                    }
                }
                if (!subscriber) {
                    reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                } else if (ApprovalParkingFull(client.Id())) {
                    reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                } else {
                    const std::string d = valInt ? "allow" : "ask";
                    PendingApproval p;
                    p.kind = "capture_allow";
                    p.permDecision = d;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"settings_set\","
                                  "\"kind\":\"capture_allow\","
                                  "\"target_tool\":\"capture_window\","
                                  "\"decision\":\"%s\",\"ts\":%lld}",
                                  p.requestId, d.c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // 해소 시 답신
                }
            }
        } else {
            reply = "{\"ok\":false,\"error\":\"bad_key\"}";
        }
    } else if (tool == "open_notify") {
        // docs/33: toggle the notification center — safe UI command, no
        // permission gate (same tier as launch_app).
        if (ToggleClientByTitleUnsafe("Notifications", "notify")) {
            PushWindowListUnsafe();  // taskbar highlight follows the refocus
        }
        reply = "{\"ok\":true}";
    } else if (tool == "save_layout") {
        std::string name;
        if (!req.GetObjStr("args", "name", name) || name.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            std::string snapshot = "{\"name\":\"" + JsonEsc(name) + "\",\"windows\":[";
            bool first = true;
            int count = 0;
            for (auto& c : clients_) {
                if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) continue;
                char item[640];
                std::snprintf(item, sizeof(item),
                    "%s{\"title\":\"%s\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}",
                    first ? "" : ",", JsonEsc(c->Title()).c_str(),
                    c->X(), c->Y(), c->Width(), c->Height());
                snapshot += item;
                first = false;
                ++count;
            }
            snapshot += "]}";
            const std::string path = StateDir() + "\\layout_" + name + ".json";
            std::FILE* f = std::fopen(path.c_str(), "wb");
            if (!f) {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                std::fwrite(snapshot.data(), 1, snapshot.size(), f);
                std::fclose(f);
                reply = "{\"ok\":true,\"count\":" + std::to_string(count) + "}";
            }
        }
    } else if (tool == "restore_layout") {
        std::string name;
        if (!req.GetObjStr("args", "name", name) || name.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            const std::string path = StateDir() + "\\layout_" + name + ".json";
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (!f) {
                reply = "{\"ok\":false,\"error\":\"layout_not_found\"}";
            } else {
                char buf[65536];
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                buf[n] = '\0';
                jk::agent::AgentJson layout(buf);
                int arrLen = 0, restored = 0;
                std::string unmatched;
                if (!layout.ok() || !layout.GetArraySize("windows", arrLen)) {
                    reply = "{\"ok\":false,\"error\":\"bad_layout\"}";
                } else {
                    for (int i = 0; i < arrLen; ++i) {
                        std::string title;
                        int x = 0, y = 0;
                        layout.GetArrStr("windows", i, "title", title);
                        layout.GetArrInt("windows", i, "x", x);
                        layout.GetArrInt("windows", i, "y", y);
                        if (title.empty()) continue;
                        // Match by title — the stable key a layout snapshot
                        // has (surface ids change across restarts).
                        bool matched = false;
                        for (auto& c : clients_) {
                            if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) continue;
                            if (c->Title() != title) continue;
                            c->SetPosition(x, y);
                            if (compositor_) {
                                compositor_->SetLayerPosition(c->Id(), x, y);
                            }
                            matched = true;
                            ++restored;
                            break;
                        }
                        if (!matched) {
                            if (!unmatched.empty()) unmatched += ",";
                            unmatched += "\"" + JsonEsc(title) + "\"";
                        }
                    }
                    reply = "{\"ok\":true,\"restored\":" + std::to_string(restored) +
                            ",\"unmatched\":[" + unmatched + "]}";
                }
            }
        }
    } else if (tool == "publish_event") {
        // M2b trigger scripts: client→subscriber event publishing. Any
        // client may publish; the server stamps the envelope and relays it
        // to every agent-event subscriber. "data" passes through as raw
        // JSON so trigger filters can shape arbitrary payloads.
        std::string topic, data;
        // opus 리뷰 NIT-3 (docs/54 §11): 서버 네임스페이스 토픽은 스크립트
        // publish로 흉내 낼 수 없다 — window.*는 vplayer 전체화면 미러가
        // 자기 id 니들로 수용하고, audio.*는 코어 펌프가 마스터 게인에
        // 적용한다. 스크립트 커스텀 토픽은 이 접두어 밖의 것으로.
        // 리뷰 Task 2 Important-1 수술: 접두 검사가 topic 파싱 전(빈 문자열
        // 상태)에 돌아 c9ba8bf 이래 데드코드 — 검사를 파싱 후 평가하는
        // 람다로 이동해 게이트를 살린다 (file.open_result 스푸핑 봉쇄 —
        // 스펙 2026-09-19-filedlg-voice-nav 결정 6이 이 게이트에 의존).
        auto topicReserved = [](const std::string& t) {
            // 앱 발행 카탈로그 행의 정확-토픽 면제 — 접두 일괄 봉쇄는
            // source:"app" 행의 정당 발행자까지 죽인다(표 주석의 함정 기록).
            // 면제 목록 = 예약 접두 아래 사는 "app" 행: terminal.output(터미널
            // 앱 M2b), agent.notify(jktriggers desktop.notify — 트리거 액션).
            // 접두 예약 자체는 유지: window./audio. 등 특권 서버 소비자
            // (vplayer 미러/코어 펌프)의 스푸핑 봉쇄가 이 게이트의 존재 이유.
            if (t == "terminal.output" || t == "agent.notify") return false;
            for (const char* p : kReservedTopicPrefixes) {
                if (t.compare(0, std::strlen(p), p) == 0) return true;
            }
            return false;
        };
        if (!req.GetObjStr("args", "topic", topic) || topic.empty() ||
            topic.size() > 96 || JsonEsc(topic).size() > 96 ||
            !req.GetObjRaw("args", "data", data) || data.empty() ||
            data.size() > 4096) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else if (topicReserved(topic)) {
            reply = "{\"ok\":false,\"error\":\"reserved_topic\"}";
        } else {
            char ev[4352];
            std::snprintf(ev, sizeof(ev),
                          "{\"topic\":\"%s\",\"data\":%s,\"ts\":%lld}",
                          JsonEsc(topic).c_str(), data.c_str(),
                          static_cast<long long>(
                              std::chrono::duration_cast<
                                  std::chrono::milliseconds>(
                                  std::chrono::system_clock::now()
                                      .time_since_epoch())
                                  .count()));
            // docs/38: connection-level cap — 60 events / 10s (same figure as
            // the local handler cap so the local cap binds first for
            // self-loops — deterministic notify/stop point; spec §2 interaction
            // note). Over-cap events are dropped but answered ok so senders
            // don't turn into retry bombs. The budget counts allowed events
            // only (a drop consumes nothing — same semantics as the jktriggers
            // source cap) and must stay lock-free — this branch runs with
            // clientsMutex_ already held.
            constexpr int kPublishCap = 60;
            constexpr uint64_t kPublishWindowMs = 10000;
            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            PublishBudget& b = publishBudgets_[client.Id()];
            if (nowMs - b.windowStartMs >= kPublishWindowMs) {
                b.windowStartMs = nowMs;
                b.count = 0;
                b.logged = false;
            }
            if (b.count >= kPublishCap) {
                // Over cap: no budget consumption on the drop path.
                if (!b.logged) {
                    b.logged = true;
                    // One log line per connection per window — stdout is the
                    // server log (run_test.sh redirects it, read_log tails it).
                    std::printf("[server] publish_event rate-capped (conn %u)\n",
                                client.Id());
                    std::fflush(stdout);
                }
                reply = "{\"ok\":true,\"dropped\":1}";
            } else {
                ++b.count;
                PushAgentEventJson(ev);
                reply = "{\"ok\":true}";
            }
        }
    } else if (tool == "capture_window") {
        // docs/35: read the client's shm surface (RGBA32) directly — the
        // app framebuffer, no screen DPI involvement. Safe tier (same as
        // launch_app).
        // 게이트 (docs/54 §11 opus M2 픽스): permissions.json 파일값이
        // 서버에서 강제된다 — "ask"면 거부(capture_ask; 설정 허브 캡처
        // 스위치의 승인 파킹이 "allow"로 뒤집을 때까지), "deny"면
        // permission_denied, 없음/allow는 docs/35 안전 계층 그대로.
        const AgentDecision gate = AgentToolAllowed("capture_window");
        if (gate == AgentDecision::Ask) {
            reply = "{\"ok\":false,\"error\":\"capture_ask\"}";
        } else if (gate == AgentDecision::Deny) {
            reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else {
        int id = 0;
        req.GetObjInt("args", "id", id);
        // 사전 검사는 원본 그대로 유지 — 레이어 부재는 window_not_found,
        // PNG 기록 실패만 write_failed(스펙 §5 3단 헬퍼 추출, 동작 불변).
        JKCompositorLayer* layer =
            compositor_ ? compositor_->FindLayerById(static_cast<uint32_t>(id))
                        : nullptr;
        if (!layer || !layer->Pixels() || layer->Width() <= 0 ||
            layer->Height() <= 0) {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        } else {
            const std::string dir = StateDir() + "\\screenshots";
            CreateDirectoryA(dir.c_str(), nullptr);
            const long long ts =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
            const std::string path = dir + "\\shot_" + std::to_string(ts) +
                                     "_" + std::to_string(id) + ".png";
            if (CaptureLayerToPng(static_cast<uint32_t>(id), path)) {
                reply = "{\"ok\":true,\"path\":\"" + JsonEsc(path) + "\"}";
            } else {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            }
        }
        }
    } else if (tool == "capture_region") {
        // docs/35: composited-frame readback (SDL_RenderReadPixels), then
        // crop. The requester's own layer is hidden for the readback so a
        // rubber-band overlay does not appear in its own screenshot. Args
        // are logical desktop points — the framebuffer is physical pixels.
        // 게이트: capture_window와 동일 (docs/54 §11 opus M2 픽스).
        const AgentDecision gateR = AgentToolAllowed("capture_region");
        if (gateR == AgentDecision::Ask) {
            reply = "{\"ok\":false,\"error\":\"capture_ask\"}";
        } else if (gateR == AgentDecision::Deny) {
            reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else {
        int x = 0, y = 0, w = 0, h = 0;
        req.GetObjInt("args", "x", x);
        req.GetObjInt("args", "y", y);
        req.GetObjInt("args", "w", w);
        req.GetObjInt("args", "h", h);
        const int outW = compositor_ ? compositor_->OutputWidth() : 0;
        const int outH = compositor_ ? compositor_->OutputHeight() : 0;
        if (outW <= 0 || outH <= 0 || w <= 0 || h <= 0) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else {
            // Intersect with the output bounds (logical points).
            int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
            int x1 = x + w > outW ? outW : x + w;
            int y1 = y + h > outH ? outH : y + h;
            if (x0 >= x1 || y0 >= y1) {
                reply = "{\"ok\":false,\"error\":\"bad_request\"}";
            } else {
                const float scale = compositor_->OutputScale();
                const int fx = static_cast<int>(x0 * scale);
                const int fy = static_cast<int>(y0 * scale);
                const int fw = static_cast<int>((x1 - x0) * scale);
                const int fh = static_cast<int>((y1 - y0) * scale);
                const int fullW =
                    static_cast<int>(outW * scale);
                const int fullH = static_cast<int>(outH * scale);

                JKCompositorLayer* self =
                    compositor_->FindLayerById(client.Id());
                const bool hideSelf = self && self->IsVisible();
                if (hideSelf) self->SetVisible(false);
                // Hide the viewer that launched this overlay too (docs/35):
                // it is the capture UI and must not appear in its own shot.
                JKCompositorLayer* spawner = nullptr;
                auto sp = overlaySpawner_.find(client.Id());
                if (sp != overlaySpawner_.end() && sp->second != client.Id()) {
                    spawner = compositor_->FindLayerById(sp->second);
                }
                const bool hideSpawner = spawner && spawner->IsVisible();
                if (hideSpawner) spawner->SetVisible(false);
                Composite(false);   // clear + draw, no present
                std::vector<uint8_t> full(
                    static_cast<size_t>(fullW) * fullH * 4);
                SDL_Rect fullRect{0, 0, fullW, fullH};
                const int got = SDL_RenderReadPixels(
                    renderer_, &fullRect, SDL_PIXELFORMAT_RGBA32,
                    full.data(), fullW * 4);
                if (hideSpawner) spawner->SetVisible(true);
                if (hideSelf) self->SetVisible(true);
                Composite(false);
                if (got != 0) {
                    reply = "{\"ok\":false,\"error\":\"read_failed\"}";
                } else {
                    // Row-wise crop into the capture buffer.
                    std::vector<uint8_t> crop(
                        static_cast<size_t>(fw) * fh * 4);
                    for (int row = 0; row < fh; ++row) {
                        std::memcpy(crop.data() +
                                        static_cast<size_t>(row) * fw * 4,
                                    full.data() +
                                        (static_cast<size_t>(fy + row) *
                                             fullW +
                                         fx) * 4,
                                    static_cast<size_t>(fw) * 4);
                    }
                    const std::string dir = StateDir() + "\\screenshots";
                    CreateDirectoryA(dir.c_str(), nullptr);
                    const long long ts =
                        std::chrono::duration_cast<
                            std::chrono::milliseconds>(
                            std::chrono::system_clock::now()
                                .time_since_epoch())
                            .count();
                    const std::string path =
                        dir + "\\shot_" + std::to_string(ts) + "_region.png";
                    if (WritePng(path, fw, fh, crop.data())) {
                        reply = "{\"ok\":true,\"path\":\"" + JsonEsc(path) +
                                "\"}";
                    } else {
                        reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                    }
                }
            }
        }
        }
    } else if (tool == "trigger_toggle") {
        // docs/34: write state/triggers.json (single source of truth) then
        // publish triggers.reload — jktriggers re-reads the file on the
        // event. Safe tier (no gate), same as launch_app.
        std::string name;
        int on = -1;
        req.GetObjStr("args", "name", name);
        req.GetObjInt("args", "on", on);
        if (name.empty() || on < 0) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            const std::string path = StateDir() + "\\triggers.json";
            std::map<std::string, int> flags;
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (f) {
                char buf[4096] = {};
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                jk::agent::AgentJson json(buf);
                int cnt = 0;
                if (json.ok() && json.GetArraySize("triggers", cnt)) {
                    for (int i = 0; i < cnt && i < 64; ++i) {
                        std::string nm;
                        int en = 1;
                        if (json.GetArrStr("triggers", i, "name", nm)) {
                            json.GetArrInt("triggers", i, "enabled", en);
                            flags[nm] = en;
                        }
                    }
                }
            }
            flags[name] = on;
            std::string out = "{\"triggers\":[";
            bool first = true;
            for (const auto& kv : flags) {
                if (!first) out += ",";
                first = false;
                out += "{\"name\":\"" + JsonEsc(kv.first) + "\",\"enabled\":" +
                       std::to_string(kv.second) + "}";
            }
            out += "]}";
            reply = "{\"ok\":true}";
            if (std::FILE* w = std::fopen(path.c_str(), "wb")) {
                std::fwrite(out.data(), 1, out.size(), w);
                std::fclose(w);
            } else {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            }
            if (reply.find("\"ok\":true") != std::string::npos) {
                char ev[128];
                std::snprintf(ev, sizeof(ev),
                              "{\"topic\":\"triggers.reload\",\"data\":{},"
                              "\"ts\":%lld}",
                              static_cast<long long>(
                                  std::chrono::duration_cast<
                                      std::chrono::milliseconds>(
                                      std::chrono::system_clock::now()
                                          .time_since_epoch())
                                      .count()));
                PushAgentEventJson(ev);
            }
        }
    } else if (tool == "trigger_list") {
        // docs/34: flat rows from jktriggers' loaded manifest merged with
        // the flags file (missing entry = enabled).
        std::map<std::string, std::pair<std::vector<std::string>, int>> merged;
        auto readState = [&](const char* file, bool isManifest) {
            std::FILE* f =
                std::fopen((StateDir() + "\\" + file).c_str(), "rb");
            if (!f) return;
            char buf[8192] = {};
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            jk::agent::AgentJson json(buf);
            int cnt = 0;
            if (!json.ok() || !json.GetArraySize("triggers", cnt)) return;
            for (int i = 0; i < cnt && i < 256; ++i) {
                std::string nm, topic;
                if (!json.GetArrStr("triggers", i, "name", nm)) continue;
                auto& entry = merged[nm];
                if (isManifest) {
                    // operator[] default is 0 — absent flags mean enabled.
                    entry.second = 1;
                    if (json.GetArrStr("triggers", i, "topic", topic) &&
                        !topic.empty()) {
                        entry.first.push_back(topic);
                    }
                } else {
                    int en = 1;
                    json.GetArrInt("triggers", i, "enabled", en);
                    entry.second = en;
                }
            }
        };
        readState("triggers_loaded.json", true);
        readState("triggers.json", false);
        std::string out = "{\"ok\":true,\"triggers\":[";
        bool first = true;
        for (const auto& kv : merged) {
            if (!first) out += ",";
            first = false;
            out += "{\"name\":\"" + JsonEsc(kv.first) + "\",\"topics\":[";
            for (size_t i = 0; i < kv.second.first.size(); ++i) {
                if (i) out += ",";
                out += "\"" + JsonEsc(kv.second.first[i]) + "\"";
            }
            out += "],\"enabled\":" + std::to_string(kv.second.second) + "}";
        }
        reply = out + "]}";
    } else if (tool == "trust_list") {
        // Script trust store (docs/37 spec): the loader's trust.json records,
        // fingerprints truncated to 15 chars ("sha256:"+8hex) for display.
        // docs/38: a missing/corrupt store is distinguished from an empty one
        // — trust_store_unreadable on fopen or parse failure; a valid file
        // with zero records still answers ok with an empty array.
        std::FILE* f =
            std::fopen((StateDir() + "\\trust.json").c_str(), "rb");
        if (!f) {
            reply = "{\"ok\":false,\"error\":\"trust_store_unreadable\"}";
        } else {
            char buf[65536] = {};
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            buf[n] = '\0';
            jk::agent::AgentJson json(buf);
            int cnt = 0;
            if (!json.ok() || !json.GetArraySize("records", cnt)) {
                reply = "{\"ok\":false,\"error\":\"trust_store_unreadable\"}";
            } else {
                std::string out = "{\"ok\":true,\"records\":[";
                bool first = true;
                for (int i = 0; i < cnt && i < 512; ++i) {
                    std::string fp, name, source;
                    // ts is epoch ms (int64 in the loader's writer) but the
                    // AgentJson accessor has no int64 getter — display-only
                    // int truncation is accepted (jktriggers reads ts via a
                    // direct QuickJS reader instead).
                    int ts = 0;
                    if (!json.GetArrStr("records", i, "fingerprint", fp))
                        continue;
                    json.GetArrStr("records", i, "name", name);
                    json.GetArrStr("records", i, "source", source);
                    json.GetArrInt("records", i, "ts", ts);
                    if (!first) out += ",";
                    first = false;
                    // 에이전트 관리자 해지용 전체 지문 (스펙 §2.3 전제) — 15자
                    // 절단 fingerprint는 표시용으로 유지.
                    out += "{\"fingerprint\":\"" +
                           (fp.size() > 15 ? fp.substr(0, 15) : fp) +
                           "\",\"name\":\"" + JsonEsc(name) +
                           "\",\"source\":\"" + JsonEsc(source) +
                           "\",\"fp\":\"" + JsonEsc(fp) +
                           "\",\"ts\":" + std::to_string(ts) + "}";
                }
                out += "]}";
                reply = out;
            }
        }
    } else if (tool == "events_list") {
        // docs/32: the structured event catalog. Static rows describe the
        // system topics (source + payload shape); fired/last_ts come from
        // the runtime stats and subscribers from the live connections.
        // Topics published via publish_event that are not cataloged are
        // appended as dynamic "app" rows.
        size_t subscribers = 0;
        for (auto& c : clients_) {
            if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
                ++subscribers;
            }
        }
        static const struct {
            const char* topic;
            const char* source;  // server | app
            const char* desc;
            const char* fields;  // JSON array literal of payload paths
        } kCatalog[] = {
            {"window.created", "server", "클라 창(레이어) 생성",
             "[\"id\",\"title\",\"pid\"]"},
            {"window.focused", "server", "창 포커스 이동",
             "[\"id\",\"title\",\"pid\"]"},
            {"window.destroyed", "server", "창 소멸(연결 종료, 정상 종료 포함)",
             "[\"id\",\"title\",\"pid\"]"},
            {"window.maximized", "server",
             "창 최대화 (크롬 최대화 버튼 / 제목 더블클릭, docs/39)",
             "[\"id\",\"title\"]"},
            {"window.restored", "server",
             "창 복원 (최대화 해제 — 버튼/더블클릭/제목·가장자리 드래그, docs/39)",
             "[\"id\",\"title\"]"},
            {"window.fullscreen", "server",
             "창 전체화면 진입 (window_fullscreen 도구/vplayer F11·더블클릭, "
             "docs/50 §11)",
             "[\"id\",\"title\"]"},
            {"window.fullscreen_exit", "server",
             "창 전체화면 해제 (window_fullscreen 도구/vplayer F11·더블클릭·OSD "
             "버튼, docs/50 §11)",
             "[\"id\",\"title\"]"},
            {"app.crashed", "server", "앱 비정상 종료(exit code 0/259 외)",
             "[\"id\",\"title\",\"pid\"]"},
            {"agent.approval_request", "server",
             "ask 권한 승인 요청 (docs/31) + trust_request (docs/37) + "
             "capture_allow 키별 Ask (docs/54) + send_input (docs/62 정복 사다리)",
             "\"request,tool,kind,title/target_id/name,origin,fingerprint\""},
            {"agent.approval_resolved", "server",
             "승인 결정: allow/deny/timeout", "[\"request\",\"decision\"]"},
            {"terminal.output", "app", "터미널 출력(250ms 코얼레싱, VT 제거)",
             "[\"data.text\"]"},
            {"agent.notify", "app",
             "알림 방송 — desktop.notify()가 발행, 채팅 [알림] 줄 + 알림 센터가 소비",
             "[\"data.title\",\"data.body\"]"},
            {"triggers.reload", "server", "트리거 플래그 변경 재적재 신호", "[]"},
            {"audio.master", "server",
             "마스터 볼륨/뮤트 변경 방송 (settings_set, docs/54) — 코어 펌프가 "
             "JKSoundManager에 적용",
             "\"data.mute,data.volume\""},
            // 앱 도구 허브 (스펙 §4.1): 등록/소멸 시 publish. 레슨 ⑧ 신규
            // 토픽은 카탈로그 즉시 등록.
            {"agent.app_tools_changed", "server",
             "앱 도구 등록/소멸(연결 수명)", "[\"topic\"]"},
            // file_open wait:event 비동기 모드 (스펙 §6 결정 5): filedlg가
            // 해소한 결과/취소와 만료 스캔의 expired를 방송. 레슨 ⑧ 신규
            // 토픽은 카탈로그 즉시 등록.
            {"file.open_result", "server",
             "file_open(wait:event) 해소 — ok=true path / 취소 ok=false / "
             "만료 expired (docs/48 파킹 파이프라인의 이벤트 통지)",
             "[\"ok\",\"path\",\"error\"]"},
        };
        std::string out = "{\"ok\":true,\"subscribers\":" +
                          std::to_string(subscribers) + ",\"events\":[";
        bool first = true;
        auto appendEntry = [&](const std::string& topic, const char* source,
                               const char* desc, const char* fields) {
            const auto it = topicStats_.find(topic);
            const uint64_t fired =
                it == topicStats_.end() ? 0 : it->second.fired;
            const long long lastTs =
                it == topicStats_.end() ? 0 : it->second.lastTs;
            if (!first) out += ",";
            first = false;
            out += "{\"topic\":\"" + JsonEsc(topic) + "\",\"source\":\"" +
                   source + "\",\"desc\":\"" + JsonEsc(desc) +
                   "\",\"fields\":" + fields + ",\"fired\":" +
                   std::to_string(fired) + ",\"last_ts\":" +
                   std::to_string(lastTs) + "}";
        };
        for (const auto& e : kCatalog) {
            appendEntry(e.topic, e.source, e.desc, e.fields);
        }
        for (const auto& kv : topicStats_) {
            bool inCatalog = false;
            for (const auto& e : kCatalog) inCatalog |= (e.topic == kv.first);
            if (!inCatalog) {
                appendEntry(kv.first, "app",
                            "publish_event로 발행된 동적 토픽", "[]");
            }
        }
        reply = out + "]}";
    } else if (tool == "agent_permissions") {
        // 스펙 §2.1: permissions.json + 기본값 병합. gate 뱃지로 게이트
        // 소비처를 정직 표기 — "none" 행의 파일값은 서버 무력(브로커만).
        // file은 "" = 오버라이드 없음 (AgentJson이 null을 못 읽는다).
        char exePath[1024] = {};
        GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
        std::string dir = exePath;
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir = dir.substr(0, slash);
        const std::string permPath = dir + "\\permissions.json";
        char pbuf[4096] = {};
        bool fileExists = false;
        if (std::FILE* f = std::fopen(permPath.c_str(), "rb")) {
            fileExists = true;
            std::fread(pbuf, 1, sizeof(pbuf) - 1, f);
            std::fclose(f);
        }
        // AgentJson은 복사/대입 불가(docs/38) — 파일이 없으면 "{}"로 생성.
        jk::agent::AgentJson perm(fileExists ? pbuf : "{}");
        if (fileExists && !perm.ok()) {
            // 파일 없음(기본값)과 파싱 실패(수동 편집 실수)를 구분 —
            // trust_list의 docs/38 선례.
            reply = "{\"ok\":false,\"error\":\"permissions_unreadable\"}";
        } else {
            std::string out = "{\"ok\":true,\"perms\":[";
            bool first = true;
            for (const AgentPermRow& row : kPermMatrix) {
                std::string fileVal;
                const bool hasFile = perm.ok() &&
                    perm.GetStr(row.tool, fileVal) &&
                    (fileVal == "allow" || fileVal == "ask" ||
                     fileVal == "deny");
                std::string effective = row.deflt;
                if (std::string(row.gate) == "server(fixed)") {
                    effective = "ask";
                } else if (std::string(row.gate) == "server") {
                    if (hasFile) effective = fileVal;
                } else if (std::string(row.gate) == "server(flip)") {
                    // docs/54 §11 opus M2: 캡처 쌍 — 파일값이 서버 강제
                    // ("ask" = capture_ask 거부, flip 승인이 해소).
                    if (hasFile) effective = fileVal;
                } else if (std::string(row.gate) == "server(files)") {
                    // 파일 허브 (스펙 2026-09-18-file-hub §2.2): 파일값이
                    // 서버 강제. 파일 없음 = 소스 분리 — 표시는 에이전트
                    // 쪽 최악값(ask)을 쓴다(사용자 window 연결은 무승인).
                    if (hasFile) effective = fileVal;
                    else effective = "ask";
                } else if (std::string(row.gate) == "server(audit)") {
                    // files_audit (opus 리뷰 MINOR-3): 파일값 강제(deny/ask
                    // 파킹) — 파일 없음 = 기본 allow.
                    if (hasFile) effective = fileVal;
                } else {
                    effective = "allow";   // broker/none — 서버 미게이트
                }
                if (!first) out += ",";
                first = false;
                out += "{\"tool\":\"" + std::string(row.tool) +
                       "\",\"gate\":\"" + row.gate + "\",\"file\":\"" +
                       (hasFile ? fileVal : std::string()) +
                       "\",\"effective\":\"" + effective +
                       "\",\"default\":\"" + row.deflt + "\"}";
            }
            reply = out + "]}";
        }
    } else if (tool == "installed_list") {
        // 스펙 §2.4: 셸 런처 스캔의 이름/kind. shell_ 미기동 = 빈 배열(정상).
        std::vector<std::pair<std::string, const char*>> rows;
        if (shell_) shell_->ListInstalled(rows);
        std::string out = "{\"ok\":true,\"installed\":[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ",";
            out += "{\"name\":\"" + JsonEsc(rows[i].first) +
                   "\",\"kind\":\"" + rows[i].second + "\"}";
        }
        reply = out + "]}";
    } else if (tool == "read_receipts") {
        // 스펙 §2.5: 브로커 receipts.jsonl 꼬리 — ts/tool/ok만 반환
        // (result 전문은 args에 경로/명령어가 실릴 수 있다).
        int limit = 50;
        req.GetObjInt("args", "limit", limit);
        if (limit <= 0) limit = 50;
        if (limit > 200) limit = 200;
        const std::string path = StateDir() + "\\receipts.jsonl";
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            reply = "{\"ok\":true,\"rows\":[]}";   // 브로커 미사용 = 정상
        } else {
            std::fseek(f, 0, SEEK_END);
            const long size = std::ftell(f);
            const long start = size > 262144 ? size - 262144 : 0;
            std::fseek(f, start, SEEK_SET);
            std::vector<char> buf(static_cast<size_t>(size - start) + 1);
            const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
            std::fclose(f);
            buf[n] = '\0';
            std::vector<std::string> lines;
            size_t pos = 0;
            while (pos < n) {
                const char* begin = buf.data() + pos;
                const char* nl = static_cast<const char*>(
                    std::memchr(begin, '\n', n - pos));
                const size_t len = nl ? (size_t)(nl - begin) : (n - pos);
                if (len > 0) lines.push_back(std::string(begin, len));
                pos += len + (nl ? 1 : 0);
            }
            std::string out = "{\"ok\":true,\"rows\":[";
            int used = 0;
            for (size_t i = lines.size(); i-- > 0 && used < limit;) {
                const std::string& line = lines[i];
                // JSONL 행의 result 중첩은 2레벨 리더의 관심사가 아니다 —
                // ts/tool은 행 파서(AgentJson) + ts는 raw 스캔(숫자 필드),
                // ok는 result 내 raw 스캔.
                jk::agent::AgentJson row(line.c_str());
                std::string toolName;
                if (!row.ok() || !row.GetStr("tool", toolName)) continue;
                long long ts = 0;
                {
                    const size_t tp = line.find("\"ts\":");
                    if (tp != std::string::npos) {
                        ts = std::atoll(line.c_str() + tp + 5);
                    }
                }
                const bool okFlag =
                    line.find("\"result\":") != std::string::npos &&
                    line.find("\"ok\":true") != std::string::npos;
                if (used) out += ",";
                // 직렬화 규약(파서 계약): ts = epoch 초(2레벨 GetArrInt 경유,
                // ms → 초 절단), ok = "0"/"1" 문자열(리더가 bool을 못 읽는다).
                out += "{\"ts\":" + std::to_string(ts / 1000) +
                       ",\"tool\":\"" + JsonEsc(toolName) +
                       "\",\"ok\":\"" + (okFlag ? "1" : "0") + "\"}";
                ++used;
            }
            reply = out + "]}";
        }
    } else if (tool == "notes_read") {
        // 노트 허브 (스펙 2026-09-18-notes-hub §2.2): state/notes.json 전체
        // 반환. ts는 epoch ms 원문 저장 — 클라 응답은 epoch 초 절단
        // (read_receipts와 동일 2레벨 규약).
        std::vector<NoteRow> notes, items;
        int next = 1;
        if (!ReadNotes(notes, items, next)) {
            std::printf("[server] notes.json unreadable — starting empty\n");
            std::fflush(stdout);
        }
        std::string out = "{\"ok\":true,\"notes\":[";
        for (size_t i = 0; i < notes.size(); ++i) {
            if (i) out += ",";
            out += "{\"id\":" + std::to_string(notes[i].id) +
                   ",\"text\":\"" + JsonEsc(notes[i].text) +
                   "\",\"win\":" + std::to_string(notes[i].win) +
                   ",\"ts\":" + std::to_string(
                       std::atoll(notes[i].tsRaw.c_str()) / 1000) +
                   ",\"src\":\"" + JsonEsc(notes[i].src) + "\"}";
        }
        out += "],\"backlog\":[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i) out += ",";
            out += "{\"id\":" + std::to_string(items[i].id) +
                   ",\"title\":\"" + JsonEsc(items[i].text) +
                   "\",\"state\":" + std::to_string(items[i].state) +
                   ",\"ts\":" + std::to_string(
                       std::atoll(items[i].tsRaw.c_str()) / 1000) +
                   ",\"src\":\"" + JsonEsc(items[i].src) + "\"}";
        }
        reply = out + "]}";
    } else if (tool == "notes_write") {
        // op 화이트리스트(§2.2): add_note/add_item/move_item/del. RMW는
        // 행 파싱 → 재직렬화(서버가 유일 쓰기자). id는 서버 채번기(next).
        std::string op, text;
        int id = 0, st = -1, win = -1;
        req.GetObjStr("args", "op", op);
        req.GetObjStr("args", "text", text);
        req.GetObjInt("args", "id", id);
        req.GetObjInt("args", "state", st);
        req.GetObjInt("args", "win", win);
        if (op != "add_note" && op != "add_item" && op != "move_item" &&
            op != "del") {
            reply = "{\"ok\":false,\"error\":\"bad_op\"}";
        } else if (op == "add_note" &&
                   (text.empty() || text.size() > 512)) {
            reply = "{\"ok\":false,\"error\":\"bad_text\"}";
        } else if (op == "add_item" &&
                   (text.empty() || text.size() > 128)) {
            reply = "{\"ok\":false,\"error\":\"bad_text\"}";
        } else if (op == "add_item" && st > 2) {
            // 스펙 §2.2: state 0..2 — 명시 out-of-range는 bad_state(opus
            // NIT-4). 생략(st==-1)은 대기(0)로 채번 시 적용.
            reply = "{\"ok\":false,\"error\":\"bad_state\"}";
        } else if (op == "move_item" && (id <= 0 || st < 0 || st > 2)) {
            reply = "{\"ok\":false,\"error\":\"bad_state\"}";
        } else if (op == "del" && id <= 0) {
            reply = "{\"ok\":false,\"error\":\"bad_id\"}";
        } else {
            std::vector<NoteRow> notes, items;
            int next = 1;
            if (!ReadNotes(notes, items, next)) {
                reply = "{\"ok\":false,\"error\":\"notes_unreadable\"}";
            } else if (op == "add_note" || op == "add_item") {
                NoteRow r;
                r.isItem = (op == "add_item");
                r.id = next;
                r.text = text;
                r.src = client.IsControlOnly() ? "agent" : "user";
                r.tsRaw = std::to_string(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count());
                if (r.isItem) {
                    r.state = st >= 0 ? st : 0;   // 생략(-1) → 대기
                    items.push_back(r);
                } else {
                    r.win = win > 0 ? win : 0;
                    notes.push_back(r);
                }
                if (WriteNotesFile(notes, items, next + 1)) {
                    reply = "{\"ok\":true,\"id\":" + std::to_string(next) +
                            ",\"written\":true}";
                    // §2.4: 에이전트 노트는 알림 센터로 유입(agent.notify —
                    // 기존 토픽, 카탈로그 변경 없음). 사용자 추가는 방송 없음.
                    // 방송은 publish_event와 같은 연결 예산(docs/38 60/10s)을
                    // 쓴다(opus MINOR-1 — 노트 스팸 = 토스트 스팸 + 히스토리
                    // 전체 재쓰기). 캡 초과분은 **노트가 아니라 토스트만
                    // 버린다** — 노트는 데이터라 남고, senders가 재시도 폭탄
                    // 으로 변하지 않게 ok 유지.
                    if (r.src == "agent") {
                        constexpr int kPublishCap = 60;
                        constexpr uint64_t kPublishWindowMs = 10000;
                        const uint64_t nowMs = static_cast<uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::milliseconds>(
                                std::chrono::steady_clock::now()
                                    .time_since_epoch())
                                .count());
                        PublishBudget& b = publishBudgets_[client.Id()];
                        bool overCap = false;
                        if (nowMs - b.windowStartMs >= kPublishWindowMs) {
                            b.windowStartMs = nowMs;
                            b.count = 0;
                            b.logged = false;
                        }
                        if (b.count >= kPublishCap) {
                            overCap = true;
                            if (!b.logged) {
                                b.logged = true;
                                std::printf("[server] notes notify "
                                            "rate-capped (conn %u)\n",
                                            client.Id());
                                std::fflush(stdout);
                            }
                        }
                        if (!overCap) {
                            ++b.count;
                            char ev[896];
                            std::snprintf(
                                ev, sizeof(ev),
                                "{\"topic\":\"agent.notify\",\"data\":{"
                                "\"title\":\"[노트] %s\",\"body\":\"%s\"},"
                                "\"ts\":%lld}",
                                (r.isItem ? "백로그" : "코멘트"),
                                JsonEsc(Utf8TrimTo(r.text, 128)).c_str(),
                                static_cast<long long>(
                                    std::time(nullptr)) * 1000);
                            PushAgentEventJson(ev);
                        }
                    }
                } else {
                    reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                }
            } else if (op == "move_item") {
                bool found = false;
                for (NoteRow& r : items) {
                    if (r.id == id) { r.state = st; found = true; break; }
                }
                if (!found) {
                    reply = "{\"ok\":false,\"error\":\"id_not_found\"}";
                } else if (WriteNotesFile(notes, items, next)) {
                    reply = "{\"ok\":true,\"written\":true}";
                } else {
                    reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                }
            } else {   // del
                const size_t beforeN = notes.size(), beforeI = items.size();
                for (size_t i = notes.size(); i > 0; --i) {
                    if (notes[i - 1].id == id) {
                        notes.erase(notes.begin() +
                                    static_cast<long>(i - 1));
                    }
                }
                for (size_t i = items.size(); i > 0; --i) {
                    if (items[i - 1].id == id) {
                        items.erase(items.begin() +
                                    static_cast<long>(i - 1));
                    }
                }
                if (notes.size() == beforeN && items.size() == beforeI) {
                    reply = "{\"ok\":false,\"error\":\"id_not_found\"}";
                } else if (WriteNotesFile(notes, items, next)) {
                    reply = "{\"ok\":true,\"written\":true}";
                } else {
                    reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                }
            }
        }
    } else if (tool == "files_list" || tool == "files_read") {
        // 파일 허브 (스펙 2026-09-18-file-hub §2.2): 최초의 파일 콘텐츠
        // 도구 — 읽기 전용. 게이트 "server(files)": 소스 분리 — 사용자
        // (window 연결)는 무승인(자기 파일), 에이전트(control-only)는 파킹
        // (kind files_access). permissions.json 명시 "allow"면 에이전트도
        // 무승인, "deny"면 전 소스 거부, 없음/ask가 위 기본.
        std::string path;
        int maxBytes = 0;
        req.GetObjStr("args", "path", path);
        req.GetObjInt("args", "maxBytes", maxBytes);
        const std::string perm = FilesPermRaw(tool);
        if (perm == "deny") {
            // 사용자가 영구 거부한 행위 — window 연결도 거부된다.
            reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else if (!ValidFilePath(path)) {
            reply = "{\"ok\":false,\"error\":\"bad_path\"}";
        } else if (perm == "allow" || !client.IsControlOnly()) {
            reply = tool == "files_list" ? FilesListOpJson(path)
                                         : FilesReadOpJson(path, maxBytes);
        } else {
            // 에이전트 ask — 승인 파킹(close_window 선례). 구독자 검사는
            // control-only만 센다(opus 리뷰 M3, docs/54 §11 — 코어가 모든
            // ImGui 클라를 구독시키므로 전체 구독자 검사는 공허하다).
            bool subscriber = false;
            for (auto& c : clients_) {
                if (c && c->IsControlOnly() && c->AgentEventSubscriber() &&
                    !c->IsDisconnected()) {
                    subscriber = true;
                    break;
                }
            }
            if (!subscriber) {
                reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
            } else if (ApprovalParkingFull(client.Id())) {
                reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
            } else {
                PendingApproval p;
                p.kind = "files_access";
                p.requestId = nextApprovalId_++;
                p.queryId = queryId;
                p.requesterId = client.Id();
                p.expiresAt = std::time(nullptr) + 60;
                // 재실행 원본 — 승인 시점에 deny 재검사 + op 재실행(파킹
                // 대기 중 permissions.json이 바뀌면 최신 게이트가 강제).
                p.filesTool = tool;
                p.filesPath = path;
                p.filesMaxBytes = maxBytes;
                char buf[1024];   // path 256 + JsonEsc 확장(백슬래시 2배) 여유
                const int evLen =
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"%s\","
                                  "\"kind\":\"files_access\","
                                  "\"title\":\"%s\",\"ts\":%lld}",
                                  p.requestId, tool.c_str(),
                                  JsonEsc(path).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                if (evLen < 0 || static_cast<size_t>(evLen) >= sizeof(buf)) {
                    // 잘린 이벤트는 무효 JSON(opus 리뷰 MINOR-2) — 파킹하지
                    // 않고 승인 불가로 답한다(발행-후-검사 순서).
                    reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                } else {
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                }
            }
        }
    } else if (tool == "files_audit") {
        // 감사 열람 — 기본 allow(저위험), 단 파일값 강제(opus 리뷰 MINOR-3):
        // deny=전 소스 거부, ask+에이전트=files 도구와 동일 파킹. 브로커가
        // 쓴 receipts의 files_* 행만 — 신규 감사 파일 없음(단일 감사원).
        int limit = 0;
        req.GetObjInt("args", "limit", limit);
        const std::string auditPerm = FilesPermRaw("files_audit");
        if (auditPerm == "deny") {
            reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else if (!client.IsControlOnly() || auditPerm == "allow" ||
                   auditPerm == "missing") {
            reply = FilesAuditOpJson(limit);
        } else {
            // ask + 에이전트 — files_list/read와 동일한 승인 파킹.
            bool subscriber = false;
            for (auto& c : clients_) {
                if (c && c->IsControlOnly() && c->AgentEventSubscriber() &&
                    !c->IsDisconnected()) {
                    subscriber = true;
                    break;
                }
            }
            if (!subscriber) {
                reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
            } else if (ApprovalParkingFull(client.Id())) {
                reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
            } else {
                PendingApproval p;
                p.kind = "files_access";
                p.requestId = nextApprovalId_++;
                p.queryId = queryId;
                p.requesterId = client.Id();
                p.expiresAt = std::time(nullptr) + 60;
                p.filesTool = "files_audit";
                p.filesLimit = limit;   // 재실행 원본 — 승인 시점 재실행
                char buf2[256];         // 경로 없음 — tool 이름뿐, 잘림 여유
                const int evLen2 =
                    std::snprintf(buf2, sizeof(buf2),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"files_audit\","
                                  "\"kind\":\"files_access\","
                                  "\"title\":\"files_audit\",\"ts\":%lld}",
                                  p.requestId,
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                if (evLen2 < 0 ||
                    static_cast<size_t>(evLen2) >= sizeof(buf2)) {
                    reply =
                        "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                } else {
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf2);
                    replied = false;
                }
            }
        }
    } else if (tool == "launch_chat") {
        // M2 chat: open the Win32 chat window (approval surface). One API,
        // many faces — MCP agents can open it too.
        SpawnProcess("jkchat.exe", "");
        reply = "{\"ok\":true}";
    } else if (tool == "approve") {
        // M2 chat: resolve one pending approval. The parked query's reply
        // goes to the ORIGINAL requester; the approver gets the ack below.
        int request = 0;
        std::string decision;
        req.GetObjInt("args", "request", request);
        req.GetObjStr("args", "decision", decision);
        const bool allow = (decision == "allow");
        bool resolved = false;
        bool selfApprove = false;
        // 앱 도구 허브 (스펙 §4.3): app_tool 승인은 응답을 즉시 쓰지 않는다 —
        // 중계를 시작하고 응답은 앱의 AgentToolResult가 회송(HandleToolResult).
        bool deferred = false;
        for (auto it = pendingApprovals_.begin();
             it != pendingApprovals_.end(); ++it) {
            if (it->requestId != static_cast<uint32_t>(request)) continue;
            // 승인 파이프라인의 2단 우회 봉쇄(스펙 §7 리뷰 후속): 파킹을
            // 자기 연결에서 approve하면 승인 없는 허가가 된다. kind를
            // 2종(permission_set/trust_revoke)으로 한정하면 trust_request/
            // run_console_app 파킹이 우회를 남긴다(opus 최종리뷰 M1 — 자기
            // 구독으로 승인 불가 상태를 스스로 해소해 지문 선기록/스폰이
            // 그대로 관통). close_window만 예외 — ask 모드에서 채팅 자신의
            // /close를 자기 승인 스트립으로 해소하는 것은 docs/31 §3의 설계된
            // UX다(non-blocking 채팅의 존재 이유). 나머지 전종에는 legit
            // 요청자-자체승인 경로가 없다. 파킹은 건드리지 않는다 — 다른
            // 표면(채팅)의 승인은 여전히 가능하다.
            if (it->requesterId == client.Id() && it->kind != "close_window") {
                selfApprove = true;
                break;
            }
            resolved = true;
            if (allow && it->kind == "close_window") {
                for (auto& c : clients_) {
                    if (c && c->Id() == it->targetId && !c->IsDisconnected()) {
                        ipc::WriteMessage(c->Transport(), ipc::MsgType::Close,
                                          std::vector<uint8_t>{});
                        break;
                    }
                }
            }
            if (allow && it->kind == "run_console_app" && shell_) {
                // P4 SDK §5: 승인 시점에 스캔에서 cmd를 다시 읽는다 — 승인
                // 대기 중 매니페스트가 바뀌었으면 최신 cmd가 스폰된다(파킹된
                // cmd를 신뢰하지 않음 — 지문 재계산이 아니라 재조회로 방어).
                std::string cmd, dir, fp;
                if (shell_->ConsoleAppInfo(it->name, cmd, dir, fp)) {
                    SpawnConsoleApp(cmd, dir, it->name);
                }
            }
            for (auto& c : clients_) {
                if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
                    std::string result;
                    if (!allow) {
                        result = "{\"ok\":false,\"error\":\"denied_by_user\"}";
                    } else if (it->kind == "permission_set") {
                        // 쓰기 실패는 요청자 reply에 표면화 (docs/52 선례).
                        const std::string err = WritePermissionsEntry(
                            it->permTool, it->permDecision);
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else if (it->kind == "capture_allow") {
                        // 설정 허브 §2.2 키별 Ask 해소: 두 캡처 도구를 함께
                        // 쓴다 — 쓰기는 RMW 2회(같은 값이라 멱등 방향).
                        std::string err = WritePermissionsEntry(
                            "capture_window", it->permDecision);
                        if (err.empty()) {
                            err = WritePermissionsEntry("capture_region",
                                                        it->permDecision);
                        }
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else if (it->kind == "trust_revoke") {
                        const std::string err =
                            RevokeTrustRecord(it->fingerprint);
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true,"
                              "\"restart_needed\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else if (it->kind == "files_access") {
                        // 승인 = 원 요청 재실행(스펙 2026-09-18-file-hub
                        // §2.2) — 승인 시점에 deny 재검사(파킹 대기 중
                        // permissions.json이 바뀌면 최신 게이트가 강제 —
                        // run_console_app의 재조회 선례) + 경로 재검증.
                        if (FilesPermRaw(it->filesTool) == "deny") {
                            result =
                                "{\"ok\":false,\"error\":\"permission_denied\"}";
                        } else if (it->filesTool == "files_audit") {
                            // 감사 열람 파킹(MINOR-3) — 경로 없음, limit 재실행.
                            result = FilesAuditOpJson(it->filesLimit);
                        } else if (!ValidFilePath(it->filesPath)) {
                            result = "{\"ok\":false,\"error\":\"bad_path\"}";
                        } else if (it->filesTool == "files_read") {
                            result = FilesReadOpJson(it->filesPath,
                                                     it->filesMaxBytes);
                        } else {   // "files_list"
                            result = FilesListOpJson(it->filesPath);
                        }
                    } else if (it->kind == "send_input") {
                        // 승인 시점 게이트 재검사(파킹 대기 중 permissions.json이
                        // 바뀌면 최신 게이트 강제 — files_access/run_console_app
                        // 선례) + 원 요청 재실행.
                        if (AgentToolAllowed("send_input") ==
                            AgentDecision::Deny) {
                            result =
                                "{\"ok\":false,\"error\":\"permission_denied\"}";
                        } else {
                            jk::agent::AgentJson args(it->sendArgs);
                            SendInputOp op;
                            const std::string buildErr =
                                args.ok() ? BuildSendInputOp(args, &op)
                                          : "bad_args";
                            const std::string ex =
                                buildErr.empty() ? ExecuteSendInputOp(op)
                                                 : buildErr;
                            result = ex.empty()
                                ? "{\"ok\":true,\"sent\":true}"
                                : "{\"ok\":false,\"error\":\"" + ex + "\"}";
                        }
                    } else if (it->kind == "app_tool") {
                        // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.3/§9):
                        // 승인 = 중계 시작 — files_access의 재실행형이 아니라
                        // 파킹-응답형. resolve 시점에 10s 타이머가 시작되므로
                        // 승인 대기가 tool_timeout 오발을 낳지 않는다. 대기 중
                        // 앱이 닫혔으면 승인해도 중계 불가 — tool_gone(연결
                        // 생존 재확인 + 매니페스트 재조회, run_console_app의
                        // 승인 시점 재조회 선례).
                        auto mit = appToolManifests_.find(it->appToolConnId);
                        JKClientConnection* target = nullptr;
                        if (mit != appToolManifests_.end() &&
                            mit->second.app == it->appToolApp) {
                            for (auto& c : clients_) {
                                if (c && c->Id() == it->appToolConnId &&
                                    !c->IsDisconnected()) {
                                    target = c.get();
                                    break;
                                }
                            }
                        }
                        if (!target) {
                            result = "{\"ok\":false,\"error\":\"tool_gone\"}";
                        } else if (mit->second.modal &&
                                   pendingFileDialog_.dialogConnId !=
                                       mit->second.connId) {
                            // 모달 가드 — resolve 중계 경로 (스펙 §5 케이스
                            // ③): 파킹 대기 중 슬롯이 만료/해소/재사용되고
                            // 새 file_open이 슬롯을 가져가면, 늦게 승인된
                            // 이 요청은 고아(또는 타) 다이얼로그를 때린다.
                            // 첫 중계 경로(app_tool)와 동일 조건 — 판정은
                            // 앱이 아니라 슬롯 진실원 pendingFileDialog_
                            // .dialogConnId가 한다.
                            result = "{\"ok\":false,\"error\":\"tool_gone\"}";
                        } else if (!it->semKind.empty() &&
                                   CursorActStale(mit->second, *it)) {
                            // 의미 커서 (스펙 2026-09-22-semantic-cursor §3
                            // act, Task 2): 승인 시점 재선언 재검증 — 파킹 시
                            // 고정한 셀 rect가 최신 선언과 어긋나면(재선언으로
                            // 격자/기하가 바뀜) 승인해도 배너가 보여준 칸과
                            // 다른 칸을 때린다 — 거부(files_access의 승인
                            // 시점 재검증 선례). 해소 기록은 아래 공통 경로의
                            // approval_resolved 이벤트가 계속 발행해 보인다.
                            result =
                                "{\"ok\":false,\"error\":\"bad_grid\"}";
                        } else {
                            const uint32_t reqId = nextToolReqId_++;
                            InflightAppTool inf;
                            inf.reqId = reqId;
                            inf.queryId = it->queryId;
                            inf.requesterConnId = it->requesterId;
                            inf.targetConnId = target->Id();
                            inf.windowId = mit->second.windowId;
                            inf.expiresAt = std::time(nullptr) + 10;
                            // MINOR-3 — reset act 승인 경로: ok 회송 시 커서
                            // 정의 전이((0,0)) 리셋(스펙 §4).
                            inf.resetCursorOnOk =
                                (it->semKind == "reset");
                            inflightAppTools_[reqId] = inf;
                            ipc::WriteAgentToolCall(
                                target->Transport(), reqId,
                                "{\"app\":\"" + JsonEsc(it->appToolApp) +
                                    "\",\"tool\":\"" + JsonEsc(it->appToolTool) +
                                    "\",\"args\":" +
                                    (it->appToolArgs.empty() ? "{}"
                                                             : it->appToolArgs) +
                                    "}");
                            deferred = true;  // HandleToolResult가 응답한다
                        }
                    } else {
                        result = "{\"ok\":true}";
                    }
                    const int flag = (result.find("\"ok\":true") !=
                                      std::string::npos)
                                         ? 1 : 0;
                    if (!deferred) {
                        ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                            it->queryId, flag, result);
                    }
                    break;
                }
            }
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "{\"topic\":\"agent.approval_resolved\","
                          "\"request\":%u,\"decision\":\"%s\"}",
                          it->requestId, allow ? "allow" : "deny");
            PushAgentEventJson(buf);
            pendingApprovals_.erase(it);
            reply = allow ? "{\"ok\":true,\"approved\":true}"
                          : "{\"ok\":true,\"approved\":false}";
            break;
        }
        if (selfApprove) {
            reply = "{\"ok\":false,\"error\":\"self_approve\"}";
        } else if (!resolved) {
            reply = "{\"ok\":false,\"error\":\"unknown_request\"}";
        }
    } else if (tool == "file_open") {
        // 파일 열기 대화상자 (설계 specs/2026-09-13-file-dialog §1b): 쿼리를
        // 파킹한 뒤 filedlg:<json args> appName 접두로 다이얼로그를 띄운다.
        // 파킹은 jkchat close_window/trust_request와 같은 pendingApprovals_
        // 기계를 재사용 — 해소는 다이얼로그의 file_open_result가 담당하고,
        // 요청자가 먼저 닫히면 만료 스캔이 회수한다. 권한 게이트 없음 —
        // launch_app 같은 안전 계층 (AgentToolAllowed 기본 allow).
        std::string filter, start, title, wait;
        req.GetObjStr("args", "filter", filter);
        req.GetObjStr("args", "start", start);
        req.GetObjStr("args", "title", title);
        // wait 모드 (스펙 §6 결정 5): ""/"reply" = 현행(파킹 응답이 해소 때
        // 도착), "event" = 즉시 parked ack + 해소를 file.open_result 이벤트로.
        // 폰 브로커가 최대 600s 블록되는 것을 끊는 게 존재 이유. 알 수 없는
        // 값은 오류 표(스펙 §8)대로 즉답.
        req.GetObjStr("args", "wait", wait);
        // 256자 상한 (trust_request의 bad_name 선례) — 1차 가지치기. 실제
        // cmdLine 경계는 아래의 이스케이프 후 크기 검사다 (원시 길이만으로는
        // 인용 확장을 못 잡는다).
        if (!wait.empty() && wait != "reply" && wait != "event") {
            reply = "{\"ok\":false,\"error\":\"bad_args\"}";
        } else if (filter.size() > 256 || start.size() > 256 ||
                   title.size() > 256) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else if (pendingFileDialog_.requesterConnId != 0) {
            // 설계 리스크 2: 1슬롯 선착순 — 대화상자가 열려 있으면 후발은
            // 파킹해도 해소자가 없으므로 즉시 오류 (만료 대기보다 정직).
            reply = "{\"ok\":false,\"error\":\"dialog_busy\"}";
        } else {
            // 스폰 인자: args 그대로의 json (선택 필드만 — 없으면 키 생략).
            std::string jsonArgs = "{";
            bool first = true;
            auto appendField = [&](const char* key, const std::string& v) {
                if (v.empty()) return;
                if (!first) jsonArgs += ",";
                first = false;
                jsonArgs += std::string("\"") + key + "\":\"" + JsonEsc(v) +
                            "\"";
            };
            appendField("filter", filter);
            appendField("start", start);
            appendField("title", title);
            jsonArgs += "}";
            // publish_event의 이스케이프 후 크기 검사 선례: cmdLine 잘림은
            // 조용히 일어나므로 스폰 전에 이스케이프된 크기를 검문한다. 1000
            // = SpawnProcess cmdLine 2048 − 최악 exe 경로(~1026, 인용 포함)
            // − "--filedlg \"\"" 골격 — 원시 json에서 인용 확장분까지 포함한
            // 상계(JsonEsc는 인용+백슬래시를 모두 늘리므로 보수적).
            if (JsonEsc(jsonArgs).size() > 1000) {
                reply = "{\"ok\":false,\"error\":\"bad_request\"}";
            } else if (ApprovalParkingFull(client.Id())) {
                // 파킹 플러드 상한 (docs/56 §2b) — file_open도 승인 파킹의
                // 한 종류라 동일 노출(1슬롯 dialog_busy 검사가 먼저 온다).
                reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
            } else {
                // 만료는 승인 파이프라인의 60s가 아니라 600s — 사용자가
                // 다이얼로그에서 고민하는 시간을 감안한다. 요청자 연결이 먼저
                // 닫혀도 이 만료 스캔이 회수한다 (신규 무효화 코드 없음).
                PendingApproval p;
                p.kind = "file_open";
                p.requestId = nextApprovalId_++;
                p.queryId = queryId;
                p.requesterId = client.Id();
                p.targetId = 0;  // 대상 창 없음 — 다이얼로그가 해소자
                p.expiresAt = std::time(nullptr) + 600;
                pendingApprovals_.push_back(p);
                auto parkedIt = std::prev(pendingApprovals_.end());
                if (!SpawnClient(("filedlg:" + jsonArgs).c_str(), false)) {
                    // 스폰 실패 — 파킹 즉시 해소 (오류 응답). 저장해둔
                    // 반복자로 지운다 (pop_back의 순서 가정 제거).
                    pendingApprovals_.erase(parkedIt);
                    reply = "{\"ok\":false,\"error\":\"spawn_failed\"}";
                } else {
                    pendingFileDialog_.requesterConnId = client.Id();
                    pendingFileDialog_.requestId = p.requestId;
                    pendingFileDialog_.waitAsync = (wait == "event");
                    pendingFileDialog_.filter = filter;
                    pendingFileDialog_.start = start;
                    pendingFileDialog_.title = title;
                    if (pendingFileDialog_.waitAsync) {
                        // 스펙 §6 결정 5: 즉시 parked ack — 해소는
                        // file_open_result가 이벤트로 방송한다.
                        replied = true;
                        reply = "{\"ok\":true,\"parked\":true}";
                    } else {
                        // 현행 reply 모드 — file_open_result(또는 만료)가
                        // 응답한다.
                        replied = false;
                    }
                }
            }
        }
    } else if (tool == "file_dialog_params") {
        // filedlg 앱 기동 직후 1회 — file_open이 채운 파라미터를 꺼내 간다
        // (설계 D3: 모듈 ABI 무변경, 스폰 인자 회수는 쿼리로). 선착순 1회 —
        // 재요청은 오류. requesterConnId로 요청자-다이얼로그 상관관계를
        // 전달하고, 상관관계 자체는 결과 회수까지 슬롯에 남는다.
        if (pendingFileDialog_.requesterConnId == 0 ||
            pendingFileDialog_.paramsTaken) {
            reply = "{\"ok\":false,\"error\":\"no_pending_dialog\"}";
        } else {
            std::string out = "{\"ok\":true,\"requesterConnId\":" +
                              std::to_string(pendingFileDialog_.requesterConnId);
            auto appendParam = [&](const char* key, const std::string& v) {
                out += std::string(",\"") + key + "\":\"" + JsonEsc(v) + "\"";
            };
            appendParam("filter", pendingFileDialog_.filter);
            appendParam("start", pendingFileDialog_.start);
            appendParam("title", pendingFileDialog_.title);
            reply = out + "}";
            // 스펙 §5: 슬롯이 파라미터를 준 다이얼로그 연결을 기록 — 모달
            // 가드(app_tool 중계 재검증)의 진실원. paramsTaken과 함께
            // 선착순 소진 시점에 확정된다.
            pendingFileDialog_.dialogConnId = client.Id();
            pendingFileDialog_.paramsTaken = true;  // 선착순 소진
        }
    } else if (tool == "file_open_result") {
        // filedlg 앱 종료 결과 — 요청자의 파킹 쿼리를 완료한다 (approve 도구의
        // 파킹 해소 선례). 파킹이 이미 만료/부재면 no-op + parked:false.
        int ok = 0;
        std::string path;
        req.GetObjInt("args", "ok", ok);
        req.GetObjStr("args", "path", path);
        // 발신자 상관 검증 (final-review MAJOR-1): filedlg가
        // file_dialog_params로 받은 requesterConnId를 결과에 되울린다.
        // 필드 부재(구버전 다이얼로그/수조작) 또는 슬롯의 요청자와 불일치면
        // 아무것도 해소하지 않는 parked:false no-op — 슬롯도 지우지 않는다.
        // 검증이 없으면 만료 회수 후에도 살아 있던 고아 다이얼로그의 결과가
        // 새 요청자에게 잘못 전달된다(회귀 시나리오). 수동 `--client
        // filedlg` 실행(파킹 슬롯 없음)은 requesterConnId 0으로 발신하므로
        // 어느 경로든 무해한 no-op으로 수렴한다.
        int senderConnId = -1;
        req.GetObjInt("args", "requesterConnId", senderConnId);
        // 슬롯 리바인드 (docs/59 §13): 다이얼로그는 원 요청자 id를 에코한다 —
        // 요청자 교체 후엔 에코가 슬롯의 새 requesterConnId와 영구 불일치.
        // 발신 연결이 슬롯의 dialogConnId(파라미터를 준 다이얼로그 본인)와
        // 일치하면 요청자 에코보다 강한 진실원이므로 해소를 수락한다.
        // dialogConnId==0(파라미터 수락 전) 구간은 OR가 살아 있는 연결과
        // 0을 비교할 수 없어 위변조 여지 없음(기존 불변 유지).
        const bool senderMatched =
            senderConnId >= 0 &&
            (static_cast<uint32_t>(senderConnId) ==
                 pendingFileDialog_.requesterConnId ||
             (pendingFileDialog_.dialogConnId != 0 &&
              static_cast<uint32_t>(client.Id()) ==
                  pendingFileDialog_.dialogConnId));
        bool resolved = false;
        // dialogConnId 진실원의 해소 경로 적용 (final-review Important-2):
        // senderMatched는 "요청자 에코 일치"만 검사하므로 슬롯 재사용(고아
        // 다이얼로그 + 동일 요청자의 재호출) 조합에서 고아의 결과가 "새" 슬롯을
        // 대신 해소하는 교차배달이 가능했다 — ClientFileDialogApp.h의
        // "an orphan dialog must resolve nothing" 계약 집행. 슬롯이 파라미터를
        // 준 다이얼로그 연결(dialogConnId)과 발신 연결이 일치할 때만 해소.
        // dialogConnId 미기록(params 수락 전 창)은 기존 동작 유지이며, 수동
        // 기동(`--client filedlg`)은 requesterConnId 0이라 위 조건 어디든
        // 무해한 no-op으로 수렴한다(기존 불변).
        if (senderMatched && pendingFileDialog_.requesterConnId != 0 &&
            (pendingFileDialog_.dialogConnId == 0 ||
             static_cast<uint32_t>(client.Id()) ==
                 pendingFileDialog_.dialogConnId)) {
            // waitAsync는 슬롯 소진(아래 reset) 전에 판독 — 리셋 후엔 판독
            // 불가. 상관 검증/슬롯 소진은 waitAsync 양쪽이 공유한다.
            const bool waitAsync = pendingFileDialog_.waitAsync;
            for (auto it = pendingApprovals_.begin();
                 it != pendingApprovals_.end(); ++it) {
                if (it->kind != "file_open" ||
                    it->requestId != pendingFileDialog_.requestId) {
                    continue;
                }
                resolved = true;
                if (!waitAsync) {
                    for (auto& c : clients_) {
                        if (c && c->Id() == it->requesterId &&
                            !c->IsDisconnected()) {
                            // ok=false(취소)는 ok 플래그 0 + {"ok":false}.
                            const std::string result = ok
                                ? (path.empty()
                                       ? "{\"ok\":true}"
                                       : "{\"ok\":true,\"path\":\"" +
                                             JsonEsc(path) + "\"}")
                                : "{\"ok\":false}";
                            ipc::WriteAgentJson(c->Transport(),
                                                ipc::MsgType::AgentReply,
                                                it->queryId, ok ? 1 : 0,
                                                result);
                            break;
                        }
                    }
                }
                pendingApprovals_.erase(it);
                break;
            }
            if (waitAsync) {
                // 스펙 §6 결정 5 + 리바인드 (docs/59 §13): file.open_result
                // 이벤트는 승인 항목 매칭과 독립으로 방송 — 정상 waitAsync
                // 슬롯(항목 있음)과 리바인드 슬롯(항목 없음 — 요청자 교체로
                // requestId=0) 양쪽이 이 한 경로로 방송된다. 항목 미매칭
                // waitAsync의 기존 무통지 경로(해소됐는데 구독자가 영원히 못
                // 받는)도 여기서 마감. 원래 쿼리는 parked ack로 이미 회답됐다
                // — AgentReply는 생략(이중 전달 금지). 취소(ok=false)는
                // 사용자의 정당한 행동이라 error 멤버 없음 — "expired"만이
                // 오류 문자열(스펙 §6 페이로드).
                std::string ev = ok
                    ? (path.empty()
                           ? "{\"topic\":\"file.open_result\",\"ok\":true}"
                           : "{\"topic\":\"file.open_result\",\"ok\":true,"
                             "\"path\":\"" +
                                 JsonEsc(path) + "\"}")
                    : "{\"topic\":\"file.open_result\",\"ok\":false}";
                PushAgentEventJson(ev);
            }
            pendingFileDialog_ = PendingFileDialog{};  // 슬롯 소진 (결과와 무관)
        }
        reply = resolved ? "{\"ok\":true,\"parked\":true}"
                         : "{\"tool\":\"file_open_result\","
                           "\"ok\":true,\"parked\":false}";
    } else {
        reply = "{\"ok\":false,\"error\":\"not_implemented\"}";
    }
    // The ask path parks the query — its reply is sent when the approval
    // resolves (approve tool), when the filedlg app answers (file_open_result)
    // or when it times out (expiry scan below).
    if (replied) {
        ipc::WriteAgentJson(client.Transport(), ipc::MsgType::AgentReply,
                            queryId, 1, reply);
    }
}

// Push a desktop event JSON to every subscribed control-only client.
// Callers hold clientsMutex_ (the call sites do).
void JKWindowServer::PushAgentEvent(const char* topic, uint32_t id,
                                    const std::string& title, uint32_t pid) {
    const long long ts = static_cast<long long>(std::time(nullptr)) * 1000;
    char buf[640];
    std::snprintf(buf, sizeof(buf),
                  "{\"topic\":\"%s\",\"id\":%u,\"title\":\"%s\",\"pid\":%u,\"ts\":%lld}",
                  topic, id, JsonEsc(title).c_str(), pid, ts);
    PushAgentEventJson(buf);
}

// Push a fully-formed agent event JSON (topic included) to subscribers.
void JKWindowServer::PushAgentEventJson(const std::string& json) {
    // events_list stats (docs/32): every emit site starts the envelope with
    // {"topic":"..." — a raw scan avoids a JSON parse on the hot path.
    static constexpr char kKey[] = "\"topic\":\"";
    const size_t keyPos = json.find(kKey);
    if (keyPos != std::string::npos) {
        const size_t start = keyPos + sizeof(kKey) - 1;
        const size_t end = json.find('"', start);
        if (end != std::string::npos) {
            TopicStat& st = topicStats_[json.substr(start, end - start)];
            ++st.fired;
            st.lastTs = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());
        }
    }
    for (auto& c : clients_) {
        // M2a: window clients can opt in too (the palette does, via
        // AgentEventSubscribe on its regular connection); control-only agent
        // connections declare the flag at connect time.
        if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
            ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentEvent, 0, 1, json);
        }
    }
}

// 앱 도구 허브 (스펙 §4.1): 소문자+숫자+밑줄 토큰 검증. std::regex 대신
// 수기 — 기존 sha256/fingerprint 검증기 관용구.
static bool ValidAppToolToken(const std::string& s, size_t maxLen) {
    if (s.empty() || s.size() > maxLen) return false;
    if (s[0] < 'a' || s[0] > 'z') return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

// MINOR-3 (fix round 1): 서버 측 스텝 델타 상한 — JKSemanticCursor의
// kMaxDelta는 익명 namespace라 미수출. 동일 값을 서버가 자체 보유해 move
// 사전 검증에서 사용(이중 방어의 서버 쪽 — 수치 일치는 클래스 헤더 주석의
// 계약).
namespace { constexpr int kCursorMaxDelta = 1 << 20; }

// MINOR-2 (fix round 1): move 에러 응답에 이동 전 위치 에코(스펙 §4 —
// 상태 직렬화). 파싱 실패(bad_args)도 현재 위치를 실어 LLM 재시도 힌트를
// 준다. error는 신뢰 토큰(서버 상수)만 들어온다 — 문자열 이스케이프 불요.
static std::string MoveErrorReply(const JKSemanticCursor& cur,
                                  const char* error) {
    std::string out = "{\"ok\":false,\"error\":\"";
    out += error;
    out += "\",\"row\":";
    out += std::to_string(cur.Row());
    out += ",\"col\":";
    out += std::to_string(cur.Col());
    out += "}";
    return out;
}

// MINOR-1 (fix round 1): 앱이 자기 act 도구를 이미 등록한 경우 — 선언 커서
// 블록의 kinds를 앱 자체 act 스키마의 "kind" enum에 병합해 카탈로그
// tools/list가 kinds를 보이게 한다(플래그십 경로 — minesweeper가
// tools:[act,snapshot]+cursor를 함께 선언).
// AgentJson throwaway 런타임 재파싱 대신 문자열 인지 브레이스 매처(원문
// 보존 — 스키마는 2KiB 상한이라 상수 비용). 병합 불가(비객체 스키마/결과
// 2KiB 초과)면 스키마를 그대로 둔다 — 등록 실패로 끌어내리지 않는다(선언
// 자체는 유효).
// fix round 2: 전 구간 문자열 인지 스캔으로 재작성 — (NIT) 라운드 1의
// find("properties") 진입점은 스키마 문자열 리터럴 내부의 "properties"/"{"
// (description 텍스트 등)를 착진했고, (MAJOR) kind 키 스캔은 여는 따옴표를
// 먼저 소비한 뒤 테스트하는 순서라 도달 불가 — kind 선언 스키마에
// 중복 "kind" 키를 주입(JSON.parse last-wins로 앱 원 enum 승리, 선언
// kinds 유실). 키 검사는 따옴표 소비 전에 수행하고, 라운드 2에서
// 빈 properties 삽입 시 쉼표를 생략(트레일링 콤마 → CLI JSON.parse
// 파산 방지, docs/59 §12). 위치가 어긋나면(닫힘 불가/비객체 값) 무변경.
// schema는 in/out(참조 치환). kinds는 이미 검증 토큰(ValidAppToolToken).
static size_t JsonMatchBrace(const std::string& s, size_t i) {
    const char open = s[i];
    const char close = (open == '{') ? '}' : ']';
    int depth = 0;
    bool inStr = false;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (inStr) {
            if (c == '\\') { ++i; continue; }
            if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') { inStr = true; continue; }
        if (c == open) ++depth;
        else if (c == close && --depth == 0) return i;
    }
    return std::string::npos;
}

static void InjectKindsEnum(std::string& schema,
                            const std::vector<std::string>& kinds) {
    std::string kindObj = "{\"type\":\"string\",\"enum\":[";
    for (size_t i = 0; i < kinds.size(); ++i) {
        if (i) kindObj += ",";
        kindObj += "\"" + kinds[i] + "\"";
    }
    kindObj += "]}";

    // 1) 루트 객체 확인 — 비객체 스키마는 무변경(잘못된 대상에 주입 금지).
    const size_t start = schema.find_first_not_of(" \t\r\n");
    if (start == std::string::npos || schema[start] != '{') return;

    // 2) 문자열 인지 루트 스캔 — 루트 직속(깊이 1) 키 중 "properties" 탐색.
    //    스키마 내부 문자열 리터럴(예: description에 든 "properties")은
    //    inStr로 건너뛴다.
    size_t propsOpen = std::string::npos, propsClose = std::string::npos;
    {
        int depth = 1;
        bool inStr = false;
        for (size_t i = start + 1; i < schema.size(); ++i) {
            char c = schema[i];
            if (inStr) {
                if (c == '\\') { ++i; continue; }
                if (c == '"') inStr = false;
                continue;
            }
            if (c == '"') {
                if (depth == 1 && i + 12 <= schema.size() &&
                    schema.compare(i, 12, "\"properties\"") == 0) {
                    size_t v = schema.find(':', i + 12);
                    if (v == std::string::npos) return;
                    do { ++v; }
                    while (v < schema.size() &&
                           (schema[v] == ' ' || schema[v] == '\t'));
                    if (v >= schema.size() || schema[v] != '{') return;
                    propsOpen = v;
                    propsClose = JsonMatchBrace(schema, propsOpen);
                    if (propsClose == std::string::npos) return;
                    break;
                }
                inStr = true;
                continue;
            }
            if (c == '{' || c == '[') ++depth;
            else if (c == '}' || c == ']') {
                --depth;
                if (depth == 0) break;   // 루트 종료
            }
        }
    }
    if (propsOpen == std::string::npos) {
        // properties 없는(루트 객체는 확인된) 스키마 — 표준 형태로 전면 교체.
        schema = "{\"type\":\"object\",\"properties\":{\"kind\":" + kindObj +
                 "},\"required\":[\"kind\",\"row\",\"col\"]}";
        return;
    }

    // 3) props 직속(상대 깊이 0) 키 중 "kind" 탐색 — 키 검사는 따옴표를
    //    소비하기 전에(라운드 1 결함: 소비 후 검사라 도달 불가).
    size_t kindOpen = std::string::npos, kindClose = std::string::npos;
    bool propsEmpty = true;
    {
        int depth = 0;
        bool inStr = false;
        for (size_t i = propsOpen + 1; i < propsClose; ++i) {
            char c = schema[i];
            if (!inStr && c != ' ' && c != '\t' && c != '\r' && c != '\n')
                propsEmpty = false;
            if (inStr) {
                if (c == '\\') { ++i; continue; }
                if (c == '"') inStr = false;
                continue;
            }
            if (c == '"') {
                if (depth == 0 && i + 6 <= propsClose &&
                    schema.compare(i, 6, "\"kind\"") == 0) {
                    size_t v = schema.find(':', i + 6);
                    if (v == std::string::npos || v >= propsClose) return;
                    do { ++v; }
                    while (v < propsClose &&
                           (schema[v] == ' ' || schema[v] == '\t'));
                    if (v >= propsClose || schema[v] != '{') return;
                    kindOpen = v;
                    kindClose = JsonMatchBrace(schema, kindOpen);
                    if (kindClose == std::string::npos ||
                        kindClose >= propsClose)
                        return;
                    break;
                }
                inStr = true;
                continue;
            }
            if (c == '{') ++depth;
            else if (c == '}') --depth;
        }
    }
    std::string out;
    if (kindOpen != std::string::npos) {
        // 3a) kind 존재 — 값 객체 전체를 치환(기존 enum을 선언 kinds로
        // 통일 — 카탈로그는 서버가 아는 진실원).
        out = schema.substr(0, kindOpen) + kindObj +
              schema.substr(kindClose + 1);
    } else if (propsEmpty) {
        // 3b-i) 빈 properties — 뒤 내용이 없어 쉼표 없이(트레일링 콤마 금지).
        out = schema.substr(0, propsOpen + 1) + "\"kind\":" + kindObj +
              schema.substr(propsClose);
    } else {
        // 3b-ii) kind 부재 — props 머리에 주입(뒤 내용이 있으므로 쉼표).
        out = schema.substr(0, propsOpen + 1) + "\"kind\":" + kindObj + "," +
              schema.substr(propsOpen + 1);
    }
    if (out.size() > 2 * 1024) return;   // 등록 상한 초과 — 병합 보류
    schema = std::move(out);
}

// 스펙 §4.1: 등록 검증 — bad_app/bad_name/too_many_tools/schema_too_large/
// namespace_conflict. 셸 특권 연결의 등록은 봉쇄(셸 도구 표면 오염 방지).
// ack는 AgentReply 재사용(queryId=0 상수 — 앱 쿼리 id는 1부터 시작하는
// 기존 관례와 충돌 없음; 앱은 미인지 queryId reply를 무시한다 — 정보성).
void JKWindowServer::HandleToolRegister(JKClientConnection& client,
                                        const std::string& json) {
    auto ack = [&](bool ok, const char* err) {
        std::string body = ok ? std::string("{\"ok\":true}")
            : std::string("{\"ok\":false,\"error\":\"") + err + "\"}";
        ipc::WriteAgentJson(client.Transport(), ipc::MsgType::AgentReply,
                            0, ok ? 1u : 0u, body);
    };
    if (client.IsShell()) { ack(false, "shell_denied"); return; }
    jk::agent::AgentJson req(json);
    std::string app;
    if (!req.ok() || !req.GetStr("app", app)) { ack(false, "bad_request"); return; }
    if (!ValidAppToolToken(app, 16)) { ack(false, "bad_app"); return; }
    int toolCount = 0;
    if (!req.GetArraySize("tools", toolCount)) { ack(false, "bad_request"); return; }
    if (toolCount < 0 || toolCount > 32) { ack(false, "too_many_tools"); return; }
    // namespace_conflict: 코어 도구명(kPermMatrix 전 행 = 서버 도구 전체
    // 목록 — "app_tool"/"list_app_tools" 행 포함)만 금지. 같은 connId
    // 재등록은 언제나 upsert 허용. (조건문 없이 전 app를 검사한다 —
    // app="app_tool"은 kPermMatrix의 app_tool 행과 걸려서 자동 봉쇄.)
    // ⚠ 스펙 §4.1의 "기존 등록된 다른 연결의 app와 충돌" 절은 §4.2 다중
    // 인스턴스 변별(windowId 직행/단독 후보/ambiguous+후보)과 모순 —
    // §4.2가 승리(사용자 확정 설계, 코디네이터 러링 2026-09-19). 같은 app의
    // 연결간 중복 등록은 허용되고, 복수 후보는 app_tool이 ambiguous로
    // 자기교정을 유도한다(브로커는 tools/list에서 이름 유니온으로 중복 흡수).
    for (const auto& row : kPermMatrix)
        if (app == row.tool) { ack(false, "namespace_conflict"); return; }
    // 스펙 §4.1(구속력): 이벤트 예약 접두도 namespace_conflict — 최종리뷰
    // Important 1. 서버 토픽 접두(kReservedTopicPrefixes — publish_event와
    // 동일 원본, events_list 카탈로그 "server" 접두와 동일 집합)와 app명의
    // 정확 일치(app="window"/"agent"…) 또는 도구명의 접두 충돌
    // ("window.created" 류 점 표기명)를 금지한다. docs/54 NIT-3 토픽 위장
    // 방어 계열 선례 — 후속 소비자가 카탈로그를 접두 해석해도 충돌층이
    // 되지 않게 계약상 봉쇄. (도구명은 ValidAppToolToken이 점을 이미
    // 배제하므로 접두 검사는 사실상 도달하지 않는 2중 방어.)
    for (const char* p : kReservedTopicPrefixes) {
        const size_t plen = std::strlen(p);
        const std::string tok(p, plen - 1);   // 마지막 '.' 제외 토큰
        if (app == tok) { ack(false, "namespace_conflict"); return; }
        for (int i = 0; i < toolCount; ++i) {
            std::string tn;
            if (req.GetArrStr("tools", i, "name", tn) &&
                tn.compare(0, plen, p) == 0) {
                ack(false, "namespace_conflict"); return;
            }
        }
    }
    // 의미 커서 (스펙 2026-09-22-semantic-cursor §2): 선택 cursor 블록.
    // 부재 = 미선언(기존 동작 불변). 파싱 실패/미지원 값은 등록 전체 거부
    // (fail-closed — "등록 성공 + 커서만 유실" 혼종 봉쇄: 커서 선언은 런타임
    // 재전송 가능하므로 앱은 cursor 블록을 빼고 재시도할 수 있다).
    AppToolManifest::CursorDecl cd;
    std::string cursorRaw;
    if (req.GetRaw("cursor", cursorRaw) && !cursorRaw.empty() &&
        cursorRaw != "null") {
        // NIT-8 (fix round 1): 컨트롤 전용 연결의 커서 선언은 등록 시점 거부
        // (fail-closed) — windowId==0은 커서 규약의 소유 창 자격이 없고
        // read 중계 대상(snapshot 도구)도 세이프 존 밖. ack(false) 관용구.
        if (client.IsControlOnly()) {
            ack(false, "cursor_window_required");
            return;
        }
        if (cursorRaw.size() > 4 * 1024) { ack(false, "bad_cursor"); return; }
        jk::agent::AgentJson cb(cursorRaw);
        std::string ty, space, owner;
        if (!cb.ok() || !cb.GetStr("type", ty) || ty != "cell-grid") {
            ack(false, "bad_cursor"); return;
        }
        // coordSpace 부재 = client (v1 유일 좌표계 — 스펙 §2).
        if (cb.GetStr("coordSpace", space) && space != "client") {
            ack(false, "bad_cursor"); return;
        }
        // cursorOwner 부재 = platform (v1 유일 소유권 — "app"은 명시되어야
        // 거절된다: 네이티브 커서 앱의 무성의 선언을 조용히 플랫폼으로 뒤집지
        // 않는 것이 fail-closed).
        if (cb.GetStr("cursorOwner", owner) && owner != "platform") {
            ack(false, "cursor_owner_unsupported"); return;
        }
        if (!cb.GetObjInt("origin", "x", cd.originX) || cd.originX < 0 ||
            cd.originX > 100000 ||
            !cb.GetObjInt("origin", "y", cd.originY) || cd.originY < 0 ||
            cd.originY > 100000 ||
            !cb.GetInt("cellW", cd.cellW) || cd.cellW < 1 ||
            cd.cellW > 4096 ||
            !cb.GetInt("cellH", cd.cellH) || cd.cellH < 1 ||
            cd.cellH > 4096 ||
            !cb.GetInt("rows", cd.rows) || cd.rows < 1 || cd.rows > 1024 ||
            !cb.GetInt("cols", cd.cols) || cd.cols < 1 || cd.cols > 1024) {
            ack(false, "bad_cursor"); return;
        }
        // act.kinds는 3단 배열 — AgentJson의 2단 리더로는 못 읽는다(레슨 39):
        // act 원문을 재파싱해 읽는다.
        std::string actRaw;
        if (!cb.GetRaw("act", actRaw)) { ack(false, "bad_cursor"); return; }
        jk::agent::AgentJson actj(actRaw);
        std::string gate;
        if (!actj.ok() || !actj.GetStr("gate", gate) || gate != "ask") {
            ack(false, "bad_cursor"); return;   // v1 act 게이트 ask 고정(스펙 §2)
        }
        int kinds = 0;
        if (!actj.GetArraySize("kinds", kinds) || kinds < 1 || kinds > 32) {
            ack(false, "bad_cursor"); return;
        }
        for (int i = 0; i < kinds; ++i) {
            std::string k;
            if (!actj.GetArrValStr("kinds", i, k) ||
                !ValidAppToolToken(k, 24)) {
                ack(false, "bad_cursor"); return;
            }
            cd.actKinds.push_back(std::move(k));
        }
        cd.valid = true;
    }
    AppToolManifest m;
    m.connId = client.Id();
    m.app = app;
    m.windowId = client.IsControlOnly() ? 0u : client.Id();
    m.title = client.Title();
    // 스펙 §4: 모달 플래그(선택 필드, 부재=false). AgentJson에 bool 리더가
    // 없으므로 기존 파서 계약(파킹 쿼리 ok/ok 필드 등 — bool은 0/1 int,
    // jkagentd settings_set 선례)대로 GetInt로 수용한다. 0이 아니면 true.
    int modalArg = 0;
    m.modal = req.GetInt("modal", modalArg) && modalArg != 0;
    for (int i = 0; i < toolCount; ++i) {
        AppToolDef d;
        if (!req.GetArrStr("tools", i, "name", d.name) ||
            !ValidAppToolToken(d.name, 32)) { ack(false, "bad_name"); return; }
        req.GetArrStr("tools", i, "description", d.description);
        if (d.description.size() > 512) { ack(false, "schema_too_large"); return; }
        // inputSchema는 원문 JSON — AgentJson::GetArrRaw로 배열 원소 안
        // 필드를 원문으로 회수. 부재 시 빈 문자열(카탈로그가 {}로 방출).
        if (!req.GetArrRaw("tools", i, "inputSchema", d.inputSchema))
            d.inputSchema.clear();
        if (d.inputSchema.size() > 2 * 1024) { ack(false, "schema_too_large"); return; }
        m.tools.push_back(std::move(d));
    }
    // 의미 커서 (스펙 §3): 선언 존재 시 합성 도구 3종을 매니페스트에 등록한다
    // — 카탈로그(list_app_tools → tools/list 동적부)와 app_tool 릴레이 후보
    // 수집이 자동으로 흘러간다. 앱 자체 도구와의 충돌은 move/read만 봉쇄
    // (플랫폼 구현이 앱 자체 도구를 가리는 혼종 방지) — act는 앱 자체 act가
    // 있으면 그 도구로 중계(스펙 §2의 앱 계약), 없으면 합성 act(이름만 — 앱이
    // act를 아직 선언하지 않으면 중계가 앱 측 에러를 돌려준다).
    if (cd.valid) {
        auto hasTool = [&](const char* n) {
            for (const AppToolDef& t : m.tools)
                if (t.name == n) return true;
            return false;
        };
        if (hasTool("move") || hasTool("read")) {
            ack(false, "cursor_name_conflict"); return;
        }
        AppToolDef mv;
        mv.name = "move";
        mv.description =
            "Semantic cursor move on the declared cell grid: to_row/to_col | "
            "dr/dc | steps[{dr,dc}] (<=32, stops at the first boundary). "
            "Echoes the reached cell. Platform-owned, no approval.";
        mv.inputSchema =
            "{\"type\":\"object\",\"properties\":{\"to_row\":{\"type\":"
            "\"integer\"},\"to_col\":{\"type\":\"integer\"},\"dr\":{\"type\":"
            "\"integer\"},\"dc\":{\"type\":\"integer\"},\"steps\":{\"type\":"
            "\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"dr\":"
            "{\"type\":\"integer\"},\"dc\":{\"type\":\"integer\"}}}}}}";
        m.tools.push_back(std::move(mv));
        AppToolDef rd;
        rd.name = "read";
        rd.description =
            "Read the semantic cursor cell plus the app's snapshot board "
            "serialization (cursor is platform-owned). Snapshot failure is an "
            "explicit error, not a silent partial read.";
        rd.inputSchema = "{\"type\":\"object\",\"properties\":{}}";
        m.tools.push_back(std::move(rd));
        if (!hasTool("act")) {
            std::string kinds = "[";
            for (size_t i = 0; i < cd.actKinds.size(); ++i) {
                if (i) kinds += ",";
                kinds += "\"" + JsonEsc(cd.actKinds[i]) + "\"";
            }
            kinds += "]";
            AppToolDef ac;
            ac.name = "act";
            ac.description =
                "Semantic act on the cursor cell (kind enum from the app's "
                "cursor declaration). Approval-gated (ask default); the app "
                "owns the game transition. reset ignores row/col (send 0,0).";
            ac.inputSchema =
                "{\"type\":\"object\",\"properties\":{\"kind\":{\"type\":"
                "\"string\",\"enum\":" + kinds + "},\"row\":{\"type\":"
                "\"integer\"},\"col\":{\"type\":\"integer\"}},\"required\":"
                "[\"kind\",\"row\",\"col\"]}";
            m.tools.push_back(std::move(ac));
        } else {
            // MINOR-1 (fix round 1): 앱이 자기 act 도구를 등록한 경로 —
            // 선언 kinds를 앱 자체 act 스키마의 "kind" enum에 병합(카탈로그
            // tools/list가 kinds를 보이게 — 플래그십 minesweeper 경로).
            for (AppToolDef& t : m.tools) {
                if (t.name != "act") continue;
                InjectKindsEnum(t.inputSchema, cd.actKinds);
                break;
            }
        }
        m.cursor = cd;
        m.cursorState.Reset(cd.rows, cd.cols);   // 커서는 (0,0)에서 시작
    }
    appToolManifests_[client.Id()] = m;   // upsert
    ack(true, "");
    PublishAppToolsChanged();
}

// 스펙 §3/§4.2: 앱의 AgentToolResult를 reqId 상관관계로 원 요청자에 회송.
// 요청자 에코 상관관계(filedlg requesterConnId 선례) — 남의 reqId는 무시.
void JKWindowServer::HandleToolResult(JKClientConnection& client,
                                      const ipc::Message& msg) {
    uint32_t reqId = 0, ok = 0; std::string json;
    if (!ipc::ReadAgentJson(msg, reqId, ok, json)) return;
    auto it = inflightAppTools_.find(reqId);
    if (it == inflightAppTools_.end()) return;
    // 상관관계를 크기 상한보다 먼저 — nextToolReqId_는 1부터 순차라 남의
    // reqId를 추측해 >16KiB를 보내면 진짜 요청자의 대기 호출을
    // result_too_large로 소멸시키는 주입이 됐다(리뷰 Important 1).
    if (it->second.targetConnId != client.Id()) return;
    // 결과 상한 (스펙 §4.1 — 16KiB→256KiB 상향, docs/60 §5 백로그 소각):
    // get_script가 대형 스크립트 원문을 result로 반환하며 16KiB에서 잘렸다.
    // args 캡(:3166, 256KiB)과 대칭 — set_script로 들어온 스크립트의
    // JSON 이스케이프 결과는 argsRaw와 동일 형태라 256KiB 안에서 왕복 보장.
    if (json.size() > 256 * 1024) {
        ReplyAppToolError(it->second, "result_too_large");
        inflightAppTools_.erase(it);
        return;
    }
    // MINOR-3 — reset act가 ok로 끝나면 커서를 정의 전이(좌상단 (0,0))으로
    // 리셋(스펙 §4 — 게임 리셋=정의 전이, 커서 상태는 플랫폼 소유). 앱 에러
    // (bad_state 등)에는 리셋하지 않는다. 요청자 발견 분석 밖에서 실행 —
    // 요청자 연결이 파킹~결과 사이 소실해도(게임 리셋은 이미 일어났으니)
    // 플랫폼 커서가 이전 칸에 남는 낙차를 막는다(NIT-2 최종리뷰).
    if (ok && it->second.resetCursorOnOk) {
        auto mit = appToolManifests_.find(it->second.targetConnId);
        if (mit != appToolManifests_.end() && mit->second.cursor.valid)
            mit->second.cursorState.Reset(mit->second.cursor.rows,
                                          mit->second.cursor.cols);
    }
    for (auto& c : clients_) {
        if (c && c->Id() == it->second.requesterConnId && !c->IsDisconnected()) {
            std::string reply;
            // 의미 커서 read (스펙 §3): 앱 snapshot 결과를 커서 헤더로 조립해
            // 회송한다. 앱이 ok=0(자체 에러)로 답해도 커서 필드는 유지 —
            // 명시 에러만 추가(부분 성공 위장 금지).
            if (it->second.composeCursorRead)
                reply = ComposeCursorRead(it->second.targetConnId,
                                          it->second.windowId, ok != 0, json,
                                          nullptr);
            if (reply.empty())
                reply = std::string("{\"ok\":true,\"windowId\":") +
                    std::to_string(it->second.windowId) + "," +
                    (ok ? "\"result\":" + json : "\"error\":" + json) + "}";
            ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                it->second.queryId, ok, reply);
            break;
        }
    }
    inflightAppTools_.erase(it);
}

void JKWindowServer::PublishAppToolsChanged() {
    PushAgentEventJson("{\"topic\":\"agent.app_tools_changed\"}");
}

void JKWindowServer::ReplyAppToolError(const InflightAppTool& inf,
                                       const char* err) {
    for (auto& c : clients_) {
        if (c && c->Id() == inf.requesterConnId && !c->IsDisconnected()) {
            // 의미 커서 read (스펙 §3): 전송 계열 실패(타임아웃/과대/회수)도
            // 커서 헤더를 유지한 명시 에러로 회송한다.
            std::string reply;
            if (inf.composeCursorRead)
                reply = ComposeCursorRead(inf.targetConnId, inf.windowId,
                                          false, std::string(), err);
            if (reply.empty())
                reply = std::string("{\"ok\":false,\"windowId\":") +
                    std::to_string(inf.windowId) + ",\"error\":\"" + err +
                    "\"}";
            ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                inf.queryId, 0, reply);
            return;
        }
    }
}

// Alt+Space (M2a): bring the palette to front if it is already open,
// otherwise spawn it.
void JKWindowServer::TogglePalette() {
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        found = ToggleClientByTitleUnsafe("Command Palette", "palette");
    }
    if (found) {
        PushWindowList();  // taskbar active highlight follows the refocus
    }
}

// Title-match toggle core (docs/33), caller holds clientsMutex_ (the
// AgentQuery hot path pre-condition — the non-recursive mutex deadlocks if
// we lock here). Focuses the existing client or spawns a new one.
// Returns true when an existing client was focused (caller may push the
// window list).
namespace {
// The notify app appends " (N)" to its own title via MsgType::WindowTitle
// (unread badge), so an exact-match toggle loses the key after the first
// badge update and spawns a duplicate — accept the bare title or the
// badged form "key (…)".
bool TitleMatchesToggleKey(const std::string& actual, const char* key) {
    const std::string k(key);
    if (actual == k) return true;
    return actual.size() > k.size() + 2 &&
           actual.rfind(k + " (", 0) == 0 && actual.back() == ')';
}
} // namespace

bool JKWindowServer::ToggleClientByTitleUnsafe(const char* title,
                                               const char* app) {
    for (auto& c : clients_) {
        if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) {
            continue;
        }
        if (TitleMatchesToggleKey(c->Title(), title)) {
            if (compositor_) compositor_->SetLayerVisible(c->Id(), true);
            FocusClient(c->Id());  // caller-holds-clientsMutex_ contract
            return true;
        }
    }
    SpawnClient(app);  // no lock inside — launch_app precedent
    return false;
}

// M2a server-side permission gate (spec §5): the broker (jkagentd) gates its
// own tool calls, but any connected face can also send AgentQuery directly
// (the palette does over its window connection). close_window is denied by
// default; <exeDir>\permissions.json — the same file the broker reads, both
// exes live in the same build directory — is the approval act.
// M2 chat / docs/37: "ask" means the inline-approval pipeline (chat window) —
// wired for close_window + trust_request; other tools degrade to allow since
// nothing parks them.
AgentDecision JKWindowServer::AgentToolAllowed(const std::string& tool) const {
    // permission_set은 파일 값을 무시하고 항상 Ask — 파일로 이 도구를
    // allow로 바꿔두면 이후 모든 권한 변경이 무승인이 되는 2단 우회 봉쇄
    // (스펙 §2.2 핵심 안전 결정).
    if (tool == "permission_set") return AgentDecision::Ask;
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    const std::string path = dir + "\\permissions.json";
    // Missing entry defaults: close_window denies (M1 rule), trust_request
    // ASKS (the gate would be pointless if unknown scripts loaded silently),
    // run_console_app ASKS (P4 SDK §5 — the agent launching local apps is an
    // explicit-approval act), trust_revoke ASKS (agent-manager — revoking is
    // safe-direction but ungated would let an agent burn the whole trust
    // store), everything else allows. "ask" pipelines: close_window +
    // trust_request + run_console_app + trust_revoke; permission_set is
    // hardwired Ask regardless of the file; other tools degrade to allow
    // since nothing parks them.
    const bool askCapable = (tool == "close_window" || tool == "trust_request" ||
                             tool == "run_console_app" || tool == "trust_revoke" ||
                             // docs/54 §11 opus M2 픽스: 캡처 2종의 파일값
                             // "ask"를 더 이상 Allow로 열화하지 않는다 —
                             // 도구 분기가 Ask를 capture_ask 거부로 소비
                             // (설정 허브 캡처 스위치의 승인 파킹이 flip).
                             tool == "capture_window" ||
                             tool == "capture_region" ||
                             // 파일 허브 (스펙 2026-09-18-file-hub §2.2):
                             // 파일 2종도 파일값 "ask"를 Allow로 열화하지
                             // 않는다 — 도구 분기가 FilesPermRaw로 소비
                             // (control-only 파킹). 이 스위치 자체는 파일
                             // 도구에서 호출되지 않지만(소스 분리 게이트),
                             // 값 일관성을 위해 묶는다.
                             tool == "files_list" ||
                             tool == "files_read" ||
                             // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub
                             // §4.3): app_tool도 파일값 "ask"를 Allow로
                             // 열화하지 않는다 — 실질 게이트는 AppToolAllowed
                             // 3단 키지만(이 스위치 자체는 app_tool 분기에서
                             // 호출되지 않음), 값 일관성을 위해 묶는다
                             // (files 도구 편입 선례).
                             tool == "app_tool" ||
                             // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder
                             // §3.1): send_input도 파일값 "ask"를 Allow로
                             // 열화하지 않는다 — 도구 분기가 승인 파킹으로
                             // 소비(kPermMatrix "ask" 기본 행과 쌍).
                             tool == "send_input");
    auto defaultDecision = [&]() -> AgentDecision {
        if (tool == "close_window") return AgentDecision::Deny;
        if (tool == "trust_request") return AgentDecision::Ask;
        if (tool == "run_console_app") return AgentDecision::Ask;
        if (tool == "trust_revoke") return AgentDecision::Ask;
        // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): 파일 부재/
        // 키 부재 기본도 ask — kPermMatrix "ask" 행과 정합(2026-09-21
        // 사용자 승인: 합성 입력은 승인 행위).
        if (tool == "send_input") return AgentDecision::Ask;
        return AgentDecision::Allow;
    };
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return defaultDecision();
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson perm(buf);
    std::string value;
    if (!perm.ok() || !perm.GetStr(tool.c_str(), value)) {
        return defaultDecision();
    }
    if (value == "allow") return AgentDecision::Allow;
    if (value == "ask") {
        return askCapable ? AgentDecision::Ask : AgentDecision::Allow;
    }
    if (value == "deny") return AgentDecision::Deny;
    return defaultDecision();
}

// 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.3): app_tool 3단 키 해석 —
// app_tool.<app>.<tool> > app_tool.<app> > app_tool (도구별 > 앱별 > 전역).
// permissions.json은 점을 포함한 평면 키 리터럴로 기록한다 — AgentJson::GetStr
// 은 JS_GetPropertyStr 한 단계라 중첩 객체는 못 읽지만, 점 키 프로퍼티는 그대로
// 읽힌다. 파일 부재/키 부재 폴백 = allow (스펙 §0 결정 3). permissions.json
// 핫리드는 AgentToolAllowed 기존 계약(호출마다 읽음) 유지. 클라이언트 락 없음
// (레슨 35 — HandleAgentQuery 보유 중 호출).
AgentDecision JKWindowServer::AppToolAllowed(const std::string& app,
                                             const std::string& tool,
                                             AgentDecision dflt) const {
    std::string k0 = "app_tool." + app + "." + tool;
    std::string k1 = "app_tool." + app;
    const char* keys[3] = {k0.c_str(), k1.c_str(), "app_tool"};
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    std::FILE* f = std::fopen((dir + "\\permissions.json").c_str(), "rb");
    if (!f) return dflt;
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson perm(buf);
    if (!perm.ok()) return dflt;
    for (const char* k : keys) {
        std::string v;
        if (perm.GetStr(k, v)) {
            if (v == "allow") return AgentDecision::Allow;
            if (v == "ask") return AgentDecision::Ask;
            if (v == "deny") return AgentDecision::Deny;
        }
    }
    return dflt;
}

// ---- 의미 커서 (스펙 2026-09-22-semantic-cursor §3) -----------------------
// 앱 도구 릴레이의 커서 선언 앱 합성 도구 인터셉트. clientsMutex_ 보유 경로
// 전용(HandleAgentQuery 호출사슬) — 락을 잡지 않는다(레슨 35).
//
//   move — 플랫폼 동기 구현: 인자(to_row/to_col | dr/dc | steps[])를 검증해
//          JKSemanticCursor 상태를 갱신하고 도달 칸을 에코. 무승인(무해 — 판
//          상태 불변). 앱 연결 무접촉(앱 무응답에도 이동 성립 — 스펙 §5).
//   read — 플랫폼 조립(비동기): 앱의 snapshot app_tool 중계를 시작하고
//          HandleToolResult가 커서 헤더를 붙여 회송한다(composeCursorRead).
//   act  — 사전 검증만(스펙 §3 act, Task 2): kind가 선언 enum 밖/인자 누락 =
//          bad_args, 격자 밖 인덱스 = bad_grid(요청 칸 에코) — 앱 도달 전
//          거부(앱이 자체 act를 등록한 경로도 선언 enum으로 검증한다).
//          유효 요청은 false → 기존 릴레이(kind/row/col 원문 패스스루,
//          게이트 ask 기본+파킹 시 셀 rect 고정).
bool JKWindowServer::HandleCursorAppTool(JKClientConnection& client,
                                         uint32_t queryId,
                                         const std::string& app,
                                         const std::string& toolName,
                                         const std::string& argsRaw,
                                         std::string& reply, bool& replied) {
    if (argsRaw.size() > 256 * 1024) return false;   // args_too_large는 기존 경로
    // 후보 수집 — app_tool 릴레이와 동일 규약(app+도구명 역매칭).
    std::vector<AppToolManifest*> cands;
    for (auto& kv : appToolManifests_) {
        AppToolManifest& m = kv.second;
        if (m.app != app || !m.cursor.valid) continue;
        bool has = false;
        for (const AppToolDef& t : m.tools)
            if (t.name == toolName) { has = true; break; }
        if (!has) continue;
        cands.push_back(&m);
    }
    if (cands.empty()) return false;
    // windowId 변별 (스펙 §4.2) — 릴레이와 동일 규약.
    {
        jk::agent::AgentJson a(argsRaw.empty() ? "{}" : argsRaw);
        int windowIdArg = 0;
        if (a.GetInt("windowId", windowIdArg) && windowIdArg > 0) {
            std::vector<AppToolManifest*> filtered;
            for (AppToolManifest* c : cands)
                if (c->windowId == static_cast<uint32_t>(windowIdArg))
                    filtered.push_back(c);
            cands.swap(filtered);
            if (cands.empty()) {
                reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
                return true;
            }
        }
    }
    if (cands.size() > 1) {
        // 복수 인스턴스 — 묵시적 추측 라우팅 금지(스펙 §4.2, 릴레이 동일).
        std::string list = "[";
        for (size_t i = 0; i < cands.size(); ++i) {
            if (i) list += ",";
            list += "{\"windowId\":" + std::to_string(cands[i]->windowId) +
                    ",\"title\":\"" + JsonEsc(cands[i]->title) + "\"}";
        }
        reply = "{\"ok\":false,\"error\":\"ambiguous\",\"candidates\":" +
                list + "]}";
        return true;
    }
    AppToolManifest* m = cands.front();
    // MINOR-4 (fix round 1): 명시 deny 존중 — permissions.json의
    // app_tool.<app>.<tool>/app_tool.<app>/app_tool 키가 "deny"면 거부.
    // 무키 폴백 = allow(스펙 §3 무해 이동/읽기 — window_move 분류), 명시
    // "ask"도 승인 행위가 아닌 이동/읽기라 allow로 소화한다(코디네이터 룰링:
    // 명시 deny만 거부). 거부 응답도 move/read 에러 규약을 따라 이동 전 위치를
    // 에코한다(MoveErrorReply — 모든 move/read 에러의 {row, col} 일관).
    // NIT-4 최종리뷰.
    if (AppToolAllowed(app, toolName) == AgentDecision::Deny) {
        reply = MoveErrorReply(m->cursorState, "denied");
        return true;
    }
    if (toolName == "act") {
        // 스펙 §3 act (Task 2): 앱 도달 전 서버 검증 — 릴레이는 args를 원문
        // 패스스루하므로 유효 요청의 kind/row/col은 무변화(위장 개입 금지).
        // 거절만 여기서 흡수한다(에코 규약 — 격자 밖은 요청 칸을 실어 회신).
        jk::agent::AgentJson a(argsRaw.empty() ? "{}" : argsRaw);
        std::string kind;
        int row = 0, col = 0;
        if (!a.GetStr("kind", kind) || !a.GetInt("row", row) ||
            !a.GetInt("col", col)) {
            reply = "{\"ok\":false,\"error\":\"bad_args\"}";
            return true;
        }
        bool known = false;
        for (const std::string& k : m->cursor.actKinds) {
            if (k == kind) { known = true; break; }
        }
        if (!known) {
            // 선언 enum 밖 kind — 앱(게임 전이 소유자)이 아닌 서버가 거부.
            reply = "{\"ok\":false,\"error\":\"bad_args\"}";
            return true;
        }
        if (row < 0 || row >= m->cursorState.Rows() ||
            col < 0 || col >= m->cursorState.Cols()) {
            // 격자 밖 인덱스 — 파킹해도 셀 rect를 고정할 수 없다(스펙 §3 act).
            // 인자는 도달했으니 요청 칸을 에코(MINOR-2 move 에코 선례).
            reply = "{\"ok\":false,\"error\":\"bad_grid\",\"row\":" +
                    std::to_string(row) + ",\"col\":" + std::to_string(col) +
                    "}";
            return true;
        }
        return false;   // 유효 — 기존 릴레이(파킹/중계)가 그대로 간다
    }
    if (toolName == "move") {
        // 게이트 없음(none-allow, window_move 분류 — 스펙 §3 무해 이동;
        // 위의 명시 deny만 예외).
        jk::agent::AgentJson a(argsRaw.empty() ? "{}" : argsRaw);
        JKSemanticCursor::Result r;
        int stepCount = 0;
        if (a.GetArraySize("steps", stepCount)) {
            if (stepCount <= 0 || stepCount > 32) {
                reply = MoveErrorReply(m->cursorState, "bad_args");
                return true;
            }
            std::vector<std::pair<int, int>> steps;
            steps.reserve(static_cast<size_t>(stepCount));
            for (int i = 0; i < stepCount; ++i) {
                // 축 생략 = 0(무이동) — 단축 스텝 {"dc":3}도 수용(관대 파싱,
                // LLM 인자 관용 — GetInt 계약과 동일).
                int dr = 0, dc = 0;
                a.GetArrInt("steps", i, "dr", dr);
                a.GetArrInt("steps", i, "dc", dc);
                // MINOR-3 (fix round 1) → 룰링 정정 (fix round 2): 스텝
                // 형식의 음수 델타는 정상 이동(dr:-1 = 위, dc:-1 = 왼쪽 —
                // RunSteps가 경계 클램프). 사전 검증은 과대 크기만 —
                // |dr|/|dc| > 1<<20은 무적용 bad_args(원자성: RunSteps
                // 도중의 부분 적용이 요청자에 보이지 않게 한다). abs(INT_MIN)
                // 오버플로를 피하려 abs() 대신 부호 있는 비교. 클래스 자체
                // 가드는 유지(이중 방어).
                if (dr > kCursorMaxDelta || dr < -kCursorMaxDelta ||
                    dc > kCursorMaxDelta || dc < -kCursorMaxDelta) {
                    reply = MoveErrorReply(m->cursorState, "bad_args");
                    return true;
                }
                steps.emplace_back(dr, dc);
            }
            r = m->cursorState.RunSteps(steps);
        } else {
            int toR = 0, toC = 0, dr = 0, dc = 0;
            const bool hasToR = a.GetInt("to_row", toR);
            const bool hasToC = a.GetInt("to_col", toC);
            const bool hasDr = a.GetInt("dr", dr);
            const bool hasDc = a.GetInt("dc", dc);
            if (hasToR && hasToC) {
                r = m->cursorState.MoveTo(toR, toC);
            } else if (hasDr && hasDc) {
                r = m->cursorState.MoveRelative(dr, dc);
            } else {
                reply = MoveErrorReply(m->cursorState, "bad_args");
                return true;
            }
        }
        if (r.ok()) {
            reply = "{\"ok\":true,\"row\":" + std::to_string(r.row) +
                    ",\"col\":" + std::to_string(r.col) + "}";
        } else {
            // MINOR-2 (fix round 1): 에러도 에코(스펙 §4 — 상태 직렬화) —
            // Result의 위치(이동 전 상태 불변)를 실어 보낸다.
            reply = MoveErrorReply(m->cursorState, r.error);
        }
        return true;
    }
    // toolName == "read" — 앱 snapshot 중계 + 커서 헤더 조립. 무승인(none-
    // allow)이지만 앱 연결 생존은 필요하다(중계 대상).
    // NIT-7 (fix round 1): snapshot 도구를 선언하지 않은 커서 앱은 즉답 —
    // 중계해 봐야 10s tool_timeout 확정 실패(카탈로그 계약 위반을 조기
    // 표면화).
    bool hasSnapshot = false;
    for (const AppToolDef& t : m->tools)
        if (t.name == "snapshot") { hasSnapshot = true; break; }
    if (!hasSnapshot) {
        reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
        return true;
    }
    JKClientConnection* conn = nullptr;
    for (const auto& c : clients_) {
        if (c && c->Id() == m->connId && !c->IsDisconnected()) {
            conn = c.get();
            break;
        }
    }
    if (!conn) {
        // 매니페스트는 살아 있지만 연결이 끊김 — 릴레이와 동일 수명 사건.
        reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
        return true;
    }
    const uint32_t reqId = nextToolReqId_++;
    InflightAppTool inf;
    inf.reqId = reqId;
    inf.queryId = queryId;
    inf.requesterConnId = client.Id();
    inf.targetConnId = conn->Id();
    inf.windowId = m->windowId;
    inf.expiresAt = std::time(nullptr) + 10;
    inf.composeCursorRead = true;
    inflightAppTools_[reqId] = inf;
    // snapshot 인자는 호출자 args 원문 패스스루(빈 args = {}). read 자체에는
    // 정의된 인자가 없다 — 앱 계약 확장 여지(포맷 옵션류)만 남긴다.
    std::string callJson = "{\"app\":\"" + JsonEsc(app) +
                           "\",\"tool\":\"snapshot\",\"args\":" +
                           (argsRaw.empty() ? "{}" : argsRaw) + "}";
    ipc::WriteAgentToolCall(conn->Transport(), reqId, callJson);
    replied = false;   // HandleToolResult(조립)가 응답한다
    return true;
}

// 커서 read 조립 (스펙 §3 read): 커서 헤더 + 앱 snapshot 원문. 성공 =
// {"ok":true,...,"snapshot":<원문>}; 실패 = ok:false + 커서 필드 유지 + 명시
// 에러(부분 성공 위장 금지 — 코디네이터 룰링: read 실패 = 커서 + 명시 에러).
std::string JKWindowServer::ComposeCursorRead(uint32_t connId,
                                              uint32_t windowId, bool ok,
                                              const std::string& snapshot,
                                              const char* transportErr) {
    auto it = appToolManifests_.find(connId);
    if (it == appToolManifests_.end() || !it->second.cursor.valid) {
        return std::string();
    }
    const AppToolManifest& m = it->second;
    char head[256];
    std::snprintf(head, sizeof(head),
                  "{\"ok\":%s,\"windowId\":%u,\"cursor\":{\"row\":%d,"
                  "\"col\":%d},\"rows\":%d,\"cols\":%d,",
                  ok ? "true" : "false", windowId, m.cursorState.Row(),
                  m.cursorState.Col(), m.cursorState.Rows(),
                  m.cursorState.Cols());
    std::string out = head;
    if (transportErr && *transportErr) {
        out += std::string("\"error\":\"") + transportErr + "\"}";
    } else if (ok) {
        // NIT-5 (fix round 1): 앱 빈 결과 = snapshot 부재 — "unknown"으로
        // 대체(빈 문자열 그대로면 JSON 파산 {"snapshot":}).
        out += "\"snapshot\":" +
               (snapshot.empty() ? std::string("\"unknown\"") : snapshot) +
               "}";
    } else {
        out += "\"error\":\"snapshot_failed\",\"detail\":" +
               (snapshot.empty() ? std::string("\"unknown\"") : snapshot) + "}";
    }
    return out;
}

// 의미 커서 (스펙 2026-09-22-semantic-cursor §3): 승인 시점 재선언 재검증
// (Task 2) — 파킹 시 고정한 셀 rect를 최신 선언으로 재산출해 비교한다.
// - 선언 소실(앱이 cursor 블록을 뺀 재등록): 파킹 때 승인받은 계약이 사라졌다
//   — 거부(배너가 보여준 칸이 이제 어느 칸인지 앱이 알 바 없다).
// - 격자 밖(row/col이 새 격자를 벗어남): rect 산출 불가 — 거부.
// - kind가 새 선언의 act.kinds enum 밖(MINOR-1 fix round 1): 동일 기하
//   재선언이 어휘만 바꿨으면("flag"→"reveal") 배너가 보여준 행위가 이제
//   앱이 수용하지 않는 행위다 — 거부(파킹 시점 검증은 당시 선언 기준이므로
//   승인 시점에 다시 물어본다).
// - rect 불일치(origin/cell 크기가 바뀜): 배너가 가리킨 픽셀 칸이 아니다 —
//   거부. 격자 변경만으로는 거부하지 않는다(스펙 §3 — "재검증 후 어긋나면
//   거부"): 재선언이 기하를 그대로 두면 rect 항등이라 통과한다.
// 거부 토큰은 resolve 경로가 단일 bad_grid로 통일(재검증 실패 = 승인된 칸이
// 무효라는 한 사건 — 사인 세분화는 백로그).
// clientsMutex_ 보유 경로 전용(승인 resolve — HandleAgentQuery 호출사슬,
// 락을 잡지 않는다 — 레슨 35).
bool JKWindowServer::CursorActStale(const AppToolManifest& m,
                                    const PendingApproval& p) const {
    if (!m.cursor.valid) {
        return true;
    }
    if (p.semRow < 0 || p.semRow >= m.cursor.rows ||
        p.semCol < 0 || p.semCol >= m.cursor.cols) {
        return true;
    }
    bool known = false;
    for (const std::string& k : m.cursor.actKinds) {
        if (k == p.semKind) { known = true; break; }
    }
    if (!known) {
        return true;
    }
    const int x = m.cursor.originX + p.semCol * m.cursor.cellW;
    const int y = m.cursor.originY + p.semRow * m.cursor.cellH;
    return x != p.semRectX || y != p.semRectY ||
           m.cursor.cellW != p.semRectW || m.cursor.cellH != p.semRectH;
}

// 파킹 플러드 상한 (docs/56 §2b): 요청자(connection id)별 미해결 승인 상한.
// 초과 요청은 파킹하지 않고 approval_overflow — 승인 스트립 도배 봉쇄.
// close_window/trust_request/permission_set 등 파킹 종류 전체가 공유한다.
bool JKWindowServer::ApprovalParkingFull(uint32_t requesterId) const {
    constexpr size_t kMaxPerRequester = 8;
    size_t n = 0;
    for (const PendingApproval& p : pendingApprovals_) {
        if (p.requesterId == requesterId && ++n >= kMaxPerRequester)
            return true;
    }
    return false;
}

// <exeDir>/state — agent-created files (layout snapshots). CreateDirectoryA
// fails harmlessly when the directory already exists.
std::string JKWindowServer::StateDir() const {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    dir = (slash == std::string::npos) ? std::string(".") : dir.substr(0, slash);
    dir += "\\state";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

// 콘솔 앱 스폰 (P4 SDK §3/§5): 터미널 위에 cmd — cwd는 앱 폴더(상대경로).
// SpawnProcess가 인용을 만들므로 cmd/cwd 끝에 백슬래시가 없어야 한다
// (453a327 레슨) — 매니페스트의 상대경로 규칙이 이를 보장한다.
void JKWindowServer::SpawnConsoleApp(const std::string& cmd, const std::string& cwd,
                                     const std::string& name) {
    SpawnProcess(clientHostExe_.c_str(),
                 std::string("terminal --cwd \"") + cwd + "\" --shell \"" + cmd + "\"",
                 name.c_str());
}

JKClientConnection* JKWindowServer::FindShellClient() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& c : clients_) {
        if (c && !c->IsDisconnected() && c->IsShell()) {
            return c.get();
        }
    }
    return nullptr;
}

// Dock the shell to the bottom edge: surface width = desktop logical width
// (ResizeSurface via the chrome-resize machinery), position (0, wh - h).
// Re-docked on every desktop size change (UpdateOutputBounds).
void JKWindowServer::DockShellClient(JKClientConnection* shell) {
    if (!compositor_ || !window_) {
        return;
    }
    if (!shell) {
        shell = FindShellClient();
    }
    if (!shell) {
        return;
    }

    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    const uint32_t id = shell->Id();
    const int barH = shell->Height();  // the client decides the thickness

    // Re-resize only when the desktop width actually changed (a ResizeSurface
    // forces the client to remap shared memory and re-layout).
    if (shell->Width() != ww) {
        CommitChromeResize(*shell, id, ww, barH, ww, barH);
    }
    compositor_->SetLayerPosition(id, 0, wh - barH);
    // Keep the connection-side position in sync: the input mapping converts
    // the mouse with client->X()/Y() (ProcessPendingClients stores the spawn
    // placement there), while drawing reads the compositor layer. Updating
    // only the layer left the input map at the centered spawn position, so
    // taskbar button clicks landed at y=361 on a 40px-tall surface and were
    // dropped — the bar rendered fine but every button was dead.
    shell->SetPosition(0, wh - barH);
}

void JKWindowServer::Composite() {
    Composite(true);
}

void JKWindowServer::Composite(bool present) {
    if (!compositor_ || !renderer_) {
        return;
    }

    // All drawing in this server is done in physical pixels: the launcher icons
    // and compositor layers are scaled by outputScale manually, and the mouse
    // hit-test uses raw physical client pixels. We deliberately do NOT call
    // SDL_RenderSetScale here because its effect differs across SDL render
    // backends (D3D vs OpenGL) and caused coordinate drift on the primary
    // high-DPI monitor. Keeping everything in physical px removes that
    // ambiguity.
    SDL_RenderSetScale(renderer_, 1.0f, 1.0f);

    // Draw the launcher desktop into the renderer first; the compositor will
    // layer client surfaces on top and then present once. The desktop shell
    // (P1 ③) owns that background; Draw no-ops on an empty desktop.
    if (shell_) {
        shell_->Draw(renderer_);
    }
    compositor_->Composite(present);
}

// 3중 링 스트로크 공용 헬퍼 (Task 3 fix round 1 NIT-2): "1px씩 안으로 들어가는
// 3중 사각형" 기법이 DrawApprovalHighlights(창 링)와 DrawSemanticCursorCells
// (커서 셀/승인 셀) 3곳에 복사돼 있던 것을 한 곳으로 — 동작 불변(작은 rect는
// 자동 축소 break, 색 지정 포함).
static void DrawRing3(SDL_Renderer* renderer, const SDL_Rect& rect,
                      int r, int g, int b) {
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    for (int i = 0; i < 3; ++i) {
        SDL_Rect ring{rect.x + i, rect.y + i, rect.w - 2 * i, rect.h - 2 * i};
        if (ring.w <= 0 || ring.h <= 0) {
            break;
        }
        SDL_RenderDrawRect(renderer, &ring);
    }
}

// 승인 대상 시각화 (스펙 2026-09-19-app-tool-hub §5 1단): 파킹된 승인의 대상
// 창 위에 호박색 링 + 상단 배너 "에이전트 승인 대기: <name>". DrawCloseOverlay
// 와 같은 컴포지트 패스의 최상위(레이어 루프 후) 단계 — 오버레이 훅을 통해
// 매 프레임 호출된다. 승인 파이프라인 자체의 기능이라 close_window /
// run_console_app 등 targetId를 갖는 모든 ask 도구가 동시 혜택을 본다.
// resolve(허용/거부/타임아웃)는 pendingApprovals_에서 항목을 지우므로 하이라이트
// 는 그 프레임부터 자동 해제 — 별도 상태·타이머 없음.
// 스레드 규약: 서버 루프 스레드 전용. pendingApprovals_는 ProcessPendingMessages
// (만료 스캔)와 HandleAgentQuery(파킹/resolve — 같은 스레드가 clientsMutex_를
// 잡고 부르는 유일 경로)에서만 쓰이므로, 이 함수(컴포지트 패스)와 읽기 경쟁이
// 생기지 않는다. preMaxRects_ 등 크롬 상태와 동일한 "락 없음" 규약.
void JKWindowServer::DrawApprovalHighlights(float outputScale) {
    if (!renderer_ || !compositor_) {
        return;
    }
    // 파킹 승인의 대상 모음 — targetId 0(제어 연결·trust_request 등 대상 창
    // 없는 파킹)은 스킵. 같은 창에 여러 승인이 파킹되면 한 번만 그린다(첫
    // 승인의 name 우선, name 없는 파킹이 먼저면 뒤의 name으로 보완).
    std::map<uint32_t, std::string> targets;
    for (const PendingApproval& p : pendingApprovals_) {
        if (!p.targetId) {
            continue;
        }
        std::string name;
        if (p.kind == "app_tool") {
            name = p.name;  // 스펙 §5: 배너 표기 "<app>.<tool>" (파킹 시 확정)
        } else if (!p.name.empty()) {
            name = p.name;
        }
        auto it = targets.find(p.targetId);
        if (it == targets.end()) {
            targets.emplace(p.targetId, std::move(name));
        } else if (it->second.empty() && !name.empty()) {
            it->second = std::move(name);
        }
    }
    if (targets.empty()) {
        return;
    }

    std::set<std::string> usedBanners;  // 이 프레임에 쓴 배너 캐시 키
    for (const auto& kv : targets) {
        JKCompositorLayer* layer = compositor_->FindLayerById(kv.first);
        if (!layer || !layer->IsVisible()) {
            continue;
        }
        // 면제: shell(docs/28)과 캡처 오버레이(docs/35) — DrawCloseOverlay의
        // 크롬 면제 목록과 동일 조건. 전체화면 레이어는 일부러 면제하지 않는다:
        // 전체화면 앱에 대한 승인이야말로 눈으로 대상을 확인해야 할 때고, 배너는
        // 크롬(X 버튼)이 아니라 승인 알림이므로 "앱이 상단 스트립을 소유" 규칙에
        // 걸리지 않는다.
        if (layer->IsShell() || layer->Title() == kCaptureOverlayTitle) {
            continue;
        }
        // 논리 포인트 → 물리 픽셀(Composite와 같은 산식 — 레이어 rect × Scale ×
        // outputScale). 링/밴드/텍스트가 전부 클릭 좌표계와 일치한다.
        const SDL_Rect rc{
            static_cast<int>(layer->X() * outputScale),
            static_cast<int>(layer->Y() * outputScale),
            static_cast<int>(layer->Width() * layer->ScaleX() * outputScale),
            static_cast<int>(layer->Height() * layer->ScaleY() * outputScale)};
        if (rc.w <= 0 || rc.h <= 0) {
            continue;
        }
        // 호박 (230,140,40) — 스펙 §5 1단 고정값(테마 무관, 승인 알림 식별색).
        // 링: DrawCloseOverlay의 SDL_RenderDrawRect 스트로크 기법 그대로 —
        // 1px씩 안으로 들어가는 3중 사각형으로 두꺼운 테두리를 만든다.
        // (fix round 1 NIT-2: 3중 스트로크 루프는 DrawRing3 공용 헬퍼로)
        DrawRing3(renderer_, rc, 230, 140, 40);
        // 상단 배너 밴드: 크롬 타이틀바(kChromeTitleBar)와 같은 두께로 대상 창의
        // 상단 스트립을 덮는다(링 안쪽 1px에 맞춰 겹침 방지).
        const int bandH = std::min(
            static_cast<int>(kChromeTitleBar * layer->ScaleY() * outputScale),
            rc.h - 3);
        if (bandH <= 0) {
            continue;
        }
        SDL_Rect band{rc.x + 3, rc.y + 3, rc.w - 6, bandH};
        SDL_RenderFillRect(renderer_, &band);
        // 배너 문자열: p.name(비었으면 대상 레이어의 창 제목 — close_window류).
        std::string banner = "에이전트 승인 대기: ";
        banner += (kv.second.empty() ? layer->Title() : kv.second);
        usedBanners.insert(banner);
        int tw = 0, th = 0;
        SDL_Texture* tex = ApprovalBannerTexture(banner, tw, th);
        if (tex && tw > 0 && th > 0) {
            const int tx = band.x + 8;
            const int ty = band.y + std::max(0, (band.h - th) / 2);
            // 밴드 오른쪽에서 자름 — 좁은 창에서 글자가 창 밖(다른 창 위)으로
            // 흘러나가 다른 대상의 배너와 겹치는 일을 막는다.
            SDL_RenderSetClipRect(renderer_, &band);
            SDL_Rect dst{tx, ty, tw, th};
            SDL_RenderCopy(renderer_, tex, nullptr, &dst);
            SDL_RenderSetClipRect(renderer_, nullptr);
        }
    }
    // 이 프레임에 안 쓰인 캐시는 즉시 폐기 — 파킹 해소(밴드 키 소멸)가 곧
    // 텍스처 해제다. 다시 파킹되면 한 번만 다시 레스터라이즈된다.
    for (auto it = approvalBannerTexs_.begin();
         it != approvalBannerTexs_.end();) {
        if (!usedBanners.count(it->first)) {
            if (it->second.tex) {
                SDL_DestroyTexture(it->second.tex);
            }
            it = approvalBannerTexs_.erase(it);
        } else {
            ++it;
        }
    }
}

// 의미 커서 셀 하이라이트 (스펙 2026-09-22-semantic-cursor §5, Task 3) — 2계층:
//  1) 커서 셀(지속): 커서 선언 앱의 최신 선언(origin/cell 크기)+플랫폼 커서
//     상태로 셀 rect를 매 프레임 산출해 액센트색 테두리. 앱 표면 무접촉
//     (앱 크래시/무응답에도 표시 생존), window_move 추종(실시간 계산),
//     비상호작용(그리기만 — 클릭은 그대로 통과). 커서가 한 번도 움직이지
//     않아도 (0,0) 표시(상태 존재 = 표시 — 초기 (0,0) 규약, 스펙 §4).
//  2) 승인 대상 셀(파킹 시): semCell 파킹의 고정 rect(client 좌표 — 파킹
//     시점 고정, 승인 시점 CursorActStale 재검증은 Task 2)에 기존 호박 3중
//     링 산식을 그대로 적용. 배너는 기존 창 링+밴드가 p.name으로 이미 그린다
//     (DrawApprovalHighlights — 여기선 셀 링만 보태다). 커서 셀은 살아 있는
//     상태를, 승인 셀은 파킹 시점의 사진을 보여준다 — 재선언으로 격자가
//     바뀌면 둘이 어긋나는 게 정상(거부 판정 자료).
// 색: 커서 셀 = 선택 액센트 파랑 (0,120,212) — JKTheme kDefault/kLight의
// selectionBg 값(테마 무관 고정 — 승인 호박(230,140,40)과 같은 "식별색 고정"
// 규약, 승인 링이 테마를 따르지 않는 선례). 호박과 채널 거리가 충분해 2계층이
// 한 프레임에 겹쳐 보여도(같은 셀에 act 파킹) 식별된다.
// 스레드 규약: DrawApprovalHighlights와 동일 — 서버 루프 스레드 전용.
// appToolManifests_·pendingApprovals_는 ProcessPendingMessages/HandleAgentQuery
// (같은 스레드)만 쓰므로 컴포지트 패스 읽기와 경쟁이 생기지 않는다(락 없음 —
// DrawApprovalHighlights의 pendingApprovals_ 규약 선례).
void JKWindowServer::DrawSemanticCursorCells(float outputScale) {
    if (!renderer_ || !compositor_) {
        return;
    }
    // 계층 1 — 커서 셀: 커서 선언 매니페스트 전체 순회 — 매니페스트는 연결
    // (connId) 단위라 복수 인스턴스가 있어도 각자 자기 창(windowId)의 셀을
    // 그린다(창 단위 소비자는 windowId로 변별).
    for (const auto& kv : appToolManifests_) {
        const AppToolManifest& m = kv.second;
        if (!m.cursor.valid || m.windowId == 0) {
            continue;  // 제어 연결 매니페스트(창 없음)는 그릴 곳이 없다
        }
        JKCompositorLayer* layer = compositor_->FindLayerById(m.windowId);
        if (!layer || !layer->IsVisible()) {
            continue;
        }
        // 면제: shell(docs/28)과 캡처 오버레이(docs/35) —
        // DrawApprovalHighlights의 크롬 면제 목록과 동일 조건.
        if (layer->IsShell() || layer->Title() == kCaptureOverlayTitle) {
            continue;
        }
        // 격자 밖 커서(축소 재선언 직후 상태 등)는 rect 산출 불가 — 스킵
        // (CursorActStale의 격자 판정과 동일 기준).
        if (m.cursorState.Row() < 0 || m.cursorState.Row() >= m.cursor.rows ||
            m.cursorState.Col() < 0 || m.cursorState.Col() >= m.cursor.cols) {
            continue;
        }
        // client → 물리 변환 (DrawApprovalHighlights와 같은 사슬 — 논리 레이어
        // rect × outputScale, client 픽셀 × 레이어 Scale × outputScale).
        const SDL_Rect rc{
            static_cast<int>(layer->X() * outputScale),
            static_cast<int>(layer->Y() * outputScale),
            static_cast<int>(layer->Width() * layer->ScaleX() * outputScale),
            static_cast<int>(layer->Height() * layer->ScaleY() * outputScale)};
        if (rc.w <= 0 || rc.h <= 0) {
            continue;
        }
        const float sx = layer->ScaleX() * outputScale;
        const float sy = layer->ScaleY() * outputScale;
        const int cx = rc.x + static_cast<int>(std::lround(
            (m.cursor.originX + m.cursorState.Col() * m.cursor.cellW) * sx));
        const int cy = rc.y + static_cast<int>(std::lround(
            (m.cursor.originY + m.cursorState.Row() * m.cursor.cellH) * sy));
        const int cw = static_cast<int>(std::lround(m.cursor.cellW * sx));
        const int ch = static_cast<int>(std::lround(m.cursor.cellH * sy));
        if (cw <= 0 || ch <= 0) {
            continue;
        }
        // 창 rect 절단 (fix round 1 MINOR-1): 교차 없음뿐 아니라 "일부만 보이는"
        // 셀(부분 교차)도 링 픽셀이 창 밖으로 새지 않게 물리 창 rect로
        // 클램프한다. 선언 파서는 origin/cell 크기/rows·cols를 각각 검증하지만
        // origin+rows×cellW/H가 표면 안에 맞는지는 검증하지 않는다(격자가
        // 표면보다 큰 선언 = 합법) — 렌더가 창 밖 픽셀을 절대 쓰지 않게 하는
        // 유일한 방어선은 이 절단이다. 교차 없음 = 그릴 것 없음(스킵).
        SDL_Rect cell{cx, cy, cw, ch};
        SDL_Rect clipped{};
        if (!SDL_IntersectRect(&cell, &rc, &clipped) || clipped.w <= 0 ||
            clipped.h <= 0) {
            continue;
        }
        // 링: 승인 링과 같은 3중 1px 스트로크(작은 셀에서는 자동 축소).
        DrawRing3(renderer_, clipped, 0, 120, 212);
    }
    // 계층 2 — 승인 대상 셀: semCell 파킹의 고정 rect에 호박 3중 링. 창 링은
    // DrawApprovalHighlights가 계속 그린다(기존 정책 — 승인 대상 창 식별).
    for (const PendingApproval& p : pendingApprovals_) {
        if (!p.semCell || p.targetId == 0) {
            continue;
        }
        JKCompositorLayer* layer = compositor_->FindLayerById(p.targetId);
        if (!layer || !layer->IsVisible()) {
            continue;
        }
        if (layer->IsShell() || layer->Title() == kCaptureOverlayTitle) {
            continue;
        }
        const SDL_Rect rc{
            static_cast<int>(layer->X() * outputScale),
            static_cast<int>(layer->Y() * outputScale),
            static_cast<int>(layer->Width() * layer->ScaleX() * outputScale),
            static_cast<int>(layer->Height() * layer->ScaleY() * outputScale)};
        if (rc.w <= 0 || rc.h <= 0) {
            continue;
        }
        const float sx = layer->ScaleX() * outputScale;
        const float sy = layer->ScaleY() * outputScale;
        const int cx = rc.x + static_cast<int>(std::lround(p.semRectX * sx));
        const int cy = rc.y + static_cast<int>(std::lround(p.semRectY * sy));
        const int cw = static_cast<int>(std::lround(p.semRectW * sx));
        const int ch = static_cast<int>(std::lround(p.semRectH * sy));
        if (cw <= 0 || ch <= 0) {
            continue;
        }
        // 창 rect 절단 (fix round 1 MINOR-1): 승인 셀도 동일 — 파킹 rect는
        // 파킹 시점 고정이라 그 이후의 재선언/리사이즈로 창 밖으로 밀릴 수
        // 있다(승인 시점 CursorActStale이 기하 불일치를 거부하지만, 승인 전
        // 파킹 표시 기간엔 이 절단이 유일한 방어선).
        SDL_Rect cell{cx, cy, cw, ch};
        SDL_Rect clipped{};
        if (!SDL_IntersectRect(&cell, &rc, &clipped) || clipped.w <= 0 ||
            clipped.h <= 0) {
            continue;
        }
        // 호박 (230,140,40) — 승인 링 고정색과 동일(같은 승인 사건의 시각).
        DrawRing3(renderer_, clipped, 230, 140, 40);
    }
}

// 배너 텍스트 래스터라이즈 (스펙 2026-09-19-app-tool-hub §5 1단): 크롬 타이틀과
// 동일한 글리프 경로 — Utf8ToKssm 변환 + JKDC 비트맵 폰트(한글 16px/영문 8px,
// HangulManager의 assets/fonts/hangul.fnt·english.fnt). 새 글리프 엔진 없음.
// 컴포지트가 ~1kHz로 도므로 매 프레임 글리프 DrawPixel은 비용이 남아, 렌더
// 타깃 텍스처에 한 번 찍어 캐시한다(문자열당 1회). 실패는 null 엔트리로 기록 —
// 매 프레임 재시도·로그 스팸을 막는다(1kHz 컴포지트 전제).
SDL_Texture* JKWindowServer::ApprovalBannerTexture(const std::string& bannerUtf8,
                                                   int& w, int& h) {
    w = 0;
    h = 0;
    auto it = approvalBannerTexs_.find(bannerUtf8);
    if (it != approvalBannerTexs_.end()) {
        w = it->second.w;
        h = it->second.h;
        return it->second.tex;
    }
    if (bannerUtf8.empty() || !renderer_ ||
        !SDL_RenderTargetSupported(renderer_)) {
        // 렌더 타깃 미지원 백엔드: 텍스트 없이 링+밴드만(하이라이트 식별은
        // 유지). SDL 가속 렌더러는 전부 타깃을 지원하므로 사실상 안 쓰는 가지.
        approvalBannerTexs_[bannerUtf8] = ApprovalBannerTex{};
        return nullptr;
    }
    if (!approvalFont_) {
        approvalFont_ = std::make_unique<HangulManager>();
        if (approvalFont_->CreationError) {
            // 폰트 파일 부재 — 클라 크롬과 동일하게 ASCII 폴백이 JKDC에
            // 내장돼 있으므로 래스터 자체는 가능하다. 폰트 없이 계속 간다.
            std::fprintf(stderr,
                         "[server] approval banner: font files missing, "
                         "built-in ASCII fallback\n");
        }
    }
    // 승인 배너 벡터 글리프 (docs/63 Task 6): approvalFont_와 동일 지연 초기화
    // (서버 루프 스레드 전용 — 락 없음). 캐시는 Init에서 채운 영구
    // bannerBackend_를 소유한다 — EnsureGlyph의 등록 경로(CreateImageFromRGBA)
    // 와 소멸자 플러시가 모두 캐시 소유 backend_에 의존하므로(nullptr이면 등록이
    // 막혀 벡터 경로가 조용히 죽는다). 업로드는 DrawGlyph의 즉시 FlushUploads
    // 폴백이 bannerBackend_.get()을 넘겨 처리 — 배너는 렌더 타깃에 동기 그리므로
    // 별도 플러시 호출 불요. Init 실패는 atlas 유지(IsLoaded()==false) — 비트맵
    // 폴백 유지 + 경고 1회, 배너마다 재시도·로그 반복을 만들지 않는다.
    if (!bannerCache_ && bannerBackend_) {
        bannerCache_ =
            std::make_unique<jk::JKResourceCache>(bannerBackend_.get());
    }
    if (!bannerAtlas_) {
        bannerAtlas_ = std::make_unique<jk::JKTextAtlas>();
        // 셀 메트릭 진실원 (docs/63 §6 text.font_scale) — 기본 1.0 = {8,16,16}.
        const jk::text::CellMetrics m = jk::text::GetCellMetrics();
        const std::string fp = textFontPath_.empty()
                                   ? jk::text::ResolveDesktopFontPath()
                                   : textFontPath_;
        if (fp.empty()) {
            std::fprintf(stderr,
                         "[server] approval banner: no vector font configured "
                         "(text.font_path empty), staying on bitmap glyphs\n");
        } else if (!bannerAtlas_->Init(fp, m.engW, m.cellH, m.hanW)) {
            std::fprintf(stderr,
                         "[server] approval banner: vector font init failed "
                         "(%s), staying on bitmap glyphs\n",
                         fp.c_str());
        } else {
            // 보조 폰트 체인 (docs/63 §6 2단계): 미설정(빈)이면 체인 없음 —
            // Init 실패는 경고 1줄, 체인 없이 계속(치명 아님).
            const std::string fb = jk::text::ResolveDesktopFallbackPath();
            if (!fb.empty() && !bannerAtlas_->InitFallback(fb)) {
                std::fprintf(stderr,
                             "[server] approval banner: fallback font init "
                             "failed (%s), glyph chain disabled\n",
                             fb.c_str());
            }
        }
    }
    // 크롬 타이틀의 LegacyFontTitle 선례: KSSM 변환이 빈 결과면 원문을 쓴다
    // (이미 KSSM인 문자열·ASCII 전용 문자열의 이중 변환 방지).
    std::string kssm = Utf8ToKssm(bannerUtf8.c_str());
    if (kssm.empty()) {
        kssm = bannerUtf8;
    }
    const JKPoint m = JKDC::MeasureText(kssm.c_str());
    constexpr int kPad = 4;
    const int texW = m.x + kPad * 2;
    const int texH = m.y > 0 ? m.y : 16;
    SDL_Texture* tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET, texW, texH);
    if (!tex) {
        approvalBannerTexs_[bannerUtf8] = ApprovalBannerTex{};
        return nullptr;
    }
    // 글자만 실은 투명 배경 텍스처 — 밴드(호박 채움) 위에 블렌딩으로 얹는다.
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_Texture* prev = SDL_GetRenderTarget(renderer_);
    SDL_SetRenderTarget(renderer_, tex);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);
    JKSDLRenderBackend backend(renderer_);
    JKDC dc(&backend);
    if (approvalFont_) {
        dc.SetHangulManager(approvalFont_.get());
    }
    // 벡터 글리프 장착 (docs/63 Task 6): 실패한 Init(IsLoaded()==false)은
    // 미장착 — EngPutCh/HanPutCh의 글리프 단위 비트맵 폴백이 그대로 쓰인다.
    // 업로드는 DrawGlyph의 즉시 FlushUploads 폴백이 bannerCache_의 소유 백엔드
    // (bannerBackend_)로 올린다 — 동기 그리기라 별도 플러시 호출은 불요.
    if (bannerAtlas_ && bannerAtlas_->IsLoaded() && bannerCache_) {
        dc.SetTextAtlas(bannerAtlas_.get(), bannerCache_.get());
    }
    dc.SetTextColor(34, 20, 4);  // 호박 밴드 위 진한 갈색 글자 (고정 대비색)
    dc.TextOut(JKPoint{kPad, kPad}, kssm.c_str());
    SDL_SetRenderTarget(renderer_, prev);
    approvalBannerTexs_[bannerUtf8] = ApprovalBannerTex{tex, texW, texH};
    w = texW;
    h = texH;
    return tex;
}

void JKWindowServer::CleanupDisconnectedClients() {
    std::vector<std::unique_ptr<JKClientConnection>> disconnected;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        // 앱 도구 허브 (스펙 §4.1): 카탈로그 변경 플래그 — 루프 밖에서 한 번만
        // publish(PushAgentEventJson은 락 프리, 레슨 35).
        bool toolsChanged = false;
        auto it = clients_.begin();
        while (it != clients_.end()) {
            auto& client = *it;
            if (client && client->IsDisconnected()) {
                // 최종 메시지 드레인 게이트: read 스레드가 마지막 메시지를 큐잉
                // 하고 disconnected_를 마킹하는 시점이 메인 루프의 Composite()
                // 도중(ProcessPendingMessages 뒤, 여기 앞)이면, 즉시 소멸시
                // 읽지 않은 큐가 함께 소멸한다 — filedlg resolve가 보낸
                // file_open_result/AgentToolResult가 유실되고 파킹 쿼리는
                // 600s 만료까지 dialog_busy로 남는다(2026-09-20 관측, err=109
                // 파산 ~1/7). disconnected_가 true면 read 루프는 이미 종료라
                // 큐는 이후 불변 — 소멸을 한 이터레이션 유보하면 다음 루프의
                // ProcessPendingMessages(여기보다 먼저 실행)가 드레인하고 그
                // 뒤 이곳에서 소멸된다.
                if (client->HasQueuedMessages()) {
                    ++it;
                    continue;
                }
                if (focusedClientId_ == client->Id()) {
                    focusedClientId_ = 0;
                }
                if (capturedClientId_ == client->Id()) {
                    capturedClientId_ = 0;
                }
                // docs/39: the layer is going away — drop its maximize state
                // so a recycled surface id cannot inherit a stale pre-max rect.
                preMaxRects_.erase(client->Id());
                // vplayer 전체화면(스펙 §2.1): maximize 동일 — 죽은 레이어의
                // 저장 rect는 무의미.
                preFsRects_.erase(client->Id());
                if (compositor_) {
                    compositor_->RemoveLayer(client->Id());
                }
                // filedlg 슬롯 회수 (final-review MINOR-2): 요청자가 다이얼로그
                // 도중 죽으면 슬롯을 즉시 비운다 — 픽스 전엔 만료 스캔(600s)까지
                // dialog_busy로 모든 file_open을 막았다. 이 연결의 파킹 쿼리는
                // 만료 스캔이 회수한다(요청자가 죽었으면 응답 대상이 없어
                // no-op) — pendingApprovals_ 기계는 건드리지 않는다.
                if (pendingFileDialog_.requesterConnId == client->Id()) {
                    // waitAsync 슬롯의 모든 종료 경로(해소/만료/요청자 회수)를
                    // 이벤트로 마감 — 수명주기 완결성 (final-review Minor-1).
                    // 회수만 하고 발행이 없으면 이후 만료 스캔의 슬롯 대조가
                    // requestId 불일치로 영구 실패해 expired 이벤트도 timeout
                    // reply도 없는 무통지 경로였다(재접속 세션의 read_events가
                    // 영원히 못 받는다 — 재접속 비복구 한계). 만료 스캔과 동일
                    // 페이로드 관례. PushAgentEventJson은 락 프리(레슨 35)라
                    // clientsMutex_ 보유 경로에서 호출 가능(만료 스캔 선례).
                    if (pendingFileDialog_.waitAsync) {
                        PushAgentEventJson("{\"topic\":\"file.open_result\","
                                           "\"ok\":false,\"error\":"
                                           "\"requester_gone\"}");
                    }
                    pendingFileDialog_ = PendingFileDialog{};
                }
                // 앱 도구 허브 (스펙 §4.1): 연결 수명에 묶인 매니페스트 소멸 +
                // 이 연결로 중계 중이던 호출 tool_gone 회수 + 카탈로그 변경
                // 이벤트. 별도 언레지스터 메시지 없음.
                if (appToolManifests_.erase(client->Id())) toolsChanged = true;
                for (auto iit = inflightAppTools_.begin();
                     iit != inflightAppTools_.end();) {
                    if (iit->second.targetConnId == client->Id()) {
                        ReplyAppToolError(iit->second, "tool_gone");
                        iit = inflightAppTools_.erase(iit);
                    } else ++iit;
                }
                // Desktop Agent event (spec §4) — app windows only: the shell
                // and control-only agents are not listable windows, so their
                // teardown is not a desktop event. Captured before the move.
                if (!client->IsControlOnly() && !client->IsShell()) {
                    // Crash detection (M2b): a spawned client whose process
                    // exited non-zero did not leave gracefully. pids not in
                    // the spawn table (externally started clients) read as
                    // graceful — we can only judge what we spawned.
                    bool crashed = false;
                    auto sh = spawnedClients_.find(client->Pid());
                    if (sh != spawnedClients_.end()) {
                        unsigned long code = 0;
                        if (GetExitCodeProcess(sh->second, &code) &&
                            code != kStillActiveExit && code != 0) {
                            crashed = true;
                        }
                        CloseHandle(sh->second);
                        spawnedClients_.erase(sh);
                    }
                    PushAgentEvent(crashed ? "app.crashed" : "window.destroyed",
                                   client->Id(), client->Title(),
                                   client->Pid());
                }
                disconnected.push_back(std::move(client));
                it = clients_.erase(it);
            } else {
                ++it;
            }
        }

        // When the focused client went away, move keyboard focus to the
        // topmost surviving layer so keys keep working without a click.
        if (focusedClientId_ == 0 && compositor_ && !clients_.empty()) {
            if (uint32_t top = compositor_->TopmostLayerId()) {
                FocusClient(top);
            }
        }

        // Shell protocol: the dead window disappears from the taskbar.
        if (!disconnected.empty()) {
            PushWindowListUnsafe();
        }

        // 앱 도구 허브 (스펙 §4.1): 연결 수명 매니페스트 소멸로 카탈로그가
        // 바뀌었으면 한 번만 방송 — PublishAppToolsChanged는 락 프리(레슨 35).
        if (toolsChanged) PublishAppToolsChanged();
    }

    // Join read threads outside the lock to avoid blocking the server main loop
    // and to prevent shutdown deadlocks.
    for (auto& client : disconnected) {
        if (client) client->StopReadThread();
    }
}

SDL_Texture* JKWindowServer::TextureFromRGBA(const jk::LoadedImage& img, const char* label) {
    if (!renderer_) return nullptr;
    // stb decodes to byte-order R,G,B,A; SDL_PIXELFORMAT_RGBA32 is exactly
    // that layout regardless of platform endianness.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint8_t*>(img.rgba.data()), img.w, img.h, 32, img.w * 4,
        SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        std::fprintf(stderr, "JKWindowServer: surface for '%s' failed: %s\n",
                     label, SDL_GetError());
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        std::fprintf(stderr, "JKWindowServer: texture for '%s' failed: %s\n",
                     label, SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

// Launch an arbitrary exe from the server's directory (SpawnClient core).
// throttleKey defaults to exeName; SpawnClient keeps the per-app key so two
// DIFFERENT apps can still launch back-to-back.
bool JKWindowServer::SpawnProcess(const char* exeName, const std::string& args,
                                  const char* throttleKey) {
#ifdef _WIN32
    const char* key = throttleKey ? throttleKey : exeName;
    // Throttle repeated spawns for the same key to avoid launching many
    // copies from a single double-click.
    {
        auto now = std::chrono::steady_clock::now();
        auto it = lastSpawnTimes_.find(key);
        if (it != lastSpawnTimes_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second);
            if (elapsed.count() < 500) {
                std::fprintf(stderr,
                             "JKWindowServer: ignoring rapid spawn for %s (%lld ms)\n",
                             key, static_cast<long long>(elapsed.count()));
                return false;
            }
        }
        lastSpawnTimes_[key] = now;
    }

    // Assume the server executable is in the same directory as the target.
    // Wide path: GetModuleFileNameA would break on non-ANSI install dirs.
    wchar_t modulePathW[1024] = {};
    const unsigned long len =
        GetModuleFileNameW(nullptr, modulePathW, 1024);
    if (len == 0 || len >= 1024) {
        std::fprintf(stderr, "JKWindowServer: GetModuleFileNameW failed\n");
        return false;
    }

    // Find the directory component.
    wchar_t* lastSlash = modulePathW;
    for (wchar_t* p = modulePathW; *p; ++p) {
        if (*p == L'\\' || *p == L'/') lastSlash = p;
    }
    // Leave a NUL after the directory; exe name is appended below.
    const bool haveDir = (lastSlash != modulePathW);
    std::wstring dirW(modulePathW, haveDir ? (lastSlash - modulePathW) : 0);
    const wchar_t* workDir = haveDir ? dirW.c_str() : nullptr;

    // Wide command line, UTF-8 args converted with CP_UTF8 (see the W-variant
    // note above). 2048 chars upper-bounds the ANSI version's byte budget.
    std::wstring cmdLine;
    cmdLine.reserve(2048);
    cmdLine += L"\"";
    cmdLine += (haveDir ? dirW : L".");
    cmdLine += L"\\";
    {
        // exeName is an ASCII literal from the spawn table; convert anyway so
        // the whole line is one encoding. The path quote is already open —
        // just append the exe and close it (a quote here would escape on the
        // backslash: "dir\"exe" is one broken token).
        int n = MultiByteToWideChar(65001, 0, exeName, -1, nullptr, 0);
        std::wstring exeW(static_cast<size_t>(n > 0 ? n : 1), L'\0');
        if (n > 0) MultiByteToWideChar(65001, 0, exeName, -1, exeW.data(), n);
        cmdLine += exeW.c_str();
        cmdLine += L"\"";
    }
    if (!args.empty()) {
        int n = MultiByteToWideChar(65001, 0, args.c_str(),
                                    static_cast<int>(args.size()),
                                    nullptr, 0);
        if (n <= 0) {
            std::fprintf(stderr, "JKWindowServer: arg UTF-8 conversion failed for %s\n",
                         exeName);
            return false;
        }
        std::wstring argsW(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(65001, 0, args.c_str(),
                            static_cast<int>(args.size()), argsW.data(), n);
        cmdLine += L" ";
        cmdLine += argsW;
    }

    LauncherStartupInfoW si{};
    si.cb = sizeof(si);
    LauncherProcessInformation pi{};

    // Set the child's working directory to the executable directory so it can
    // locate the assets/ folder regardless of where the server was launched from.
    if (!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, 0, 0,
                        nullptr, workDir, &si, &pi)) {
        std::fprintf(stderr, "JKWindowServer: CreateProcessW failed for %s\n", exeName);
        return false;
    }

    // Keep the child's process handle for crash classification (M2b): when
    // the spawned client disconnects, CleanupDisconnectedClients checks the
    // exit code and emits app.crashed for non-zero exits. The thread handle
    // is never needed again.
    if (pi.hProcess) spawnedClients_[pi.dwProcessId] = pi.hProcess;
    if (pi.hThread) CloseHandle(pi.hThread);

    std::fprintf(stderr, "JKWindowServer: spawned %s %s\n", exeName, args.c_str());
    return true;
#else
    (void)exeName;
    (void)args;
    std::fprintf(stderr, "JKWindowServer: SpawnProcess is Windows-only in this prototype\n");
    return false;
#endif // _WIN32
}

bool JKWindowServer::SpawnClient(const char* appName, bool fromJkx) {
#ifdef _WIN32
    if (fromJkx) {
        // A .jkx container path — may contain spaces, so quote it.
        std::string arg = std::string("--jkx \"") + appName + "\"";
        return SpawnProcess(clientHostExe_.c_str(), arg, appName);
    }
    // Phase A 흡수 (docs/44): appName "terminal:<cmdline>" — 콘솔 TUI 앱을
    // 터미널 위에 띄운다. 런처 fallback 셀이 이 관례를 쓴다.
    std::string name(appName);
    constexpr const char* kTermPrefix = "terminal:";
    if (name.rfind(kTermPrefix, 0) == 0) {
        return SpawnProcess(clientHostExe_.c_str(),
                            std::string("terminal --shell ") +
                                name.substr(strlen(kTermPrefix)),
                            appName);
    }
    // 파일 열기 대화상자 (설계 specs/2026-09-13-file-dialog §1b): appName
    // "filedlg:<json args>" — terminal:과 같은 계열의 두 번째 접두 관례.
    // json을 따옴표로 감싸고 내부 "만 \"로 이스케이프(--jkx 인용 선례 + CRT
    // argv 규칙) — 자식의 argv[2]가 json 그대로 온다 (--jkx argv 계약과
    // 동일). terminal: 쪽은 건드리지 않는다.
    constexpr const char* kFileDlgPrefix = "filedlg:";
    if (name.rfind(kFileDlgPrefix, 0) == 0) {
        const std::string json = name.substr(strlen(kFileDlgPrefix));
        std::string quoted = "--filedlg \"";
        for (char ch : json) {
            if (ch == '"') quoted += "\\\"";
            else           quoted += ch;
        }
        quoted += "\"";
        return SpawnProcess(clientHostExe_.c_str(), quoted, appName);
    }
    return SpawnProcess(clientHostExe_.c_str(),
                        std::string("--client ") + appName, appName);
#else
    (void)appName;
    (void)fromJkx;
    std::fprintf(stderr, "JKWindowServer: SpawnClient is Windows-only in this prototype\n");
    return false;
#endif // _WIN32
}

} // namespace server
} // namespace jk
