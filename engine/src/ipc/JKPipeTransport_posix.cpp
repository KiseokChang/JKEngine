#include <ipc/JKPipeTransport.h>

#ifndef _WIN32

#include <cerrno>
#include <cstddef>  // offsetof
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <fs/JKFs.h>  // TempDir — docs/78 TX3 폰 /tmp 부재 폴백(가드 봉합과 동일 지주)

namespace jk {
namespace ipc {

namespace {

// The literal `\\.\pipe\` prefix of the shared wire-endpoint constant
// (jk::ipc::kWindowServerPipeName) — 2 backslashes, a dot, a backslash.
constexpr const char* kPipePrefix = "\\\\.\\pipe\\";

// Win32 named-pipe name → unix socket path mapping (stage-3 full-build task
// 5, docs/69 §4 consumer wiring ②). Posix-only: on _WIN32 this TU body does
// not exist, so the mapping is inert there — call sites (JKWindowServer
// acceptor, JKClientSurface/JKAgentClient clients, src/main.cpp, jkwinserver)
// keep passing the shared constant UNCHANGED and both factories fold it in
// one place.
//
// Rule: a name with the `\\.\pipe\` prefix loses the prefix, any '/' folds to
// '_' (no path traversal out of the socket base), and the result lands at
// <base>/<folded>.sock. The base is jk::fs::TempDir(): docs/78 TX3 폰 실측 —
// Android has no writable /tmp for an app uid (bind fails EACCES), so the
// base follows the instance-lock seal (JKInstanceLock_posix.cpp) — $TMPDIR
// (Termux: files/usr/tmp), else /tmp/ (glibc parity). Everything else
// ("...\\JKWindowServerPipe" style
// already-basename names included) is R-D3's caller-supplied socket PATH and
// is used as-is — the plan-D "name used as-is" contract stays for those
// (posix_selftest case 5 relies on it).
std::string MapEndpointName(const std::string& name) {
    const size_t prefixLen = std::strlen(kPipePrefix);
    if (name.compare(0, prefixLen, kPipePrefix) != 0) return name;
    std::string folded = name.substr(prefixLen);
    for (char& c : folded)
        if (c == '/') c = '_';
    return jk::fs::TempDir() + folded + ".sock";
}

// Fill a sockaddr_un from a caller-supplied name. R-D3: the name is used
// as-is as the socket path — no /tmp forcing, no state-dir logic. Returns
// false (before touching any fd) when the path cannot fit sun_path, or is
// empty (an empty sun_path is not a valid bind target).
bool MakeUnixAddr(const std::string& name, sockaddr_un* out, socklen_t* outLen) {
    if (name.empty() || name.size() >= sizeof(out->sun_path)) return false;
    std::memset(out, 0, sizeof(*out));
    out->sun_family = AF_UNIX;
    std::memcpy(out->sun_path, name.c_str(), name.size());
    *outLen = static_cast<socklen_t>(
        offsetof(sockaddr_un, sun_path) + name.size() + 1);
    return true;
}

// Is there a LIVE server currently bound to this path? A successful connect
// means yes (it sits in the live listener's backlog); ECONNREFUSED means the
// file is a stale leftover of a dead server.
bool LiveServerHoldsPath(const std::string& name) {
    sockaddr_un addr{};
    socklen_t len = 0;
    if (!MakeUnixAddr(name, &addr, &len)) return false;
    const int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (probe < 0) return false;
    if (::connect(probe, reinterpret_cast<const sockaddr*>(&addr), len) != 0) {
        const int err = errno;
        ::close(probe);
        if (err == ECONNREFUSED) return false;  // stale file, nobody home
        // Other probe errors (EACCES, ENOENT races, ...): stay fail-closed —
        // the caller lets bind() adjudicate instead of guessing.
        return true;
    }
    ::close(probe);
    return true;
}

} // namespace

JKPipeTransport::JKPipeTransport(NativeHandle handle, bool server)
    : handle_(handle), serverSide_(server), connected_(handle >= 0) {
}

JKPipeTransport::~JKPipeTransport() {
    Close();
}

std::unique_ptr<JKPipeTransport> JKPipeTransport::CreateServer(const std::string& endpointName) {
    // Single-point name→path fold (MapEndpointName above) — the body keeps the
    // old `name` spelling and all diagnostics speak the mapped socket path.
    const std::string name = MapEndpointName(endpointName);
    // Stale/live triage of a leftover socket file before bind (win32 parity:
    // a name owned by someone else must fail closed like a CreateNamedPipe
    // open failure):
    //   - exists and NOT a socket  -> refuse (never unlink a non-socket)
    //   - exists and LIVE server   -> leave the file in place so bind() below
    //                                 fails EADDRINUSE -> fail closed
    //   - exists and stale socket  -> unlink BEFORE bind (R-D3)
    //   - does not exist           -> bind creates it
    struct stat st {};
    if (::stat(name.c_str(), &st) == 0) {
        if (!S_ISSOCK(st.st_mode)) {
            std::fprintf(stderr,
                         "JKPipeTransport::CreateServer('%s') failed: path "
                         "exists and is not a socket\n",
                         name.c_str());
            return nullptr;
        }
        if (LiveServerHoldsPath(name)) {
            std::fprintf(stderr,
                         "JKPipeTransport::CreateServer('%s') failed: a live "
                         "server already holds that socket\n",
                         name.c_str());
            return nullptr;
        }
        ::unlink(name.c_str());  // stale socket file — safe to reclaim
    }

    sockaddr_un addr{};
    socklen_t addrLen = 0;
    if (!MakeUnixAddr(name, &addr, &addrLen)) {
        std::fprintf(stderr,
                     "JKPipeTransport::CreateServer('%s') failed: path too "
                     "long for sockaddr_un.sun_path (%zu >= %zu)\n",
                     name.c_str(), name.size() + 1, sizeof(addr.sun_path));
        return nullptr;
    }

    const int listener = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) {
        std::fprintf(stderr, "JKPipeTransport::CreateServer('%s') failed: "
                             "socket (%s)\n",
                     name.c_str(), std::strerror(errno));
        return nullptr;
    }
    if (::bind(listener, reinterpret_cast<const sockaddr*>(&addr), addrLen) != 0) {
        const int err = errno;
        ::close(listener);
        std::fprintf(stderr, "JKPipeTransport::CreateServer('%s') failed: "
                             "bind (%s)\n",
                     name.c_str(), std::strerror(err));
        return nullptr;
    }
    // backlog 8 (plan H — docs/70 §6 #5): 1이면 스태일/phantom connect 한
    // 개가 단일 대기 슬롯을 점유해 이후 클라 connect가 블록한다. 서버는
    // accept를 빠르게 소진하지만 부트 직후 taskbar 스폰+프루브 connect가
    // 겹치는 순간을 커버한다. (최종리뷰 라이더: §6 #4 오인 교정 — 이 항목의
    // 소속은 §6 #5의 "전송 phantom 연결" 줄이다. R-D3 "one client at a
    // time" 직렬 서빙 계약은 serial accept 루프가 그대로 유지 — backlog는
    // 대기열 완충일 뿐 서빙 동시성을 넓히지 않는다.)
    if (::listen(listener, 8) != 0) {
        const int err = errno;
        ::close(listener);
        std::fprintf(stderr, "JKPipeTransport::CreateServer('%s') failed: "
                             "listen (%s)\n",
                     name.c_str(), std::strerror(err));
        return nullptr;
    }

