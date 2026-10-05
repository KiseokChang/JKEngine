#ifndef JKNET_H
#define JKNET_H
// jk::net — Winsock boundary adapter (docs/68 W8 stage-1). Winsock impl in
// JKNet_win32.cpp (this TU owns winsock2.h — it must precede windows.h);
// posix impl in JKNet_posix.cpp (stage 2 plan D: POSIX TCP; the unix-socket
// transport lives in JKPipeTransport — this jk::net is AF_INET only). Socket handles are
// u64 (SOCKET) so consumers stay windows.h-clean.
#include <cstdint>
#include <string>

namespace jk::net {
using Socket = std::uint64_t;
// Winsock INVALID_SOCKET == (SOCKET)(~0) — same value for posix failure.
constexpr Socket kInvalidSocket = 0xFFFFFFFFFFFFFFFFull;

// WSADATA/WSAStartup(MAKEWORD(2,2)) — false on failure. Non-Windows: true.
bool Startup();
// socket(AF_INET,SOCK_STREAM)+SO_REUSEADDR(TRUE)+bind+listen(backlog).
// bindIp empty = INADDR_ANY (LAN+loopback, token gate — docs/57 §9);
// non-empty = inet_addr(bindIp). kInvalidSocket on any failure.
// boundPortOut (nullable) reports the actual bound port via getsockname —
// port=0 (ephemeral) callers (stage-2 servers, selftest case 15) need it.
// On getsockname failure the out-param is left untouched (caller must init
// to 0 and treat 0 on success as "unknown" — opus final-review NIT-8).
// (R-C4: added 2026-10-05 — original 3-param signature keeps working via
// the default, so existing call sites are unaffected.)
Socket ListenTcp(const std::string& bindIp, std::uint16_t port, int backlog,
                 std::uint16_t* boundPortOut = nullptr);
// accept(listener) — kInvalidSocket on failure. Consumer loop stays
// detached-thread-per-conn (docs/57 as-built).
Socket Accept(Socket listener);
// Blocking recv-all loop: recv()==<=0 on EOF/error → false (original
// RecvAll(main.cpp:300) semantics verbatim).
bool RecvAll(Socket, void* buf, std::size_t n);
// Raw send() passthrough: >=1 bytes sent, <=0 EOF/error — send-loop
// callers (WsSendFrame, SendAll) keep their own loop shape (R-C2).
int Send(Socket, const void* data, int len);
// SO_RCVTIMEO/SO_SNDTIMEO both set to timeoutMs (30s slowloris/dead-phone
// guard, opus MAJOR-3). Errors ignored — original ignored them too.
void SetTimeouts(Socket, std::uint32_t timeoutMs);
// shutdown(SD_BOTH).
void ShutdownBoth(Socket);
// closesocket().
void Close(Socket);
// Primary LAN IP via UDP-connect trick (no packets sent) — "?" fallback,
// original PrimaryIp(main.cpp:1670) verbatim.
std::string PrimaryIp();
}  // namespace jk::net

#endif  // JKNET_H