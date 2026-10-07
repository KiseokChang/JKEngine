// jktalk — 단말 채팅 REPL (스펙 2026-10-07-desktop-chat-app §5 — "X11 밖 입력
// 문"의 입력 문): 폰 Termux 같은 X11 밖 단말에서 사용자가 발화를 타이핑하면
// 이 CLI가 그 발화를 해석해 데스크톱 창 서버에 도구 지시로 번역해 보낸다.
// X11 화면(DeX) 위 jkdesktop이 반응하는 그림 — 이 프로세스 자체는 UI 없이
// stdin/stdout만 가진 콘솔 앱이다(jkctl/jkagentd 선준).
//
// 구성:
//   - 대화 REPL: 프롬프트 "> " 입력 → jk::ChatRouterRoute(T1 stub 라우터)로
//     해석 → 도구 지시는 서버 위임(JKAgentClient — jkctl/jkagentd와 같은
//     {"tool":...,"args":{...}} 질의 형태) → 회신을 그대로 인쇄 → 루프.
//     /exit 또는 EOF(Ctrl+D)로 끝난다. 빈 줄은 건너뛴다.
//   - 원컷/파이프 모드: 첫 인자를 받으면(`jktalk "지뢰찾기 켜줘"` 또는
//     `--ask "..."`) 그 입력 하나만 처리하고 끝난다. stdin이 TTY가 아니면
//     (`echo "..." | ./jktalk`) 줄마다 같은 처리 — 자동화 영수증 경로.
//   - --route 모드: 서버 접속 없이 라우터 판정만 인쇄(진단 영수증 — kind+app).
//     도구 지시를 보내지 않는다.
//
// fail-loud 정직 계약: 서버 파이프가 없으면 한국어 오류를 남기고 nonzero로
// 끝난다(jkctl RunServerQuery의 "window server not running" 형제). 데스크톱
// 서버 기동은 이 CLI의 몫이 아니다(룰링 — 프로브는 서버 기동·종료를 하지
// 않는다).
//
// 백엔드 슬롯(스펙 §4 — 승격 판정=docs/80 §3, stub 유지): 아래 kBackendName
// 주석 자리. 지금은 T1 stub 라우터(jk::ChatRouterRoute)만 꽂는다.
// JKLmEngine은 이 CLI에서 아직 인스턴스화하지 않는다 — cfg 선택형 배선은
// 백로그(동기 브리지·액션 매핑·ollama HTTP 어댑터·chat.json directory).
//
// 인코딩 계약(docs/48 레슨 승계 — jkctl 이중 진입 선준): argv는 wmain이
// UTF-8로 정규화(-municode 링크). stdin은 UTF-8 바이트열이 기본(Git Bash/
// Termux 파이프·mintty). Windows 콘솔 대화형 입력만 CP949로 들어올 수 있어
// JKTALK_INPUT_CP949=1 환경변수로 CP949→UTF-8 변환을 켜는 명시 스위치를 둔다
// (jk::text 어댑터 — 자동 판별은 불가라 명시 스위치로 정직하게).

#include <agent/JKAgentClient.h>
#include <apps/ChatRouter.h>
#include <text/JKTextConv.h>

#ifdef _WIN32
#include <io.h>   // _isatty — stdin tty 판정(CRT 헤더, windows.h 아님)
#else
#include <unistd.h>   // isatty(0)
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

// 발화/앱어의 JSON 문자열 이스케이프 — ClientChatApp::EscapeJson과 동일 규약
// (제어문자는 \uXXXX, 쌍따옴표·역슬래시는 백슬래시 이스케이프).
std::string JsonEsc(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    char num[8];
    for (const char ch : in) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (ch == '"' || ch == '\\') { out += '\\'; out += ch; }
        else if (c < 0x20) {
            std::snprintf(num, sizeof(num), "\\u%04x", c);
            out += num;
        } else {
            out += ch;
        }
    }
    return out;
}

// 발화 앞뒤 공백 절단(ClientChatApp::TrimText 규약) — 라우터가 먹는 것은 내부
// 정규화 결과라 이 절단은 표시·빈 줄 판정 몫.
std::string TrimText(const std::string& raw) {
    static const char* kSpace = " \t\r\n";
    const size_t b = raw.find_first_not_of(kSpace);
    if (b == std::string::npos) return "";
    const size_t e = raw.find_last_not_of(kSpace);
    return raw.substr(b, e - b + 1);
}

