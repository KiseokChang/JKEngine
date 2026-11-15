#include <desktop/JKDesktopShell.h>
#include <json/JKJsonQuick.h>  // 매니페스트/일회용 JSON 파싱 정본
#include <fs/JKFileRead.h>  // 파일 읽기 정본(docs/106 §2-5)

#include "theme/JKTheme.h"
#include <jkhooks_android.h>  // JNI 훅 단일 ABI(음성 메뉴)

#include <JKDC.h>
#include <fs/JKFs.h>
#include <JKHangulManager.h>
#include <crypto/JKSha256.h>
#include <JKHangulUtil.h>
#include <JKImageLoader.h>
#include <JKJkxFile.h>
#include <JKResourceCache.h>
#include <JKSDLRenderBackend.h>
#include <JKTextAtlas.h>

#include <quickjs.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
// Minimal Windows API declarations for the .jkx app scan — the same local
// declaration style the server TU uses (full Windows headers conflict with
// legacy JKENGINE typedefs in other translation units). GetModuleFileNameA
// 선언은 소각 — exe-dir는 jk::fs::GetExecutablePath 어댑터가 소유(docs/68 W5).

// .jkx app discovery (ScanJkxApps).
struct JkxFindData {
    unsigned long dwFileAttributes = 0;
    unsigned long ftCreationTime[2] = {};
    unsigned long ftLastAccessTime[2] = {};
    unsigned long ftLastWriteTime[2] = {};
    unsigned long nFileSizeHigh = 0;
    unsigned long nFileSizeLow = 0;
    unsigned long dwReserved0 = 0;
    unsigned long dwReserved1 = 0;
    char cFileName[260] = {};
    char cAlternateFileName[14] = {};
};

extern "C" __declspec(dllimport) void* __stdcall FindFirstFileA(
    const char* lpFileName, JkxFindData* lpFindFileData);
extern "C" __declspec(dllimport) int __stdcall FindNextFileA(
    void* hFindFile, JkxFindData* lpFindFileData);
extern "C" __declspec(dllimport) int __stdcall FindClose(void* hFindFile);

// bcrypt externs (cmd 지문, P4 SDK §3.4)는 docs/68 W2b에서 제거 — 지문은
// 플랫폼 중립 jk::crypto::Sha256Hex로 발행한다(BCrypt digest와 바이트 동일).
// CreateDirectoryA extern도 소각 — EnsureTrustRecord의 state/ 보증은
// std::filesystem::create_directory(ec 오버로드)로 교체(플랜 F1, docs/70 §6 #3).
#endif // _WIN32

namespace jk {
namespace desktop {

JKDesktopShell::JKDesktopShell() = default;

// 툴팁 지연 부품(unique_ptr 멤버)은 전방선언 타입이라 소멸자를 cpp에서 정의.
JKDesktopShell::~JKDesktopShell() = default;

namespace {

// 콘솔 앱 매니페스트 읽기 상한 (JKTerminalConfig와 동일 — 1MiB는 매니페스트
// 로 넘치는 크기).
constexpr size_t kMaxManifestBytes = 1u << 20;

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
    // [docs/106 §2-5] 구현 = fs/JKFileRead.h (NUL 종료·상한·부분읽기 계약)
    return jk::fs::ReadFileNulBytes(path, out, kMaxManifestBytes);
}

// 콘솔 앱 스폰 cmd 지문 (P4 SDK §3.4): docs/37 스킴 재사용 — jktriggers
// Sha256Hex와 동일 형식("sha256:"+64hex). docs/68 W2b: BCrypt CNG 호출을
// 플랫폼 중립 헬퍼로 교체 — digest 바이트열 불변(서버 EnsureTrustRecord가
// 남기는 지문과 일치). 빈 cmd도 해시된다(BCrypt 경로와 동일 관찰).
std::string ConsoleAppFingerprint(const std::string& cmd) {
    return "sha256:" + jk::crypto::Sha256Hex(cmd.data(), cmd.size());
}

// trust ledger 보증 (P4 SDK §3.4): 스폰 cmd 지문을 state\trust.json에 upsert
// (docs/37 형식 — jktriggers SaveTrustRecords와 동일 레이아웃). 서버는 시작
// 때 한 번만 쓴다. 이미 같은 지문이 있으면 파일을 건드리지 않는다(읽기-수정
// -쓰기 경쟁 최소화). 파손된 스토어는 덮어쓰지 않는다 — 기록 보존이 우선.
void EnsureTrustRecord(const std::string& fingerprint, const std::string& name) {
    // jk::fs::GetExecutablePath 흡수 (docs/68 W5) — 원문 규약: 실패/구분자
    // 없음 시 조용히 중단, 뒤 구분자 없음. 플랜 F1: _WIN32 게이트 해제(posix에서
    // 백슬래치 합성 파일명이 생기던 지점) + 조립은 '/' 단일 구분자
    // (state/는 스폰 지문 trust ledger 규약 그대로). 이 TU는 무 try/catch —
    // ec 중립형 필수(서버 :3186 선례).
    const std::string exe = jk::fs::GetExecutablePath();
    if (exe.empty()) return;
    const size_t slash = exe.find_last_of("\\/");
    if (slash == std::string::npos) return;
    std::string exeDir = exe.substr(0, slash);
    std::error_code dirEc;
    std::filesystem::create_directory(std::filesystem::path(exeDir + "/state"),
                                      dirEc);
    const std::string path = exeDir + "/state/trust.json";

    std::vector<uint8_t> bytes;
    std::vector<std::string> recs;  // 재조립용 레코드 JSON 문자열
    if (ReadFileBytes(path, bytes)) {
        JSRuntime* rt = JS_NewRuntime();
        if (!rt) return;
        JSContext* ctx = JS_NewContext(rt);
        if (!ctx) {
            JS_FreeRuntime(rt);
            return;
        }
        JSValue root = JS_ParseJSON(ctx, reinterpret_cast<const char*>(bytes.data()),
                                    bytes.size() - 1, "trust.json");
        if (JS_IsException(root)) {
            std::fprintf(stderr, "JKWindowServer: trust store corrupt — console app '%s' not recorded\n",
                         name.c_str());
            JS_FreeValue(ctx, root);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            return;
        }
        if (JS_IsObject(root)) {
            JSValue arr = JS_GetPropertyStr(ctx, root, "records");
            if (JS_IsArray(arr)) {
                JSValue lenVal = JS_GetPropertyStr(ctx, arr, "length");
                int32_t len = 0;
                JS_ToInt32(ctx, &len, lenVal);
                JS_FreeValue(ctx, lenVal);
                for (int32_t i = 0; i < len; ++i) {
                    JSValue item = JS_GetPropertyUint32(ctx, arr, (uint32_t)i);
                    auto str = [&](const char* key) {
                        JSValue v = JS_GetPropertyStr(ctx, item, key);
                        std::string s;
                        if (JS_IsString(v)) {
                            size_t l = 0;
                            const char* c = JS_ToCStringLen(ctx, &l, v);
                            if (c) s.assign(c, l);
                            JS_FreeCString(ctx, c);
                        }
                        JS_FreeValue(ctx, v);
                        return s;
                    };
                    const std::string fp = str("fingerprint");
                    const std::string nm = str("name");
                    const std::string src = str("source");
                    JSValue tsVal = JS_GetPropertyStr(ctx, item, "ts");
                    int64_t ts = 0;
                    JS_ToInt64(ctx, &ts, tsVal);
                    JS_FreeValue(ctx, tsVal);
                    if (fp == fingerprint) {
                        // 이미 기록된 지문 — 스토어 변경 없이 끝낸다.
                        JS_FreeValue(ctx, item);
                        JS_FreeValue(ctx, arr);
                        JS_FreeValue(ctx, root);
                        JS_FreeContext(ctx);
                        JS_FreeRuntime(rt);
                        return;
                    }
                    if (!fp.empty()) {
                        recs.push_back("{\"fingerprint\":\"" + fp + "\",\"name\":\"" +
                                       nm + "\",\"source\":\"" + src +
                                       "\",\"ts\":" + std::to_string(ts) + "}");
                    }
                    JS_FreeValue(ctx, item);
                }
            }
            JS_FreeValue(ctx, arr);
        }
        JS_FreeValue(ctx, root);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }

    // 새 레코드 append (source "user" — jktriggers 로더와 동일 역할 표기).
    const uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
    recs.push_back("{\"fingerprint\":\"" + fingerprint + "\",\"name\":\"" + name +
                   "\",\"source\":\"user\",\"ts\":" + std::to_string(nowMs) + "}");
    std::string out = "{\"records\":[";
    for (size_t i = 0; i < recs.size(); ++i) {
        if (i) out += ",";
        out += recs[i];
    }
    out += "]}";

    std::FILE* wf = std::fopen(path.c_str(), "wb");
    if (wf) {
        const bool wrote = std::fwrite(out.data(), 1, out.size(), wf) == out.size();
        std::fclose(wf);
        if (wrote) {
            std::fprintf(stderr, "JKWindowServer: console app cmd fingerprint recorded (%s, '%s')\n",
                         fingerprint.substr(0, 15).c_str(), name.c_str());
        } else {
            // (I-2 fix r1) 기록 실패는 정직 1행 — 지문 누락이 관측 불가하던
            // 무음 경로의 소각. 관측 운반체는 stderr뿐 (서버 계약상 지문은
            // state/trust.json이 진실원).
            std::fprintf(stderr, "JKWindowServer: trust store write failed (short, '%s') — console app '%s' not recorded\n",
                         path.c_str(), name.c_str());
        }
    } else {
        // (I-2 fix r1) 스토어 열기 실패(디스크 풀·권한·dir 조립 실패)도 동일 —
        // 지문이 남지 못하는 케이스의 노출.
        std::fprintf(stderr, "JKWindowServer: trust store write failed (open, '%s') — console app '%s' not recorded\n",
                     path.c_str(), name.c_str());
    }
}

} // namespace

// Load a PNG asset pair ("<base>@1x.png" / "@2x.png") — @2x when the
// output scale is >= 1.5 — into a blended SDL texture. Returns nullptr
// when the asset is missing (callers fall back to flat drawing).
SDL_Texture* JKDesktopShell::LoadTextureScaled(const char* assetBase) {
    if (!host_.renderer) return nullptr;

    // Pick the @2x asset when the display scale is high enough for the extra
    // pixels to pay off (mixed-DPI rule: renderer ratio drives the choice).
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    char path[512];
    std::snprintf(path, sizeof(path), "%s@%s.png", assetBase, s >= 1.5f ? "2x" : "1x");

    jk::LoadedImage img;
    if (!jk::LoadImageFile(jk::ResolveAssetPath(path), img)) {
        return nullptr;
    }
    return host_.makeTexture(img, path);
}

// [2026-11-15 사용자 판정 "바탕화면이 후진 앱 서랍"] 제품/개발 경계(P1).
// 아래는 개발·검증용 앱 — 기본 숨김, 메뉴 "개발자 앱" 토글로만 표시한다.
// 파일 = exe 옆 state/desktop.json {"showDev":true} (전 표면 공통 규약).
const char* const kDevOnlyApps[] = {
    "imguidemo", "jango",      "occ",        "pcx",        "recog",
    "vector",    "vfont",      "vpres",      "testwin",    "sampletodo",
    "passworddemo", "scriptdemo", "lf",      "helix",
};
bool IsDevOnlyApp(const std::string& name) {
    for (const char* d : kDevOnlyApps) {
        if (name == d) return true;
    }
    return false;
}
// 메뉴에서 쓰는 상태/토글 — state/desktop.json {"showDev":true} 기록.
// (셸 Init이 이 파일을 읽어 필터를 적용한다.)
namespace {
std::string DesktopStatePath() {
    std::string exe = jk::fs::GetExecutablePath();
    const size_t slash = exe.find_last_of("\/");
    if (slash != std::string::npos) exe.resize(slash + 1);
    return exe + "state/desktop.json";
}
bool ReadShowDev() {
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(DesktopStatePath(), bytes)) return false;
    const std::string text(reinterpret_cast<const char*>(bytes.data()));
    return text.find("\"showDev\":true") != std::string::npos ||
           text.find("\"showDev\": true") != std::string::npos;
}
}  // namespace
bool ShellShowDevApps() { return ReadShowDev(); }
void ShellToggleShowDevApps() {
    const bool next = !ReadShowDev();
    const std::string path = DesktopStatePath();
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path(), ec);
    if (std::FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fprintf(f, "{\"showDev\":%s}\n", next ? "true" : "false");
        std::fclose(f);
        std::fprintf(stderr, "[shell] showDev -> %s (다음 실행 적용)\n",
                     next ? "true" : "false");
    }
}

