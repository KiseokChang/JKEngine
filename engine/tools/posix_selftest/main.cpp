// posix_selftest/main.cpp — WSL-verified selftest harness for posix adapter
// bodies (stage-2 plan D, task 1). Win32 has no counterpart; this exe is built
// and run only under Ubuntu-24.04 WSL via engine/tools/posix_selftest/build.sh.
// Convention mirrors the win32 in-app selftest (engine/src/main.cpp
// RunAppSelfTest): one "[PASS]/[FAIL] <case>" line per check, the total as
// "PosixSelfTest: <n> failure(s)", exit non-zero on any failure.
#include <cstdio>
#include <string>

#include <unistd.h>  // access, R_OK

#include "fs/JKFs.h"

namespace {

int g_failures = 0;

void Check(bool cond, const char* name) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
    std::fflush(stdout);
    if (!cond) ++g_failures;
}

bool EndsWith(const std::string& s, const char* suffix) {
    const std::string tail(suffix);
    return s.size() >= tail.size() &&
           s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// Case 1 (jk::fs): GetExecutablePath() must return the real path of this very
// exe (readlink("/proc/self/exe")) — a readable absolute path ending in
// ".../posix_selftest" under the WSL repo mount, not the stub's empty string.
void TestFsGetExecutablePath() {
    const std::string path = jk::fs::GetExecutablePath();
    std::printf("  fs: GetExecutablePath() -> \"%s\"\n", path.c_str());
    std::fflush(stdout);

    Check(!path.empty(), "fs: non-empty (stub would be empty)");
    Check(!path.empty() && path[0] == '/', "fs: absolute path");
    Check(EndsWith(path, "/posix_selftest"),
          "fs: ends with .../posix_selftest");
    Check(EndsWith(path, "engine/build/posix_selftest"),
          "fs: is this harness exe under the repo build dir");
    Check(access(path.c_str(), R_OK) == 0,
          "fs: exe path exists and is readable");
}

}  // namespace

int main() {
    TestFsGetExecutablePath();
    std::printf("PosixSelfTest: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}