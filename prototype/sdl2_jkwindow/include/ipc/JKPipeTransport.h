#ifndef JKPIPETRANSPORT_H
#define JKPIPETRANSPORT_H

#include <ipc/JKWireProtocol.h>
#include <memory>
#include <string>

namespace jk {
namespace ipc {

// Windows named-pipe / POSIX domain-socket transport for the wire protocol.
class JKPipeTransport : public IWireTransport {
public:
    // Create a server-side transport and block until a client connects.
    static std::unique_ptr<JKPipeTransport> CreateServer(const std::string& name);

    // Connect to an existing server-side transport.
    static std::unique_ptr<JKPipeTransport> ConnectClient(const std::string& name);

    ~JKPipeTransport() override;

    bool Write(const void* data, size_t len) override;
    bool Read(void* data, size_t len) override;
    void Close() override;
    bool IsConnected() const override;

private:
#if defined(_WIN32)
    using NativeHandle = void*; // HANDLE
#else
    using NativeHandle = int;
#endif

    NativeHandle handle_;
    bool serverSide_ = false;
    bool connected_ = false;

    JKPipeTransport(NativeHandle handle, bool server);
};

} // namespace ipc
} // namespace jk

#endif // JKPIPETRANSPORT_H
