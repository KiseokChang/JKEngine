// jk::net posix STUB (docs/68 W8 stage-1, plan C task 3). Every public
// header symbol is covered (plan B W3 lesson: a missed stub member is the
// latent trap that resurfaces in stage 2). Stage 2 replaces this TU with the
// unix-socket implementation — <sys/socket.h> socket/bind/listen/accept/
// recv/send + <arpa/inet.h> inet_addr/inet_ntop, kInvalidSocket = ~0 (the
// fd table never reaches it) — and flips the consumers' expectations to the
// same signature set. Until then every non-Windows consumer sees
// Startup()==true, ListenTcp/Accept==kInvalidSocket, RecvAll==false,
// Send==-1, SetTimeouts/ShutdownBoth==no-op, PrimaryIp=="?" — i.e. "net not
// wired yet", never a silent half-open socket.
#include <net/JKNet.h>

#include <string>

namespace jk::net {

bool Startup() { return true; }

Socket ListenTcp(const std::string& bindIp, std::uint16_t port, int backlog,
                 std::uint16_t* boundPortOut) {
    (void)bindIp;
    (void)port;
    (void)backlog;
    (void)boundPortOut;
    return kInvalidSocket;
}

Socket Accept(Socket listener) {
    (void)listener;
    return kInvalidSocket;
}

bool RecvAll(Socket s, void* buf, std::size_t n) {
    (void)s;
    (void)buf;
    (void)n;
    return false;
}

int Send(Socket s, const void* data, int len) {
    (void)s;
    (void)data;
    (void)len;
    return -1;
}

void SetTimeouts(Socket s, std::uint32_t timeoutMs) {
    (void)s;
    (void)timeoutMs;
}

void ShutdownBoth(Socket s) { (void)s; }

void Close(Socket s) { (void)s; }

std::string PrimaryIp() { return "?"; }

}  // namespace jk::net