// 라우터 Kind의 사람 표기 — --route 진단 인쇄용.
const char* KindName(jk::ChatAction::Kind kind) {
    switch (kind) {
        case jk::ChatAction::Launch:      return "Launch";
        case jk::ChatAction::Close:       return "Close";
        case jk::ChatAction::Focus:       return "Focus";
        case jk::ChatAction::ListWindows: return "ListWindows";
        case jk::ChatAction::Info:        return "Info";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// 백엔드 슬롯 (스펙 2026-10-07-desktop-chat-app §4 — 승격 판정=docs/80 §3)
//
// 현재 백엔드 = T1 stub 라우터(jk::ChatRouterRoute). 승격 판정은 stub 유지
// (docs/80 §3 — 0.5B 폰 추론 0.29 tok/s UX 불성립+배선 갭). cfg(state/chat.json
// 계열 설정)가 ollama/claude를 고르면 ProcessTurn의 ① 자리에서 JKLlmEngine을
// 인스턴스화해 발화→ChatAction+응답문을 받는다. 배선 실장=백로그(docs/80 §3). 호출
// 계약은 지금과 같다(text → ChatAction + 화면용 응답문) — 이 CLI의 나머지
// (REPL·도구 전송·인쇄)는 백엔드 무관으로 유지된다.
//
//   ① 백엔드 플러그 자리(ProcessTurn 안):
//     jk::JKLlmEngine llm;   // cfg 선택(ollama|claude) 배선 후
//     std::string guide = llm.Route(text, action);   // stub 대체
// 지금은 만들지 않는다 — "posix엔 claude/ollama CLI가 없어 지금 실장하면
// dead code"(ClientChatApp.cpp 백엔드 슬롯 주석 동일 판정).
// ---------------------------------------------------------------------------
const char* const kBackendName = "stub-router(T1 ChatRouterRoute)";

// 도구 지시를 데스크톱 창 서버로 위임(jkagentd::SendServerQuery 축소판 —
// 이 CLI는 로컬 도구가 없다, 전부 서버 질의). 연결 실패/파이프 사망은
// false로 돌려 caller가 fail-loud 인쇄를 맡는다.
jk::agent::JKAgentClient g_agent;
bool g_connected = false;

bool SendServerQuery(const std::string& tool, const std::string& argsJson,
                     std::string& replyJson) {
    if (!g_connected && !g_agent.Connect()) {
        replyJson.clear();
        return false;
    }
    if (!g_agent.Query(tool, argsJson, replyJson)) {
        g_connected = false;   // 파이프 사망 — 다음 턴에서 재접속 시도
        replyJson.clear();
        return false;
    }
    return true;
}

// 서버 파이프 부재 — fail-loud 정직 오류(스펙 §5 영수증 대상). 이 CLI는 서버를
// 기동하지 않는다(룰링) — 오류를 남기고 종료 nonzero가 정직한 전부다.
int FailNoServer(const std::string& tool) {
    std::fprintf(stderr,
                 "jktalk: 데스크톱 창 서버 파이프(%s 전송용)에 연결할 수 없습니다\n"
                 "  → jkdesktop --server(또는 jkwinserver)가 기동 중인지 확인하세요. "
                 "이 CLI는 서버를 기동하지 않습니다(fail-loud 계약).\n",
                 tool.c_str());
    return 1;
}

// 한 턴 처리: 발화 1줄 → 라우트 → 도구 지시 전송·인쇄.
// routeOnly = --route 진단 모드(서버 무접촉 — 판정 인쇄만).
// 반환: 0 성공, 1 서버 부재(fail-loud), -1 루프 종료 토큰.
int ProcessTurn(const std::string& rawLine, bool routeOnly) {
    const std::string text = TrimText(rawLine);
    if (text.empty()) return 0;                    // 빈 줄 — 건너뛴다
    if (text == "/exit" || text == "/quit") return -1;

    jk::ChatAction action;
    // 뇌호출 — 백엔드 슬롯(위 kBackendName 블록 + ① 주석). 응답문은 도구
    // 지시가 아니라 **사용자에게 보여질** 안내/확인문(ChatRouter.h 계약) —
    // 채팅 앱이 기록에 인쇄하듯 이 CLI도 그대로 인쇄한다.
    const std::string guide = jk::ChatRouterRoute(text, action);   // ① 백엔드 교체 자리(승격 판정=docs/80 §3)
    std::printf("[시스템] %s\n", guide.c_str());

    if (routeOnly) {
        // --route: 라우터 판정만 인쇄하고 끝(서버 전송 없음 — 서버 없는
        // 환경에서 라우터 경로의 성공 영수증을 뽑는 진단 모드).
        std::printf("[route] kind=%s app=%s (backend=%s)\n",
                    KindName(action.kind),
                    action.kind == jk::ChatAction::Launch
                        ? action.app.c_str() : "",
                    kBackendName);
        std::fflush(stdout);
        return 0;
    }

    // 도구 지시 실행 — 서버 위임(스펙 §2 계약). Info는 지시 없음.
    std::string tool;
    std::string args = "{}";
    switch (action.kind) {
        case jk::ChatAction::Launch:
            // launch_app{"app":...} — 서버가 존재 검증(JKWindowServer.cpp:4420
            // 부근), 표 밖 앱어는 unknown_app으로 정직 회신(ClientChatApp 동일).
            tool = "launch_app";
            args = "{\"app\":\"" + JsonEsc(action.app) + "\"}";
            break;
        case jk::ChatAction::Close:
            // 서버 계약 실측(JKWindowServer.cpp:4074): close_window는 args.id
            // 한정 — argless 폼(window_not_found 정직 회신, ClientChatApp 동일).
            tool = "close_window";
            break;
        case jk::ChatAction::Focus:
            // 동일 계약(JKWindowServer.cpp:3505) — focus_window도 args.id 한정.
            tool = "focus_window";
            break;
        case jk::ChatAction::ListWindows:
            tool = "list_windows";
            break;
        case jk::ChatAction::Info:
            return 0;   // 도구 지시 없음 — 안내문만 인쇄했다
    }

    std::string reply;
    if (!SendServerQuery(tool, args, reply)) return FailNoServer(tool);
    // 확인문 + 회신 원문(JSON) 인쇄 — 답신은 가공 없이 그대로가 계약
    // (ClientChatApp::PollReplies의 원문 인쇄 규약 승계).
    std::printf("실행 요청됨 (%s %s)\n회신: %s\n", tool.c_str(), args.c_str(),
                reply.c_str());
    std::fflush(stdout);
    return 0;
}

// REPL/파이프 본체 — 표준 입력을 줄 단위로 먹는다. isTTY=false(파이프/파일
// 모드)면 프롬프트·시작 헤더를 생략한다(자동화 경로 — 출력이 파서 친화).
// routeOnly는 그대로 ProcessTurn에 전달(--route를 파이프로도 재사용 가능).
int RunLoop(bool isTTY, bool routeOnly) {
    // 진단 헤더는 TTY 대화형에만 — 파이프 모드 출력은 순수 턴 결과만
    // (자동화 수령을 부풀리지 않는다).
    if (isTTY) {
        std::printf(
            "[jktalk] 대화 루프 시작 — backend=%s (종료: /exit 또는 Ctrl+D)\n",
            kBackendName);
    }
    std::string line;
    for (;;) {
        if (isTTY) {
            std::fputs("> ", stdout);
            std::fflush(stdout);
        }
        if (!std::getline(std::cin, line)) break;   // EOF(Ctrl+D) — 정상 종료
#ifdef _WIN32
        // Windows 콘솔 대화형 입력은 CP949로 들어올 수 있다(docs/48 레슨) —
        // JKTALK_INPUT_CP949=1로 명시 스위치(jk::text 어댑터). 파이프 모드는
        // UTF-8 바이트열이 계약(Git Bash/Termux)이라 변환하지 않는다 — 자동
        // 판별은 불가, 정직한 명시 스위치가 정답.
        if (isTTY && std::getenv("JKTALK_INPUT_CP949") != nullptr) {
            line = jk::text::Cp949ToUtf8(line);
        }
#endif
        const int rc = ProcessTurn(line, routeOnly);
        if (rc == -1) return 0;      // /exit — 정상 종료
        if (rc != 0) return rc;      // fail-loud — 정직 nonzero 종료
    }
    return 0;
}

} // namespace

// argv 인코딩 계약: UTF-8 (docs/48 레슨 — jkctl 이중 진입 패턴 승계). win32는
// wmain leg가 UTF-16 argv를 CP_UTF8로 정규화, posix는 exec가 바이트열 argv를
// 그대로 전달한다.
static int RunMain(int argc, char* argv[]);

#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall WideCharToMultiByte(
    unsigned int codePage, unsigned long dwFlags, const wchar_t* lpWideCharStr,
    int cchWideChar, char* lpMultiByteStr, int cbMultiByte,
    const char* lpDefaultChar, int* lpUsedDefaultChar);