    // Block until a client connects (win32 ConnectNamedPipe INFINITE parity).
    // EINTR retry on the blocking call.
    int conn = -1;
    while (true) {
        conn = ::accept(listener, nullptr, nullptr);
        if (conn >= 0) break;
        if (errno == EINTR) continue;
        const int err = errno;
        ::close(listener);
        std::fprintf(stderr, "JKPipeTransport::CreateServer('%s') failed: "
                             "accept (%s)\n",
                     name.c_str(), std::strerror(err));
        return nullptr;
    }

    // The connection now owns the conversation; the listener socket has served
    // its purpose (backlog already drained by accept). Closing it here means a
    // second client is refused instead of queueing against a listener nobody
    // will ever accept from — single-instance fail-closed parity.
    ::close(listener);

    return std::unique_ptr<JKPipeTransport>(new JKPipeTransport(conn, true));
}

std::unique_ptr<JKPipeTransport> JKPipeTransport::ConnectClient(const std::string& endpointName) {
    // Same single-point fold as CreateServer — both factories must see the
    // same mapped path or server and client would part ways.
    const std::string name = MapEndpointName(endpointName);
    sockaddr_un addr{};
    socklen_t addrLen = 0;
    if (!MakeUnixAddr(name, &addr, &addrLen)) {
        std::fprintf(stderr, "JKPipeTransport::ConnectClient('%s') failed: "
                             "path too long for sockaddr_un.sun_path "
                             "(%zu >= %zu)\n",
                     name.c_str(), name.size() + 1, sizeof(addr.sun_path));
        return nullptr;
    }
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        std::fprintf(stderr, "JKPipeTransport::ConnectClient('%s') failed: "
                             "socket (%s)\n",
                     name.c_str(), std::strerror(errno));
        return nullptr;
    }
    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), addrLen) != 0) {
        const int err = errno;
        ::close(fd);
        std::fprintf(stderr, "JKPipeTransport::ConnectClient('%s') failed: "
                             "connect (%s)\n",
                     name.c_str(), std::strerror(err));
        return nullptr;
    }
    return std::unique_ptr<JKPipeTransport>(new JKPipeTransport(fd, false));
}

