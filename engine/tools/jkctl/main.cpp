// jkctl — P4 SDK 앱→에이전트 원컷 CLI (specs/2026-09-16-p4-sdk-contract §4).
// notify/agent: 서버 도구 쿼리(JKAgentClient control-client, 권한은 서버
// 파이프라인). ask: 로컬 LLM 원컷 — jkchat의 claude 래퍼 관례
// (state\chat.json 설정 재사용, claude_wrapper guide §2.2).
// wmain + CreateProcessW: docs/48 CP949 argv 레슨 — UTF-8 프롬프트를
// cmd.exe std::system으로 넘기면 인코딩이 파손되므로 유니코드 경로로 간다.
#include <agent/JKAgentClient.h>

#include <windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string ExeDirA() {
    char path[1024] = {};
    GetModuleFileNameA(nullptr, path, sizeof(path));
    const std::string full(path);
    const size_t slash = full.find_last_of("\\/");
    return (slash == std::string::npos) ? std::string(".\\") : full.substr(0, slash + 1);
}

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring out(n > 0 ? n - 1 : 0, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], n);
    return out;
}

// chat.json에서 model 읽기 (jkchat LoadChatConfig의 최소형 — 없으면 기본값).
std::string LoadModel() {
    std::string model = "glm-5.3-flash:cloud";
    FILE* f = nullptr;
    if (fopen_s(&f, (ExeDirA() + "state\\chat.json").c_str(), "rb") != 0 || !f)
        return model;
    char buf[4096] = {};
    const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    const char* key = std::strstr(buf, "\"model\"");
    if (key && (key = std::strchr(key + 7, '"'))) {
        const char* end = std::strchr(key + 1, '"');
        if (end) model.assign(key + 1, static_cast<size_t>(end - key - 1));
    }
    return model;
}

int RunServerQuery(const char* tool, const std::string& argsJson) {
    jk::agent::JKAgentClient c;
    if (!c.Connect()) {
        std::fprintf(stderr, "jkctl: window server not running\n");
        return 1;
    }
    std::string reply;
    if (!c.Query("publish_event", argsJson, reply)) {
        std::fprintf(stderr, "jkctl: query failed: %s\n", reply.c_str());
        return 1;
    }
    return 0;
}

// notify — 데스크탑 알림. jktriggers와 같은 agent.notify 이벤트 경로
// (서버 publish_event가 스탬프를 찍고 jkapp_notify가 띄운다).
int Notify(const char* text) {
    std::string body;
    for (const char* p = text; *p; ++p) {
        if (*p == '"' || *p == '\\') body += '\\';
        body += *p;
    }
    const std::string args =
        "{\"topic\":\"agent.notify\",\"data\":{\"title\":\"jkctl\",\"body\":\"" +
        body + "\"}}";
    return RunServerQuery("publish_event", args);
}

// agent — 원 요청 JSON 통과 (agentctl과 같은 passthrough 형식).
int Agent(const char* requestJson) {
    jk::agent::JKAgentClient c;
    if (!c.Connect()) {
        std::fprintf(stderr, "jkctl: window server not running\n");
        return 1;
    }
    std::string reply;
    if (!c.QueryRaw(requestJson, reply)) {
        std::fprintf(stderr, "jkctl: query failed: %s\n", reply.c_str());
        return 1;
    }
    std::printf("%s\n", reply.c_str());
    return 0;
}

