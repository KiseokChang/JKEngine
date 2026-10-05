// posix_selftest/main.cpp — WSL-verified selftest harness for posix adapter
// bodies (stage-2 plan D, task 1). Win32 has no counterpart; this exe is built
// and run only under Ubuntu-24.04 WSL via engine/tools/posix_selftest/build.sh.
// Convention mirrors the win32 in-app selftest (engine/src/main.cpp
// RunAppSelfTest): one "[PASS]/[FAIL] <case>" line per check, the total as
// "PosixSelfTest: <n> failure(s)", exit non-zero on any failure.
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include <unistd.h>  // access, R_OK, close, sleep

#include <arpa/inet.h>  // inet_addr, htons (raw client below — this TU is
#include <netinet/in.h>  // posix-only, so it may include POSIX socket headers
#include <sys/socket.h>  // directly; no windows.h-cleanliness constraint here)

#include <net/JKNet.h>
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

// Monotonic ms since an arbitrary epoch — read-timeout timing check only.
double nowMs() {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
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

// Case 3 (jk::net): real POSIX TCP machinery — mirrors the win32 adapter
// (engine/src/net/JKNet_win32.cpp) contract in miniature: ephemeral listener
// with getsockname bound-port report, a raw POSIX-socket client thread doing
// a 5-byte echo round-trip, the SO_RCVTIMEO read-timeout actually firing,
// shutdown(SHUT_RDWR), and the bind-failure path (kInvalidSocket with the
// out-param untouched — R-C4). The stage-1 stub returns kInvalidSocket/false
// for everything, so every check below fails until JKNet_posix.cpp
// implements the mapping.
void TestNetAdapter() {
    Check(jk::net::Startup(), "net: Startup() true (non-Windows contract)");

    // A) Ephemeral loopback listener: port=0 must yield a real listener and
    //    the ACTUAL bound port via getsockname (host order, non-zero).
    std::uint16_t boundPort = 0;
    const jk::net::Socket lp =
        jk::net::ListenTcp("127.0.0.1", 0, 1, &boundPort);
    Check(lp != jk::net::kInvalidSocket,
          "net: ListenTcp(127.0.0.1, 0, 1) opens a listener");
    Check(boundPort != 0,
          "net: boundPortOut reports the actual ephemeral port");
    std::printf("  net: bound port = %u\n", static_cast<unsigned>(boundPort));
    std::fflush(stdout);

    // B) Bind-failure path (R-C4): "999.999.999.999" makes inet_addr return
    //    INADDR_NONE on both platforms; win32 then fails bind(255.255.255.255)
    //    (WSAEADDRNOTAVAIL) while Linux bind() would ACCEPT the broadcast
    //    address, so the posix adapter checks INADDR_NONE explicitly and must
    //    return kInvalidSocket AND leave the out-param untouched.
    std::uint16_t untouched = 7777;
    const jk::net::Socket bad =
        jk::net::ListenTcp("999.999.999.999", 8080, 1, &untouched);
    Check(bad == jk::net::kInvalidSocket,
          "net: bad bindIp 999.999.999.999 -> kInvalidSocket");
    Check(untouched == 7777,
          "net: boundPortOut untouched on ListenTcp failure (R-C4)");

    // Raw POSIX client thread (standard headers — this TU compiles natively
    // under g++, no dllimport trick needed): connect, send "hello", expect
    // the server echo, then HOLD the connection open 3s (so the server-side
    // read-timeout check below can only pass via SO_RCVTIMEO firing, not via
    // the client closing), then close.
    struct ClientResult {
        int connectErr;
        int sent;
        int recved;
        bool echoOk;
    } cr{-1, -1, -1, false};
    auto clientBody = [&cr, boundPort]() {
        const int c = socket(AF_INET, SOCK_STREAM, 0);
        if (c < 0) return;
        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_addr.s_addr = inet_addr("127.0.0.1");
        dst.sin_port = htons(boundPort);
        cr.connectErr = connect(c, reinterpret_cast<sockaddr*>(&dst),
                                sizeof(dst));
        if (cr.connectErr != 0) {
            ::close(c);
            return;
        }
        cr.sent = static_cast<int>(send(c, "hello", 5, 0));
        char echo[5] = {};
        int got = 0;
        while (got < 5) {
            const int r = recv(c, echo + got, 5 - got, 0);
            if (r <= 0) break;
            got += r;
        }
        cr.recved = got;
        cr.echoOk = got == 5 && std::string(echo, 5) == "hello";
        sleep(3);  // hold open — the timeout check must beat this close
        ::close(c);
    };
    std::thread client(clientBody);

    // C) Accept the raw client and echo 5 bytes back through the adapter.
    const jk::net::Socket acc = jk::net::Accept(lp);
    Check(acc != jk::net::kInvalidSocket, "net: Accept returns a connection");
    char buf[5] = {};
    Check(jk::net::RecvAll(acc, buf, 5) && std::string(buf, 5) == "hello",
          "net: RecvAll receives the client's 5 bytes");
    Check(jk::net::Send(acc, buf, 5) == 5,
          "net: Send echoes 5 bytes (raw passthrough)");
    jk::net::Close(lp);  // listener done — accepted conn stays valid

    // D) SetTimeouts must actually bound a read: no more data is coming and
    //    the client holds the socket open for 3s, so only a firing
    //    SO_RCVTIMEO (500ms) can end this RecvAll as false — and quickly.
    jk::net::SetTimeouts(acc, 500);
    const double t0 = nowMs();
    const bool timedOut = !jk::net::RecvAll(acc, buf, 5);
    const double elapsed = nowMs() - t0;
    std::printf("  net: read-timeout RecvAll=false after %.0f ms\n", elapsed);
    std::fflush(stdout);
    Check(timedOut, "net: RecvAll reports false on read timeout");
    Check(elapsed < 2500.0,
          "net: timeout fired well before the client's 3s close");

    // E) ShutdownBoth then Close on the accepted connection.
    jk::net::ShutdownBoth(acc);
    jk::net::Close(acc);

    client.join();
    Check(cr.connectErr == 0, "net: raw client connects to the bound port");
    Check(cr.sent == 5, "net: raw client sends 5 bytes");
    Check(cr.recved == 5 && cr.echoOk, "net: client receives its echo back");
}

}  // namespace

int main() {
    TestFsGetExecutablePath();
    TestProcessAdapter();
    TestNetAdapter();
    std::printf("PosixSelfTest: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
