#ifndef JKFS_H
#define JKFS_H
// jk::fs — filesystem boundary adapter (docs/68 W5 stage-1). Win32 impl in
// JKFs_win32.cpp; posix impl in JKFs_posix.cpp (stage 2: readlink("/proc/self/exe")).
// Supersedes 26 GetModuleFileName call sites (plan-b-inventory §7, as-built
// correction) — the MAX_PATH-truncation variants die together by returning
// std::string.

#include <string>

namespace jk::fs {
// Full path of the current executable image, in the ANSI code page bytes
// (CP_ACP — NOT UTF-8; docs/68 W5. All consumers are A-API path eaters — a
// UTF-8 roundtrip would corrupt CP949-representation dirs, docs/48 lesson).
// On posix the bytes are native UTF-8: Linux paths are byte strings and the
// filesystem (not a code page) is their encoding, so no conversion happens —
// readlink("/proc/self/exe") bytes come straight through. Empty string on
// readlink failure (fail-closed) — callers keep their existing empty-path
// fallback.
std::string GetExecutablePath();

// Process temp directory, WITH a trailing native separator — the GetTempPathA
// contract both absorbed call sites relied on ("C:\...\Temp\" append style,
// main.cpp RunClientFromJkx + scriptdemo). GetTempPathA/GetCurrentProcessId →
// TempDir()/pid (stage-3 task 7). Win32: GetTempPathA bytes verbatim (CP_ACP,
// same A-path rules as the code above; original call sites also fed the result
// straight to fopen/DeleteFileA) — on failure the original call sites' fallback
// "." returns (tempDir[260] = "." init) so a rare API failure degrades to the
// working directory exactly as before. Posix: $TMPDIR when set and an existing
// directory (trailing '/' ensured), else "/tmp/" — and when even /tmp is not
// accessible, the same "." fail-soft as win32.
std::string TempDir();
}  // namespace jk::fs

#endif  // JKFS_H