void JKDesktopShell::Init(const ShellHost& host) {
    host_ = host;
    if (!host_.renderer) return;

    launcherIcons_.clear();

    // Installed .jkx containers first (Phase C): one launcher cell per
    // apps/<name>.jkx, icon decoded from the container itself.
    ScanJkxApps();
    ScanConsoleApps();

    // Built-in process-mode fallback for apps that have no .jkx installed.
    auto hasJkx = [this](const char* name) {
        for (const auto& icon : launcherIcons_) {
            if (icon.appName == name) return true;
        }
        return false;
    };
    if (!hasJkx("minesweeper")) {
        LauncherIcon icon;
        icon.appName = "minesweeper";
        icon.title = "Minesweeper";
        launcherIcons_.push_back(icon);
    }
    if (!hasJkx("tetris")) {
        LauncherIcon icon;
        icon.appName = "tetris";
        icon.title = "Tetris";
        launcherIcons_.push_back(icon);
    }
    // [T9-브라우저] (2026-11-07 — 사용자 백로그: "브라우저 아이콘도 함께"):
    #if defined(__ANDROID__)
    if (!hasJkx("browser")) {
        LauncherIcon icon;
        icon.appName = "browser";
        icon.title = "Browser";
        launcherIcons_.push_back(icon);
    }
    #endif
    // Phase A TUI 흡수 (docs/44): 콘솔 앱은 appName "terminal:<cmdline>" 관례로
    // 터미널 위에 띄운다 (SpawnClient가 접두사를 해석). 상대경로는 jkdesktop
    // cwd(engine/build) 기준. 전용 아이콘 아트는 이후 폴리싱 — fallback 아트.
    // 툴팁 표시명은 exe 이름(접두어·경로 제거).
    #if !defined(__ANDROID__)
    // [2026-11-13 사용자 실측 원존 "hx 두 개임. lf 두 개임"] — 아래 apps-bin
    // 경로형 셀은 **posix 데스크톱(engine/build/apps-bin) 전용**인데 Android
    // 에서도 무조건 추가돼, 바로 아래 무접미(terminal:lf/hx) 셀과 같은 앱이
    // 두 번 떴다. Android는 PATH seed leg(무접미 명령) 하나만 쓴다.
    if (!hasJkx("terminal:apps-bin/lf/lf.exe")) {
        LauncherIcon icon;
        icon.appName = "terminal:apps-bin/lf/lf.exe";
        icon.title = "lf";
        launcherIcons_.push_back(icon);
    }
    if (!hasJkx("terminal:apps-bin/helix/hx.exe")) {
        LauncherIcon icon;
        icon.appName = "terminal:apps-bin/helix/hx.exe";
        icon.title = "hx";
        launcherIcons_.push_back(icon);
    }
    #endif
    #if defined(__ANDROID__)
    // [lf/helix 폰 조달 leg — Termux 바이오닉 빌드(11-07 실측: 단독 실행 원문
    // lf r42·hx 25.07.1)] — jksrv_main이 PATH seed+HELIX_RUNTIME을 심어
    // 무접미 명령으로 해석. grammars 193MB는 무장애 제외(기본 편집 leg).
    if (!hasJkx("terminal:lf")) {
        LauncherIcon icon;
        icon.appName = "terminal:lf";
        icon.title = "lf";
        launcherIcons_.push_back(icon);
    }
    if (!hasJkx("terminal:hx")) {
        LauncherIcon icon;
        icon.appName = "terminal:hx";
        icon.title = "hx";
        launcherIcons_.push_back(icon);
    }
    #endif

#if defined(__ANDROID__)
    // 스펙 §6 — 전 모듈 런처 축: nativeLibraryDir(= JK_EXE_PATH의 dir)에 포장된
    // libjkapp_<name>.so 전부를 런처 셀로. 셸 spawn dispatch는 기존 메커니즘
    // (appName → SpawnClient → lib 접두 모듈 dlopen — docs/103) 재용.
    {
        std::error_code dirEc;
        for (std::filesystem::directory_iterator it(
                 std::filesystem::path(jk::fs::GetExecutablePath()).parent_path(), dirEc);
             !dirEc && it != std::filesystem::directory_iterator(); it.increment(dirEc)) {
            const std::string fn = it->path().filename().string();
            if (fn.rfind("libjkapp_", 0) != 0) continue;
            if (fn.size() < 4 || fn.compare(fn.size() - 3, 3, ".so") != 0) continue;
            std::string appName =
                fn.substr(9, fn.size() - 9 - 3);  // libjkapp_(…)​.so
            if (appName.empty() || hasJkx(appName.c_str())) continue;
            // [2026-11-13 사용자 "같은 아이콘 많다" 원존] 내부 모듈은 사용자 앱이
            // 아니다 — taskbar = 셸 UI 자체, filedlg = 대화상자 자식. 런처에
            // 셀로 뜨면 아트가 없어 테트리스 공용 아이콘을 소비한다.
            if (appName == "taskbar" || appName == "filedlg") continue;
            LauncherIcon icon;
            icon.appName = appName;
            icon.title = appName;
            launcherIcons_.push_back(icon);
        }
        std::fprintf(stderr, "[jkdesk] launcher scan: android modules wired\n");
    }
#endif

    // Desktop background photo + launcher icon art (PNG assets, see
    // ARCHITECTURE_DOCS/20). Missing assets fall back to the flat placeholder.
    if (backgroundTexture_) {
        SDL_DestroyTexture(backgroundTexture_);
        backgroundTexture_ = nullptr;
    }
    backgroundTexture_ = LoadTextureScaled("assets/backgrounds/desktop");

    for (auto& icon : launcherIcons_) {
        if (icon.texture) continue;   // .jkx apps carry their own icon texture

        // Per-app icon art: assets/icons/launcher_<appName>. .jkx 앱이 컨테이너
        // ICON 엔트리를 빠뜨렸거나 콘솔 앱에 icon@{2x,1x}.png가 없어도 전용
        // 아이콘 PNG만 놓아두면 테트리스 공용 플레이스홀더에 겹치지 않는다.
        // terminal: 셀은 각 TUI 앱의 전용 아이콘 (docs/44). 아트가 없으면
        // 테트리스 플레이스홀더로 폴백.
        char nameBase[160];
        const char* base = "assets/icons/launcher_tetris";
        bool hasDedicated = false;
        if (icon.appName == "minesweeper") {
            base = "assets/icons/launcher_mine";
            hasDedicated = true;
        } else if (icon.appName == "terminal:apps-bin/lf/lf.exe") {
            base = "assets/icons/launcher_lf";
            hasDedicated = true;
        } else if (icon.appName == "terminal:apps-bin/helix/hx.exe") {
            base = "assets/icons/launcher_helix";
            hasDedicated = true;
        } else if (icon.appName.rfind("terminal:", 0) == 0) {
            // [2026-11-12 사용자 실측 "아이콘이 같은 놈들 많다" 원존] 안드로이드
            // 셀은 appName "terminal:lf"/"terminal:hx"(무경로) — 종전에는 위
            // 두 경로형 비교에만 걸려 여기로 떨어졌고, ':'가 안전 문자 검사에
            // 걸려 전용 아트 없음 → 테트리스 공용 아트로 겹쳤다. 접두 뒤
            // 명령 이름으로 아트를 고른다(lf/hx 계열 전부).
            const std::string cmd = icon.appName.substr(9);
            const bool isLf = cmd.rfind("lf", 0) == 0;
            const bool isHx = cmd.rfind("hx", 0) == 0 ||
                              cmd.rfind("helix", 0) == 0;
            if (isLf || isHx) {
                base = isLf ? "assets/icons/launcher_lf"
                            : "assets/icons/launcher_helix";
                hasDedicated = true;
            }
        } else {
            // appName은 .jkx 매니페스트 문자열이라 경로 특수문자 방지 — 안전한
            // 이름만 자산 경로로 쓴다.
            bool safe = !icon.appName.empty() && icon.appName.size() < 100;
            for (char c : icon.appName) {
                const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '_';
                if (!ok) { safe = false; break; }
            }
            if (safe) {
                std::snprintf(nameBase, sizeof(nameBase), "assets/icons/launcher_%s",
                              icon.appName.c_str());
                base = nameBase;
                hasDedicated = true;
            }
        }
        icon.texture = LoadTextureScaled(base);
        if (!icon.texture && hasDedicated) {
            icon.texture = LoadTextureScaled("assets/icons/launcher_tetris");
        }
        if (icon.texture) {
            std::fprintf(stderr, "JKWindowServer: launcher icon '%s' loaded\n", base);
        }
    }

    // 개발자 앱 표시 여부(기본 숨김) — state/desktop.json {"showDev":true}.
    {
        std::string exe = jk::fs::GetExecutablePath();
        const size_t slash = exe.find_last_of("\/");
        if (slash != std::string::npos) exe.resize(slash + 1);
        const std::string path = exe + "state/desktop.json";
        std::vector<uint8_t> bytes;
        if (ReadFileBytes(path, bytes)) {
            const std::string text(reinterpret_cast<const char*>(bytes.data()));
            showDevApps_ = text.find("\"showDev\":true") != std::string::npos ||
                           text.find("\"showDev\": true") != std::string::npos;
        }
        if (!showDevApps_) {
            const size_t before = launcherIcons_.size();
            launcherIcons_.erase(
                std::remove_if(launcherIcons_.begin(), launcherIcons_.end(),
                               [](const LauncherIcon& ic) {
                                   return IsDevOnlyApp(ic.appName);
                               }),
                launcherIcons_.end());
            std::fprintf(stderr,
                         "[shell] dev-only apps hidden: %d (showDev off)\n",
                         static_cast<int>(before - launcherIcons_.size()));
        }
    }

    // [2026-11-13] 같은 앱 셀 중복 제거(첫 셀 유지) — lf/hx 이중 등록 사고의
    // 재발 방지. 셀 생성 경로가 늘어날 때(플랫폼 분기·모듈 스캔·내장 폴백)
    // 같은 appName이 두 번 들어가면 런처에 같은 아이콘이 두 개 뜬다.
    {
        std::vector<LauncherIcon> unique;
        unique.reserve(launcherIcons_.size());
        for (auto& icon : launcherIcons_) {
            bool dup = false;
            for (const auto& kept : unique) {
                if (kept.appName == icon.appName) { dup = true; break; }
            }
            if (!dup) {
                unique.push_back(std::move(icon));
            } else if (icon.texture) {
                SDL_DestroyTexture(icon.texture);
                icon.texture = nullptr;
            }
        }
        launcherIcons_.swap(unique);
    }

    // Grid layout: one source of truth for cell rects, wrapping to the window
    // width (the single row overflowed the 1280px desktop at 13 .jkx apps).
    RelayoutLauncherIcons();
    Draw(host_.renderer);
}

