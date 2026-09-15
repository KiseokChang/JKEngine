// jkctl — P4 SDK 앱→에이전트 원컷 CLI (specs/2026-09-16-p4-sdk-contract §4).
// notify/agent: 서버 도구 쿼리(JKAgentClient control-client, 권한은 서버
// 파이프라인). ask: 로컬 LLM 원컷 — jkchat의 claude 래퍼 관례
// (state\chat.json 설정 재사용, claude_wrapper guide §2.2).
// wmain + CreateProcessW: docs/48 CP949 argv 레슨 — UTF-8 프롬프트를
// cmd.exe std::system으로 넘기면 인코딩이 파손되므로 유니코드 경로로 간다.
#include <agent/JKAgentClient.h>

#include <windows.h>

#include <cstdio>
#include <cstring>
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

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: jkctl notify \"<msg>\" | agent '<json>' | ask \"<question>\"\n");
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
    std::fprintf(stderr, "unknown subcommand: %s\n", a1);
    return 2;
}