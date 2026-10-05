#ifndef _WIN32
// Posix impl of jk::fs::GetExecutablePath (stage 2 plan D task 1). Mirrors the
// win32 body's shape: grow-on-truncation loop, empty string on failure.
#include "../../include/fs/JKFs.h"

#include <string>

#include <unistd.h>  // readlink

namespace jk {
namespace fs {

// The exe image path via the procfs symlink /proc/self/exe (Linux; BSDs mount
// it under procfs too — this target stays Linux/WSL). readlink does NOT NUL
// terminate, so the returned byte count is authoritative: resize() to it
// instead of strlen-scanning. A result that exactly fills the buffer is
// treated as truncation and retried with a doubled buffer (256 doubles 7
// times -> 32KiB cap), same policy as the win32 body. Total exhaustion and
// any readlink failure hand back the empty string — the caller-visible
// fallback contract is unchanged from the stub (fail-closed).
std::string GetExecutablePath() {
    static const size_t kFirstTry = 256;
    static const int kMaxAttempts = 8;  // 256 doubles 7 times -> 32KiB cap
    std::string buf(kFirstTry, '\0');
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const ssize_t len =
            readlink("/proc/self/exe", &buf[0], buf.size());
        if (len < 0) return std::string();  // failure -> empty (contract)
        if (static_cast<size_t>(len) < buf.size()) {
            buf.resize(static_cast<size_t>(len));  // bytes read, not strlen
            return buf;
        }
        // len == buffer size: possibly truncated, double and retry.
        buf.resize(buf.size() * 2);
    }
    // A path that still fills 32KiB does not happen; prefer fail-closed over
    // handing back possibly truncated bytes.
    return std::string();
}

}  // namespace fs
}  // namespace jk

#endif  // _WIN32