// Launcher cell grid (docs/21 §2) — [셸 2.0 S1 — 2026-11-09 사용자 판정
// "좀더 큰 공사" 원존·스펙 §3 S1]: 셀 = 아이콘 96px + 라벨 2단·제목순 정렬
// (호버·LaunchAt 인덱스는 같은 벡터 순서라 정렬과 무충돌). pitch 150.
void JKDesktopShell::RelayoutLauncherIcons() {
    if (!host_.renderer) return;
    // 제목순 안정 정렬(라벨 그리드의 검색성 — 표기명 = title).
    std::stable_sort(launcherIcons_.begin(), launcherIcons_.end(),
                     [](const LauncherIcon& a, const LauncherIcon& b) {
                         return a.title < b.title;
                     });
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    int pw = 1280;
    int ph = 720;
    SDL_GetRendererOutputSize(host_.renderer, &pw, &ph);
    const int logicalW = static_cast<int>(pw / s);
    // [docs/106 §3] cols=0 UB 봉합 — 좁은 표면(DeX 분할·폰 세로)에서도 최소
    // 1열(0으로 나누기 = 모듈로 0 크래시 원존).
    const int cols = std::max(1, (logicalW - 50) / 150);
    for (size_t i = 0; i < launcherIcons_.size(); ++i) {
        const int col = static_cast<int>(i) % cols;
        const int row = static_cast<int>(i) / cols;
        launcherIcons_[i].rect = JKRect{ 50 + col * 150, 50 + row * 150, 96, 130 };
    }
}

void JKDesktopShell::ScanJkxApps() {
#ifdef _WIN32
    // Enumerate <exe-dir>/apps/*.jkx. Icon textures are decoded from the
    // container's ICON entries (no temp files); spawning uses --jkx <path>.
    // jk::fs::GetExecutablePath 흡수 (docs/68 W5) — basePath 규약(뒤 "\\" 없음)
    // 유지.
    const std::string exe = jk::fs::GetExecutablePath();
    if (exe.empty()) return;
    const size_t baseCut = exe.find_last_of("\\/");
    if (baseCut == std::string::npos) return;
    const std::string basePath = exe.substr(0, baseCut);

    const std::string pattern = basePath + "\\apps\\*.jkx";
    JkxFindData fd{};
    void* find = FindFirstFileA(pattern.c_str(), &fd);
    if (!find) return;

    const float s = host_.outputScale ? host_.outputScale() : 1.0f;

    do {
        char path[1024];
        std::snprintf(path, sizeof(path), "%s\\apps\\%s",
                      basePath.c_str(), fd.cFileName);

        jk::JKJkxFile jkx;
        if (!jkx.Open(path)) continue;
        const jk::JkxManifest& mani = jkx.Manifest();
        if (mani.name.empty()) continue;

        LauncherIcon icon;
        icon.appName = mani.name;
        // 툴팁 표시명: 매니페스트 title 우선, 없으면 스폰 키.
        icon.title = !mani.title.empty() ? mani.title : mani.name;
        icon.jkxPath = path;

        // Icon entry: prefer @2x on high-scale displays.
        std::string wanted = (s >= 1.5f && !mani.icon2x.empty()) ? mani.icon2x : mani.icon;
        if (wanted.empty()) wanted = !mani.icon2x.empty() ? mani.icon2x : mani.icon;
        const int entry = wanted.empty() ? -1 : jkx.FindEntry("ICON", wanted);
        std::vector<uint8_t> png;
        jk::LoadedImage img;
        if (entry >= 0 && jkx.ReadEntry(entry, png) &&
            jk::LoadImageMemory(png.data(), png.size(), img)) {
            icon.texture = host_.makeTexture(img, mani.name.c_str());
        }

        launcherIcons_.push_back(std::move(icon));
        std::fprintf(stderr, "JKWindowServer: installed app '%s' from %s (icon %s)\n",
                     mani.name.c_str(), fd.cFileName,
                     launcherIcons_.back().texture ? "decoded" : "missing");
    } while (FindNextFileA(find, &fd));
    FindClose(find);
#else  // posix leg (#95 개방): 폰/WSL 그리드가 내장 4셀만 점화하던 갭 수리
    // — ScanJkxApps가 _WIN32 전용이면 posix 축에서 설치 .jkx(갤러리·뮤직
    // 등)가 그리드에 아예 안 떴다. win32 FindFirstFileA 대응은
    // directory_iterator — JKLibraryCatalog.cpp 1원 스캔 / JKWindowServer.cpp
    // stage-3 task 5 / TX6 posix 레그의 완결 경로를 승계한다. 판정·순서·로그
    // 행은 Win 원문 동형(캐논 등호) — 확장자 게이트만 NTFS 무시 패턴
    // "*.jkx" 대응으로 대소문자 무시 소문자화를 얹는다.
    //
    // win32 브랜치는 원문 불변(무변조 원칙) — 본 leg는 posix에서만 컴파일.
    const std::string exe = jk::fs::GetExecutablePath();
    if (exe.empty()) return;
    const size_t baseCut = exe.find_last_of("\\/");
    if (baseCut == std::string::npos) return;
    const std::string basePath = exe.substr(0, baseCut);

    const std::string appsDir = basePath + "/apps";
    std::error_code scanEc;
    if (!std::filesystem::exists(appsDir, scanEc) || scanEc) return;

    const float s = host_.outputScale ? host_.outputScale() : 1.0f;

    // (C-2 fix r1) posix 스캔 순서 결정론 — directory_iterator 순서는 무정렬이라
    // 부팅마다 셀 순서가 흔들린다. Win FindFirstFileA(NTFS)가 파일명 사전순
    // (case-insensitive)을 돌려주는 원문 뷰에 맞춰 이름 정렬로 통일 — "Win과
    // 동형" 표기의 재현성 담보(폰 눈확인 비교 기준).
    std::vector<std::filesystem::path> entries;
    for (std::filesystem::directory_iterator it(appsDir, scanEc), end;
         !scanEc && it != end; it.increment(scanEc)) {
        entries.push_back(it->path());
    }
    if (scanEc) {
        std::fprintf(stderr, "JKDesktopShell: apps dir scan ended early (%s)\n",
                     scanEc.message().c_str());
        return;
    }
    const auto byFileName = [](const std::filesystem::path& a,
                               const std::filesystem::path& b) {
        std::string na = a.filename().string();
        std::string nb = b.filename().string();
        for (char& c : na) c = static_cast<char>(std::tolower((unsigned char)c));
        for (char& c : nb) c = static_cast<char>(std::tolower((unsigned char)c));
        return na < nb;
    };
    std::sort(entries.begin(), entries.end(), byFileName);

    for (const std::filesystem::path& entry : entries) {
        const std::string jkxName = entry.filename().string();
        std::string ext = entry.extension().string();
        for (char& c : ext) c = static_cast<char>(std::tolower((unsigned char)c));
        if (ext != ".jkx") continue;

        const std::string path = entry.string();

        jk::JKJkxFile jkx;
        if (!jkx.Open(path)) continue;
        const jk::JkxManifest& mani = jkx.Manifest();
        if (mani.name.empty()) continue;

        LauncherIcon icon;
        icon.appName = mani.name;
        // 툴팁 표시명: 매니페스트 title 우선, 없으면 스폰 키 — win32 동형.
        icon.title = !mani.title.empty() ? mani.title : mani.name;
        icon.jkxPath = path;

        // Icon entry: prefer @2x on high-scale displays — win32 동형.
        std::string wanted = (s >= 1.5f && !mani.icon2x.empty()) ? mani.icon2x : mani.icon;
        if (wanted.empty()) wanted = !mani.icon2x.empty() ? mani.icon2x : mani.icon;
        const int iconEntry = wanted.empty() ? -1 : jkx.FindEntry("ICON", wanted);
        std::vector<uint8_t> png;
        jk::LoadedImage img;
        if (iconEntry >= 0 && jkx.ReadEntry(iconEntry, png) &&
            jk::LoadImageMemory(png.data(), png.size(), img)) {
            icon.texture = host_.makeTexture(img, mani.name.c_str());
        }

        launcherIcons_.push_back(std::move(icon));
        std::fprintf(stderr, "JKWindowServer: installed app '%s' from %s (icon %s)\n",
                     mani.name.c_str(), jkxName.c_str(),
                     launcherIcons_.back().texture ? "decoded" : "missing");
    }
#endif // _WIN32
}

