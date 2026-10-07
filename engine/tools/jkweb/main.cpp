// jkweb — 폰 웹 채팅 HTTP 서버 (스펙 2026-10-07-desktop-chat-app §5.1 —
// "X11 밖 입력 문"의 웹 입력 문). 폰 Termux 브라우저가 이 서버의 채팅 페이지를
// 열고(native 키보드로 타자) 발화하면, 이 프로세스가 그 발화를 해석해 데스크톱
// 창 서버에 도구 지시로 번역해 보낸다 — DeX/X11 화면 위 jkdesktop이 반응하는
// 그림. 이 프로세스 자체는 UI 없이 TCP 소켓만 가진 콘솔 앱(jkbridge HTTP 선준,
// jktalk 턴 코어 재용).
//
// 구성:
//   - GET /    → 채팅 페이지 1장(kChatPage — 단일 HTML 문자열이 .cpp에 내장,
//                한국어 UI, 입력 박스+대화 기록, fetch()로 POST /talk).
//   - POST /talk {"text":"..."} → jk::ChatRouterRoute(T1)로 해석 → 도구 지시
//     는 서버 위임(JKAgentClient — jktalk/jkctl과 같은 {"tool":...,"args":{}}
//     질의 형태) → JSON {"ok":...,"reply":...,"kind":...,"app":...,
//     "server":<회신 원문>} 회신. ok는 서버 회신의 "ok":true 유무 판정의 정직
//     신호(I1 fix r1 — argless close_window가 window_not_found로 돌아온 것을
//     라우터 안내문만으로 덮어 거짓 성공으로 보지 않게 한다, jkdesktop
//     JKWindowServer close_window args.id 한정 계약)이고, 서버 회신 원문은
//     "server" 필드로 승계(jktalk "회신: 원문 인쇄" 규약 승계)된다. 외부로
//     나가는 모든 문자열은 JsonEsc 이스케이프(응답 몸통 전부).
//   - HTTP 1.1 최소 파서: GET·POST만, Content-Length 몸통, Connection: close
//     (keep-alive 없음 — 요청 1건당 1연결, jkbridge HttpReply 규약 승계).
//     요청줄 헤드 상한 8KiB·몸통 상한 64KiB — 초과는 정직 400/413.
//   - --port N (기본 8090)·--bind (기본 loopback=127.0.0.1 | all=0.0.0.0).
//     기본은 루프백(폰 브라우저 localhost 본선 — 육안 게이트 경로)이고,
//     PC 브라우저가 폰 IP로 도달하는 LAN 접속은 --bind all 옵트인(최종
//     리뷰 I1 — 무인증 /talk가 launch_app 권한을 여는 서버를 LAN에 기본
//     개방하지 않게 한다; 공유기·사무실 Wi-Fi에서의 옆단 노출 차단. 토큰
//     게이트는 백로그). 기동 인쇄는 활성 모드를 명시하고 localhost URL이 기준.
//     스레드는 연결당 1개 detach(jkbridge HandleConn 동형 — 도구 질의 구간은
//     직렬화 락, JKAgentClient 한 인스턴스의 동시 사용을 막는 최소 안전).
//
// fail-loud 정직 계약: 데스크톱 창 서버 파이프가 없으면 /talk는 500에 한국어
// 이유 몸통으로 답한다(스펙 §5.1 "정직 500" — hang 없이 즉시). 이 서버는
// jkdesktop --server를 기동하지 않는다(jkctl RunServerQuery의 "window server
// not running" 형제, jktalk fail-loud 계약 승계).
//
// 백엔드 슬롯(스펙 §4 — jktalk의 kBackendName 블록 동형 주석): 지금은 T1 stub
// 라우터(jk::ChatRouterRoute)만 꽂는다. JKLlmEngine은 이 서버에서 아직
// 인스턴스화하지 않는다(룰링 — cfg 선택형 배선은 별도 과제, posix엔 ollama/
// claude CLI가 없어 지금 실장하면 dead code — ClientChatApp.cpp 백엔드 슬롯
// 동일 판정). 슬롯 자리는 HandleTalk의 ① 주석.
//
// 소켓 계약(jk::net adapter — docs/68 W8b): winsock/posix 양다리는
// JKNet_win32.cpp / JKNet_posix.cpp가 소유한다. 이 TU는 winsock2.h를 만지지
// 않는다 — HTTP 기하(RecvAll·Send·ListenTcp)만 adapter로 맞춘다.

