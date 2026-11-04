// T7 — LLM HTTP 어댑터의 본체(헤더 주석 — 계약 원존).
#include <agent/JKLlmTurnHttp.h>
#include <agent/JKAgentJson.h>
#include <net/JKNet.h>

#include <atomic>
#include <cctype>   // tolower (NormalizeOllamaMode)
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <cstdlib>  // atoi

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

namespace jk {
namespace agent {

// JSON 문자열 이스케이프(작성면 — JKAgentJson는 회독만 제공, 쓰기 헬퍼 부재
// 원존). EscapeJson 4호 복제(플랜 백로그 원존)의 5호 — 이 TU가 신규 유일
// 공개 창(백로그: 흡수 통합은 별도 라인).
namespace {
std::string JsonEscapeWrite(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}
}  // namespace

std::string NormalizeOllamaMode(const std::string& raw) {
    std::string s = raw;
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    size_t b = 0, e = s.size();
    const auto wsp = [](char c) {
        return c == ' ' || c == '\t';
    };
    while (b < e && wsp(s[b])) ++b;
    while (e > b && wsp(s[e - 1])) --e;
    s = s.substr(b, e - b);
    if (s == "off") return "off";  // 계약 명시 — 실질 = 기존 leg 원존
    if (s == "local") return "local";
    if (s == "cloud") return "cloud";
    return "off";  // 위배·빈 값 = fail-closed off
}

std::string BuildOllamaChatJson(const std::string& model,
                                const std::string& systemPrompt,
                                const std::string& prompt) {
    std::string body = "{\"model\":\"";
    body += JsonEscapeWrite(model);
    body += "\",\"messages\":[{\"role\":\"system\",\"content\":\"";
    body += JsonEscapeWrite(systemPrompt);
    body += "\"},{\"role\":\"user\",\"content\":\"";
    body += JsonEscapeWrite(prompt);
    body += "\"}],\"stream\":false}";
    return body;
}

bool ParseOllamaChatReply(const std::string& body, std::string& content) {
    content.clear();
    if (body.empty()) return false;
    // 1) 표준 단발 개체: {"model":..,"message":{.., "content":..},"done":true}
    AgentJson one(body);
    std::string c;
    if (one.ok() && one.GetObjStr("message", "content", c)) {
        content = c;
        return true;
    }
    // 2) 방어: 데몬 버전 편차의 NDJSON 스트림 — 행별 content를 원순 연결한다
    //    (역순 스윕 = 연결 순서 반전 결함 스캔 수리 — 원순 find 스윕). 파스
    //    실패 행 무시.
    {
        std::string concat;
        size_t at = 0;
        while (at < body.size()) {
            const size_t nl = body.find('\n', at);
            const size_t lineEnd = (nl == std::string::npos) ? body.size() : nl;
            const std::string line = body.substr(at, lineEnd - at);
            if (!line.empty()) {
                AgentJson j(line);
                std::string c2;
                if (j.ok() && j.GetObjStr("message", "content", c2)) {
                    concat += c2;
                }
            }
            if (nl == std::string::npos) break;
            at = nl + 1;
        }
        if (!concat.empty()) {
            content = concat;
            return true;
        }
    }
    // 3) 정직 실패 — 데몬 에러 개체({"error":...})면 원문 인용.
    if (one.ok()) {
        std::string err;
        if (one.GetStr("error", err)) {
            content = err;
            return false;
        }
    }
    return false;
}

// ── 평문(로컬) HTTP/1.1 POST ────────────────────────────────────────────
// hostNorm = NormalizeOllamaHost 통과분("host:port"·"http://host:port" 양형
// — NormalizeOllamaHost가 스킴+포트를 가진 상대를 원문 보존한다). 스킴 접두
// 는 여기서 떼어 https:// 상대면 **정직 거부**(평문 leg의 fail-loud — https
// 는 cloud transport 소관). ConnectTcp(jk::net 신설 — T2)의 getaddrinfo로
// 기명 호스트도 받는다(#119 릴레이 계약의 PC 데몬 상대).
namespace {

void SplitHostPort(const std::string& hostNorm, std::string& hostOut,
                   uint16_t* portOut) {
    hostOut = hostNorm;
    const size_t scheme = hostOut.find("://");
    if (scheme != std::string::npos) hostOut = hostOut.substr(scheme + 3);
    const size_t colon = hostOut.find(':');
    uint16_t port = 0;
    if (colon != std::string::npos) {
        port = static_cast<uint16_t>(std::atoi(hostOut.c_str() + colon + 1));
        hostOut = hostOut.substr(0, colon);
    }
    if (port == 0) port = 11434;  // ollama 기본 — 부재 시 fallbackPort 재용
    if (portOut) *portOut = port;
}

// 응답 본문 추출: 헤더 구분 후 chunked면 dechunk, 아니면 그대로.
std::string ExtractHttpBody(const std::string& resp) {
    const size_t sep = resp.find("\r\n\r\n");
    if (sep == std::string::npos) return {};
    std::string head = resp.substr(0, sep);
    std::string rest = resp.substr(sep + 4);
    if (head.find("Transfer-Encoding:") != std::string::npos &&
        head.find("chunked") != std::string::npos) {
        std::string out;
        size_t at = 0;
        while (at < rest.size()) {
            const size_t nl = rest.find("\r\n", at);
            if (nl == std::string::npos) break;
            const int len = static_cast<int>(
                std::strtol(rest.substr(at, nl - at).c_str(), nullptr, 16));
            if (len <= 0) break;  // 0 = 마지막 청크
            const size_t begin = nl + 2;
            if (begin + static_cast<size_t>(len) > rest.size()) break;
            out += rest.substr(begin, static_cast<size_t>(len));
            at = begin + static_cast<size_t>(len) + 2;
        }
        return out;
    }
    return rest;  // Content-Length 편차 무시(EOF 단일 읽기 — 연결당 수명 계약)
}

}  // namespace

LlmHttpPostResult HttpPostPlain(const std::string& hostNorm,
                                uint16_t fallbackPort, const std::string& path,
                                const std::string& bodyJson, int timeoutMs) {
    LlmHttpPostResult out;
    std::string host;
    uint16_t port = fallbackPort;
    SplitHostPort(hostNorm, host, &port);
    if (host.empty()) {
        out.status = "empty host";
        return out;
    }
    // 스킴이 남아 https면 정직 거부(평문 leg 경계 — cloud는 transport 소관).
    if (hostNorm.rfind("https:", 0) == 0) {
        out.status = "https needs cloud transport (see SetHttpsPostTransport)";
        return out;
    }

    if (!net::Startup()) {
        out.status = "net startup failed";
        return out;
    }
    net::Socket s = net::ConnectTcp(host, port);
    if (s == net::kInvalidSocket) {
        out.status = "connect failed";
        return out;
    }
    net::SetTimeouts(s, static_cast<std::uint32_t>(timeoutMs > 0 ? timeoutMs : 60000));

    std::string req = "POST " + path + " HTTP/1.1\r\n";
    req += "Host: " + host +
           (port == 11434 ? std::string() : ":" + std::to_string(port)) + "\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: " + std::to_string(bodyJson.size()) + "\r\n";
    req += "Connection: close\r\n\r\n";
    req += bodyJson;

    if (net::Send(s, req.data(), static_cast<int>(req.size())) <= 0) {
        out.status = "send failed";
        net::Close(s);
        return out;
    }
    // 단발 recv 루프(jk::net::Recv — T2): 0 = 정직 EOF(왕복 종료 계약)·-1 =
    // 타임아웃/오류. 4MB 상한 — 데몬 회신 상한 방어.
    std::string resp;
    char buf[4096];
    while (resp.size() < (1u << 22)) {
        const int got = net::Recv(s, buf, sizeof(buf));
        if (got <= 0) break;
        resp += buf;
    }
    net::ShutdownBoth(s);
    net::Close(s);
    if (resp.empty()) {
        out.status = "empty response";
        return out;
    }
    // 상태행 진실원.
    const size_t firstSpc = resp.find(' ');
    if (firstSpc != std::string::npos)
        out.status = resp.substr(firstSpc + 1, 3);
    const std::string httpBody = ExtractHttpBody(resp);
    out.body = httpBody;
    out.ok = (out.status == "200") && !httpBody.empty();
    return out;
}

namespace {
std::atomic<HttpsPostFn> g_httpsTransport{ nullptr };
}  // namespace

void SetHttpsPostTransport(HttpsPostFn fn) {
    g_httpsTransport.store(fn, std::memory_order_release);
}

HttpsPostFn GetHttpsPostTransport() {
    return g_httpsTransport.load(std::memory_order_acquire);
}

}  // namespace agent
}  // namespace jk