// 콘솔 앱 스캔 (P4 SDK, specs/2026-09-16-p4-sdk-contract §3): apps/<name>/
// manifest.json 디렉터리를 .jkx와 같은 스캔 대상으로 받는다. JSON 파싱은
// quickjs throwaway 런타임(JKTerminalConfig::Load 패턴). .jkx와 이름이
// 겹치면 .jkx가 이긴다(스캔 순서 + 명시적 스킵).
void JKDesktopShell::ScanConsoleApps() {
#ifdef _WIN32
    // jk::fs::GetExecutablePath 흡수 (docs/68 W5) — basePath 규약(뒤 "\\" 없음)
    // 유지.
    const std::string exe = jk::fs::GetExecutablePath();
    if (exe.empty()) return;
    const size_t baseCut = exe.find_last_of("\\/");
    if (baseCut == std::string::npos) return;
    const std::string basePath = exe.substr(0, baseCut);

    const std::string pattern = basePath + "\\apps\\*";
    JkxFindData fd{};
    void* find = FindFirstFileA(pattern.c_str(), &fd);
    if (!find) return;

    constexpr unsigned long kDir = 0x10;  // FILE_ATTRIBUTE_DIRECTORY
    do {
        if (!(fd.dwFileAttributes & kDir) || fd.cFileName[0] == '.') continue;

        const std::string dirName = fd.cFileName;
        const std::string manifestPath =
            basePath + "\\apps\\" + dirName + "\\manifest.json";
        std::vector<uint8_t> bytes;
        if (!ReadFileBytes(manifestPath, bytes)) continue;  // no manifest → not a console app

        // [docs/106 §2-5] 매니페스트 파싱 = json/JKJsonQuick 정본.
        const auto mf = jk::json::ParseManifest(bytes, "manifest.json");
        std::string name = mf.name;
        std::string cmd = mf.cmd;
        std::string desc = mf.desc;
        if (!mf.ok || name.empty() || cmd.empty()) continue;

        // .jkx 우선: 같은 이름의 컨테이너가 이미 있으면 매니페스트 앱은 스킵.
        bool taken = false;
        for (const auto& icon : launcherIcons_) taken |= (icon.appName == name);
        if (taken) {
            std::fprintf(stderr, "JKWindowServer: console app '%s' skipped (.jkx wins)\n",
                         name.c_str());
            continue;
        }

        LauncherIcon icon;
        icon.appName = name;
        // 툴팁 표시명: manifest.json desc가 있으면 그걸 쓴다(이름보다 정보량).
        icon.title = desc;
        icon.consoleDir = std::string("apps\\") + dirName;
        icon.consoleCmd = cmd;

        // 스폰 cmd 지문 보증 (P4 SDK §3.4, docs/37 형식). 이미 있으면
        // EnsureTrustRecord가 파일을 건드리지 않는다.
        const std::string fp = ConsoleAppFingerprint(cmd);
        if (!fp.empty()) EnsureTrustRecord(fp, name);

        // 아이콘(선택): apps/<name>/icon@{2x,1x}.png — 없으면 placeholder 사각형.
        const float s = host_.outputScale ? host_.outputScale() : 1.0f;
        const char* pick = s >= 1.5f ? "icon@2x.png" : "icon@1x.png";
        char iconPath[1024];
        std::snprintf(iconPath, sizeof(iconPath), "%s\\apps\\%s\\%s", basePath,
                      dirName.c_str(), pick);
        jk::LoadedImage img;
        if (jk::LoadImageFile(iconPath, img)) {
            icon.texture = host_.makeTexture(img, name.c_str());
        }

        launcherIcons_.push_back(std::move(icon));
        std::fprintf(stderr,
                     "JKWindowServer: console app '%s' (cmd='%s', dir='%s', icon %s)\n",
                     name.c_str(), cmd.c_str(),
                     launcherIcons_.back().consoleDir.c_str(),
                     launcherIcons_.back().texture ? "decoded" : "missing");
    } while (FindNextFileA(find, &fd));
    FindClose(find);
#else  // posix leg (#95 개방): ScanJkxApps leg와 동일 근거 — 콘솔 앱 폴더를
    // posix 축에서도 스캔한다. win32 attribute 필터 대응은 is_directory +
    // 숨김 dir 스킵(JKLibraryCatalog.cpp 2원 스캔 동형), 매니페스트 파싱
    // 파트는 원문 그대로 승계. 스폰 키만 플랫폼 차를 얹는다:
    //   win32 = cmd 원문 그대로(cwd=앱 폴더 — 런처 전용 계약, 불변)
    //   posix = cmd_posix(옵션 필드)를 스폰 키로 승격 + 파일 존재 게이트
    //           (fail-closed) — bare Windows cmd는 posix PATH 밖 죽은 키라는
    //           카탈로그 판정의 1:1 승계(JKLibraryCatalog.cpp posix branch).
    // spawn cwd "apps/<dir>"는 '/' 단일 구분자 — posix SpawnProcess의
    // child cwd=exe dir 고정 실측 기준 상대키계약(내장 lf/hx 키와 동일
    // 기점). 453a327 렛슨: cmd/cwd에 뒤따르는 백슬래시 없음.
    const std::string exe = jk::fs::GetExecutablePath();
    if (exe.empty()) return;
    const size_t baseCut = exe.find_last_of("\\/");
    if (baseCut == std::string::npos) return;
    const std::string basePath = exe.substr(0, baseCut);

    const std::string appsDir = basePath + "/apps";
    std::error_code scanEc;
    if (!std::filesystem::exists(appsDir, scanEc) || scanEc) return;

    const float s = host_.outputScale ? host_.outputScale() : 1.0f;

    // (C-2 fix r1) 결정론 순서 — jkx 스캔 leg의 byFileName 정렬과 동일 원문
    // (Win NTFS 파일명 사전순 뷰에 정렬).
    std::vector<std::filesystem::path> entries;
    for (std::filesystem::directory_iterator it(appsDir, scanEc), end;
         !scanEc && it != end; it.increment(scanEc)) {
        entries.push_back(it->path());
    }
    if (scanEc) {
        std::fprintf(stderr, "JKDesktopShell: apps dir scan ended early (%s)\n",
                     scanEc.message().c_str());
        return;
    }
    const auto byFileName = [](const std::filesystem::path& a,
                               const std::filesystem::path& b) {
        std::string na = a.filename().string();
        std::string nb = b.filename().string();
        for (char& c : na) c = static_cast<char>(std::tolower((unsigned char)c));
        for (char& c : nb) c = static_cast<char>(std::tolower((unsigned char)c));
        return na < nb;
    };
    std::sort(entries.begin(), entries.end(), byFileName);

    for (const std::filesystem::path& entry : entries) {
        std::error_code dirEc;
        if (!std::filesystem::is_directory(entry, dirEc) || dirEc) continue;
        const std::string dirName = entry.filename().string();
        if (dirName.empty() || dirName[0] == '.') continue;

        const std::string manifestPath = entry.string() +
                                         std::string("/manifest.json");
        std::vector<uint8_t> bytes;
        if (!ReadFileBytes(manifestPath, bytes)) continue;  // no manifest → not a console app

        // [docs/106 §2-5] 매니페스트 파싱 = json/JKJsonQuick 정본(win32 동형).
        const auto mf = jk::json::ParseManifest(bytes, "manifest.json");
        std::string name = mf.name;
        std::string cmd = mf.cmd;
        std::string desc = mf.desc;
        std::string cmdPosix = mf.cmdPosix;
        if (!mf.ok || name.empty() || cmd.empty()) continue;

        // posix 스폰 키 — (M-1 fix r1) 절대키 해상: cmd_posix는 exe-dir 상대
        // 규약(JKLibraryCatalog 계약)이므로 basePath 결합해 cwd 무관 절대키로
        // 해상한다. SpawnConsoleApp의 `--cwd apps/<dir>` 조립은 Win 계약 그대로
        // 남겨두어도, 터미널 child(셸) cwd가 앱 폴더여도 /bin/sh -c <절대키>가
        // 성립한다 — 키를 basePath 상대로 남기면
        // `apps/<dir>/`+`apps/<sample>/x.sh` 중복 접두로 소실(리뷰 M-1 재현
        // 원장: cd buildwsl/apps/sampletodo && ls apps/sampletodo/sampletodo.sh
        // → No such file). 존재 게이트 fail-closed는 유지. cmd_posix 부재
        // 매니페스트는 cmd 원문(카탈로그 계약 1:1) — Windows 전용 cmd는 posix
        // PATH 밖 죽은 키일 수 있다는 한계는 카탈로그와 공유.
        std::string spawnKey = cmd;
        if (!cmdPosix.empty()) {
            std::error_code twinEc;
            const std::string twinPath = basePath + "/" + cmdPosix;
            if (!std::filesystem::exists(twinPath, twinEc) || twinEc) {
                std::fprintf(stderr,
                             "JKWindowServer: console app '%s' skipped "
                             "(posix spawn file missing: '%s')\n",
                             name.c_str(), cmdPosix.c_str());
                continue;
            }
            spawnKey = twinPath;
        }

        // .jkx 우선: 같은 이름의 컨테이너가 이미 있으면 매니페스트 앱은 스킵 —
        // 판정·로그 행 win32 원문 동형.
        bool taken = false;
        for (const auto& icon : launcherIcons_) taken |= (icon.appName == name);
        if (taken) {
            std::fprintf(stderr, "JKWindowServer: console app '%s' skipped (.jkx wins)\n",
                         name.c_str());
            continue;
        }

        LauncherIcon icon;
        icon.appName = name;
        // 툴팁 표시명: manifest.json desc가 있으면 그걸 쓴다 — win32 동형.
        icon.title = desc;
        icon.consoleDir = std::string("apps/") + dirName;
        icon.consoleCmd = spawnKey;

        // 스폰 cmd 지문 보증 (P4 SDK §3.4) — 실제 스폰 키 기준 해시.
        const std::string fp = ConsoleAppFingerprint(spawnKey);
        if (!fp.empty()) EnsureTrustRecord(fp, name);

        // 아이콘(선택): apps/<name>/icon@{2x,1x}.png — 없으면 placeholder 사각형.
        const char* pick = s >= 1.5f ? "icon@2x.png" : "icon@1x.png";
        const std::string iconPath = entry.string() + "/" + pick;
        jk::LoadedImage img;
        if (jk::LoadImageFile(iconPath, img)) {
            icon.texture = host_.makeTexture(img, name.c_str());
        }

        launcherIcons_.push_back(std::move(icon));
        std::fprintf(stderr,
                     "JKWindowServer: console app '%s' (cmd='%s', dir='%s', icon %s)\n",
                     name.c_str(), spawnKey.c_str(),
                     launcherIcons_.back().consoleDir.c_str(),
                     launcherIcons_.back().texture ? "decoded" : "missing");
    }
#endif // _WIN32
}

