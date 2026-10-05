#ifndef _WIN32
// jk::net posix impl (docs/68 W8 stage-2, plan D task 3) — the stage-1 stub
// replaced by real <sys/socket.h> TCP. Mirrors JKNet_win32.cpp error/return
// contract function-for-function (9 public symbols, docs/superpowers/plans/
// 2026-10-05-linux-stage1-surgery-c.md R-C1..R-C4):
//  - Startup       = no-op true (WSAStartup has no posix analogue; R-C3 keeps
//                    cleanup deliberately absent).
//  - ListenTcp     = socket+SO_REUSEADDR(1)+bind+listen, win32 parity. A bad
//                    bindIp string makes inet_addr return INADDR_NONE on BOTH
//                    platforms, so both compute s_addr=255.255.255.255 — but
//                    win32 bind() rejects that (WSAEADDRNOTAVAIL) while Linux
//                    bind() ACCEPTS it, hence the explicit fail-closed guard
//                    below (selftest case 3B exercises it). boundPortOut
//                    (R-C4) reports the ACTUAL bound port via getsockname
//                    (host order) and is untouched on any failure.
//  - RecvAll       = same exact-size loop; Send = raw send() passthrough
//                    with NO retry loop (R-C2 — loop senders keep their own
//                    shape).
//  - SetTimeouts   = SO_RCVTIMEO/SO_SNDTIMEO via timeval {ms/1000,
//                    (ms%1000)*1000} — the posix stand-in for win32's DWORD
//                    milliseconds; errors ignored, same as win32.
//  - ShutdownBoth  = shutdown(SHUT_RDWR) (== SD_BOTH == 2 win32 parity).
//  - PrimaryIp     = UDP-connect trick verbatim (no packets ever sent),
//                    10.255.255.254, same as win32.
// EINTR widening (posix-only; win32 has no analogue): accept() and the
// RecvAll recv() loop retry on EINTR — a spuriously interrupted blocking
// syscall must not surface as kInvalidSocket/EOF to callers whose win32
// twins never saw EINTR. Send keeps raw semantics (no retry, R-C2 parity).
// Posix socket fds are int; they are stored in the u64 Socket via
// static_cast (positive fd sign-extends to the same value; kInvalidSocket
// ~0ull can never collide with a real fd, the fd table never reaches it).
// The _WIN32 guard keeps this TU compile-empty on Windows (JKFs_posix.cpp /
// JKProcess_posix.cpp precedent) — the two net TUs must never both define
// the 9 strong symbols in one link.
#include <arpa/inet.h>   // inet_addr, inet_ntop, htonl/htons/ntohs
#include <netinet/in.h>  // sockaddr_in, INADDR_ANY
#include <sys/socket.h>  // socket/bind/listen/accept/recv/send/shutdown
#include <sys/time.h>    // timeval (SO_RCVTIMEO/SO_SNDTIMEO)
#include <unistd.h>      // close

#include <cerrno>
#include <cstdint>
#include <string>

#include <net/JKNet.h>

