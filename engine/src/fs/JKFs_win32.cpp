#ifdef _WIN32
// Win32 impl of jk::fs::GetExecutablePath (docs/68 W5). Hand-declared
// dllimports, JKWindowServer.cpp:118 convention (windows.h-clean consumers).
#include "../../include/fs/JKFs.h"

extern "C" __declspec(dllimport) unsigned long __stdcall
GetModuleFileNameA(void* module, char* out, unsigned long size);
extern "C" __declspec(dllimport) unsigned long __stdcall
GetTempPathA(unsigned long nBufferLength, char* lpBuffer);

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

// Temp dir with trailing '\' (stage-3 task 7), win32 leg = GetTempPathA 원문.
// Buffer 196 — the call-site original pass (260-64) verbatim; GetTempPathA
// returns the copied string length NOT counting the NUL and 0 on failure. A
// return >= the requested size means truncation: only the first 195 bytes
// landed in the buffer, and the original call sites proceeded with what fit
// (tempDir kept whatever the API wrote) — mirror that, don't fail hard, the
// caller then fails on the fopen of the constructed name exactly as before.
std::string TempDir() {
    char buf[260] = {};
    const unsigned long n = GetTempPathA(196, buf);
    if (n == 0) {
        // Original call sites initialized tempDir to "." and only overwrote it
        // on success — API failure keeps the same "." fallback.
        return std::string(".");
    }
    // Success or truncation: the buffer carries min(n, 195) bytes + NUL — the
    // original call sites proceeded with exactly what the API wrote.
    return std::string(buf, static_cast<size_t>(n < 196 ? n : 195));
}

// clock_cast<FileTimeToSys> port, win32 leg: the MSVC STL has the C++20
// original — call it straight through (same header contract, docs/78 TX2).
std::chrono::system_clock::time_point FileTimeToSys(
    const std::filesystem::file_time_type& ft) {
    return std::chrono::clock_cast<std::chrono::system_clock>(ft);
}

}  // namespace fs
}  // namespace jk

#endif  // _WIN32
