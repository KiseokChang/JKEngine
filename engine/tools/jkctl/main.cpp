// jkctl — P4 SDK 앱→에이전트 원컷 CLI (specs/2026-09-16-p4-sdk-contract §4).
// notify/agent: 서버 도구 쿼리(JKAgentClient control-client, 권한은 서버
// 파이프라인). ask: 로컬 LLM 원컷 — jkchat의 claude 래퍼 관례
// (state\chat.json 설정 재사용, claude_wrapper guide §2.2).
// pack/install: docs/51 C 후보 패키지 매니저 — zip 배포(miniz 벤더링) +
// 설치 시 trust 지문 선기록(docs/51 §3.4, EnsureTrustRecord와 동일 규약).
// wmain + CreateProcessW: docs/48 CP949 argv 레슨 — UTF-8 프롬프트를
// cmd.exe std::system으로 넘기면 인코딩이 파손되므로 유니코드 경로로 간다.
#include <agent/JKAgentClient.h>
#include <miniz.h>
#include <miniz_zip.h>

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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
// --attach는 파일 본문을 프롬프트에 텍스트 블록으로 첨부한다(docs/51 C 후보
// 잔여 — 스펙 §4 예제). 텍스트 전용: NUL 포함은 거부(경로를 프롬프트에
// 넣는 구안 유지). 16KiB 절단(UTF-8 경계 보정). CP949 파일은 ACP 경유
// 재인코딩(docs/48 레슨). CreateProcessW cmdLine 32767 한계 — 최종 길이
// 검사로 조용한 잘림을 막는다(publish_event의 이스케이프 후 크기 검사 선례).
struct AskRequest {
    std::string question;
    std::vector<std::string> attaches;
};

int Ask(const AskRequest& req) {
    if (req.question.empty()) {
        std::fprintf(stderr, "ask: empty question\n");
        return 2;
    }
    std::string prompt = req.question;
    for (const std::string& p : req.attaches) {
        std::ifstream in(p, std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "ask: cannot read attachment: %s\n", p.c_str());
            return 2;
        }
        std::string raw((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
        if (raw.size() > 16 * 1024) {
            // UTF-8 연속 바이트(0x80-0xBF)에서 물러나 잘라낸다 — 절단이
            // 멀티바이트 문자 중간에 끊기면 모델 입력에 FFFD 파손이 온다.
            size_t cut = 16 * 1024;
            while (cut > 0 && (static_cast<unsigned char>(raw[cut]) & 0xC0) == 0x80)
                --cut;
            raw.resize(cut);
        }
        if (raw.find('\0') != std::string::npos) {
            std::fprintf(stderr, "ask: binary attachment not supported: %s "
                                 "(pass the path in the prompt instead)\n",
                         p.c_str());
            return 2;
        }
        // UTF-8 검증 실패 = ANSI/CP949 텍스트일 확률 — ACP 경유 정규화.
        {
            const int wlen = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, raw.c_str(),
                static_cast<int>(raw.size()), nullptr, 0);
            if (wlen == 0 && !raw.empty()) {
                const int wlenA = MultiByteToWideChar(
                    CP_ACP, 0, raw.c_str(), static_cast<int>(raw.size()),
                    nullptr, 0);
                std::wstring w(wlenA > 0 ? wlenA : 0, L'\0');
                if (wlenA > 0)
                    MultiByteToWideChar(CP_ACP, 0, raw.c_str(),
                                        static_cast<int>(raw.size()), &w[0],
                                        wlenA);
                const int u8len = WideCharToMultiByte(
                    CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string u8(u8len > 0 ? u8len : 0, '\0');
                if (u8len > 0)
                    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &u8[0],
                                        u8len, nullptr, nullptr);
                raw = u8;
            }
        }
        std::string base = p;
        const size_t slash = base.find_last_of("\\/");
        if (slash != std::string::npos) base = base.substr(slash + 1);
        prompt += "\n\n--- attached file: " + base + " ---\n" + raw +
                  "\n--- end of " + base + " ---";
    }
    std::string esc;
    for (const char c : prompt) {
        if (c == '"') esc += "\\\"";
        else if (c == '\n') esc += "\\n";
        else if (c == '\r') esc += "\\r";
        else esc += c;
    }
    const std::string cmdA = "ollama launch claude --model \"" + LoadModel() +
                             "\" -- -p \"" + esc + "\"";
    std::wstring cmd = Utf8ToWide(cmdA);
    // CreateProcessW cmdLine 상한 32767 wchar — 초과 시 CreateProcess 실패
    // 원인을 알기 어렵다. 30000 여유로 미리 거부.
    if (cmd.size() > 30000) {
        std::fprintf(stderr,
                     "ask: prompt too large (%zu chars) — reduce attachments\n",
                     cmd.size());
        return 2;
    }

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

// --- 패키지 매니저 공용 (docs/51 C 후보 잔여: zip 배포 + trust 선기록) ---

// bcrypt SHA-256 → "sha256:"+64hex (docs/51 §3.4 ConsoleAppFingerprint와
// 동일 형식·동일 CNG 구현 — 서버 EnsureTrustRecord가 남기는 지문과
// 바이트 단위로 일치해야 한다). 빈 입력/실패 시 "" 반환.
std::string Sha256Hex(const std::string& data) {
    void* alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, L"SHA256", nullptr, 0) != 0) return "";
    void* h = nullptr;
    uint8_t digest[32] = {};
    bool ok = BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0;
    if (ok && !data.empty())
        ok = BCryptHashData(h, (unsigned char*)data.data(),
                            (unsigned long)data.size(), 0) == 0;
    if (ok) ok = BCryptFinishHash(h, digest, sizeof(digest), 0) == 0;
    if (h) BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) return "";
    static const char* kHex = "0123456789abcdef";
    std::string out = "sha256:";
    for (uint8_t b : digest) {
        out += kHex[b >> 4];
        out += kHex[b & 0xf];
    }
    return out;
}

