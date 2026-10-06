// 라이브러리 카탈로그 (스펙 2026-10-06-app-library §2 — 설치 앱 3원 스캔).
// 렛슨 1행: 폰 런처가 built-in만 보였던 갭(스펙 §4)의 해소는 라이브러리 스캔이
//   아니라 본 카탈로그의 posix 개방 자체 — 런처는 접촉 없음(Q1 룰링).
//
// 창·imgui 무접촉 pure 스캔 — CLI(library-list)·클라 앱(jkapp_library)·
// 셀프테스트(케이스 1m)가 같은 진실원을 먹는다. 런처(JKDesktopShell
// ScanJkxApps/ScanConsoleApps)는 이 카탈로그를 쓰지 않고, 본 파일은 런처의
// 스캔 규약을 std::filesystem 어댑터리로 이식한 쪽이다(#ifdef _WIN32 스캔의
// posix 개방 — TX6 slot-pack 레그 선례).
//
// 읽기 전용 설비(스펙 §0) — 어떤 파일도 생성·수정하지 않는다(uninstall 스킵
// 결제 2026-10-07). std::filesystem은 error_code 중립형 — 퍼블릭 경로 무
// try/catch 규약(launch_app fileExistsFn 주석 review r1 HIGH 계약).

#include <JKLibraryCatalog.h>

#include <JKJkxFile.h>

#include <quickjs.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace jk {

namespace {

// manifest.json → 바이트(NUL 포함, 뒤 1바이트 제외 전승은 파싱 몫). 런처
// JKDesktopShell.cpp ReadFileBytes / JKTerminalConfig.cpp 리더 선례 그대로 —
// JS_ParseJSON은 buf[buf_len] == '\0'를 요구한다(docs/27 lesson 3).
constexpr size_t kMaxManifestBytes = 1 << 20;  // 1 MiB — manifest.json 상한

bool ReadManifestBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0 || static_cast<size_t>(size) > kMaxManifestBytes) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(size) + 1);  // + NUL: JS_ParseJSON requires
    const size_t read = std::fread(out.data(), 1, static_cast<size_t>(size), f);
    std::fclose(f);
    if (read != static_cast<size_t>(size)) return false;
    out[static_cast<size_t>(size)] = '\0';
    return true;
}

// 이미 채운 엔트리 중 동명 스폰 키 존재. 스캔 순서(jkx→콘솔→내장)가 곧
// 우선순위 — 런처 Init의 hasJkx 람다(JKDesktopShell.cpp:251) 재용.
bool TakenName(const std::vector<LibraryEntry>& out, const std::string& name) {
    for (const auto& e : out) {
        if (e.appName == name) return true;
    }
    return false;
}

} // namespace

