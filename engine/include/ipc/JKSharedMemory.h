#ifndef JKSHAREDMEMORY_H
#define JKSHAREDMEMORY_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>

namespace jk {
namespace ipc {

// Cross-platform shared memory segment used for client surface pixels.
// The server creates the segment; the client opens it by name.
class JKSharedMemory {
public:
    JKSharedMemory();
    ~JKSharedMemory();

    // Create a new segment. On Windows this creates a named file mapping.
    bool Create(const std::string& name, size_t size);

    // Open an existing segment created by the server.
    bool Open(const std::string& name, size_t size);

    void Close();

    uint8_t* Data() const;
    size_t Size() const;

    bool IsValid() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ipc
} // namespace jk

#endif // JKSHAREDMEMORY_H
