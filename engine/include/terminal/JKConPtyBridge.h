#ifndef JKCONPTYBRIDGE_H
#define JKCONPTYBRIDGE_H

// ConPTY bridge (docs/22 §2): spawns a shell attached to a Windows pseudo
// console, pumps its output on a reader thread, and forwards input/resizes
// from the UI thread. The kernel32 ConPTY entry points are loaded dynamically
// (GetProcAddress) so no ConPTY SDK header is required; HPCON is treated as
// an opaque void*.
//
// Lifecycle: Start() → WriteInput/DrainOutput/Resize → Stop() (also run by
// the destructor). Close order per docs/22 §8.2: ClosePseudoConsole first
// (conhost flushes and closes the pipes, which unblocks the reader), then a
// bounded wait on the shell, then TerminateProcess, then handle cleanup.

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace jk {

class JKConPtyBridge {
public:
    JKConPtyBridge() = default;
    ~JKConPtyBridge();

    JKConPtyBridge(const JKConPtyBridge&) = delete;
    JKConPtyBridge& operator=(const JKConPtyBridge&) = delete;

    // Spawns `commandLine` (e.g. "powershell.exe -NoLogo") in a pseudo console
    // of the given cell size. Returns false (object stays inert) on failure.
    bool Start(const std::string& commandLine, int cols, int rows);

    bool IsValid() const { return started_; }
    bool ShellExited() const { return exited_; }

    // Appends pty output bytes to `out` (drain from the UI thread).
    void DrainOutput(std::string& out);

    void WriteInput(const char* data, size_t len);
    void Resize(int cols, int rows);

    // Tears the shell down. Idempotent.
    void Stop();

private:
    void ReaderThread();

    std::thread reader_;
    std::mutex outMutex_;
    std::string outBuf_;                 // bytes not yet drained (cap 1 MiB)

    // Opaque Win32 handles (HANDLE / HPCON) — never dereferenced.
    void* hpcon_ = nullptr;
    void* proc_ = nullptr;
    void* procThread_ = nullptr;
    void* inWrite_ = nullptr;            // our end → pty stdin
    void* outRead_ = nullptr;            // our end ← pty stdout

    std::atomic<bool> started_{ false };
    std::atomic<bool> exited_{ false };
};

} // namespace jk

#endif // JKCONPTYBRIDGE_H