int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::string> utf8(static_cast<size_t>(argc > 0 ? argc : 1));
    std::vector<char*> ptrs(static_cast<size_t>(argc > 0 ? argc : 1), nullptr);
    for (int i = 0; i < argc; ++i) {
        const int n = WideCharToMultiByte(65001 /* CP_UTF8 */, 0, argv[i], -1,
                                          nullptr, 0, nullptr, nullptr);
        // 변환 실패(n<=0)도 argv 자리를 nullptr로 남기지 않는다 — 빈 문자열을
        // 놔두면 RunMain의 빈-발화 검사가 정상 반응한다(jkctl WCTM leg 규약).
        if (n > 1) {
            utf8[static_cast<size_t>(i)].resize(static_cast<size_t>(n) - 1);
            WideCharToMultiByte(65001, 0, argv[i], -1,
                                utf8[static_cast<size_t>(i)].data(), n,
                                nullptr, nullptr);
        }
        ptrs[static_cast<size_t>(i)] = utf8[static_cast<size_t>(i)].data();
    }
    return RunMain(argc, ptrs.data());
}
#else
// posix leg (jkctl 이중 진입): byte-wise argv is already the contract.
int main(int argc, char* argv[]) {
    return RunMain(argc, argv);
}
#endif // _WIN32