// ask — 로컬 LLM 원컷: 응답이 jkctl의 stdout으로 통과한다(동기 원컷, §4).
// 인용 이스케이프는 jkchat BuildEngineCmd와 동일(따옴표만) — claude CLI 인자
// 경로가 나머지 문자를 그대로 전달한다.
int Ask(const char* question) {
    std::string esc;
    for (const char* p = question; *p; ++p) {
        if (*p == '"') esc += "\\\"";
        else esc += *p;
    }
    const std::string prompt = "-p \"" + esc + "\"";
    const std::string cmdA = "ollama launch claude --model \"" + LoadModel() +
                             "\" -- " + prompt;
    std::wstring cmd = Utf8ToWide(cmdA);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, cmd.empty() ? nullptr : cmd.data(), nullptr,
                        nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        std::fprintf(stderr, "jkctl: LLM launch failed (err=%lu) — ollama/claude CLI 확인\n",
                     GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (code == 0) ? 0 : 1;
}

// init — docs/51 C 후보 "템플릿 생성기": templates/console-app을 <name>\로
// 복사하고 "myapp" 토큰을 치환한다. 템플릿 위치는 <exeDir>\templates\
// console-app (build_with_temp.sh가 런타임 루트로 동기화).
int Init(const std::string& name) {
    if (name.empty() || name.size() > 64) {
        std::fprintf(stderr, "init: name must be 1..64 chars\n");
        return 2;
    }
    for (char c : name) {
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) {
            std::fprintf(stderr, "init: name may contain [A-Za-z0-9_-] only\n");
            return 2;
        }
    }
    const std::string tplDir = ExeDirA() + "templates\\console-app";
    const char* files[] = { "manifest.json", "README.md", "main.cmd" };
    // Template existence check BEFORE creating the target dir — a missing
    // template otherwise leaves a stray empty <name>\ in the cwd.
    for (const char* f : files) {
        if (!std::filesystem::exists(tplDir + "\\" + f)) {
            std::fprintf(stderr, "init: template not found: %s\\%s\n",
                         tplDir.c_str(), f);
            return 2;
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(name, ec);
    if (ec) {
        std::fprintf(stderr, "init: cannot create %s (%s)\n", name.c_str(),
                     ec.message().c_str());
        return 2;
    }
    for (const char* f : files) {
        std::ifstream in(tplDir + "\\" + f, std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "init: template not found: %s\\%s\n",
                         tplDir.c_str(), f);
            return 2;
        }
        std::string body((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
        // 토큰 치환 — 템플릿은 "myapp" 이름으로 배포된다.
        const std::string token = "myapp";
        for (size_t p = body.find(token); p != std::string::npos;
             p = body.find(token, p + name.size()))
            body.replace(p, token.size(), name);
        std::ofstream out(std::string(name) + "\\" + f, std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "init: cannot write %s\\%s\n", name.c_str(),
                         f);
            return 2;
        }
        out << body;
        if (!out) {
            std::fprintf(stderr, "init: write failed: %s\\%s\n", name.c_str(),
                         f);
            return 2;
        }
    }
    std::printf("created %s\\ (manifest.json, README.md, main.cmd)\n"
                "install: jkctl install %s   then restart the desktop\n",
                name.c_str(), name.c_str());
    return 0;
}

// install — docs/51 C 후보 "패키지 매니저" MVP: 콘솔 앱 폴더(manifest.json
// 포함)를 <exeDir>\apps\<name>\로 복사한다. zip 배포 + trust 지문 검증
// 설치는 C 후보 잔여. 서버 스캔은 시작 시 1회라 재시작이 필요하다 —
// 지문은 스캔 시점에 EnsureTrustRecord가 남긴다(docs/51 §3.4).
int Install(const std::string& folder) {
    std::ifstream in(folder + "\\manifest.json", std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "install: not a console app (missing %s\\manifest.json)\n",
                     folder.c_str());
        return 2;
    }
    std::string body((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    // "name" 추출 — 서버/LoadModel과 같은 문자열 스캔 관용구.
    const size_t k = body.find("\"name\"");
    if (k == std::string::npos) {
        std::fprintf(stderr, "install: manifest has no \"name\"\n");
        return 2;
    }
    const size_t colon = body.find(':', k);
    const size_t q1 = body.find('"', colon);
    const size_t q2 = (q1 == std::string::npos) ? std::string::npos
                                                : body.find('"', q1 + 1);
    if (q1 == std::string::npos || q2 == std::string::npos) {
        std::fprintf(stderr, "install: bad manifest name\n");
        return 2;
    }
    const std::string name = body.substr(q1 + 1, q2 - q1 - 1);
    // Same validation as init — the server scan keys on this name, so a
    // loose name here becomes a launcher key (and Windows reserves CON/NUL).
    bool nameOk = !name.empty() && name.size() <= 64;
    for (char c : name) {
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) {
            nameOk = false;
            break;
        }
    }
    if (!nameOk) {
        std::fprintf(stderr, "install: bad name '%s' ([A-Za-z0-9_-] 1..64)\n",
                     name.c_str());
        return 2;
    }
    const std::string dst = ExeDirA() + "apps\\" + name;
    std::error_code ec;
    if (std::filesystem::exists(dst)) {
        std::fprintf(stderr, "install: already exists: %s (remove it first)\n",
                     dst.c_str());
        return 2;
    }
    std::filesystem::create_directories(ExeDirA() + "apps", ec);
    std::filesystem::copy(folder, dst,
                          std::filesystem::copy_options::recursive, ec);
    if (ec) {
        std::fprintf(stderr, "install: copy failed (%s)\n", ec.message().c_str());
        return 2;
    }
    std::printf("installed %s -> %s\n"
                "restart the desktop to scan it into the launcher\n",
                folder.c_str(), dst.c_str());
    return 0;
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: jkctl notify \"<msg>\" | agent '<json>' | ask \"<question>\"\n"
                     "       jkctl init \"<name>\" | install \"<folder>\"\n");
        return 2;
    }
    // argv를 UTF-8로 정규화 (docs/48 레슨 — 이후 모든 처리는 UTF-8).
    char a1[512] = {};
    char a2[4096] = {};
    WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, a1, sizeof(a1), nullptr, nullptr);
    WideCharToMultiByte(CP_UTF8, 0, argv[2], -1, a2, sizeof(a2), nullptr, nullptr);

    const std::string sub = a1;
    if (sub == "notify") return Notify(a2);
    if (sub == "agent") return Agent(a2);
    if (sub == "ask") return Ask(a2);
    if (sub == "init") return Init(a2);
    if (sub == "install") return Install(a2);
    std::fprintf(stderr, "unknown subcommand: %s\n", a1);
    return 2;
}