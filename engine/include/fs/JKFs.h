#ifndef JKFS_H
#define JKFS_H
// jk::fs — filesystem boundary adapter (docs/68 W5 stage-1). Win32 impl in
// JKFs_win32.cpp; posix returns false/empty (stage 2: readlink("/proc/self/exe")).
// Supersedes 25 GetModuleFileName call sites (plan-b-inventory §7) — the
// MAX_PATH-truncation variants die together by returning std::string.

#include <string>

namespace jk::fs {
// Full path of the current executable image, UTF-8. Empty string on
// non-Windows (stage 2) — callers keep their existing empty-path fallback.
std::string GetExecutablePath();
}  // namespace jk::fs

#endif  // JKFS_H