// Frame background painter (formerly DrawLauncherBackground): this only
// draws — the compositor calls SDL_RenderPresent once per frame.
// [UI v2 — 2026-11-13] 바탕화면 우클릭 메뉴. 항목 3종(새로고침·테마 순환·
// 닫기). 텍스트는 라벨 텍스처(투명 배경) 재사용 — 새 래스터 경로 0.
namespace {
constexpr int kMenuItemCount = 6;


// [실측 원존 2026-11-14 "메뉴 글자 깨짐"] 종전 항목 높이 = 고정 26×s —
// 라벨 텍스처 높이(셀 메트릭 + 패드)가 더 크면 세로로 잘려 글자가 깨졌다.
// 항목 높이는 항상 라벨이 들어가는 크기(셀높이+12pt 하한 26pt).
inline int MenuItemHeightPx(float s) {
    const int cellH = static_cast<int>(jk::text::GetCellMetrics().cellH);
    const int logical = std::max(26, cellH + 12);
    return static_cast<int>(logical * s);
}
// [2026-11-14] 테마 항목에 **현재 프리셋**을 표시 — "테마 바꾸기"만 있으면
// 무엇에서 무엇으로 가는지 알 수 없다(사용자 피드백). 표기 = 한글 프리셋명.
const char* ThemePresetLabelKo(const jk::theme::JKTheme* t) {
    if (t == &jk::theme::kLight) return "라이트";
    if (t == &jk::theme::kClassic) return "클래식";
    return "다크";
}

std::string MenuItemLabel(int i) {
    switch (i) {
        case 0: return "새로고침";
        case 1: return std::string("테마: ") +
                       ThemePresetLabelKo(&jk::theme::current()) + " ▸";
        case 2: return "음성으로 말하기";
        case 3: return "오버레이 버튼";
        case 4: return std::string("개발자 앱: ") +
                       (jk::desktop::ShellShowDevApps() ? "보임" : "숨김");
        default: return "닫기";
    }
}
}  // namespace