bool JKPipeTransport::Write(const void* data, size_t len) {
    if (!connected_ || handle_ < 0) return false;

    // MSG_NOSIGNAL: a dead peer must surface as a -1/EPIPE failure here, not
    // as a process-wide SIGPIPE kill (the thread that hits the broken wire is
    // arbitrary — win32 WriteFile has no such hazard to mirror).
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t remaining = len;
    while (remaining > 0) {
        const size_t toWrite =
            remaining > 0x7FFFFFFF ? 0x7FFFFFFF : remaining;  // win32 chunk cap
        const ssize_t n = ::send(handle_, p, toWrite, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            // EPIPE / ECONNRESET / ENOTCONN: broken wire. Do NOT Close() here
            // — another thread may still be parked in Read on this handle
            // (win32 parity: mark dead, teardown closes).
            connected_ = false;
            return false;
        }
        if (n == 0) {  // defensive: a stream socket never sends 0 bytes
            connected_ = false;
            return false;
        }
        p += n;
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

bool JKPipeTransport::Read(void* data, size_t len) {
    if (!connected_ || handle_ < 0) return false;

    uint8_t* p = static_cast<uint8_t*>(data);
    size_t remaining = len;
    while (remaining > 0) {
        const size_t toRead =
            remaining > 0x7FFFFFFF ? 0x7FFFFFFF : remaining;  // win32 chunk cap
        const ssize_t n = ::recv(handle_, p, toRead, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            // ECONNRESET / ENOTCONN (and the ECONNRESET-like wake-up after a
            // CancelPendingIo shutdown): pending operation FAILED, win32
            // parity. Mark dead — no Close() here (thread-safety note above).
            connected_ = false;
            return false;
        }
        if (n == 0) {
            // Peer EOF — includes the wake-up of a reader cancelled via
            // CancelPendingIo (shutdown(SHUT_RDWR) makes the blocked recv
            // report 0). Same verdict as win32's ERROR_OPERATION_ABORTED /
            // ERROR_BROKEN_PIPE: failure, fail-closed.
            connected_ = false;
            return false;
        }
        p += n;
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

void JKPipeTransport::CancelPendingIo() {
    if (handle_ >= 0) {
        // Unix stream sockets have no overlapped I/O, so the posix equivalent
        // of CancelIoEx aborting I/O issued by ANY thread is shutting the
        // socket down: a reader parked in recv on this fd wakes immediately
        // (0/ECONNRESET) and Read() returns failure. Win32 contract upheld:
        // the pending operation returns failure — Close() must still only run
        // once the reader thread has joined; this call does NOT close the fd.
        ::shutdown(handle_, SHUT_RDWR);
    }
}

void JKPipeTransport::Close() {
    if (handle_ >= 0) {
        // Win32 parity: server = FlushFileBuffers + DisconnectNamedPipe +
        // CloseHandle; client = FlushFileBuffers + CloseHandle. On a socket,
        // close() flushes buffered data; the explicit shutdown first makes any
        // peer parked in recv see EOF the way DisconnectNamedPipe does.
        ::shutdown(handle_, SHUT_RDWR);
        ::close(handle_);
        handle_ = -1;
    }
    connected_ = false;
}

bool JKPipeTransport::IsConnected() const {
    return connected_;
}

} // namespace ipc
} // namespace jk

#endif // !_WIN32
