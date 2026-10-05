// posix_selftest/main.cpp — WSL-verified selftest harness for posix adapter
// bodies (stage-2 plan D, task 1). Win32 has no counterpart; this exe is built
// and run only under Ubuntu-24.04 WSL via engine/tools/posix_selftest/build.sh.
// Convention mirrors the win32 in-app selftest (engine/src/main.cpp
// RunAppSelfTest): one "[PASS]/[FAIL] <case>" line per check, the total as
// "PosixSelfTest: <n> failure(s)", exit non-zero on any failure.
#include <cstdio>
#include <string>

#include <unistd.h>  // access, R_OK

#include <process/JKProcess.h>

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

// Case 2 (jk::process): real posix spawn machinery — mirrors the win32
// in-app selftest's jk::process case (engine/src/main.cpp case 14) in
// miniature: sh -c echo round-trip, stdout/stderr separation, contract-(a)
// EOF observation, the STILL_ACTIVE(259) polling convention, and the
// kill-on-close job trio (contract b) actually killing a sleeping tree.
// The stage-1 stub returns ok=false/empty for everything, so every check
// below fails until JKProcess_posix.cpp implements the mapping.
void TestProcessAdapter() {
    constexpr uint32_t kStillActiveExit = 259;  // STILL_ACTIVE (win32 parity)
    constexpr int kErrBrokenPipe = 109;         // ERROR_BROKEN_PIPE family
    constexpr int kErrNoData = 232;             // ERROR_NO_DATA family

    // Same drain shape as the win32 selftest: drains until an OBSERVED
    // EOF/broken verdict; a contract-(a) violation (parent keeping a write
    // end) only ends the loop on the safety bail and fails the check.
    auto drainPipe = [](void* pipe, std::string* sink) -> bool {
        char buf[4096];
        bool open = true;
        bool closed = false;
        int iters = 0;  // safety bail
        while (open) {
            if (++iters > 500) break;
            uint32_t avail = 0;
            int broken = 0;
            const bool data = jk::process::PeekPipeAvail(pipe, &avail, &broken);
            if (data && avail > 0) {
                const int got = jk::process::ReadPipeData(pipe, buf, sizeof(buf));
                if (got > 0) {
                    sink->append(buf, static_cast<size_t>(got));
                    continue;
                }
                open = false;
                closed = true;
            } else if (broken == kErrBrokenPipe || broken == kErrNoData) {
                open = false;
                closed = true;
            } else {
                usleep(20 * 1000);
            }
        }
        return closed;
    };

    // A) sh -c echo round-trip through inherited stdio pipes, with separate
    //    stdout/stderr streams (the JKLlmEngine reply-JSON contract).
    jk::process::SpawnOptions opt;
    opt.commandLineUtf8 =
        "echo out-mark-3361; echo err-mark-3361 >&2";
    opt.inheritedStdioPipes = true;
    const jk::process::SpawnResult r = jk::process::Spawn(opt);
    Check(r.ok, "process: sh -c spawn ok (stub reports error string instead)");
    Check(r.ok && r.process && r.pid != 0,
          "process: spawn returns process handle + pid");
    Check(r.ok && r.error.empty(), "process: successful spawn carries no error");
    Check(r.stdoutRead && r.stderrRead,
          "process: both parent pipe read ends returned");
    std::string out, errOut;
    const bool outEof = drainPipe(r.stdoutRead, &out);
    const bool errEof = drainPipe(r.stderrRead, &errOut);
    std::printf("  process: stdout=\"%s\" stderr=\"%s\"\n", out.c_str(),
                errOut.c_str());
    std::fflush(stdout);
    Check(out.find("out-mark-3361") != std::string::npos,
          "process: stdout round-trips through adapter pipe");
    Check(errOut.find("err-mark-3361") != std::string::npos,
          "process: stderr lands on the separate stderr pipe");
    Check(out.find("err-mark-3361") == std::string::npos,
          "process: streams are separated (no stderr leakage into stdout)");
    Check(outEof && errEof,
          "process: EOF observed on both pipes (contract a: parent holds "
          "read ends only)");
    uint32_t code = 0;
    bool exited = false;
    for (int i = 0; i < 500; ++i) {  // 10s budget — echo exits at once
        if (jk::process::GetExitCode(r.process, &code) &&
            code != kStillActiveExit) {
            exited = true;
            break;
        }
        usleep(20 * 1000);
    }
    Check(exited && code == 0, "process: GetExitCode reports clean exit 0");
    jk::process::CloseHandleLike(r.stdoutRead);
    jk::process::CloseHandleLike(r.stderrRead);
    jk::process::CloseHandleLike(r.process);

    // B) STILL_ACTIVE(259) preservation: a 1s sleeper must report 259 while
    //    running, then 0 once reaped — the 259 convention carried over.
    jk::process::SpawnOptions sleeper;
    sleeper.commandLineUtf8 = "sleep 1";
    const jk::process::SpawnResult s = jk::process::Spawn(sleeper);
    Check(s.ok && s.process, "process: sleeper spawns (no stdio pipes)");
    uint32_t live = 0;
    Check(jk::process::GetExitCode(s.process, &live) &&
              live == kStillActiveExit,
          "process: running child reports STILL_ACTIVE 259");
    exited = false;
    code = 0;
    for (int i = 0; i < 300; ++i) {  // 6s budget — sleep 1 ends well inside
        if (jk::process::GetExitCode(s.process, &code) &&
            code != kStillActiveExit) {
            exited = true;
            break;
        }
        usleep(20 * 1000);
    }
    Check(exited && code == 0, "process: sleeper reaps with exit 0 after 259");
    jk::process::CloseHandleLike(s.process);

    // C) Job trio (contract b): CreateKillOnCloseJob -> AssignToJob ->
    //    TerminateJobTree must actually kill a sleeping tree.
    jk::process::SpawnOptions slow;
    slow.commandLineUtf8 = "sleep 30 && echo never-3361";
    const jk::process::SpawnResult p = jk::process::Spawn(slow);
    Check(p.ok && p.process, "process: job smoke child spawns");
    void* job = jk::process::CreateKillOnCloseJob();
    Check(job != nullptr, "process: kill-on-close job handle is non-null");
    Check(jk::process::AssignToJob(job, p),
          "process: kill-on-close job assigns spawned process");
    Check(jk::process::TerminateJobTree(job, 1),
          "process: terminate job tree reports success");
    bool killed = false;
    code = 0;
    for (int i = 0; i < 300; ++i) {  // 6s budget — SIGKILL reaps at once
        if (jk::process::GetExitCode(p.process, &code) &&
            code != kStillActiveExit && code != 0) {
            killed = true;
            break;
        }
        usleep(20 * 1000);
    }
    Check(killed, "process: terminated tree child dies non-zero (not 259)");
    jk::process::CloseHandleLike(p.process);
    jk::process::CloseHandleLike(job);  // close IS the kill; child dead

    // D) workingDir mapping: sh -c pwd under /tmp must report /tmp.
    jk::process::SpawnOptions wd;
    wd.commandLineUtf8 = "pwd";
    wd.workingDir = "/tmp";
    wd.inheritedStdioPipes = true;
    const jk::process::SpawnResult w = jk::process::Spawn(wd);
    Check(w.ok && w.stdoutRead, "process: workingDir spawn ok");
    std::string wdOut;
    drainPipe(w.stdoutRead, &wdOut);
    Check(wdOut.find("/tmp") == 0,
          "process: workingDir honored (pwd reports /tmp)");
    jk::process::CloseHandleLike(w.stdoutRead);
    jk::process::CloseHandleLike(w.stderrRead);
    jk::process::CloseHandleLike(w.process);

    // E) Contract (b) on posix: handle close alone (no explicit Terminate)
    //    must kill the tree — close-handle == tree death.
    jk::process::SpawnOptions linger;
    linger.commandLineUtf8 = "sleep 30 && echo never-3362";
    const jk::process::SpawnResult l = jk::process::Spawn(linger);
    Check(l.ok && l.process, "process: close-kill child spawns");
    void* job2 = jk::process::CreateKillOnCloseJob();
    Check(job2 && jk::process::AssignToJob(job2, l),
          "process: close-kill job assigns child");
    jk::process::CloseHandleLike(job2);  // the close itself must be the kill
    bool closed = false;
    code = 0;
    for (int i = 0; i < 300; ++i) {  // 6s budget — SIGKILL reaps at once
        if (jk::process::GetExitCode(l.process, &code) &&
            code != kStillActiveExit && code != 0) {
            closed = true;
            break;
        }
        usleep(20 * 1000);
    }
    Check(closed, "process: closing the job handle kills the tree (contract b)");
    jk::process::CloseHandleLike(l.process);
}

}  // namespace

int main() {
    TestFsGetExecutablePath();
    TestProcessAdapter();
    std::printf("PosixSelfTest: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