void JKDesktopShell::Draw(SDL_Renderer* renderer) {
    if (!renderer || launcherIcons_.empty()) return;

    // All server drawing is in physical pixels. icon.rect is stored in SDL
    // logical points, so multiply by the compositor output scale to get the
    // physical-pixel rect. The mouse hit-test uses the same physical rect.
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;

    // 테마 세대 감지(핸드스왕 P3 핵스와프) — 색이 텍스처에 베이크된
    // 캐시(라벨·툴팁·시계)는 세대가 바뀌면 파괴해야 낡은 색이 잔존하지 않는다.
    if (themeGen_ != jk::theme::Generation()) {
        InvalidateThemeTextCaches();
        themeGen_ = jk::theme::Generation();
    }
    // [docs/106 §3] 표면 크기 변화 = 그리드 재계산(비Android 리사이즈·DeX 창
    // 모드 전환에서 1회만 계산되던 결함). 크기 무변이면 무비용(정수 비교 2회).
    {
        int pwNow = 0;
        int phNow = 0;
        SDL_GetRendererOutputSize(renderer, &pwNow, &phNow);
        if (pwNow != lastOutW_ || phNow != lastOutH_) {
            lastOutW_ = pwNow;
            lastOutH_ = phNow;
            RelayoutLauncherIcons();
        }
    }

    const auto& t = jk::theme::current();
    SDL_SetRenderDrawColor(renderer, t.desktopBgFallback.r, t.desktopBgFallback.g, t.desktopBgFallback.b, 255);
    SDL_RenderClear(renderer);

    // Desktop background photo stretched to the full window.
    if (backgroundTexture_) {
        int pw = 0;
        int ph = 0;
        SDL_GetRendererOutputSize(renderer, &pw, &ph);
        SDL_Rect dst{ 0, 0, pw, ph };
        SDL_RenderCopy(renderer, backgroundTexture_, nullptr, &dst);
        // [2026-11-14 사용자 판정] 바탕(사용자 사진)은 테마가 건드리지 않는다 —
        // 글자색은 테마 팔레트를 따르고, 가독성은 렌더 쪽 **대비 외곽선**이
        // 보장한다(TextTextureEx 비ASCII 경로).
    }

    for (auto& icon : launcherIcons_) {
        SDL_Rect rc{
            static_cast<int>(icon.rect.x * s),
            static_cast<int>(icon.rect.y * s),
            static_cast<int>(icon.rect.w * s),
            static_cast<int>(icon.rect.h * s),
        };
        if (icon.texture) {
            // Square icon art in the top part of the 64x80 cell; the rest of
            // the cell is label space (the server has no text renderer).
            SDL_Rect art{
                rc.x,
                rc.y,
                static_cast<int>(icon.rect.w * s),
                static_cast<int>(icon.rect.w * s),
            };
            SDL_RenderCopy(renderer, icon.texture, nullptr, &art);
        } else {
            if (icon.appName == "minesweeper") {
                SDL_SetRenderDrawColor(renderer, t.launcherCellPlaceholder.r, t.launcherCellPlaceholder.g, t.launcherCellPlaceholder.b, 255);
            } else if (icon.appName == "tetris") {
                // 의도적 리터럴 잔존 — 앱 식별색 (P2 테마 스왑 제외).
                SDL_SetRenderDrawColor(renderer, 128, 0, 128, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, t.launcherCellFace.r, t.launcherCellFace.g, t.launcherCellFace.b, 255);
            }
            SDL_RenderFillRect(renderer, &rc);
            SDL_SetRenderDrawColor(renderer, t.launcherCellOutline.r, t.launcherCellOutline.g, t.launcherCellOutline.b, 255);
            SDL_RenderDrawRect(renderer, &rc);
        }

        // [셸 2.0 S1 — 라벨] 아이콘 아래 제목 텍스처(표기명 — 1회 렌더 캐시).
        const std::string& label = !icon.title.empty() ? icon.title : icon.appName;
        if (!label.empty()) {
            if (!icon.labelTex) {
                int lw = 0;
                int lh = 0;
                // [2026-11-14] 라벨 색 = 그 자리 배경 명암 적응(테마 색 고정 금지 —
                // 라이트 프리셋의 어두운 글자가 어두운 사진 위에서 사라졌다).
                // [2026-11-14 사용자 판정 2] 라벨 색은 **테마 팔레트를 따른다**
                // (테마를 바꿨는데 글자색이 그대로면 테마가 반쪽이다). 가독성은
                // 배경 적응색이 아니라 **대비 외곽선**(TextTextureEx)이 보장한다.
                icon.labelTex = LabelTextureColored(
                    label, jk::theme::current().launcherLabelText, &lw, &lh);
                icon.labelTexW = lw;
                icon.labelTexH = lh;
            }
            if (icon.labelTex && icon.labelTexW > 0) {
                // [docs/109 §2.1] 라벨 블릿 = 툴팁 산식(×outputScale) — 종전
                // 1× 블릿이라 HiDPI(s=2)에서 라벨만 절반 크기였다. 텍스처는
                // 셀 메트릭 해상도로 굽고 그리는 쪽이 s를 곱한다(단일 계약).
                // [docs/106 §3] 셀 폭 클램프 — 셀(96pt)보다 긴 제목은 비율
                // 축소 + 가로 중앙 정렬(구: 셀 밖으로 삐져나옴).
                const int cellW = static_cast<int>(icon.rect.w * s);
                int lw = static_cast<int>(icon.labelTexW * s);
                int lh = static_cast<int>(icon.labelTexH * s);
                if (lw > cellW && lw > 0) {
                    lh = static_cast<int>(static_cast<long long>(lh) * cellW / lw);
                    lw = cellW;
                }
                SDL_Rect lbl{
                    rc.x + (cellW - lw) / 2,
                    rc.y + static_cast<int>(100 * s),
                    lw,
                    lh,
                };
                SDL_RenderCopy(renderer, icon.labelTex, nullptr, &lbl);
            }
        }
    }

    // [셸 2.0 S4 — 시계 위젯] 바탕화면 우상단 큰 HH:MM — 분이 바뀔 때만
    // 텍스처 재렌더(자기 자리의 리렌더 = LabelTexture 캐시 미적용 — 본체가
    // 자기 파괴). 토큰 = onDynamicDraw(분 교차 시 자리 재발행).
    {
        time_t nowt = time(nullptr);
        tm lt{};
#ifdef _WIN32
        localtime_s(&lt, &nowt);
#else
        tm* pp = std::localtime(&nowt);
        if (pp) lt = *pp;
#endif
        bool clockRedrawn = false;
        if (lt.tm_min != clockMinute_ || !clockTex_) {
            clockRedrawn = true;
            clockMinute_ = lt.tm_min;
            if (clockTex_) SDL_DestroyTexture(clockTex_);
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
            int cw = 0;
            int ch = 0;
            // 시계 글자색 = 테마 팔레트(가독성은 외곽선이 보장).
            clockTex_ = LabelTextureColored(
                buf, jk::theme::current().desktopClockText, &cw, &ch);
            clockW_ = cw;
            clockH_ = ch;
        }
        if (clockTex_ && clockW_ > 0) {
            int pw = 0;
            int ph = 0;
            SDL_GetRendererOutputSize(renderer, &pw, &ph);
            // [docs/109 §2.2] 시계 배율 = 명명 상수(토큰 아님 — 의도된 확대).
            constexpr int kClockScale = 4;
            const int scale = kClockScale;
            SDL_Rect dst{ pw - clockW_ * scale - static_cast<int>(20 * s),
                          static_cast<int>(20 * s), clockW_ * scale,
                          clockH_ * scale };
            SDL_RenderCopy(renderer, clockTex_, nullptr, &dst);
            // [docs/106 §3] 사건 판정 = 자리 이동/첫 착지/분 교차(내용 갱신).
            // 구 수형 = clockLastRect_ 영원히 0 → 매 프레임 사건(더티 스킵 무력화).
            if (host_.onDynamicDraw) {
                const bool moved =
                    clockLastRect_.x != dst.x || clockLastRect_.y != dst.y ||
                    clockLastRect_.w != dst.w || clockLastRect_.h != dst.h;
                if (moved) {
                    if (clockLastRect_.w > 0) host_.onDynamicDraw(clockLastRect_);
                    host_.onDynamicDraw(dst);   // 첫 착지 / 자리 이동
                } else if (clockRedrawn) {
                    host_.onDynamicDraw(dst);   // 같은 자리·새 분(텍스처 교체)
                }
            }
            clockLastRect_ = dst;
        }
    }

    // 호버 툴팁 (docs/67 후속): 300ms 머문 아이콘 셀 위에 마지막으로 얹는다 —
    // 아이콘·배경 위에 항상 깔리도록 그리기 순서상 맨 뒤. 활성화 판정은 여기서
    // (정지 커서 승격 — UpdateHover 주석 참조).
    if (hoverIndex_ >= 0 && !hoverActive_ &&
        SDL_GetTicks() - hoverStartMs_ >= kTooltipHoverDelayMs) {
        hoverActive_ = true;
    }
    // (T2 fix r1 NT2-1) 툴팁 전이 = rect 사건 없는 동적 드로잉 — 이번 프레임에
    // 실제 그린 dst와 마지막 표기를 비교해 전이(off→on·자리 이동·on→off)만
    // onDynamicDraw로 흘린다(전이가 없는 프레임은 사건 0 — 스킵 이득 유지).
    SDL_Rect tooltipNow{0, 0, 0, 0};
    if (hoverActive_ && hoverIndex_ >= 0 &&
        hoverIndex_ < static_cast<int>(launcherIcons_.size())) {
        tooltipNow = DrawTooltip(renderer,
                                 launcherIcons_[static_cast<size_t>(hoverIndex_)]);
    }
    if (host_.onDynamicDraw) {
        if (tooltipNow.w > 0) {
            if (tooltipShownLast_ &&
                (tooltipLastRect_.x != tooltipNow.x ||
                 tooltipLastRect_.y != tooltipNow.y ||
                 tooltipLastRect_.w != tooltipNow.w ||
                 tooltipLastRect_.h != tooltipNow.h)) {
                host_.onDynamicDraw(tooltipLastRect_);  // 이전 자리 소거 사건
                host_.onDynamicDraw(tooltipNow);        // 새 자리 착지 사건
            } else if (!tooltipShownLast_) {
                host_.onDynamicDraw(tooltipNow);        // off→on
            }
            tooltipLastRect_ = tooltipNow;
            tooltipShownLast_ = true;
        } else if (tooltipShownLast_) {
            host_.onDynamicDraw(tooltipLastRect_);  // on→off — 배경 재칠 사건
            tooltipShownLast_ = false;
        }
    }

    // [UI v2 — 2026-11-13] 바탕화면 우클릭 메뉴 — 최상위(툴팁 뒤).
    if (menuOpen_ && menuRect_.w > 0) {
        const auto& th = jk::theme::current();
        SDL_Rect panel = menuRect_;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, th.taskbarBg.r, th.taskbarBg.g,
                               th.taskbarBg.b, 245);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, th.taskbarBorderInactive.r,
                               th.taskbarBorderInactive.g,
                               th.taskbarBorderInactive.b, 255);
        SDL_RenderDrawRect(renderer, &panel);
        const int itemH = MenuItemHeightPx(s);
        const int pad = static_cast<int>(4 * s);
        JKSDLRenderBackend menuBackend(renderer);
        JKDC dc(&menuBackend);
        textParts_.AttachTo(dc);
        // [실측 2026-11-14 "메뉴 글자 깨짐"] 라벨 텍스처(KSSM 경유) 대신
        // 태스크바 네비 셀과 같은 **아틀라스 유니코드 직행**으로 그린다 —
        // KSSM 부분집합 밖/변환 실패 시 원문 바이트가 그대로 그려져 깨지던
        // 경로를 메뉴에서는 배제(3개 단문자열이라 매 프레임 직접 그리기 무비용).
        const auto& th2 = jk::theme::current();
        dc.SetTextColor(th2.launcherLabelText.r, th2.launcherLabelText.g,
                        th2.launcherLabelText.b);
        const jk::text::CellMetrics cm = jk::text::GetCellMetrics();
        for (int i = 0; i < kMenuItemCount; ++i) {
            const std::string label = MenuItemLabel(i);
            const int rowY = panel.y + pad + i * itemH;
            const int y = rowY + (itemH - static_cast<int>(cm.cellH * s)) / 2;
            int x = panel.x + pad * 2;
            for (size_t k = 0; k < label.size();) {
                const unsigned char c = static_cast<unsigned char>(label[k]);
                uint32_t cp = c;
                int len = 1;
                if (c >= 0xF0) { cp = c & 0x07; len = 4; }
                else if (c >= 0xE0) { cp = c & 0x0F; len = 3; }
                else if (c >= 0xC0) { cp = c & 0x1F; len = 2; }
                for (int kk = 1; kk < len && k + kk < label.size(); ++kk) {
                    cp = (cp << 6) | (label[k + kk] & 0x3F);
                }
                const int adv = static_cast<int>(
                    (len == 1 ? cm.engW : cm.hanW) * s);
                (void)dc.DrawUnicodeGlyph(JKPoint{x, y}, cp, adv);
                x += adv;
                k += static_cast<size_t>(len);
            }
        }
        textParts_.FlushUploads();   // 화면 직행 글리프도 업로드 반영 보장
    }
}

void JKDesktopShell::Destroy() {
    for (auto& icon : launcherIcons_) {
        if (icon.texture) {
            SDL_DestroyTexture(icon.texture);
            icon.texture = nullptr;
        }
    }
    launcherIcons_.clear();
    if (backgroundTexture_) {
        SDL_DestroyTexture(backgroundTexture_);
        backgroundTexture_ = nullptr;
    }
    if (clockTex_) {
        SDL_DestroyTexture(clockTex_);
        clockTex_ = nullptr;
    }
    // 툴팁 텍스처 캐시 + 렌더 지연 부품 (캐시가 소유 백엔드에 의존하므로
    // 텍스처 파괴 후 플러시 순 — 승인 배너 정리 경로 선례).
    for (auto& kv : tooltipTexs_) {
        if (kv.second.tex) {
            SDL_DestroyTexture(kv.second.tex);
            kv.second.tex = nullptr;
        }
    }
    tooltipTexs_.clear();
    ClearHover();
    textParts_.Reset();   // 캐시 업로드 정리 → 부품 파괴(순서 계약은 번들 소관)
}

