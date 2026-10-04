// jk::net win32 impl (docs/68 W8 stage-1, plan C task 3). This TU OWNS
// winsock2.h — MSDN precedence rule: winsock2.h must be included BEFORE
// windows.h (or any header that pulls winsock.h) to avoid the winsock.h
// duplicate-definition conflict. Consumers stay windows.h-clean: they see
// only jk::net::Socket (u64) and these functions.
// Contracts (stage 2 consumes this header — docs/superpowers/plans/
// 2026-10-05-linux-stage1-surgery-c.md R-C1..R-C4):
//  - Startup       = WSAStartup(MAKEWORD(2,2)); WSACleanup deliberately ABSENT
//                    (R-C3: the absorbed callers never called it — YAGNI).
//  - ListenTcp     = socket+SO_REUSEADDR(TRUE)+bind+listen, verbatim from
//                    jkbridge main() (docs/57 §9 bind contract). boundPortOut
//                    (R-C4) reports the actual bound port (getsockname, host
//                    order) so port=0 ephemeral callers can refer to it.
//  - RecvAll       = jkbridge RecvAll loop verbatim; Send = raw send()
//                    passthrough (R-C2 — loop senders keep their shape);
//  - SetTimeouts   = SO_RCVTIMEO/SO_SNDTIMEO both, errors ignored (as before);
//  - PrimaryIp     = UDP-connect trick verbatim (no packets ever sent).
#ifdef _WIN32

#include <winsock2.h>   // MUST precede any windows.h-adjacent header
#include <ws2tcpip.h>   // inet_addr/inet_ntop/getsockname

#include <net/JKNet.h>

#include <string>

namespace jk::net {

bool Startup() {
    WSADATA wsa{};
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

Socket ListenTcp(const std::string& bindIp, std::uint16_t port, int backlog,
                 std::uint16_t* boundPortOut) {
    const SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return kInvalidSocket;
    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    // bindIp empty = INADDR_ANY (LAN+loopback, token gate — docs/57 §9);
    // non-empty = inet_addr (the caller validated it — bridge config gate).
    addr.sin_addr.s_addr =
        bindIp.empty() ? INADDR_ANY : inet_addr(bindIp.c_str());
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(s, backlog) != 0) {
        closesocket(s);  // absorbed callers leaked the socket here; the
                         // adapter closes — the visible behavior (bind
                         // failure reported) is unchanged.
        return kInvalidSocket;
    }
    if (boundPortOut) {  // R-C4: report the ACTUAL bound port (port=0!)
        sockaddr_in bound{};
        int len = sizeof(bound);
        if (getsockname(s, reinterpret_cast<sockaddr*>(&bound), &len) == 0) {
            *boundPortOut = ntohs(bound.sin_port);
        }
    }
    return static_cast<Socket>(s);
}

Socket Accept(Socket listener) {
    return static_cast<Socket>(accept(static_cast<SOCKET>(listener), nullptr,
                                      nullptr));
}

bool RecvAll(Socket s, void* buf, std::size_t n) {
    // jkbridge RecvAll(main.cpp:305) verbatim — TCP may fragment anywhere;
    // every header read uses this exact-size loop.
    auto* p = static_cast<char*>(buf);
    std::size_t got = 0;
    while (got < n) {
        const int r =
            recv(static_cast<SOCKET>(s), p + got, static_cast<int>(n - got), 0);
        if (r <= 0) return false;
        got += static_cast<std::size_t>(r);
    }
    return true;
}

int Send(Socket s, const void* data, int len) {
    // R-C2: raw passthrough — one send() call, r<=0 means EOF/error. The
    // loop shape stays with the caller (WsSendFrame/SendAll).
    return send(static_cast<SOCKET>(s), static_cast<const char*>(data), len,
                0);
}

void SetTimeouts(Socket s, std::uint32_t timeoutMs) {
    const DWORD t = static_cast<DWORD>(timeoutMs);
    const SOCKET h = static_cast<SOCKET>(s);
    // Errors ignored — the absorbed HandleConn setsockopt pair ignored them.
    setsockopt(h, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&t), sizeof(t));
    setsockopt(h, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&t), sizeof(t));
}

void ShutdownBoth(Socket s) {
    shutdown(static_cast<SOCKET>(s), SD_BOTH);
}

void Close(Socket s) { closesocket(static_cast<SOCKET>(s)); }

std::string PrimaryIp() {
    // UDP-connect trick (no packets sent) — jkbridge PrimaryIp verbatim.
    const SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return "?";
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(9);
    dst.sin_addr.s_addr = htonl(0x0AFFFFFE);  // 10.255.255.254 — never sent
    std::string ip = "?";
    if (connect(s, reinterpret_cast<sockaddr*>(&dst), sizeof(dst)) == 0) {
        sockaddr_in local{};
        int len = sizeof(local);
        if (getsockname(s, reinterpret_cast<sockaddr*>(&local), &len) == 0) {
            char buf[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) {
                ip = buf;
            }
        }
    }
    closesocket(s);
    return ip;
}

}  // namespace jk::net
#endif  // _WIN32