#include <agent/JKAgentClient.h>
#include <apps/ChatRouter.h>
#include <net/JKNet.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// JSON 문자열 이스케이프/추출 (jktalk::JsonEsc 동형 — ClientChatApp 규약)
// ---------------------------------------------------------------------------

// 발화/회신의 JSON 문자열 이스케이프 — 제어문자는 \uXXXX, 쌍따옴표·역슬래시는
// 백슬래시 이스케이프. 응답 몸통의 모든 외부 유래 문자열(라우터 안내문·앱어·
// 오류문)에 적용한다.
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
// 정규화 결과라 이 절단은 빈 발화 판정 몫.
std::string TrimText(const std::string& raw) {
    static const char* kSpace = " \t\r\n";
    const size_t b = raw.find_first_not_of(kSpace);
    if (b == std::string::npos) return "";
    const size_t e = raw.find_last_not_of(kSpace);
    return raw.substr(b, e - b + 1);
}

// 라우터 Kind의 사람 표기 — 응답 JSON "kind" 필드용(jktalk --route 인쇄와 같은
// 표기). Error는 라우터 밖(서버 위임 실패)의 응답 전용 값.
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

// JSON 몸통 한 키의 문자열 값을 뽑는 최소 파서 — {"text":"..."} 전용(실제 HTTP
// 프레임워크를 끌어오지 않는다 — 스펙 §5.1). 이스케이프는 JSON 규약의 기본형
// (\" \\ \/ \b \f \n \r \t \uXXXX)을 푼다. 키 부재·모양 불량은 빈 문자열.
std::string JsonGetString(const std::string& body, const char* key) {
    // 키 탐색 — 중첩 개체 없는 단일 키 몸통이라 순차 find로 충분.
    const std::string want = std::string("\"") + key + "\"";
    size_t at = body.find(want);
    while (at != std::string::npos) {
        // 키 뒤 선택 공백·':'·공백을 넘겨 값 시작 따옴표를 찾는다.
        size_t v = at + want.size();
        while (v < body.size() && (body[v] == ' ' || body[v] == '\t')) v++;
        if (v < body.size() && body[v] == ':') {
            v++;
            while (v < body.size() && (body[v] == ' ' || body[v] == '\t')) v++;
            if (v < body.size() && body[v] == '"') {
                v++;
                std::string out;
                while (v < body.size()) {
                    const char ch = body[v];
                    if (ch == '"') return out;              // 값 끝
                    if (ch == '\\' && v + 1 < body.size()) {
                        v++;
                        const char e = body[v];
                        switch (e) {
                            case '"':  out += '"';  break;
                            case '\\': out += '\\'; break;
                            case '/':  out += '/';  break;
                            case 'b':  out += '\b'; break;
                            case 'f':  out += '\f'; break;
                            case 'n':  out += '\n'; break;
                            case 'r':  out += '\r'; break;
                            case 't':  out += '\t'; break;
                            case 'u': {
                                if (v + 4 >= body.size()) return out;
                                char hex[5] = {0};
                                for (int i = 0; i < 4; ++i)
                                    hex[i] = body[v + 1 + static_cast<size_t>(i)];
                                // 16진 무효도 정직하게 — 0으로 떨어지는 것보다
                                // 원본 유지가 낫지만 최소 파서는 유효 몸통이
                                // 계약(browser JSON.stringify)이라 간단히 간다.
                                const unsigned cp =
                                    static_cast<unsigned>(
                                        std::strtoul(hex, nullptr, 16) & 0xFFFF);
                                if (cp < 0x80) {
                                    out += static_cast<char>(cp);
                                } else if (cp < 0x800) {
                                    out += static_cast<char>(0xC0 | (cp >> 6));
                                    out += static_cast<char>(0x80 | (cp & 0x3F));
                                } else {
                                    // 3바이트 UTF-8 — 한글 등 BMP 범위.
                                    out += static_cast<char>(0xE0 | (cp >> 12));
                                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                    out += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                                v += 4;
                                break;
                            }
                            default:   out += e; break;
                        }
                        v++;
                        continue;
                    }
                    out += ch;
                    v++;
                }
                return out;   // 닫는 따옴표 없는 비정상 몸통 — 지금까지 뽑은 값
            }
        }
        at = body.find(want, at + 1);   // 겹치는 이름(예: "not_text") 회피 재탐색
    }
    return "";
}

// ---------------------------------------------------------------------------
// 서버 위임 (jktalk::SendServerQuery 재용 — 전부 서버 질의, 로컬 도구 없음)
// ---------------------------------------------------------------------------

jk::agent::JKAgentClient g_agent;
bool g_connected = false;
std::mutex g_turnMtx;   // 연결당 1스레드 — JKAgentClient 1인스턴스 직렬화 몫

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

// ---------------------------------------------------------------------------
// 정적 채팅 페이지 — 단일 HTML 문자열(한국어 UI, 브라우저 native 키보드:
// <input>이라 IME·한글 조합을 브라우저가 몫는다 — ImGui 입력 상자의 IME 문을
// 우회하는 이 입력문의 존재 이유, 스펙 §5.1).
// ---------------------------------------------------------------------------
const char* const kChatPage =
    "<!doctype html>\n"
    "<html lang=\"ko\">\n"
    "<head>\n"
    "<meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
    "<title>JK 채팅</title>\n"
    "<style>\n"
    "body{font-family:system-ui,sans-serif;margin:0;display:flex;\n"
    "     flex-direction:column;height:100vh;background:#111;color:#eee}\n"
    "#log{flex:1;overflow-y:auto;padding:12px;font-size:16px;line-height:1.5}\n"
    ".me{color:#7cf;margin:4px 0}.bot{color:#eee;margin:4px 0}\n"
    ".meta{font-size:12px;color:#888;margin-left:6px}\n"
    ".err{color:#f77;margin:4px 0}\n"
    "#f{display:flex;gap:6px;padding:10px;border-top:1px solid #333;\n"
    "   background:#1a1a1a}\n"
    "#t{flex:1;font-size:18px;padding:10px;border-radius:6px;border:1px solid\n"
    "   #444;background:#222;color:#eee}\n"
    "button{font-size:16px;padding:10px 16px;border-radius:6px;border:0;\n"
    "        background:#379;color:#fff}\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "<div id=\"log\"><div class=\"bot\">JK 채팅 — 발화를 입력하세요.\n"
    "예: 지뢰찾기 켜줘 / 창 목록</div></div>\n"
    "<form id=\"f\" onsubmit=\"return send()\">\n"
    "<input id=\"t\" autocomplete=\"off\" placeholder=\"발화 입력…\">\n"
    "<button type=\"submit\">보내기</button>\n"
    "</form>\n"
    "<script>\n"
    "function esc(s){var d=document.createElement('div');\n"
    "  d.textContent=s;return d.innerHTML;}\n"
    "function add(cls,txt,meta){var l=document.getElementById('log');\n"
    "  var e=document.createElement('div');e.className=cls;\n"
    "  e.innerHTML=esc(txt)+(meta?'<span class=meta>'+esc(meta)+'</span>':'');\n"
    "  l.appendChild(e);l.scrollTop=l.scrollHeight;}\n"
    "function send(){var t=document.getElementById('t');\n"
    "  var v=t.value.trim();if(!v)return false;t.value='';add('me',v);\n"
    "  fetch('/talk',{method:'POST',\n"
    "    headers:{'Content-Type':'application/json'},\n"
    "    body:JSON.stringify({text:v})})\n"
    "   .then(function(r){return r.json().then(function(j){\n"
    "     return {ok:r.ok, j:j};});})\n"
    "   .then(function(x){var j=x.j;\n"
    "     var meta=j.app?('kind='+j.kind+' app='+j.app):('kind='+j.kind);\n"
    "     if(j.server)meta+=' | 서버 '+j.server;\n"
    "     if(x.ok&&j.ok!==false){add('bot',j.reply||'(빈 회신)',meta);}\n"
    "     else{add('err',j.reply||j.error||('HTTP 오류 '+x.ok),meta);}})\n"
    "   .catch(function(e){add('err','전송 실패: '+e,'');});\n"
    "  return false;}\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

// 헤더 한 줄 값 — 대소문자 무시 스캔(jkbridge::HeaderValue 규약 승계; curl은
// "Content-Length:"로 보낸다).
std::string HeaderValue(const std::string& head, const char* name) {
    std::string lower = head;
    for (char& c : lower) c = static_cast<char>(tolower(c));
    std::string want = name;
    for (char& c : want) c = static_cast<char>(tolower(c));
    want += ':';
    const size_t at = lower.find(want);
    if (at == std::string::npos) return "";
    size_t v = at + want.size();
    while (v < head.size() && (head[v] == ' ' || head[v] == '\t')) v++;
    size_t e = v;
    while (e < head.size() && head[e] != '\r' && head[e] != '\n') e++;
    return head.substr(v, e - v);
}

// ---------------------------------------------------------------------------
// HTTP 기하 (jkbridge main.cpp 파서·응답 승계 — Connection: close 규약)
// ---------------------------------------------------------------------------

// 요청 헤드 읽기(\r\n\r\n까지, 8KiB 상한). 1바이트씩 recv — 느리지만 정확:
// 청크 읽기는 종결자 너머의 몸통 바이트를 삼킬 수 있다(jkbridge W8b 주석 그대로).
bool ReadHttpHead(jk::net::Socket s, std::string& head) {
    char c;
    while (head.size() < 8192) {
        if (!jk::net::RecvAll(s, &c, 1)) return false;
        head += c;
        if (head.size() >= 4 &&
            head.compare(head.size() - 4, 4, "\r\n\r\n") == 0) return true;
    }
    return false;
}

void SendAll(jk::net::Socket s, const char* p, size_t n) {
    while (n > 0) {
        const int r = jk::net::Send(s, p, static_cast<int>(n));
        if (r <= 0) return;
        p += r;
        n -= static_cast<size_t>(r);
    }
}

// HTTP/1.1 응답 — Connection: close(요청 1건당 1연결). 상존값: 200 OK /
// 400 Bad Request / 404 Not Found / 413 Payload Too Large / 500 Server Error.
void HttpReply(jk::net::Socket s, int code, const std::string& body,
               const char* contentType) {
    const char* reason = code == 200 ? "OK"
                       : code == 400 ? "Bad Request"
                       : code == 404 ? "Not Found"
                       : code == 413 ? "Payload Too Large"
                                     : "Service Unavailable";
    char head[256];
    std::snprintf(head, sizeof(head),
                  "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n"
                  "Content-Type: %s\r\nCache-Control: no-store\r\n"
                  "Connection: close\r\n\r\n",
                  code, reason, body.size(), contentType);
    SendAll(s, head, std::strlen(head));
    if (!body.empty()) SendAll(s, body.data(), body.size());
}

// ---------------------------------------------------------------------------
// 턴 코어 (jktalk::ProcessTurn의 도구 지시 절반 승계 — 어휘 2본 금지 계약:
// 라우트 판정은 jk::ChatRouterRoute 단일 진실원, 여기에 두지 않는다)
// ---------------------------------------------------------------------------

// 발화 1개 → 라우트 → 서버 위임 → JSON 응답 몸통. 서버 부재는 500과 한국어
// 이유(jkwebFailNoServer 대응 — HTTP 세계에서는 종료코드 대신 상태 코드가 정직
// 의 표기, 스펙 §5.1 "정직 500").
std::string HandleTalk(const std::string& body, int& code) {
    const std::string text = TrimText(JsonGetString(body, "text"));
    if (text.empty()) {
        code = 400;
        return "{\"ok\":false,\"reply\":\"빈 발화입니다.\",\"kind\":\"Error\","
               "\"app\":\"\"}";
    }

    jk::ChatAction action;
    // 뇌호출 — 백엔드 슬롯(jktalk kBackendName 블록 동형). T7 이후 cfg 선택형
    // 배선이 여기 JKLlmEngine을 인스턴스화한다(text → ChatAction + 안내문,
    // 호출 계약 동일 — 나머지는 백엔드 무관으로 유지).
    //   ① jk::JKLlmEngine llm; std::string guide = llm.Route(text, action);
    const std::string guide = jk::ChatRouterRoute(text, action);
    const std::string escKind = KindName(action.kind);
    const std::string escApp =
        action.kind == jk::ChatAction::Launch || action.kind == jk::ChatAction::Close
            ? action.app : "";

    // 도구 지시 — Info는 지시 없음(서버 무접촉, 안내문만 회신 — jktalk 동형).
    std::string tool;
    std::string args = "{}";
    switch (action.kind) {
        case jk::ChatAction::Launch:
            // launch_app{"app":...} — 서버가 존재 검증, 표 밖은 unknown_app
            // 정직 회신(JKWindowServer.jkapp_<app> 모듈 판정).
            tool = "launch_app";
            args = "{\"app\":\"" + JsonEsc(action.app) + "\"}";
            break;
        case jk::ChatAction::Close:
            // 서버 계약 실측(JKWindowServer.cpp:4074): close_window는 args.id
            // 한정 — argless 폼(window_not_found 정직 회신).
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
            code = 200;
            return "{\"ok\":true,\"reply\":\"" + JsonEsc(guide) +
                   "\",\"kind\":\"" + escKind + "\",\"app\":\"\"}";
    }

    // 도구 지시 전송 — 전역 1인스턴스 직렬화(연결당 스레드 최소 안전).
    std::string reply;
    bool sent;
    {
        const std::lock_guard<std::mutex> lock(g_turnMtx);
        sent = SendServerQuery(tool, args, reply);
    }
    if (!sent) {
        code = 500;
        return "{\"ok\":false,\"reply\":\"데스크톱 창 서버 파이프(" +
               JsonEsc(tool) +
               " 전송용)에 연결할 수 없습니다 — jkdesktop --server(또는 "
               "jkwinserver)가 기동 중인지 확인하세요. 이 서버는 창 서버를 "
               "기동하지 않습니다(fail-loud 계약).\""
               ",\"kind\":\"Error\",\"app\":\"\"}";
    }
    // 정직 승계(I1 — fix r1): 서버 회신을 "server" 필드로 원문 승계(jktalk의
    // "회신: 원문 인쇄" 규약, ClientChatApp "결과를 대화 기록에 반영" 계약의
    // 형제)하고, ok 신호는 회신의 "ok":true 유무로 판정한다 — argless
    // close_window가 서버의 window_not_found(JKWindowServer.cpp:4076 부근,
    // close_window는 args.id 한정)를 돌려도 kind=Close 안내문만으로 브라우저에
    // 거짓 성공이 되지 않게 한다. ok 필드가 없는 모양의 회신도 정직하게 false
    // ("판정 못 하면 성공이라 부르지 않는다") — 서버 원문은 어느 경로든
    // "server" 필드로 그대로 실려 숨김이 없다.
    const bool serverOk = reply.find("\"ok\":true") != std::string::npos;
    code = 200;
    return "{\"ok\":" + std::string(serverOk ? "true" : "false") +
           ",\"reply\":\"" + JsonEsc(guide) + "\",\"kind\":\"" + escKind +
           "\",\"app\":\"" + JsonEsc(escApp) + "\",\"server\":\"" +
           JsonEsc(reply) + "\"}";
}

// 연결 1건 처리 — HTTP 1.1 GET/POST만, 나머지는 정직 404/400.
void HandleConn(jk::net::Socket conn) {
    // 30s 읽기·쓰기 타임아웃 — slowloris·죽은 클라이언트 소켓 누수 가드
    // (jkbridge opus MAJOR-3 레슨, adapter SO_RCV/SNDTIMEO 양쪽).
    jk::net::SetTimeouts(conn, 30 * 1000);
    std::string head;
    if (!ReadHttpHead(conn, head)) {
        jk::net::Close(conn);
        return;
    }
    const size_t sp1 = head.find(' ');
    const size_t sp2 = sp1 == std::string::npos ? std::string::npos
                                                : head.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) {
        HttpReply(conn, 400, "{\"error\":\"malformed request\"}",
                  "application/json");
        jk::net::Close(conn);
        return;
    }
    const std::string method = head.substr(0, sp1);
    const std::string target = head.substr(sp1 + 1, sp2 - sp1 - 1);

    if (method == "GET" && target == "/") {
        HttpReply(conn, 200, kChatPage, "text/html; charset=utf-8");
    } else if (method == "POST" && target == "/talk") {
        // 몸통: Content-Length 만큼 읽는다(상한 64KiB — 발화는 수백 바이트).
        const std::string clStr = HeaderValue(head, "Content-Length");
        if (clStr.empty()) {
            HttpReply(conn, 400, "{\"ok\":false,\"reply\":\"Content-Length "
                                 "헤더가 없습니다.\",\"kind\":\"Error\","
                                 "\"app\":\"\"}",
                      "application/json");
            jk::net::Close(conn);
            return;
        }
        const size_t cl =
            static_cast<size_t>(std::strtoul(clStr.c_str(), nullptr, 10));
        if (cl > 65536) {
            HttpReply(conn, 413, "{\"ok\":false,\"reply\":\"몸통이 너무 "
                                 "큽니다.\",\"kind\":\"Error\",\"app\":\"\"}",
                      "application/json");
            jk::net::Close(conn);
            return;
        }
        std::string body(cl, '\0');
        if (cl > 0 && !jk::net::RecvAll(conn, &body[0], cl)) {
            jk::net::Close(conn);
            return;
        }
        int code = 200;
        const std::string resp = HandleTalk(body, code);
        HttpReply(conn, code, resp, "application/json");
    } else {
        HttpReply(conn, 404, "{\"ok\":false,\"reply\":\"없는 경로입니다. "
                             "(GET / 또는 POST /talk)\",\"kind\":\"Error\","
                             "\"app\":\"\"}",
                  "application/json");
    }
    jk::net::Close(conn);
}

} // namespace

