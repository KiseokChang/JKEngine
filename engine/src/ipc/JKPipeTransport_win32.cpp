#include <ipc/JKPipeTransport.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

namespace jk {
namespace ipc {

JKPipeTransport::JKPipeTransport(NativeHandle handle, bool server)
    : handle_(handle), serverSide_(server), connected_(handle != INVALID_HANDLE_VALUE) {
}

JKPipeTransport::~JKPipeTransport() {
    Close();
}

std::unique_ptr<JKPipeTransport> JKPipeTransport::CreateServer(const std::string& name) {
    // FILE_FLAG_OVERLAPPED is required: multiple threads perform concurrent
    // blocking I/O on this handle (a reader thread parked in ReadFile must not
    // block the main thread's WriteFile). Synchronous (non-overlapped) handles
    // serialize one I/O at a time, which deadlocks the server instantly.
    HANDLE h = CreateNamedPipeA(
        name.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        65536,
        65536,
        0,
        nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "CreateNamedPipeA('%s') failed: %lu\n",
                     name.c_str(), GetLastError());
        return nullptr;
    }

    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        CloseHandle(h);
        return nullptr;
    }
    BOOL connected = ConnectNamedPipe(h, &ov);
    DWORD err = GetLastError();
    if (!connected && err == ERROR_PIPE_CONNECTED) {
        connected = TRUE;
    } else if (!connected && err == ERROR_IO_PENDING) {
        // Block until a client connects (same semantics as a sync accept).
        connected = (WaitForSingleObject(ov.hEvent, INFINITE) == WAIT_OBJECT_0);
        if (connected) {
            DWORD dummy = 0;
            connected = GetOverlappedResult(h, &ov, &dummy, TRUE);
        }
    }
    CloseHandle(ov.hEvent);
    if (!connected) {
        std::fprintf(stderr, "ConnectNamedPipe('%s') failed: %lu\n",
                     name.c_str(), GetLastError());
        CloseHandle(h);
        return nullptr;
    }

    return std::unique_ptr<JKPipeTransport>(new JKPipeTransport(h, true));
}

std::unique_ptr<JKPipeTransport> JKPipeTransport::ConnectClient(const std::string& name) {
    HANDLE h = CreateFileA(
        name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "CreateFileA('%s') failed: %lu\n",
                     name.c_str(), GetLastError());
        return nullptr;
    }

    return std::unique_ptr<JKPipeTransport>(new JKPipeTransport(h, false));
}

namespace {

// Run one overlapped pipe operation to completion. Returns true with the
// transferred byte count in outBytes.
bool RunOverlapped(HANDLE handle, BOOL started, OVERLAPPED& ov, DWORD& outBytes) {
    if (!started) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            outBytes = 0;
            return false;
        }
    }
    return GetOverlappedResult(handle, &ov, &outBytes, TRUE) && outBytes > 0;
}

} // namespace

bool JKPipeTransport::Write(const void* data, size_t len) {
    if (!connected_ || handle_ == INVALID_HANDLE_VALUE) return false;

    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t remaining = len;
    while (remaining > 0) {
        DWORD toWrite = remaining > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<DWORD>(remaining);
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) {
            connected_ = false;
            return false;
        }
        DWORD written = 0;
        BOOL ok = WriteFile(handle_, p, toWrite, &written, &ov);
        bool done = RunOverlapped(handle_, ok, ov, written);
        CloseHandle(ov.hEvent);
        if (!done) {
            // Do NOT Close() here: another thread (the read thread) may still
            // have I/O in flight on this handle. Just mark the transport dead;
            // the teardown path (StopReadThread: join, then Close) closes it.
            connected_ = false;
            return false;
        }
        p += written;
        remaining -= written;
    }
    return true;
}

bool JKPipeTransport::Read(void* data, size_t len) {
    if (!connected_ || handle_ == INVALID_HANDLE_VALUE) return false;

    uint8_t* p = static_cast<uint8_t*>(data);
    size_t remaining = len;
    while (remaining > 0) {
        DWORD toRead = remaining > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<DWORD>(remaining);
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) {
            connected_ = false;
            return false;
        }
        DWORD read = 0;
        BOOL ok = ReadFile(handle_, p, toRead, &read, &ov);
        bool done = RunOverlapped(handle_, ok, ov, read);
        CloseHandle(ov.hEvent);
        if (!done) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED ||
                err == ERROR_OPERATION_ABORTED) {
                // Do NOT Close() here: the main thread may be inside Write on
                // this handle, or StopReadThread may be about to join us.
                // Marking the transport dead is enough — teardown closes it.
                connected_ = false;
                return false;
            }
            connected_ = false;
            return false;
        }
        p += read;
        remaining -= read;
    }
    return true;
}

void JKPipeTransport::CancelPendingIo() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        // Unlike CancelIo (calling thread only), CancelIoEx aborts I/O issued
        // by ANY thread — it wakes a reader parked in GetOverlappedResult.
        ::CancelIoEx(handle_, nullptr);
    }
}

void JKPipeTransport::Close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        if (serverSide_) {
            FlushFileBuffers(handle_);
            DisconnectNamedPipe(handle_);
        }
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    connected_ = false;
}

bool JKPipeTransport::IsConnected() const {
    return connected_;
}

} // namespace ipc
} // namespace jk

#endif // _WIN32