int LibraryScan(const std::string& basePath, std::vector<LibraryEntry>& out) {
    const std::string appsDir = basePath + "/apps";

    std::error_code ec;
    if (basePath.empty() || !std::filesystem::exists(appsDir, ec) || ec) {
        return 0;  // apps/ 부재 = 라이브러리 빈 목록이 정당한 상태(오류 전파 없음)
    }

    // ── 1원: .jkx 컨테이너 (런처 ScanJkxApps 규약 이식 — win32 FindFirstFileA
    // 대응은 directory_iterator, TX6 posix 레그 선례). 런처와 달리 조용히
    // 스캔한다 — 수십 개 정상 사이즈에 1행/앱 로그는 소음.
    for (std::filesystem::directory_iterator it(appsDir, ec), end;
         !ec && it != end; it.increment(ec)) {
        const std::filesystem::path p = it->path();
        // 확장자 .jkx만 — 대소문자 무시(NTFS 관측+런처 패턴 "*.jkx" 동일).
        std::string ext = p.extension().string();
        for (char& c : ext) c = static_cast<char>(std::tolower((unsigned char)c));
        if (ext != ".jkx") continue;

        JKJkxFile jkx;
        if (!jkx.Open(p.string())) continue;  // 무효 컨테이너 — 런처 동일 조용
        const JkxManifest& mani = jkx.Manifest();
        if (mani.name.empty()) continue;      // Parse false 계약(name/module 필수)

        LibraryEntry e;
        e.source = LibrarySource::Jkx;
        e.appName = mani.name;
        // 표시명: MANI title 우선, 없으면 스폰 키(런처 툴팁 규약 동일).
        e.title = !mani.title.empty() ? mani.title : mani.name;
        // 능력 원문 대입 — 정규화 금지. 원문이 배지의 원천이다(docs/76: 컴마
        // 목록 원문 보존, 토큰 분해는 소비자 JKScriptHost 몫).
        e.capabilities = mani.capabilities;
        // MANI 원문 전승(스펙 §3 상세 "MANI 원문") — 패키지 내 첫 MANI형 엔트리
        // (TypeForName 규약상 manifest.txt·manifest.json 모두 MANI)의 bytes
        // 그대로. 파싱 몫(JkxManifest)과 별개의 원문 — 정규화·재조립 없음.
        for (size_t mi = 0; mi < jkx.Entries().size(); ++mi) {
            if (std::strcmp(jkx.Entries()[mi].type, "MANI") != 0) continue;
            std::vector<uint8_t> raw;
            if (jkx.ReadEntry(static_cast<int>(mi), raw)) {
                e.manifestRaw.assign(raw.begin(), raw.end());
            }
            break;  // 첫 MANI형 엔트리가 곧 진실원 — 컨테이너다 매니페스트 1개
        }
        // ICON 유무: 런처 wanted 산식 재용(2x 우선) — 유무 판정이라 스케일 없음.
        const std::string wanted = !mani.icon2x.empty() ? mani.icon2x : mani.icon;
        e.hasIcon = !wanted.empty() && jkx.FindEntry("ICON", wanted) >= 0;
        e.path = p.string();
        std::error_code fsEc;
        const auto sz = std::filesystem::file_size(p, fsEc);
        if (!fsEc) e.sizeBytes = static_cast<long long>(sz);  // 실패 0 — 치명 아님
        out.push_back(std::move(e));
    }

    // ── 2원: 콘솔 앱 (ScanConsoleApps :423 규약 본사 복사 — apps/<dir>/
    // manifest.json(name/cmd/desc), throwaway quickjs 런타임).
    for (std::filesystem::directory_iterator it(appsDir, ec), end;
         !ec && it != end; it.increment(ec)) {
        std::error_code dirEc;
        if (!it->is_directory(dirEc) || dirEc) continue;  // 스캐터 파일(.jkx 등) 스킵
        const std::string dirName = it->path().filename().string();
        if (dirName.empty() || dirName[0] == '.') continue;  // 숨김 dir 스킵(런처 동일)

        const std::string manifestPath = it->path().string() + "/manifest.json";
        std::vector<uint8_t> bytes;
        if (!ReadManifestBytes(manifestPath, bytes)) continue;  // 무매니페스트=앱 아님

        // Throwaway runtime — parse + field extraction, then it's gone.
        // (ScanConsoleApps :448-479 본사 복사 — getString 람다 포함.)
        JSRuntime* rt = JS_NewRuntime();
        if (!rt) continue;
        JSContext* ctx = JS_NewContext(rt);
        if (!ctx) {
            JS_FreeRuntime(rt);
            continue;
        }
        JSValue root = JS_ParseJSON(ctx, reinterpret_cast<const char*>(bytes.data()),
                                    bytes.size() - 1, "manifest.json");
        std::string name, cmd, desc;
        if (!JS_IsException(root) && JS_IsObject(root)) {
            auto getString = [&](const char* key, std::string* field) {
                JSValue v = JS_GetPropertyStr(ctx, root, key);
                if (JS_IsString(v)) {
                    size_t len = 0;
                    const char* s = JS_ToCStringLen(ctx, &len, v);
                    if (s) {
                        *field = std::string(s, len);
                        JS_FreeCString(ctx, s);
                    }
                }
                JS_FreeValue(ctx, v);
            };
            getString("name", &name);
            getString("cmd", &cmd);
            getString("desc", &desc);
        }
        JS_FreeValue(ctx, root);
        if (ctx) JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        if (name.empty() || cmd.empty()) continue;

        // .jkx 우선: 같은 이름의 컨테이너가 이미 있으면 매니페스트 앱은 스킵
        // (런처 동일 문구 — 로그 행까지).
        if (TakenName(out, name)) {
            std::fprintf(stderr, "JKLibraryCatalog: console app '%s' skipped (.jkx wins)\n",
                         name.c_str());
            continue;
        }

        LibraryEntry e;
        e.source = LibrarySource::Console;
        // 런치 계약 복원(final review Item 1 — 룰링 확정): 콘솔 엔트리 appName =
        // "terminal:" + cmd. launch_app의 존재 검증(JKWindowServer)은
        // jkapp_<app> 모듈 파일을 요구하므로 manifest 이름을 그대로 보내면
        // unknown_app로 죽는다 — 접두 면제 경로(launch_app 규약, 내장 lf/hx와
        // 동일 계약)로만 콘솔 앱이 스폰된다. 이름은 동명 스킵 판정 위에서만 쓴다.
        e.appName = "terminal:" + cmd;
        // 표시명: manifest.json desc가 있으면 그걸 쓴다(이름보다 정보량 — 런처
        // 규약), 없으면 스폰 키 폴백.
        e.title = !desc.empty() ? desc : name;
        e.capabilities = "";  // 콘솔 매니페스트엔 능력 선언이 없다(""=선언 없음 배지)
        e.path = it->path().string();  // 콘솔 dir 절대 경로
        // manifest.json 원문 그대로(스펙 §3 상세) — bytes의 끝 1바이트는 파싱용
        // NUL이므로 제외 전승(ReadManifestBytes 계약).
        e.manifestRaw.assign(bytes.begin(), bytes.end() - 1);
        out.push_back(std::move(e));
        // 콘솔 스캔만 stderr 1행/앱(launch_app 존재 검증 디버그 도움).
        std::fprintf(stderr, "JKLibraryCatalog: console app '%s' (cmd='%s', dir='%s')\n",
                     name.c_str(), cmd.c_str(), dirName.c_str());
    }

    // ── 3원: 내장 (런처 Init 폴백 선례). minesweeper·tetris·chat는 늘 후보 —
    // 파일 존재로 게이트하지 않는다(lf/hx와의 차이). 단 설치앱(.jkx·콘솔)에
    // 동명이 이미 있으면 스킵 — 내장은 "폴백 아이콘"(스펙 §2-3)이므로 동명
    // 설치앱이 이기면 내장 자리가 없다(런처 hasJkx 규약 동일).
    auto pushBuiltin = [&](const char* appName, const char* title) {
        if (TakenName(out, appName)) return;
        LibraryEntry e;
        e.source = LibrarySource::Builtin;
        e.appName = appName;
        e.title = title;
        out.push_back(std::move(e));
    };
    pushBuiltin("minesweeper", "Minesweeper");
    pushBuiltin("tetris", "Tetris");
    pushBuiltin("chat", "Chat");   // 스펙 2026-10-07-desktop-chat-app — T2

    // terminal: lf/hx — 파일 존재 시만(폰 기본값=부재 제외, 조용히). 접미는
    // 플랫폼 차: win32 .exe / posix 무접미.
#ifdef _WIN32
    const std::string lfTarget = basePath + "/apps-bin/lf/lf.exe";
    const std::string hxTarget = basePath + "/apps-bin/helix/hx.exe";
#else
    const std::string lfTarget = basePath + "/apps-bin/lf/lf";
    const std::string hxTarget = basePath + "/apps-bin/helix/hx";
#endif
    const std::string targets[2] = {lfTarget, hxTarget};
    for (const std::string& target : targets) {
        std::error_code fileEc;
        if (!std::filesystem::exists(target, fileEc) || fileEc) continue;
        // appName = "terminal:" + basePath 하위 상대경로(슬래시 정규화 — 폰·
        // WSL 표기 일치). 런처 폴백 4종(JKDesktopShell.cpp:257-284)의 캡처:
        // "terminal:apps-bin/lf/lf.exe"·"terminal:apps-bin/helix/hx.exe".
        std::string rel = target;
        if (rel.rfind(basePath, 0) == 0) rel.erase(0, basePath.size());
        for (char& c : rel) {
            if (c == '\\') c = '/';
        }
        if (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
        const std::string termKey = "terminal:" + rel;
        if (TakenName(out, termKey)) continue;  // 동명 설치앱 우선(런처 hasJkx 동일)

        LibraryEntry e;
        e.source = LibrarySource::Builtin;
        e.appName = termKey;
        // 표시명 = exe 이름(접두어·경로 제거 — 런처 툴팁 규약): lf / hx.
        e.title = std::filesystem::path(target).stem().string();
        e.capabilities = "";  // 내장 = 선언 없음(숨기지 않는다 — 배지 계약)
        e.path = "";          // Builtin 엔트리 path=""
        out.push_back(std::move(e));
    }

    return static_cast<int>(out.size());
}

} // namespace jk
