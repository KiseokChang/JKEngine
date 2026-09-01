#include <ipc/JKPipeTransport.h>

#ifndef _WIN32

#include <cstdio>
#include <unistd.h>

namespace jk {
namespace ipc {

JKPipeTransport::JKPipeTransport(NativeHandle handle, bool server)
    : handle_(handle), serverSide_(server), connected_(handle >= 0) {
}

JKPipeTransport::~JKPipeTransport() {
    Close();
}

std::unique_ptr<JKPipeTransport> JKPipeTransport::CreateServer(const std::string& name) {
    (void)name;
    std::fprintf(stderr, "JKPipeTransport::CreateServer not implemented for POSIX\n");
    return nullptr;
}

std::unique_ptr<JKPipeTransport> JKPipeTransport::ConnectClient(const std::string& name) {
    (void)name;
    std::fprintf(stderr, "JKPipeTransport::ConnectClient not implemented for POSIX\n");
    return nullptr;
}

bool JKPipeTransport::Write(const void* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

bool JKPipeTransport::Read(void* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

void JKPipeTransport::Close() {
    if (handle_ >= 0) {
        close(handle_);
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
