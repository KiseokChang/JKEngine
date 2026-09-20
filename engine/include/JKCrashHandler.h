#ifndef JKCRASHHANDLER_H
#define JKCRASHHANDLER_H

#include <string>

namespace jk {

// Crash evidence + log persistence (docs/57 §13, 2026-09-20 live defect):
// the window server died silently at 16:12 (bridge "server connection lost",
// no WER record, console log gone with the process) — with no file log and
// no dump the failure is uninvestigable. These two hooks fix that:
//
//   InstallCrashHandler(dir, tag) — SEH unhandled-exception filter writing a
//       dbghelp minidump to <dir>\<tag>_<yyyymmdd_hhmmss>.dmp, plus a
//       SIGABRT/abort marker file. Best effort: never throws, never blocks.
//
//   MirrorLogToFiles(dir, tag) — mirrors stdout/stderr into
//       <dir>\<tag>_yyyymmdd.log (row-flushed) while KEEPING the console
//       alive: stdout/stderr are redirected into an anonymous pipe drained by
//       a daemon thread that writes both to the saved console handle and the
//       log file. The console stays visible for the user; the file survives
//       process death so a silent crash leaves a truth source behind.
//
// Windows implementation; on other platforms both are no-ops.

// Install the unhandled-exception minidump filter. Call once, early.
void InstallCrashHandler(const std::string& dumpDir, const std::string& tag);

// Begin mirroring stdout/stderr to <dir>\<tag>_<date>.log. Call once, early,
// before any log output you care about. Returns false if the mirror could
// not be set up (console keeps working regardless).
bool MirrorLogToFiles(const std::string& logDir, const std::string& tag);

} // namespace jk

#endif // JKCRASHHANDLER_H