// manifest.json에서 "key":"value" 추출 — install MVP의 문자열 스캔 관용구를
// 공용화. 못 찾으면 "".
std::string ManifestString(const std::string& body, const char* key) {
    const std::string pat = std::string("\"") + key + "\"";
    const size_t k = body.find(pat);
    if (k == std::string::npos) return "";
    const size_t colon = body.find(':', k + pat.size());
    const size_t q1 = body.find('"', colon);
    const size_t q2 = (q1 == std::string::npos) ? std::string::npos
                                                : body.find('"', q1 + 1);
    if (colon == std::string::npos || q1 == std::string::npos ||
        q2 == std::string::npos)
        return "";
    return body.substr(q1 + 1, q2 - q1 - 1);
}

bool ValidAppName(const std::string& name) {
    if (name.empty() || name.size() > 64) return false;
    for (char c : name) {
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_'))
            return false;
    }
    return true;
}

// 설치 시 trust 지문 선기록 (docs/51 §3.4): 서버 재시작 전에도 첫 스폰이
// 신뢰 상태로 시작한다. 서버 EnsureTrustRecord와 동일 규약 — 이미 같은
// 지문이 있으면 파일을 건드리지 않고, 파손된 스토어는 절대 덮어쓰지
// 않는다(기록 보존 우선). 파싱 없이 "records":[" 뒤에 레코드를 스플라이스
// 한다(jkctl은 quickjs에 링크하지 않는다 — 서브스트링 규약으로 충분).
void TrustPreRecord(const std::string& fingerprint, const std::string& name) {
    if (fingerprint.empty()) return;
    char exePath[1024] = {};
    if (!GetModuleFileNameA(nullptr, exePath, sizeof(exePath))) return;
    std::string exeDir = exePath;
    const size_t slash = exeDir.find_last_of("\\/");
    if (slash == std::string::npos) return;
    exeDir.resize(slash);
    CreateDirectoryA((exeDir + "\\state").c_str(), nullptr);
    const std::string path = exeDir + "\\state\\trust.json";
    const std::string fpKey = "\"fingerprint\":\"" + fingerprint + "\"";

    std::string body;
    bool have = false;
    {
        std::ifstream in(path, std::ios::binary);
        if (in) {
            body.assign((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
            have = true;
        }
    }
    const uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
    const std::string rec = "{\"fingerprint\":\"" + fingerprint +
                            "\",\"name\":\"" + name +
                            "\",\"source\":\"user\",\"ts\":" +
                            std::to_string(nowMs) + "}";

    if (have) {
        if (body.find(fpKey) != std::string::npos)
            return;  // 이미 기록된 지문 — 스토어 무변경 (서버 규약 동일).
        const size_t arr = body.find("\"records\":[");
        if (arr == std::string::npos) {
            std::fprintf(stderr,
                         "jkctl: trust.json unrecognizable — skipped pre-record "
                         "(server startup scan will record it)\n");
            return;
        }
        // "records":[ 바로 뒤에 스플라이스 — 기존 레코드는 전부 보존된다.
        // 빈 배열 뒤엔 쉼표 없이, 아니면 rec 뒤에 쉼표를 붙인다(스플라이스
        // 위치가 배열 머리이므로 뒤쪽 경계가 언제나 새 레코드와 기존 레코드
        // 사이). 쉼표 누락 시 store 전체가 파싱 불능 — fail-closed 서버가
        // 절대 고치지 않으므로 여기서 반드시 올바른 JSON을 쓴다.
        const size_t insert = arr + strlen("\"records\":[");
        size_t p = insert;
        while (p < body.size() &&
               (body[p] == ' ' || body[p] == '\t' || body[p] == '\n' ||
                body[p] == '\r'))
            ++p;
        body.insert(insert, p < body.size() && body[p] == ']' ? rec : rec + ",");
    } else {
        body = "{\"records\":[" + rec + "]}";
    }
    FILE* wf = nullptr;
    if (fopen_s(&wf, path.c_str(), "wb") != 0 || !wf) {
        std::fprintf(stderr, "jkctl: cannot write trust.json — skipped pre-record\n");
        return;
    }
    std::fwrite(body.data(), 1, body.size(), wf);
    std::fclose(wf);
    std::printf("trust fingerprint pre-recorded (%s, '%s')\n",
                fingerprint.substr(0, 15).c_str(), name.c_str());
}
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

// 설치 공용 꼬리: 스테이지 디렉터리의 manifest를 검증 → trust 선기록 →
// apps\<name>\ 복사. fromZip이면 복사 후 임시 언팩 정리. label은 사용자
// 안내에 표시할 원본 경로(zip 설치 시 임시 언팩 경로 대신 원본을 보여준다).
int InstallFromDir(const std::string& staged, bool fromZip,
                   const std::string& label) {
    std::ifstream in(staged + "\\manifest.json", std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "install: not a console app (missing %s\\manifest.json)\n",
                     staged.c_str());
        return 2;
    }
    // 서버 스캔의 kMaxManifestBytes(1 MiB — JKDesktopShell)와 동일 상한 —
    // 서버가 거부할 초대형 매니페스트를 설치해 고아 디렉터리를 만들지 않는다.
    in.seekg(0, std::ios::end);
    if (in.tellg() > (1u << 20)) {
        std::fprintf(stderr, "install: manifest.json too large (>1 MiB)\n");
        return 2;
    }
    in.seekg(0, std::ios::beg);
    std::string body((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    const std::string name = ManifestString(body, "name");
    if (!ValidAppName(name)) {
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
    // copy (rename 아님): zip 언팩 직후 rename은 AV 스캔 공유 위반
    // (Permission denied)에 걸릴 수 있다 — 실측. 임시 언팩 정리는 꼬리에서.
    std::filesystem::copy(staged, dst,
                          std::filesystem::copy_options::recursive, ec);
    if (ec) {
        std::fprintf(stderr, "install: copy failed (%s)\n", ec.message().c_str());
        return 2;
    }
    // trust 지문 선기록 — manifest cmd의 SHA-256 (docs/51 §3.4와 동일
    // 원문: 서버는 설치된 manifest.json의 cmd 값을 그대로 해시한다).
    TrustPreRecord(Sha256Hex(ManifestString(body, "cmd")), name);
    if (fromZip) {
        // 임시 언팩 정리 — 방금 쓴 파일은 AV 스캔 락에 걸릴 수 있으므로
        // 3회 재시도(300ms 간격). 실패해도 설치 자체는 성공이므로 경고만.
        bool cleaned = false;
        for (int attempt = 0; attempt < 3 && !cleaned; ++attempt) {
            ec.clear();
            std::filesystem::remove_all(staged, ec);
            cleaned = !ec;
            if (!cleaned) Sleep(300);
        }
        if (!cleaned)
            std::fprintf(stderr,
                         "install: warning: staging %s left behind (AV scan "
                         "lock — safe to delete manually)\n",
                         staged.c_str());
    }
    std::printf("installed %s -> %s\n"
                "restart the desktop to scan it into the launcher\n",
                label.c_str(), dst.c_str());
    return 0;
}

// pack — docs/51 C 후보 "패키지 매니저": 콘솔 앱 폴더를 <name>.zip으로
// 배포 포장한다 (deflate — miniz 벤더링). 아카이브 경로는 폴더 상대경로에
// '/' 구분자 통일(zip 관례, 대상 OS 무관).
int Pack(const std::string& folder) {
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) {
        std::fprintf(stderr, "pack: not a directory: %s\n", folder.c_str());
        return 2;
    }
    std::ifstream in(folder + "\\manifest.json", std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "pack: not a console app (missing %s\\manifest.json)\n",
                     folder.c_str());
        return 2;
    }
    std::string body((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    const std::string name = ManifestString(body, "name");
    if (!ValidAppName(name)) {
        std::fprintf(stderr, "pack: bad name '%s' in manifest ([A-Za-z0-9_-] 1..64)\n",
                     name.c_str());
        return 2;
    }
    const std::string zipPath = name + ".zip";

    std::vector<std::string> files;
    for (auto it = std::filesystem::recursive_directory_iterator(
             folder, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_regular_file(ec)) files.push_back(it->path().string());
    }
    if (ec) {
        std::fprintf(stderr, "pack: walk failed (%s)\n", ec.message().c_str());
        return 2;
    }

    mz_zip_archive za = {};
    if (!mz_zip_writer_init_file(&za, zipPath.c_str(), 0)) {
        std::fprintf(stderr, "pack: cannot create %s\n", zipPath.c_str());
        return 2;
    }
    bool ok = true;
    std::string prefix = std::filesystem::absolute(folder, ec).string();
    std::replace(prefix.begin(), prefix.end(), '\\', '/');
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';
    for (const std::string& f : files) {
        std::string abs = std::filesystem::absolute(f, ec).string();
        if (abs.size() <= prefix.size()) continue;
        std::string arc = abs.substr(prefix.size());
        std::replace(arc.begin(), arc.end(), '\\', '/');
        if (!mz_zip_writer_add_file(&za, arc.c_str(), f.c_str(), nullptr, 0,
                                    MZ_DEFAULT_LEVEL)) {
            std::fprintf(stderr, "pack: add failed: %s\n", f.c_str());
            ok = false;
            break;
        }
    }
    if (ok) ok = mz_zip_writer_finalize_archive(&za);
    mz_zip_writer_end(&za);
    if (!ok) {
        std::fprintf(stderr, "pack: finalize failed\n");
        std::remove(zipPath.c_str());
        return 2;
    }
    std::printf("packed %s (%zu file(s))\n", zipPath.c_str(), files.size());
    return 0;
}

// install — 배포본(zip) 또는 폴더를 apps\<name>\로 설치한다. zip은 zip-slip
// 방어(절대경로/.. 구성요소 거부) 후 임시 폴더에 언팩 → 공용 설치 경로.
int Install(const std::string& path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec))
        return InstallFromDir(path, false, path);
    if (path.size() < 4 || path.substr(path.size() - 4) != ".zip") {
        std::fprintf(stderr, "install: pass a console app folder or a .zip package\n");
        return 2;
    }

    mz_zip_archive za = {};
    if (!mz_zip_reader_init_file(&za, path.c_str(), 0)) {
        std::fprintf(stderr, "install: cannot open zip: %s\n", path.c_str());
        return 2;
    }
    // 임시 언팩 디렉터리 — <exeDir>\tmp\install_<zip basename>
    std::string base = std::filesystem::path(path).filename().string();
    const size_t dot = base.size() >= 4 ? base.size() - 4 : 0;
    std::string tmp = ExeDirA() + "tmp\\install_" + base.substr(0, dot);
    std::filesystem::remove_all(tmp, ec);  // 이전 실패 잔여 제거
    std::filesystem::create_directories(tmp, ec);
    if (ec) {
        std::fprintf(stderr, "install: cannot stage %s (%s)\n", tmp.c_str(),
                     ec.message().c_str());
        mz_zip_reader_end(&za);
        return 2;
    }
    const int n = (int)mz_zip_reader_get_num_files(&za);
    int rc = 0;
    for (int i = 0; i < n; ++i) {
        mz_zip_archive_file_stat st = {};
        if (!mz_zip_reader_file_stat(&za, i, &st)) continue;
        const std::string nm = st.m_filename;
        // zip-slip 방어: 절대경로/드라이브/.. 구성요소는 설치 거부. Win32는
        // '\'도 구분자로 처리하므로 raw entry name(ZIP 사양상 '\' 허용)의
        // .. 구성요소 검사는 양쪽 구분자를 모두 본다 — 그렇지 않으면
        // "..\..\x" 항목이 스테이징 밖에 적재된다(리뷰 MAJOR 실증).
        bool unsafe = nm.empty() || nm[0] == '/' || nm[0] == '\\' ||
                      (nm.size() >= 2 && nm[1] == ':');
        for (size_t p = 0; !unsafe && p + 1 < nm.size(); ++p) {
            if (nm[p] == '.' && nm[p + 1] == '.' &&
                (p == 0 || nm[p - 1] == '/' || nm[p - 1] == '\\') &&
                (p + 2 >= nm.size() || nm[p + 2] == '/' ||
                 nm[p + 2] == '\\'))
                unsafe = true;
        }
        if (unsafe) {
            std::fprintf(stderr, "install: unsafe archive path: %s\n", nm.c_str());
            rc = 2;
            break;
        }
        std::string outPath = tmp + "\\" + nm;
        std::replace(outPath.begin(), outPath.end(), '/', '\\');
        if (st.m_is_directory) {
            std::filesystem::create_directories(outPath, ec);
            continue;
        }
        // 엔트리의 상위 디렉터리가 zip에 별도 행으로 없을 수 있다 — 보장.
        std::filesystem::create_directories(
            std::filesystem::path(outPath).parent_path(), ec);
        if (!mz_zip_reader_extract_to_file(&za, i, outPath.c_str(), 0)) {
            std::fprintf(stderr, "install: extract failed: %s\n", nm.c_str());
            rc = 2;
            break;
        }
    }
    mz_zip_reader_end(&za);
    if (rc != 0) {
        std::filesystem::remove_all(tmp, ec);
        return rc;
    }
    const int irc = InstallFromDir(tmp, true, path);
    if (irc != 0) {
        // 거부(중복/무효 매니페스트) 시에도 스테이징 잔여를 남기지 않는다
        // (언팩 페이로드가 tmp에 남는 것을 막음 — 리뷰 MINOR).
        std::error_code cec;
        std::filesystem::remove_all(tmp, cec);
        return irc;
    }
    return irc;
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: jkctl notify \"<msg>\" | agent '<json>' | ask \"<q>\" [--attach <file>]...\n"
                     "       jkctl init \"<name>\" | pack \"<folder>\"\n"
                     "       jkctl install \"<folder-or-package.zip>\"\n");
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
    if (sub == "ask") {
        AskRequest req;
        for (int i = 2; i < argc; ++i) {
            char a8[1024] = {};
            WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, a8, sizeof(a8) - 1,
                                nullptr, nullptr);
            if (std::strcmp(a8, "--attach") == 0) {
                if (i + 1 >= argc) {
                    std::fprintf(stderr, "ask: --attach needs a path\n");
                    return 2;
                }
                char p8[1024] = {};
                WideCharToMultiByte(CP_UTF8, 0, argv[++i], -1, p8,
                                    sizeof(p8) - 1, nullptr, nullptr);
                req.attaches.push_back(p8);
            } else if (req.question.empty()) {
                req.question = a8;
            } else {
                std::fprintf(stderr, "usage: jkctl ask \"<question>\" [--attach <file>]...\n");
                return 2;
            }
        }
        if (req.question.empty()) {
            std::fprintf(stderr, "usage: jkctl ask \"<question>\" [--attach <file>]...\n");
            return 2;
        }
        return Ask(req);
    }
    if (sub == "init") return Init(a2);
    if (sub == "pack") return Pack(a2);
    if (sub == "install") return Install(a2);
    std::fprintf(stderr, "unknown subcommand: %s\n", a1);
    return 2;
}