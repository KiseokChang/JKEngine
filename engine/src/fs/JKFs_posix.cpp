#ifndef _WIN32
// Posix impl of jk::fs::GetExecutablePath (stage 2 plan D task 1). Mirrors the
// win32 body's shape: grow-on-truncation loop, empty string on failure.
#include "../../include/fs/JKFs.h"

#include <string>

#include <cstdlib>   // getenv
#include <filesystem>
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

// Temp dir with trailing '/' (stage-3 task 7), posix leg. $TMPDIR when set and
// an existing directory, else /tmp — a TMPDIR that is set but unusable degrades
// to /tmp rather than poisoning the extraction path; if even /tmp is not a
// directory the win32 leg's fail-soft "." (the original call sites' initial
// value) comes back. ec neutral-type only — no throwing overload (no
// try/catch consumers of this TU).
std::string TempDir() {
    const char* env = std::getenv("TMPDIR");
    std::string dir = (env != nullptr && env[0] != '\0') ? std::string(env)
                                                         : std::string("/tmp");
    if (dir.empty() || dir.back() != '/') dir += '/';
    std::error_code ec;
    const bool usable =
        std::filesystem::is_directory(std::filesystem::path(dir), ec) && !ec;
    if (!usable) {
        if (dir != "/tmp/") {
            std::error_code tmpEc;
            if (std::filesystem::is_directory(std::filesystem::path("/tmp/"),
                                              tmpEc) &&
                !tmpEc)
                return std::string("/tmp/");
        }
        return std::string(".");  // win32 leg fail-soft twin
    }
    return dir;
}

// clock_cast port, posix leg (docs/78 TX2): libstdc++ (WSL/glibc) implements
// the C++20 original — but libc++ (Termux/bionic) never did, and there
// file_clock's epoch is the Unix epoch with nanosecond ticks, making file→
// system a plain duration cast that yields the same true wall-clock values
// as clock_cast would. Either branch keeps every call site's numeric form.
std::chrono::system_clock::time_point FileTimeToSys(
    const std::filesystem::file_time_type& ft) {
#if defined(_LIBCPP_VERSION)
    // libc++: file_time_type is nanoseconds since the Unix epoch — a plain
    // duration recast into a system_clock time_point is the whole conversion
    // (std::time_point_cast refuses cross-clock per the standard's own
    // same-clock requirement, so build the time_point from the duration).
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            ft.time_since_epoch()));
#else
    return std::chrono::clock_cast<std::chrono::system_clock>(ft);
#endif
}

}  // namespace fs
}  // namespace jk

#endif  // _WIN32

