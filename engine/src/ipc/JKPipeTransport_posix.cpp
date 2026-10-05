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

namespace jk {
namespace ipc {

namespace {

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

std::unique_ptr<JKPipeTransport> JKPipeTransport::CreateServer(const std::string& name) {
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
    // Backlog 1: a single-server transport, one client at a time (R-D3).
    if (::listen(listener, 1) != 0) {
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

std::unique_ptr<JKPipeTransport> JKPipeTransport::ConnectClient(const std::string& name) {
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
