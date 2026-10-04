#ifdef _WIN32
// Win32 impl of jk::fs::GetExecutablePath (docs/68 W5). Hand-declared
// dllimports, JKWindowServer.cpp:118 convention (windows.h-clean consumers).
#include "../../include/fs/JKFs.h"

extern "C" __declspec(dllimport) unsigned long __stdcall
GetModuleFileNameA(void* module, char* out, unsigned long size);

namespace jk {
namespace fs {

// Full path of the current executable image in ANSI (CP_ACP) bytes — every
// consumer feeds the result to fopen/FindFirstFileA style A-APIs, so the
// A form round-trips; converting to real UTF-8 here would BREAK those
// consumers for CP949-representable install dirs (docs/48 CP949 lesson).
// No GetLastError retry loop: a return value that fills the buffer means
// "possibly truncated", and doubling the buffer until it doesn't is both
// sufficient and SetLastError-free.
std::string GetExecutablePath() {
    static const unsigned long kFirstTry = 1024;
    static const int kMaxAttempts = 8;  // 1024 doubles 7 times -> 128KiB cap
    std::string buf(kFirstTry, '\0');
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const unsigned long len = GetModuleFileNameA(
            nullptr,            // nullptr module = the running exe image
            &buf[0],
            static_cast<unsigned long>(buf.size()));
        if (len == 0) return std::string();  // API failure -> empty (contract)
        if (len < buf.size() - 1u) {
            buf.resize(len);
            return buf;
        }
        // len reached the buffer size: assume truncation, double and retry.
        buf.resize(buf.size() * 2);
    }
    // A path that still fills 128KiB does not happen on Windows; hand back
    // what we have rather than failing outright.
    const size_t nul = buf.find('\0');
    return nul == std::string::npos ? buf : buf.substr(0, nul);
}

}  // namespace fs
}  // namespace jk

#endif  // _WIN32