int JKDesktopShell::HitTest(int x, int y) const {
    // (x, y) are physical client px. icon.rect is in logical points.
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    for (size_t i = 0; i < launcherIcons_.size(); ++i) {
        const auto& r = launcherIcons_[i].rect;
        const int rx = static_cast<int>(r.x * s);
        const int ry = static_cast<int>(r.y * s);
        const int rw = static_cast<int>(r.w * s);
        const int rh = static_cast<int>(r.h * s);
        if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// 마우스 호버 툴팁 (docs/67 후속): 서버 마우스 경로가 모션마다 히트 인덱스를
// 중계한다. 인덱스 변화(이동·벗어남)는 지연 타이머를 리셋한다. 활성화 판정은
// Draw()가 매 프레임 한다 — SDL 모션은 커서가 움직일 때만 도착하므로 정지
// 커서는 추가 모션 없이 머무름만으로 300ms 경과 후 툴팁이 떠야 한다.
void JKDesktopShell::UpdateHover(int hitIndex) {
    if (hitIndex != hoverIndex_) {
        hoverIndex_ = hitIndex;
        hoverStartMs_ = SDL_GetTicks();
        hoverActive_ = false;
    }
}

void JKDesktopShell::ClearHover() {
    hoverIndex_ = -1;
    hoverActive_ = false;
}

// [docs/106 §3 — 테마 토큰] 색이 베이크된 텍스처만 파괴(아틀라스 귀자 이미지는 색과 무관 — 유지).
void JKDesktopShell::InvalidateThemeTextCaches() {
    for (auto& kv : tooltipTexs_) {
        if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    }
    tooltipTexs_.clear();
    for (auto& icon : launcherIcons_) {
        if (icon.labelTex) {
            SDL_DestroyTexture(icon.labelTex);
            icon.labelTex = nullptr;
        }
        icon.labelTexW = 0;
        icon.labelTexH = 0;
    }
    if (clockTex_) {
        SDL_DestroyTexture(clockTex_);
        clockTex_ = nullptr;
    }
    clockMinute_ = -1;
    std::fprintf(stderr, "[shell] theme gen %u — text caches invalidated\n",
                 themeGen_ + 1);
}

SDL_Texture* JKDesktopShell::TextTexture(const std::string& utf8, int* w,
                                         int* h, bool boxed) {
    const auto& t = jk::theme::current();
    return TextTextureEx(utf8, w, h, boxed,
                         boxed ? t.tooltipText : t.launcherLabelText);
}

SDL_Texture* JKDesktopShell::LabelTextureColored(const std::string& utf8,
                                                 SDL_Color color, int* w,
                                                 int* h) {
    return TextTextureEx(utf8, w, h, false, color);
}

SDL_Texture* JKDesktopShell::TextTextureEx(const std::string& utf8, int* w,
                                           int* h, bool boxed,
                                           SDL_Color textColor) {
    // [셸 2.0 S1] 캐시 키 접두(T:툴팁/L:라벨/C:색변형) — 동텍스트·동색 이중
    // 캐시 충돌 소각. 색이 베이크되므로 키에 RGBA를 넣는다(시계/라벨 분리).
    char colorKey[24];
    std::snprintf(colorKey, sizeof(colorKey), "%02x%02x%02x%02x",
                  textColor.r, textColor.g, textColor.b, textColor.a);
    const std::string ckey = std::string(boxed ? "T:" : "L:") + colorKey + ":" +
                             utf8;
    *w = 0;
    *h = 0;
    auto it = tooltipTexs_.find(ckey);
    if (it != tooltipTexs_.end()) {
        *w = it->second.w;
        *h = it->second.h;
        return it->second.tex;
    }
    if (!host_.renderer || !SDL_RenderTargetSupported(host_.renderer)) {
        // 렌더 타깃 미지원 백엔드 — 툴팁 없이 계속(사실상 안 쓰는 가지, 배너 선례).
        tooltipTexs_[ckey] = TooltipTex{};
        return nullptr;
    }
    // 렌더 지연 부품 = text/JKTextParts 번들(docs/106 §3 통합). 초기화
    // 파라미터(셀 메트릭·폴백 체인)가 한 곳에 있고, 실패 = 비트맵 폴백.
    (void)textParts_.Ensure(host_.renderer);
    // [실측 원존 2026-11-14 "한글 라벨 깨짐"] 종전 경로 = UTF-8을 **KSSM
    // 완성형 코드로 변환해 TextOut** — 아틀라스는 유니코드 글리프 표라서
    // KSSM 코드를 유니코드로 오인하고 엉뚱한 글리프를 그린다(ASCII 라벨은
    // 우연히 일치해 드러나지 않았다). 비ASCII + 아틀라스 준비 = 유니코드
    // 직행(태스크바 네비 셀과 같은 수형), 그 외 = 종전 KSSM/비트맵 평태.
    constexpr int kPad = 5;
    bool nonAscii = false;
    for (unsigned char c : utf8) {
        if (c >= 0x80) { nonAscii = true; break; }
    }
    // [2026-11-15 P1/A1 — 텍스트 경로 단일화] 아틀라스가 준비돼 있으면 **ASCII도
    // 유니코드 직행**으로 그린다. 종전엔 한글만 직행/영문은 KSSM 경로라 같은
    // 라벨도 두 파이프라인을 탔다(깨짐·사라짐이 표면별로 다르게 나타난 원인).
    // KSSM/비트맵 경로는 아틀라스 부재(폰트 없음) 폴백으로만 남긴다.
    (void)nonAscii;
    const bool useUnicode = textParts_.Ready();
    std::string kssm;
    int texW = 0;
    int texH = 0;
    jk::text::CellMetrics cm{};
    if (useUnicode) {
        cm = jk::text::GetCellMetrics();
        int drawW = 0;
        for (size_t i = 0; i < utf8.size();) {
            const unsigned char c = static_cast<unsigned char>(utf8[i]);
            const int len = (c < 0x80) ? 1 : (c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : 2));
            drawW += (len == 1) ? cm.engW : cm.hanW;
            i += static_cast<size_t>(len);
        }
        texW = drawW + kPad * 2;
        texH = static_cast<int>(cm.cellH) + kPad * 2;
    } else {
        kssm = Utf8ToKssm(utf8.c_str());
        if (kssm.empty()) {
            kssm = utf8;
        }
        const JKPoint m = JKDC::MeasureText(kssm.c_str());
        texW = m.x + kPad * 2;
        texH = (m.y > 0 ? m.y : 16) + kPad * 2;
    }
    SDL_Texture* tex = SDL_CreateTexture(host_.renderer, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET, texW, texH);
    if (!tex) {
        tooltipTexs_[ckey] = TooltipTex{};
        return nullptr;
    }
    SDL_Texture* prev = SDL_GetRenderTarget(host_.renderer);
    SDL_SetRenderTarget(host_.renderer, tex);
    if (boxed) {
        // 툴팁 박스 면·테두리 = 테마 토큰(docs/106 §3 누락 봉합).
        const auto& th = jk::theme::current();
        SDL_SetRenderDrawColor(host_.renderer, th.tooltipBg.r, th.tooltipBg.g,
                               th.tooltipBg.b, th.tooltipBg.a);
        SDL_RenderClear(host_.renderer);
        SDL_SetRenderDrawColor(host_.renderer, th.tooltipBorder.r,
                               th.tooltipBorder.g, th.tooltipBorder.b,
                               th.tooltipBorder.a);
        SDL_Rect border{ 0, 0, texW, texH };
        SDL_RenderDrawRect(host_.renderer, &border);
    } else {
        // 라벨(셸 2.0 S1) — 투명 배경(아래 벽지 위에 글자만 합성).
        SDL_SetRenderDrawBlendMode(host_.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(host_.renderer, 0, 0, 0, 0);
        SDL_RenderClear(host_.renderer);
    }
    JKSDLRenderBackend backend(host_.renderer);
    JKDC dc(&backend);
    textParts_.AttachTo(dc);   // 벡터 글리프(없으면 비트맵 폴백 그대로)
    dc.SetTextColor(textColor.r, textColor.g, textColor.b);
    if (useUnicode) {
        // [2026-11-14 보강] 적응색만으로는 사진의 밝기 경계(얼룩)에서 일부
        // 라벨이 배경에 묻힌다(사용자 "일부만 먹힘"). 대비색 1px 외곽선을
        // 먼저 깔고 본문을 올린다 — 어떤 배경 조각 위에서도 읽힌다.
        auto glyphPass = [&](int ox, int oy, SDL_Color col) {
            dc.SetTextColor(col.r, col.g, col.b);
            int x = kPad;
            for (size_t i = 0; i < utf8.size();) {
                const unsigned char c = static_cast<unsigned char>(utf8[i]);
                uint32_t cp = c;
                int len = 1;
                if (c >= 0xF0) { cp = c & 0x07; len = 4; }
                else if (c >= 0xE0) { cp = c & 0x0F; len = 3; }
                else if (c >= 0xC0) { cp = c & 0x1F; len = 2; }
                for (int k = 1;
                     k < len && i + static_cast<size_t>(k) < utf8.size(); ++k) {
                    cp = (cp << 6) | (utf8[i + static_cast<size_t>(k)] & 0x3F);
                }
                const int adv = (len == 1) ? cm.engW : cm.hanW;
                (void)dc.DrawUnicodeGlyph(JKPoint{ x + ox, kPad + oy }, cp, adv);
                x += adv;
                i += static_cast<size_t>(len);
            }
        };
        // 본문이 밝은 글자면 검은 외곽선, 어두운 글자면 흰 외곽선.
        const bool darkText =
            textColor.r < 128 && textColor.g < 128 && textColor.b < 128;
        const SDL_Color halo = darkText ? SDL_Color{ 245, 245, 245, 255 }
                                        : SDL_Color{ 12, 12, 14, 255 };
        glyphPass(-1, 0, halo);
        glyphPass(1, 0, halo);
        glyphPass(0, -1, halo);
        glyphPass(0, 1, halo);
        glyphPass(0, 0, textColor);
    } else {
        // 같은 대비 외곽선을 ASCII/KSSM 경로에도 적용 — 여기 빠뜨리면
        // 라이트 테마에서 영문 라벨이 어두운 사진 위에 묻힌다(2026-11-14).
        const bool darkText =
            textColor.r < 128 && textColor.g < 128 && textColor.b < 128;
        const SDL_Color halo = darkText ? SDL_Color{ 245, 245, 245, 255 }
                                        : SDL_Color{ 12, 12, 14, 255 };
        dc.SetTextColor(halo.r, halo.g, halo.b);
        dc.TextOut(JKPoint{ kPad - 1, kPad }, kssm.c_str());
        dc.TextOut(JKPoint{ kPad + 1, kPad }, kssm.c_str());
        dc.TextOut(JKPoint{ kPad, kPad - 1 }, kssm.c_str());
        dc.TextOut(JKPoint{ kPad, kPad + 1 }, kssm.c_str());
        dc.SetTextColor(textColor.r, textColor.g, textColor.b);
        dc.TextOut(JKPoint{kPad, kPad}, kssm.c_str());
    }
    // [실측 원존 2026-11-14 "새로고침 후 글자 사라짐"] 아틀라스 업로드 플러시
    // 없이 타겟을 읽으면 방금 만든 글리프가 빈 텍스처로 남는다(태스크바 미니
    // 텍스처 실측 원존 동형). 타겟 전환 **전에** 반영.
    textParts_.FlushUploads();
    SDL_SetRenderTarget(host_.renderer, prev);
    tooltipTexs_[ckey] = TooltipTex{tex, texW, texH};
    // 프로브 단정 지점: 표시명별 텍스처는 1회만 렌더되므로 로그에도 1회만
    // 찍힌다(캐시 적중 재호버는 무로그).
    std::fprintf(stderr, "[shell] text texture '%s' rendered (%dx%d)\n",
                 utf8.c_str(), texW, texH);
    *w = texW;
    *h = texH;
    return tex;
}

SDL_Texture* JKDesktopShell::TooltipTexture(const std::string& utf8, int* w, int* h) {
    return TextTexture(utf8, w, h, true);
}

// [셸 2.0 S1] 라벨 텍스처 — 박스 없는 투명 배경 변형(표기명 캐시 — 동텍스트
// 툴팁과 독립 키).
SDL_Texture* JKDesktopShell::LabelTexture(const std::string& utf8, int* w, int* h) {
    return TextTexture(utf8, w, h, false);
}

// (T2 fix r1 NT2-1) 반환 = 실제 그린 dst rect(물리 픽셀) — 못 그리면 0x0.
// 판정은 Draw() 쪽 전이 블록이 반환값으로 한다(사건은 전이时만 — 매 프레임
// 사건이면 스킵 이득 소멸).
SDL_Rect JKDesktopShell::DrawTooltip(SDL_Renderer* renderer,
                                     const LauncherIcon& icon) {
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    const std::string& label = !icon.title.empty() ? icon.title : icon.appName;
    if (label.empty()) return SDL_Rect{0, 0, 0, 0};
    int tw = 0;
    int th = 0;
    SDL_Texture* tex = TooltipTexture(label, &tw, &th);
    if (!tex || tw <= 0 || th <= 0) return SDL_Rect{0, 0, 0, 0};

    // 셀 아래 4pt 간격(논리 좌표). 데스크톱 바닥에 닿으면 셀 위로 뒤집고,
    // 오른쪽/왼쪽 클램프로 화면 밖을 막는다. 최종 좌표·크기는 물리 픽셀 —
    // Draw()의 나머지(rect × outputScale)와 같은 산식. 텍스처는 셀 메트릭
    // 크기라 스케일 >1에선 늘어나 그려진다(승인 배너의 raw 드로잉과 달리
    // 주변 UI와 크기를 맞춘다).
    int pw = 0;
    int ph = 0;
    SDL_GetRendererOutputSize(renderer, &pw, &ph);
    const int dw = static_cast<int>(tw * s);
    const int dh = static_cast<int>(th * s);
    int px = static_cast<int>(icon.rect.x * s);
    int py = static_cast<int>((icon.rect.y + icon.rect.h + 4) * s);
    if (px + dw > pw) px = pw - dw;
    if (px < 0) px = 0;
    if (py + dh > ph) {
        py = static_cast<int>(icon.rect.y * s) - dh - 4;
    }
    if (py < 0) py = 0;
    SDL_Rect dst{ px, py, dw, dh };
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    return dst;
}

void JKDesktopShell::OpenDesktopMenu(int x, int y) {
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    const int itemH = MenuItemHeightPx(s);
    const int w = static_cast<int>(150 * s);
    const int h = itemH * kMenuItemCount + static_cast<int>(8 * s);
    int px = x;
    int py = y;
    int pw = 0;
    int ph = 0;
    if (host_.renderer) SDL_GetRendererOutputSize(host_.renderer, &pw, &ph);
    if (pw > 0 && px + w > pw) px = pw - w;
    if (ph > 0 && py + h > ph) py = ph - h;
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (menuOpen_ && host_.onDynamicDraw && menuRect_.w > 0) {
        host_.onDynamicDraw(menuRect_);   // 이전 자리 소거
    }
    menuRect_ = SDL_Rect{ px, py, w, h };
    menuOpen_ = true;
    menuHover_ = -1;
    if (host_.onDynamicDraw) host_.onDynamicDraw(menuRect_);
    ClearHover();   // 툴팁과 겹치지 않게
}

void JKDesktopShell::CloseDesktopMenu() {
    if (!menuOpen_) return;
    menuOpen_ = false;
    menuHover_ = -1;
    if (host_.onDynamicDraw && menuRect_.w > 0) host_.onDynamicDraw(menuRect_);
    menuRect_ = SDL_Rect{ 0, 0, 0, 0 };
}

bool JKDesktopShell::HandleMenuClick(int x, int y) {
    if (!menuOpen_) return false;
    const bool inside = x >= menuRect_.x && x < menuRect_.x + menuRect_.w &&
                        y >= menuRect_.y && y < menuRect_.y + menuRect_.h;
    if (!inside) {
        CloseDesktopMenu();
        return false;
    }
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    const int itemH = MenuItemHeightPx(s);
    const int pad = static_cast<int>(4 * s);
    const int idx = (y - menuRect_.y - pad) / (itemH > 0 ? itemH : 1);
    CloseDesktopMenu();
    switch (idx) {
        case 0: {   // 새로고침 — 그리드 재계산. **직접 Draw 금지**: 합성기 밖
            // (이 콜은 마우스 이벤트 처리 중)에서 그리면 잘못된 타겟에 그려져
            // 다른 표면의 글자가 통째로 사라진다(2026-11-14 "새로고침 실행
            // 후 글자 사라짐" 실측). 다음 컴포지트 프레임에 다시 그리도록
            // 화면 전체 rect 사건만 흘린다.
            RelayoutLauncherIcons();
            ClearHover();
            int pw = 0;
            int ph = 0;
            if (host_.renderer) SDL_GetRendererOutputSize(host_.renderer, &pw, &ph);
            if (host_.onDynamicDraw && pw > 0 && ph > 0) {
                host_.onDynamicDraw(SDL_Rect{ 0, 0, pw, ph });
            }
            break;
        }
        case 1: {   // 테마 순환: 다크 → 라이트 → 클래식 → 다크
            // [실측 원존 2026-11-14 "테마 바꾸기 하니 다른 작업 없고 글자 사라짐"]
            // 종전 = setTheme만 호출 → theme.json 미기록(클라 폴링 미추종) +
            // 재드로잉 사건 부재로 부분 리페인트만 돌고, 세대 변화가 라벨
            // 텍스처를 파괴해 글자만 소실. 순환·기록은 theme::ApplyNextPreset
            // 정본(셀프테스트 2-5j~l)으로 승격.
            if (!jk::theme::ApplyNextPreset()) {
                std::fprintf(stderr, "[shell] theme preset write failed\n");
            }
            InvalidateThemeTextCaches();
            ClearHover();
            int pw = 0;
            int ph = 0;
            if (host_.renderer) SDL_GetRendererOutputSize(host_.renderer, &pw, &ph);
            if (host_.onDynamicDraw && pw > 0 && ph > 0) {
                host_.onDynamicDraw(SDL_Rect{ 0, 0, pw, ph });
            }
            break;
        }
        case 2: {   // 음성으로 말하기 — 기존 JNI 훅 재사용(상시 청취는 보류:
            // 배터리·프라이버시 판정 전. docs/106 §3 UI v2 잔여 참조).
            if (JKAndroidVoiceBegin) {
                (void)JKAndroidVoiceBegin();
            }
            break;
        }
        case 4: {   // 개발자 앱 표시 토글 — 파일에 기록, 다음 실행부터 적용
            // (셸 재초기화는 자산 스캔·텍스처 재생성을 동반해 클릭 처리 중에
            //  하면 위험 — 정직하게 "다음 실행" 계약으로 둔다).
            jk::desktop::ShellToggleShowDevApps();
            break;
        }
        case 3: {   // UI v2 MVP — 다른 앱 위에 뜨는 JK 버튼 토글.
            // 권한(SYSTEM_ALERT_WINDOW) 없으면 Java 쪽이 설정 화면만 연다.
            if (JkToggleOverlayButton) {
                (void)JkToggleOverlayButton();
            }
            break;
        }
        default:
            break;  // 닫기
    }
    return true;
}

bool JKDesktopShell::LaunchAt(int x, int y) {
    const int icon = HitTest(x, y);
    if (icon < 0) return false;
    ClearHover();  // 실행 직후 툴팁 즉시 숨김
    const LauncherIcon& item = launcherIcons_[static_cast<size_t>(icon)];
    if (!item.consoleCmd.empty()) {
        // 콘솔 앱 kind (P4 SDK §3): 터미널 위에 스폰 — cwd는 앱 폴더.
        if (host_.spawnConsole) {
            host_.spawnConsole(item.consoleCmd, item.consoleDir, item.appName);
        }
        return true;
    }
    if (item.jkxPath.empty()) {
        if (host_.launch) host_.launch(item.appName.c_str(), false);
    } else {
        if (host_.launch) host_.launch(item.jkxPath.c_str(), /*fromJkx=*/true);
    }
    return true;
}

bool JKDesktopShell::ConsoleAppInfo(const std::string& name, std::string& cmd,
                                    std::string& dir, std::string& fingerprint) const {
    for (const auto& icon : launcherIcons_) {
        if (icon.consoleCmd.empty() || icon.appName != name) continue;
        cmd = icon.consoleCmd;
        dir = icon.consoleDir;
        fingerprint = ConsoleAppFingerprint(cmd);
        return true;
    }
    return false;
}

// 에이전트 관리자 (스펙 §2.4): 런처 스캔 결과의 이름/kind만 돌려준다.
void JKDesktopShell::ListInstalled(
        std::vector<std::pair<std::string, const char*>>& out) const {
    for (const LauncherIcon& icon : launcherIcons_) {
        const char* kind = !icon.consoleCmd.empty()
            ? "console"
            : (!icon.jkxPath.empty() ? "jkx" : "builtin");
        out.emplace_back(icon.appName, kind);
    }
}

} // namespace desktop
} // namespace jk
