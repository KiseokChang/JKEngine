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
    HANDLE h = CreateNamedPipeA(
        name.c_str(),
        PIPE_ACCESS_DUPLEX,
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

    BOOL connected = ConnectNamedPipe(h, nullptr) ? TRUE
        : (GetLastError() == ERROR_PIPE_CONNECTED);
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
        0,
        nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "CreateFileA('%s') failed: %lu\n",
                     name.c_str(), GetLastError());
        return nullptr;
    }

    return std::unique_ptr<JKPipeTransport>(new JKPipeTransport(h, false));
}

bool JKPipeTransport::Write(const void* data, size_t len) {
    if (!connected_ || handle_ == INVALID_HANDLE_VALUE) return false;

    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t remaining = len;
    while (remaining > 0) {
        DWORD written = 0;
        DWORD toWrite = remaining > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<DWORD>(remaining);
        if (!WriteFile(handle_, p, toWrite, &written, nullptr)) {
            std::fprintf(stderr, "WriteFile failed: %lu\n", GetLastError());
            Close();
            return false;
        }
        if (written == 0) {
            Close();
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
        DWORD read = 0;
        DWORD toRead = remaining > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<DWORD>(remaining);
        if (!ReadFile(handle_, p, toRead, &read, nullptr)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                Close();
                return false;
            }
            std::fprintf(stderr, "ReadFile failed: %lu\n", err);
            Close();
            return false;
        }
        if (read == 0) {
            Close();
            return false;
        }
        p += read;
        remaining -= read;
    }
    return true;
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