// argv 인코딩 계약(jktalk 승계 — docs/48 레슨): UTF-8. win32는 wmain leg가
// UTF-16 argv를 CP_UTF8로 정규화, posix는 exec가 바이트열 argv를 그대로.
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
int main(int argc, char* argv[]) {
    return RunMain(argc, argv);
}
#endif // _WIN32

// 본체: --port N(기본 8090) → bind 0.0.0.0(전 인터페이스 — PC 브라우저도 폰
// IP로 도달) → 기동 인쇄는 localhost URL이 기준 → accept 루프.
// 본체: --port N(기본 8090)·--bind(기본 loopback 127.0.0.1, all=INADDR_ANY
// 옵트인) → listen → 기동 인쇄(활성 모드 명시, localhost URL 기준) → accept
// 루프.
static int RunMain(int argc, char* argv[]) {
    int port = 8090;
    bool bindAll = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "usage: jkweb [--port N] [--bind all]\n");
                return 2;
            }
            port = std::atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                std::fprintf(stderr, "jkweb: invalid port\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--bind") == 0) {
            if (i + 1 >= argc || std::strcmp(argv[i + 1], "all") != 0) {
                std::fprintf(stderr, "jkweb: --bind는 all 만 받는다\n"
                                     "(기본=127.0.0.1 루프백, 옵트인=--bind all)\n");
                return 2;
            }
            ++i;
            bindAll = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("usage: jkweb [--port N] [--bind all]\n"
                        "   폰 웹 채팅 HTTP 서버(기본 8090)\n"
                        "  GET /          채팅 페이지\n"
                        "  POST /talk     {\"text\":\"...\"} → 라우트 → 창 서버 위임\n"
                        "  --port N       수신 포트(기본 8090)\n"
                        "  --bind all     0.0.0.0 수신(LAN 옵트인 — 같은 네트워크의\n"
                        "                 브라우저가 http://<이 기기 IP>:N/ 로 접속.\n"
                        "                 집 Wi-Fi 내 한정 권장). 기본은 127.0.0.1\n"
                        "                 루프백 — 무인증 /talk가 launch_app 권한을\n"
                        "                 여므로 기본 개방은 하지 않는다.\n");
            return 0;
        } else {
            std::fprintf(stderr, "usage: jkweb [--port N] [--bind all]\n");
            return 2;
        }
    }

    if (!jk::net::Startup()) {
        std::fprintf(stderr, "jkweb: 소켓 초기화 실패 (WSAStartup)\n");
        return 1;
    }
    // bind 기본=루프백(127.0.0.1 — 폰 브라우저 localhost 본선). --bind all만
    // INADDR_ANY(LAN+loopback — PC 브라우저도 폰 IP로 도달, 집 Wi-Fi 내 한정
    // 옵트인). 이 서버는 무인증 /talk에서 launch_app 권한을 여므로 LAN 개방은
    // 명시 스위치로 제한한다(최종 리뷰 I1 — 토큰 게이트는 백로그).
    const jk::net::Socket listener = jk::net::ListenTcp(
        bindAll ? "" : "127.0.0.1", static_cast<std::uint16_t>(port), 8);
    if (listener == jk::net::kInvalidSocket) {
        std::fprintf(stderr, "jkweb: bind/listen failed — 포트 %d\n", port);
        return 1;
    }

    std::printf("jkweb — 폰 웹 채팅 서버\n");
    std::printf("  URL: http://localhost:%d/\n", port);
    if (bindAll) {
        std::printf("  bind: 0.0.0.0 (--bind all — 같은 네트워크의 브라우저는 "
                    "http://<이 기기 IP>:%d/ 로도 접속 가능)\n", port);
    } else {
        std::printf("  bind: 127.0.0.1 (루프백 기본 — LAN 접속은 "
                    "--bind all로 (집 Wi-Fi 내 한정))\n");
    }
    std::printf("  GET / 채팅 페이지 · POST /talk {\"text\":\"...\"} · "
                "도구 지시는 창 서버 위임(jkdesktop --server 미기동 시 정직 500)\n");
    std::fflush(stdout);

    for (;;) {
        const jk::net::Socket conn = jk::net::Accept(listener);
        if (conn == jk::net::kInvalidSocket) continue;
        std::thread(HandleConn, conn).detach();   // 연결당 1스레드(jkbridge 동형)
    }
}
