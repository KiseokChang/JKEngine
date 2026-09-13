#ifndef JK_WIRE_ENDPOINTS_H
#define JK_WIRE_ENDPOINTS_H

// Wire endpoint names shared by every process on the bus (server host,
// client host, agent clients). The server's acceptor listens on the same
// name all clients connect to — one constant, no per-file literals.
namespace jk {
namespace ipc {

inline constexpr char kWindowServerPipeName[] = "\\\\.\\pipe\\JKWindowServerPipe";

} // namespace ipc
} // namespace jk

#endif // JK_WIRE_ENDPOINTS_H