// 본체: 모드 판정은 3종 — 첫 인자(--ask/--route/발화) 주어지면 원컷, 아니면
// stdin이 TTY냐 아니냐로 REPL/파이프. 원컷·파이프 모두 같은 ProcessTurn.
static int RunMain(int argc, char* argv[]) {
    std::string oneShot;         // 원컷 발화(빈 문자열 = 원컷 아님)
    bool routeOnly = false;      // --route 진단 모드
    bool haveOneShot = false;    // 원컷 여부 — 빈 발화("")도 원컷으로 취급

    if (argc > 1) {
        const std::string flag = argv[1];
        if (flag == "--ask") {
            if (argc < 3 || std::strlen(argv[2]) >= 4096) {
                std::fprintf(stderr, "usage: jktalk --ask \"<발화>\"\n");
                return 2;
            }
            oneShot = argv[2];
            haveOneShot = true;
        } else if (flag == "--route") {
            if (argc < 3 || std::strlen(argv[2]) >= 4096) {
                std::fprintf(stderr, "usage: jktalk --route \"<발화>\"\n");
                return 2;
            }
            oneShot = argv[2];
            routeOnly = true;
            haveOneShot = true;
        } else if (flag == "--help") {
            std::printf(
                "usage: jktalk                     대화 REPL(프롬프트 '> ')\n"
                "       jktalk \"<발화>\"            원컷 — 발화 1개만 처리\n"
                "       jktalk --ask \"<발화>\"      원컷(명시형 — 동일)\n"
                "       jktalk --route \"<발화>\"    라우터 진단 — 서버 무접촉, 판정만 인쇄\n"
                "       echo \"<발화>\" | jktalk     파이프 모드 — 자동화 경로\n"
                "       /exit (또는 Ctrl+D)        REPL 종료\n");
            return 0;
        } else {
            // 나머지 첫 인자 = 발화 그 자체 (`jktalk "지뢰찾기 켜줘"`). 널무성의
            // 인자 상한(jkctl ask 축약 — 4KiB)을 위반하면 절단 대신 거부.
            if (std::strlen(argv[1]) >= 4096) {
                std::fprintf(stderr, "jktalk: argument too long\n");
                return 2;
            }
            oneShot = argv[1];
            haveOneShot = true;
        }
    }

    if (haveOneShot) {
        // 원컷 — 발화 하나만 처리하고 끝(자동화 영수증 경로).
        return ProcessTurn(oneShot, routeOnly);
    }
    // REPL 또는 파이프 — stdin이 TTY냐가 경계.
#ifdef _WIN32
    const bool isTTY = _isatty(_fileno(stdin)) != 0;
#else
    const bool isTTY = ::isatty(0) != 0;
#endif
    return RunLoop(isTTY, routeOnly);
}