namespace jk::net {

bool Startup() {
    return true;  // non-Windows contract: no WSAStartup analogue
}

Socket ListenTcp(const std::string& bindIp, std::uint16_t port, int backlog,
                 std::uint16_t* boundPortOut) {
    const int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return kInvalidSocket;
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    // bindIp empty = INADDR_ANY (LAN+loopback, token gate — docs/57 §9);
    // non-empty = inet_addr (the caller validated it — bridge config gate).
    addr.sin_addr.s_addr =
        bindIp.empty() ? INADDR_ANY : inet_addr(bindIp.c_str());
    if (!bindIp.empty() && addr.sin_addr.s_addr == INADDR_NONE) {
        // Posix widening (stage-2 as-built): glibc inet_addr also returns
        // INADDR_NONE for garbage like "999.999.999.999", so both platforms
        // compute s_addr=0xFFFFFFFF — but win32's bind(255.255.255.255)
        // fails (WSAEADDRNOTAVAIL) and reaches kInvalidSocket through the
        // normal path, while Linux bind() ACCEPTS the broadcast address.
        // Fail closed here to keep the win32 observable outcome instead of
        // silently leaving a broadcast-bound listener in the fd table.
        close(s);
        return kInvalidSocket;
    }
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(s, backlog) != 0) {
        close(s);  // stage-1 documented widening: the failed listener is
                   // closed before kInvalidSocket (win32 twin does the
                   // same; visible behavior — bind failure reported and
                   // no half-bound socket left in the table).
        return kInvalidSocket;
    }
    if (boundPortOut) {  // R-C4: report the ACTUAL bound port (port=0!)
        sockaddr_in bound{};
        socklen_t len = sizeof(bound);
        if (getsockname(s, reinterpret_cast<sockaddr*>(&bound), &len) == 0) {
            *boundPortOut = ntohs(bound.sin_port);
        }
        // getsockname failure: out-param left untouched (caller inits to 0).
    }
    return static_cast<Socket>(s);
}

Socket Accept(Socket listener) {
    int c;
    do {  // EINTR retry — posix-only widening, see file comment
        c = accept(static_cast<int>(listener), nullptr, nullptr);
    } while (c < 0 && errno == EINTR);
    return c < 0 ? kInvalidSocket : static_cast<Socket>(c);
}

bool RecvAll(Socket s, void* buf, std::size_t n) {
    // Exact-size loop (jkbridge RecvAll verbatim shape) — TCP may fragment
    // anywhere; every header read uses this. EINTR retries (posix-only
    // widening); r <= 0 is EOF/error -> false.
    auto* p = static_cast<char*>(buf);
    std::size_t got = 0;
    while (got < n) {
        ssize_t r = recv(static_cast<int>(s), p + got,
                         static_cast<size_t>(n - got), 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        got += static_cast<std::size_t>(r);
    }
    return true;
}

int Send(Socket s, const void* data, int len) {
    // R-C2: raw passthrough — one send() call, r<=0 means EOF/error. The
    // loop shape stays with the caller (WsSendFrame/SendAll). No EINTR
    // retry: win32 Send has none either (parity over convenience). MSG_NOSIGNAL
    // so a peer-closed write cannot kill the process by SIGPIPE (a hazard
    // win32 lacks — the transport TU already uses the same flag; opus
    // final-review finding: jk::net on posix had none).
    return static_cast<int>(send(static_cast<int>(s),
                                 static_cast<const char*>(data),
                                 static_cast<size_t>(len), MSG_NOSIGNAL));
}

void SetTimeouts(Socket s, std::uint32_t timeoutMs) {
    const int h = static_cast<int>(s);
    // timeval is the posix stand-in for win32's DWORD milliseconds.
    const timeval tv{static_cast<time_t>(timeoutMs / 1000),
                     static_cast<suseconds_t>((timeoutMs % 1000) * 1000)};
    // Errors ignored — the absorbed win32 HandleConn pair ignored them.
    setsockopt(h, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(h, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

void ShutdownBoth(Socket s) {
    shutdown(static_cast<int>(s), SHUT_RDWR);  // == SD_BOTH (2)
}

void Close(Socket s) { close(static_cast<int>(s)); }

std::string PrimaryIp() {
    // UDP-connect trick (no packets sent) — jkbridge PrimaryIp verbatim.
    const int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return "?";
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(9);
    dst.sin_addr.s_addr = htonl(0x0AFFFFFE);  // 10.255.255.254 — never sent
    std::string ip = "?";
    if (connect(s, reinterpret_cast<sockaddr*>(&dst), sizeof(dst)) == 0) {
        sockaddr_in local{};
        socklen_t len = sizeof(local);
        if (getsockname(s, reinterpret_cast<sockaddr*>(&local), &len) == 0) {
            char buf[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) {
                ip = buf;
            }
        }
    }
    close(s);
    return ip;
}

}  // namespace jk::net
#endif  // _WIN32
