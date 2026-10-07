// posix_selftest/main.cpp — WSL-verified selftest harness for posix adapter
// bodies (stage-2 plan D, task 1). Win32 has no counterpart; this exe is built
// and run only under Ubuntu-24.04 WSL via engine/tools/posix_selftest/build.sh.
// Convention mirrors the win32 in-app selftest (engine/src/main.cpp
// RunAppSelfTest): one "[PASS]/[FAIL] <case>" line per check, the total as
// "PosixSelfTest: <n> failure(s)", exit non-zero on any failure.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>  // std::memcmp
#include <ctime>  // nanosleep (D-T3 hygiene: nanosleep, not usleep)
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>  // stat, S_ISSOCK (socket-file liveness triage below)
#include <unistd.h>  // access, unlink, getpid, R_OK, close, sleep

#include <arpa/inet.h>  // inet_addr, htons (raw client below — this TU is
#include <netinet/in.h>  // posix-only, so it may include POSIX socket headers
#include <sys/socket.h>  // directly; no windows.h-cleanliness constraint here)

#include <agent/JKLlmEngine.h>  // kStubShellCmdPosix (case 10), TurnSync (case 15)
#include <ipc/JKPipeTransport.h>
#include <ipc/JKWireEndpoints.h>
#include <fs/JKInstanceLock.h>
#include <net/JKNet.h>
#include <port/JKCrtShim.h>
#include <process/JKProcess.h>
#include <terminal/JKConPtyBridge.h>

#include "fs/JKFs.h"
#include "text/JKTextConv.h"

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

// nanosleep-based nap — the stage-2 timing-hygiene ruling (task 4 fix round,
// commit aacaa66) says nanosleep, not usleep. Case-6 polling uses this.
void NapMs(int ms) {
    timespec ts{};
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (static_cast<long>(ms) % 1000L) * 1000000L;
    nanosleep(&ts, nullptr);
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

// Case 4 (JKConPtyBridge): real posix pty machinery — mirrors the win32
// bridge's observable contract (engine/src/terminal/JKConPtyBridge_win32.cpp,
// close order per docs/22 §8.2). R-D2 mapping: Start(commandLine) executes
// `/bin/sh -c <commandLine>` on an openpty() master, so the child's bytes come
// back raw through DrainOutput, WriteInput bytes round-trip through the tty,
// Resize(120, 30) must reach the slave side's winsize (`stty size` prints it
// as "rows cols" — that makes ROWS/COLS argument order observable), and
// ProcessExited() must report the spawned command's death (the win32 body's
// process-handle-signal convention) — with the exit status observable through
// the pty stream via an inner `sh -c 'exit 7'` echoed as "PSTATUS=7".
// The stage-1 stub returns false/no-op for everything, so every check below
// fails until JKConPtyBridge_posix.cpp implements the mapping.
void TestPtyBridge() {
    // Drain loop shared by every marker check: completion-condition polling
    // (drain-until-marker) with generous per-marker budgets — no wall-clock
    // deltas (D-T3 timing hygiene). Each check gates on startedA/live so a
    // stub RED run (Start false) skips the wait loops instead of burning
    // them against an inert bridge.
    auto drainUntil = [](jk::JKConPtyBridge& pty, const char* marker,
                         int budgetIters, std::string* seen) -> bool {
        for (int i = 0; i < budgetIters; ++i) {
            std::string out;
            pty.DrainOutput(out);
            seen->append(out);
            if (seen->find(marker) != std::string::npos) return true;
            if (pty.ShellExited()) {
                // The reader appends everything before setting the flag, but
                // drain once more so a last-chunk race can not hide bytes.
                std::string tail;
                pty.DrainOutput(tail);
                seen->append(tail);
                return seen->find(marker) != std::string::npos;
            }
            usleep(20 * 1000);
        }
        return false;
    };

    // A) Spawn + byte-stream capture + process-death + exit-code
    //    observability. The command is the plan's `echo HANGUL_TEST; exit 7`
    //    with one indirection: an inner `sh -c 'exit 7'` carries the status
    //    so it is observable BOTH ways — as "PSTATUS=7" through the pty byte
    //    stream, and as the actual exit status of the bridge's own child
    //    (`exit $?` propagates it), which ProcessExited() must report.
    jk::JKConPtyBridge pty;
    const bool startedA =
        pty.Start("echo HANGUL_TEST; sh -c 'exit 7'; echo PSTATUS=$?; "
                  "exit $?",
                  80, 24);
    Check(startedA, "pty: Start spawns sh -c in a pty (stub returns false)");
    Check(pty.IsValid() == startedA, "pty: IsValid mirrors started_");
    if (startedA) {
        std::string seen;
        Check(drainUntil(pty, "HANGUL_TEST", 500, &seen),
              "pty: DrainOutput captures child bytes (HANGUL_TEST marker)");
        std::printf("  pty: child output \"%.200s\"\n", seen.c_str());
        std::fflush(stdout);
        bool shellEof = false;
        for (int i = 0; i < 500; ++i) {  // 10s budget — echo exits at once
            if (pty.ShellExited()) {
                shellEof = true;
                break;
            }
            std::string tail;
            pty.DrainOutput(tail);
            seen.append(tail);
            usleep(20 * 1000);
        }
        Check(shellEof,
              "pty: ShellExited fires on pty read EOF/EIO (child gone)");
        bool procDone = false;
        for (int i = 0; i < 500; ++i) {  // 10s budget — exit 7 ends at once
            if (pty.ProcessExited()) {
                procDone = true;
                break;
            }
            usleep(20 * 1000);
        }
        Check(procDone,
              "pty: ProcessExited fires when the command ends (exit 7)");
        Check(pty.ProcessExited(),
              "pty: ProcessExited stays true across repeated calls (reap "
              "cache)");
        // Exit status the header can not expose directly (bool-only API, the
        // win32 body's convention): watch it through the pty byte stream —
        // the inner `sh -c 'exit 7'` status lands as "PSTATUS=7".
        Check(seen.find("PSTATUS=7") != std::string::npos,
              "pty: exit 7 observable through the pty stream (PSTATUS=7)");
        pty.Stop();
        Check(!pty.IsValid(), "pty: Stop clears started_");
        pty.Stop();  // second call on an already-stopped bridge
        Check(!pty.IsValid(),
              "pty: second Stop keeps the bridge inert (idempotent)");
    }

    // B) WriteInput round-trip + winsize observability. The child loops
    //    `stty size` forever (rows-space-cols line), so the initial Start
    //    size (24 80) is observable right away and a Resize(120, 30) shows
    //    up as "30 120" — a direct check of TIOCSWINSZ and of the cols/rows
    //    argument order. The echo of the written bytes proves WriteInput.
    jk::JKConPtyBridge live;
    const bool startedB =
        live.Start("while :; do stty size; sleep 0.2; done", 80, 24);
    Check(startedB, "pty: live bridge spawns a stty-size loop");
    if (startedB) {
        std::string liveSeen;
        Check(drainUntil(live, "24 80", 300, &liveSeen),
              "pty: Start(80, 24) sized the pty (stty size prints 24 80)");
        live.Resize(120, 30);
        live.WriteInput("jkpty-3361\n", 11);
        Check(drainUntil(live, "jkpty-3361", 300, &liveSeen),
              "pty: WriteInput round-trips (bytes echo back through the pty)");
        Check(drainUntil(live, "30 120", 300, &liveSeen),
              "pty: Resize(120, 30) reaches the tty (stty size prints 30 120)");
        std::printf("  pty: live output \"%.200s\"\n", liveSeen.c_str());
        std::fflush(stdout);
        live.Stop();
        Check(!live.IsValid(), "pty: live bridge stopped");
    }

    // C) docs/22 §8.2 close order on a STILL-RUNNING child: Stop must close
    //    the pty, unblock the reader, bound the wait, and SIGKILL the pgid —
    //    i.e. Stop must RETURN promptly instead of hanging in a join on a
    //    reader stuck in read(). Generous upper bound only (8s), never a
    //    wall-clock lower bound (D-T3 hygiene).
    jk::JKConPtyBridge sleeper;
    Check(sleeper.Start("sleep 30", 80, 24), "pty: stop-probe spawns sleep 30");
    if (sleeper.IsValid()) {
        usleep(300 * 1000);  // let the reader actually block inside read()
        const double tStop = nowMs();
        sleeper.Stop();
        const double stopMs = nowMs() - tStop;
        std::printf("  pty: Stop on a running child returned in %.0f ms\n",
                    stopMs);
        std::fflush(stdout);
        Check(stopMs < 8000.0,
              "pty: Stop on a running child returns bounded (no hung join)");
    }
}

// Case 5 (JKPipeTransport): real unix-domain-socket transport (R-D3) —
// mirrors the win32 observable contract in miniature: CreateServer BLOCKS
// until a client connects (ConnectNamedPipe parity), ConnectClient reaches
// the server through the name used as-is as the socket path, a 100-byte wire
// round-trip in BOTH directions (the transport is byte-agnostic — pure pipe
// semantics, no JKWireProtocol framing), CancelPendingIo wakes a reader
// parked in Read and that Read must FAIL (win32 CancelIoEx parity: the
// pending operation returns failure, never an innocent success), and Close
// teardown is complete + idempotent. The stage-1 stub returns nullptr/false
// for everything, so every check below fails until JKPipeTransport_posix.cpp
// implements the mapping.
void TestPipeTransport() {
    // R-D3: the name is a socket path supplied by the caller, used as-is.
    // Unique-per-run under /tmp (pid suffix); unlink first so a crashed prior
    // run's stale file cannot block this run's own bind.
    const std::string sockPath =
        "/tmp/jkpipe_slftest_" + std::to_string(::getpid()) + ".sock";
    ::unlink(sockPath.c_str());

    // Server thread: CreateServer blocks inside accept until the client lands.
    std::unique_ptr<jk::ipc::JKPipeTransport> server;
    std::thread serverThread(
        [&server, &sockPath]() {
            server = jk::ipc::JKPipeTransport::CreateServer(sockPath);
        });

    // Poll for the socket FILE first (completion condition — the server bound
    // its path), then connect; the retry loop also beats the bind/listen race
    // between the file appearing and the backlog being open. No wall-clock
    // deltas (D-T3 hygiene) — 10s completion budgets like cases 2-4.
    std::unique_ptr<jk::ipc::JKPipeTransport> client;
    struct stat st {};
    for (int i = 0; i < 500 && !client; ++i) {  // 10s budget
        if (::stat(sockPath.c_str(), &st) == 0 && S_ISSOCK(st.st_mode)) {
            client = jk::ipc::JKPipeTransport::ConnectClient(sockPath);
        }
        if (!client) usleep(20 * 1000);
    }
    Check(client != nullptr,
          "transport: ConnectClient connects through the socket path");
    if (!client) {
        // Stub RED (or a real failure): CreateServer would stay parked in
        // accept forever — detach; process exit terminates it.
        serverThread.detach();
        return;
    }
    serverThread.join();
    Check(server != nullptr,
          "transport: CreateServer unblocks once the client connects");
    if (!server) {  // keep the rest gated (same shape as case 4's startedA)
        client->Close();
        ::unlink(sockPath.c_str());
        return;
    }

    // Byte filler: a printable marker at offset 0 (grep-friendly in logs) and
    // a deterministic pattern over the rest.
    auto fill = [](std::vector<uint8_t>& v, const char* tag, uint8_t salt) {
        for (size_t i = 0; i < v.size(); ++i) {
            v[i] = static_cast<uint8_t>(
                (salt * (i + 1)) ^ (i * 31 + 7));
        }
        const std::string t(tag);
        for (size_t i = 0; i < t.size() && i < v.size(); ++i) {
            v[i] = static_cast<uint8_t>(t[i]);
        }
    };

    // A) client -> server: 100-byte Write then exact Read.
    std::vector<uint8_t> tx(100), rx(100, 0);
    fill(tx, "JKPT-C2S-3361", 0x5A);
    Check(client->Write(tx.data(), tx.size()),
          "transport: client Write of 100 bytes succeeds");
    Check(server->Read(rx.data(), rx.size()),
          "transport: server Read of 100 bytes succeeds");
    Check(rx == tx,
          "transport: client->server 100 bytes round-trip exactly");

    // B) reverse direction: server -> client.
    std::vector<uint8_t> tx2(100), rx2(100, 0);
    fill(tx2, "JKPT-S2C-3361", 0xA5);
    Check(server->Write(tx2.data(), tx2.size()),
          "transport: server Write of 100 bytes succeeds");
    Check(client->Read(rx2.data(), rx2.size()),
          "transport: client Read of 100 bytes succeeds");
    Check(rx2 == tx2,
          "transport: server->client 100 bytes round-trip exactly");
    Check(client->IsConnected() && server->IsConnected(),
          "transport: both ends report connected after the round-trip");

    // C) CancelPendingIo wakes a reader parked inside Read and that Read must
    //    FAIL (win32 parity — CancelIoEx makes the pending op return
    //    failure). Same probe shape as case 4C: settle so the reader is truly
    //    blocked inside recv, cancel, then completion-poll its verdict.
    std::atomic<bool> entered{false};
    std::atomic<int> verdict{-1};  // -1 pending, 1 success, 0 failure
    std::thread reader([&server, &entered, &verdict]() {
        uint8_t buf[16] = {};
        entered = true;
        verdict = server->Read(buf, sizeof(buf)) ? 1 : 0;
    });
    for (int i = 0; i < 500 && !entered; ++i) usleep(20 * 1000);
    usleep(200 * 1000);  // settle: let the reader block inside recv
    server->CancelPendingIo();
    for (int i = 0; i < 500 && verdict == -1; ++i) usleep(20 * 1000);
    reader.join();
    Check(verdict == 0,
          "transport: CancelPendingIo wakes the blocked Read with failure");
    Check(!server->IsConnected(),
          "transport: cancelled transport reads as disconnected (fail-closed)");

    // D) Close teardown — full, and idempotent (IWireTransport: "Safe to call
    //    multiple times").
    client->Close();
    server->Close();
    Check(!client->IsConnected() && !server->IsConnected(),
          "transport: Close disconnects both ends");
    client->Close();
    server->Close();
    Check(!client->IsConnected() && !server->IsConnected(),
          "transport: repeated Close is safe (idempotent)");

    // Case-end cleanup of the server's socket file (name is caller-owned —
    // the transport carries no name member, so unlink is the caller's job).
    ::unlink(sockPath.c_str());
    Check(::stat(sockPath.c_str(), &st) != 0,
          "transport: socket file removed at case end");
}

// Case 6 (jk::fs::InstanceLock): the single-instance guard adapter —
// flock(LOCK_EX|LOCK_NB) on the /tmp/<name>.lock file (docs/62 §8 flock 봉쇄,
// plan D task 6). Contracts under test:
//   A) same-process discipline: first acquire owns; a second acquire while
//      holding is refused (win32 parity — re-opening a named mutex you already
//      own reports ERROR_ALREADY_EXISTS); Release drops early; re-acquire owns
//      again; the underlying lock file lives at /tmp/<name>.lock; release
//      with nothing held is a safe no-op.
//   B) cross-process exclusion via a jk::process-spawned flock(1) holder
//      child: while the foreign holder owns the lock our acquire must fail,
//      and once it dies the lock is acquirable again. Completion-condition
//      polling everywhere (D-T3) — budgets only, no wall-clock delta asserts.
//   C) name hygiene: a '/' in lockName cannot escape /tmp (folds to '_');
//      backslashes (real consumer name "Local\jkdesktop-server-...") are
//      legal filename bytes and stay literal.
// The lock FILE is never unlinked by the adapter (unlink+flock inode race —
// a second acquirer would create a fresh inode and hold a disjoint "lock");
// stale files are harmless because flock dies with the holding fd, so the
// HARNESS unlinks its own scratch names at case end (case-5 shape).
void TestInstanceLock() {
    constexpr uint32_t kStillActiveExit = 259;  // STILL_ACTIVE (win32 parity)
    const std::string name = "jkinstance_slftest_" + std::to_string(::getpid());
    const std::string lockPath = "/tmp/" + name + ".lock";
    ::unlink(lockPath.c_str());  // pre-clean (unique name — no live holder)

    // A) same-process acquire / refuse / release / re-acquire cycle.
    Check(jk::fs::AcquireInstanceLock(name), "instance: first acquire owns");
    Check(::access(lockPath.c_str(), F_OK) == 0,
          "instance: lock file lives at /tmp/<name>.lock");
    Check(!jk::fs::AcquireInstanceLock(name),
          "instance: second acquire refused while held");
    jk::fs::ReleaseInstanceLock();
    Check(jk::fs::AcquireInstanceLock(name),
          "instance: re-acquire after release succeeds");
    jk::fs::ReleaseInstanceLock();  // drop for the cross-process probe
    jk::fs::ReleaseInstanceLock();  // nothing held — must be a safe no-op

    // B) cross-process exclusion. The holder child blocks in flock(1) until
    //    the lock frees, then holds it 5s; its stderr flows through to the
    //    harness output for hard-failure diagnostics (exit 3 = could not
    //    take the lock within its own 20s wait).
    jk::process::SpawnOptions holder;
    holder.commandLineUtf8 =
        "flock -w 20 " + lockPath + " -c 'sleep 5' || "
        "{ echo 'jkinst_holder: could not acquire+hold the lock within "
        "20s' >&2; exit 3; }";
    const jk::process::SpawnResult h = jk::process::Spawn(holder);
    Check(h.ok && h.process, "instance: lock-holder child spawns");
    if (!(h.ok && h.process)) return;

    // Wait for the child to actually OWN the lock: our acquire flips to false
    // (completion condition — no wall-clock deltas). If we still manage to
    // acquire during the start-up race, release again so the child's blocking
    // flock can get in; a child that dies before taking the lock (flock
    // timeout, missing utility) surfaces via its exit code below.
    bool heldByChild = false;
    for (int i = 0; i < 1500 && !heldByChild; ++i) {  // 30s ≫ holder's -w 20
        uint32_t code = 0;
        if (jk::process::GetExitCode(h.process, &code) &&
            code != kStillActiveExit) {
            break;  // holder left the game early
        }
        if (jk::fs::AcquireInstanceLock(name)) {
            jk::fs::ReleaseInstanceLock();  // not held yet — try again later
        } else {
            heldByChild = true;
        }
        NapMs(20);
    }
    if (!heldByChild) {
        uint32_t code = 0;
        if (::jk::process::GetExitCode(h.process, &code)) {
            std::fprintf(stderr, "  instance: holder child exited %u without "
                                 "holding the lock\n",
                         static_cast<unsigned>(code));
        }
        std::fflush(stderr);
    }
    Check(heldByChild,
          "instance: spawned holder excludes our acquire (cross-process)");
    if (heldByChild) {
        Check(!jk::fs::AcquireInstanceLock(name),
              "instance: acquire stays refused while the holder owns it");
    }

    // Wait for the holder to drop the lock by dying — completion poll with a
    // generous budget (hold window is 5s).
    uint32_t hcode = 0;
    bool hGone = false;
    for (int i = 0; i < 2500; ++i) {  // 50s ≫ sleep 5
        if (jk::process::GetExitCode(h.process, &hcode) &&
            hcode != kStillActiveExit) {
            hGone = true;
            break;
        }
        NapMs(20);
    }
    Check(hGone && hcode == 0,
          "instance: holder child exits cleanly after its hold window");
    jk::process::CloseHandleLike(h.process);

    // C) after the holder is gone the lock is ours again, and the FILE
    //    persists — the adapter never unlinks (TU comment: unlink+flock race).
    Check(jk::fs::AcquireInstanceLock(name),
          "instance: lock acquirable again once the holder is gone");
    Check(::access(lockPath.c_str(), F_OK) == 0,
          "instance: lock file persists across holders (never unlinked)");
    jk::fs::ReleaseInstanceLock();

    // D) name hygiene: a '/' in the name must not escape /tmp. The real win32
    //    guard name is "Local\jkdesktop-server-<basename>" (backslashes —
    //    legal bytes); the hostile shape is '../...' which folds to '.._...'.
    const std::string pid = std::to_string(::getpid());
    const std::string travName = "../jkinstance_slftest_trav_" + pid;
    const std::string travPath = "/tmp/.._jkinstance_slftest_trav_" + pid + ".lock";
    Check(jk::fs::AcquireInstanceLock(travName),
          "instance: name with ../ acquires (folded flat)");
    Check(::access(travPath.c_str(), F_OK) == 0,
          "instance: ../ name stays inside /tmp (no traversal escape)");
    jk::fs::ReleaseInstanceLock();

    // Case-end cleanup — the adapter never unlinks, so the harness owns its
    // scratch names (same shape as case 5's socket-file unlink).
    ::unlink(lockPath.c_str());
    ::unlink(travPath.c_str());
    Check(::access(lockPath.c_str(), F_OK) != 0 &&
              ::access(travPath.c_str(), F_OK) != 0,
          "instance: harness scratch lock files removed at case end");
}

// Case 7 (jk::text): charset adapter, posix iconv leg (stage-3 full-build
// task 4). Contracts under test:
//   A) UTF-8 -> CP949 -> UTF-8 round-trip identity on three CP949-
//      representable sentences covering the three payload classes the
//      KSSM bitmap-font path feeds: hangul (wCodeTable leg input), hanja
//      (0xCA-0xFD row arithmetic input), KS X 1001 symbol rows (identity
//      pairs A1-A2, docs/65 O5).
//   B) UTF-8 -> UTF-16 -> UTF-8 round-trips (posix wstring is 4-byte wchar_t
//      — the surrogate-pair repacking must be invisible across a round-trip).
//   C) fail-closed contract: any invalid/truncated sequence -> EMPTY string
//      (win32 MBTW MB_ERR_INVALID_CHARS observation), both directions, all
//      four entry points. No '?' substitution, no partial bytes.
void TestTextConv() {
    // A) three-sentence round trips.
    const char* kSentences[3] = {
        "한글 조합형 자소 완성형 123",      // hangul + ASCII
        "漢字測試 一丁世界",                // hanja row (0xCA-0xFD)
        "■□●◆·「」!?…",                    // KS X 1001 symbol rows A1-A2
    };
    for (int i = 0; i < 3; ++i) {
        const std::string sent = kSentences[i];
        const std::string euc = jk::text::Utf8ToCp949(sent);
        Check(!euc.empty(), "text: sentence N utf8->cp949 non-empty");
        Check(jk::text::Cp949ToUtf8(euc) == sent,
              "text: sentence N cp949->utf8 round-trips identical");

        // B) wstring round-trip on the same sentences (surrogate repacking).
        const std::wstring w = jk::text::Utf8ToUtf16(sent);
        Check(!w.empty(), "text: sentence N utf8->utf16 non-empty");
        Check(jk::text::Utf16ToUtf8(w) == sent,
              "text: sentence N utf16->utf8 round-trips identical");

        // A supplementary observation: BMP codepoints must map 1 wchar_t =
        // 1 UTF-16 unit (pair count sanity on the hangul sentence).
        if (!w.empty()) {
            const size_t bmpUnits =
                static_cast<size_t>(std::count_if(sent.begin(), sent.end(),
                    [](char c) { return (static_cast<unsigned char>(c) & 0xC0)
                                       != 0x80; }));
            Check(w.size() == bmpUnits,
                  "text: BMP chars are 1:1 wstring codes (no phantom units)");
        }
    }

    // C) fail-closed: every invalid sequence -> empty, no substitution.
    Check(jk::text::Utf8ToCp949("\xED\xA0\x80hello").empty(),
          "text: utf8 utf-16 surrogate leak -> empty cp949");
    Check(jk::text::Utf8ToCp949("abc\xC3").empty(),
          "text: utf8 truncated lead -> empty cp949");
    Check(jk::text::Utf8ToCp949("abc\xE2\x82").empty(),
          "text: utf8 truncated 3-byte -> empty cp949");
    Check(jk::text::Utf8ToUtf16("abc\xC4").empty(),
          "text: utf8 truncated lead -> empty utf16");
    Check(jk::text::Cp949ToUtf8("\xB7").empty(),
          "text: cp949 lone lead byte -> empty");
    Check(jk::text::Cp949ToUtf8("\x81\x40").empty(),
          "text: cp949 invalid trail byte -> empty");
    Check(jk::text::Cp949ToUtf8("abc\xFE\x41").empty(),
          "text: cp949 lead+ascii trail -> empty");

    // utf16 direction fail-closed: a surrogate scalar in a wstring code is an
    // invalid sequence (win32 Utf16ToUtf8 callers can never produce one).
    Check(jk::text::Utf16ToUtf8(std::wstring(1, L'\xD800')).empty(),
          "text: utf16 lone high surrogate scalar -> empty");
    Check(jk::text::Utf16ToUtf8(std::wstring(1, L'\xDC00')).empty(),
          "text: utf16 lone low surrogate scalar -> empty");

    // Empty inputs keep the fail-closed observation ({} == {} everywhere).
    Check(jk::text::Utf8ToCp949("").empty() &&
              jk::text::Cp949ToUtf8("").empty() &&
              jk::text::Utf8ToUtf16("").empty() &&
              jk::text::Utf16ToUtf8(L"").empty(),
          "text: empty in -> empty out on all four entry points");

    // ASCII passthrough must survive byte-exactly (the KSSM loop leans on it).
    Check(jk::text::Utf8ToCp949("abc def\n") == "abc def\n",
          "text: ascii utf8->cp949 passthrough byte-exact");
    Check(jk::text::Utf16ToUtf8(jk::text::Utf8ToUtf16("jk-3361")) ==
              "jk-3361",
          "text: ascii utf16 wstring round-trip byte-exact");
}

// Case 8 (jk::process::ListProcessImages — posix /proc leg, stage-3 full-build
// task 5): the Toolhelp32 snapshot block absorbed from JKWindowServer.cpp
// needs a posix counterpart, and its only real consumer is the guard refusal
// hint (ScanServerCandidates). Contracts under test:
//   A) the scan yields entries (a /proc-less environment would be empty);
//   B) THIS process is found — self-PID discovery, the brief's 실측 requirement;
//   C) our image name equals the basename we were invoked with (argv[0] —
//      the cmdline argv[0] basename mapping documented in JKProcess.h);
//   D) the consumer-shape match (ASCII-lowercase FULL-name ==) finds us while
//      an unrelated literal does NOT — exact-match contract, no substring/prefix
//      bleed (the original loop's `exe == "jkwinserver.exe"` semantics).
void TestProcessScan(char* const* argv) {
    const std::vector<jk::process::ProcessImageInfo> scan =
        jk::process::ListProcessImages();
    std::printf("  process-scan: %zu image entries\n", scan.size());
    std::fflush(stdout);
    Check(!scan.empty(), "proc-scan: the scan yields entries (empty /proc?)");

    const unsigned long self = static_cast<unsigned long>(::getpid());
    const std::string invoked = argv[0] ? argv[0] : "";
    const size_t invokedSlash = invoked.find_last_of('/');
    const std::string invokedBase =
        invokedSlash == std::string::npos ? invoked : invoked.substr(invokedSlash + 1);
    auto lower = [](std::string s) {
        for (char& c : s)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        return s;
    };

    bool selfFound = false;
    std::string selfName;
    std::vector<unsigned long> selfMatches;  // consumer-shape exact match
    for (const jk::process::ProcessImageInfo& e : scan) {
        if (e.pid == self) {
            selfFound = true;
            selfName = e.imageName;
        }
        // The exact-match contract as ScanServerCandidates applies it.
        if (lower(e.imageName) == lower(invokedBase))
            selfMatches.push_back(e.pid);
    }
    std::printf("  process-scan: self pid=%lu image=\"%s\" (argv[0]=\"%s\")\n",
                self, selfName.c_str(), invoked.c_str());
    std::fflush(stdout);

    Check(selfFound, "proc-scan: this process appears in the scan (self PID)");
    Check(selfFound && selfName == invokedBase,
          "proc-scan: image name equals the invoked basename (argv[0] leg)");
    Check(std::find(selfMatches.begin(), selfMatches.end(), self) !=
                  selfMatches.end(),
          "proc-scan: exact-match contract finds self (consumer shape)");
    if (self != 1) {  // a healthy /proc always carries pid 1 (init/systemd)
        Check(std::any_of(scan.begin(), scan.end(),
                          [](const jk::process::ProcessImageInfo& e) {
                              return e.pid == 1;
                          }),
              "proc-scan: pid 1 (init) is among the entries");
    }
}

// Case 9 (JKPipeTransport win32-pipe-name → unix-socket mapping, docs/69 §4
// consumer wiring ② — stage-3 full-build task 5): the shared endpoint constant
// jk::ipc::kWindowServerPipeName is the literal win32 named-pipe name
// `\\.\pipe\JKWindowServerPipe`; the posix factories fold it ONCE
// (MapEndpointName — prefix stripped, '/'→'_', /tmp/<folded>.sock) so every
// call site keeps passing the constant unchanged. Under test:
//   A) CreateServer(kWindowServerPipeName) lands its socket at
//      /tmp/JKWindowServerPipe.sock (the fold actually fired);
//   B) ConnectClient(the same constant) reaches THAT server and the wire
//      round-trips bytes (both factories share one fold or clients part ways);
//   C) teardown leaves the mapped file unlinkable (case-5 shape, scratch-free).
// A real jkserver holding the constant's socket in this same WSL would make
// the bind fail — the gate environment does not run one.
void TestPipeEndpointMapping() {
    const std::string mapped = "/tmp/JKWindowServerPipe.sock";
    ::unlink(mapped.c_str());  // scratch-free start (unique-enough constant)

    std::unique_ptr<jk::ipc::JKPipeTransport> server;
    std::thread serverThread([&server]() {
        server = jk::ipc::JKPipeTransport::CreateServer(
            jk::ipc::kWindowServerPipeName);
    });

    std::unique_ptr<jk::ipc::JKPipeTransport> client;
    struct stat st {};
    for (int i = 0; i < 500 && !client; ++i) {  // 10s budget (case-5 shape)
        if (::stat(mapped.c_str(), &st) == 0 && S_ISSOCK(st.st_mode)) {
            client = jk::ipc::JKPipeTransport::ConnectClient(
                jk::ipc::kWindowServerPipeName);
        }
        if (!client) usleep(20 * 1000);
    }
    Check(client != nullptr,
          "endpoint-map: ConnectClient reaches the pipe-name constant");
    if (!client) {
        serverThread.detach();
        return;
    }
    serverThread.join();
    Check(server != nullptr,
          "endpoint-map: CreateServer accepted through the constant");
    if (!server) {
        client->Close();
        ::unlink(mapped.c_str());
        return;
    }

    const uint8_t tx[8] = {'J', 'K', 'E', 'P', 'M', 'A', 'P', '1'};
    uint8_t rx[8] = {};
    Check(client->Write(tx, sizeof(tx)) && server->Read(rx, sizeof(rx)) &&
              std::memcmp(rx, tx, sizeof(tx)) == 0,
          "endpoint-map: constant-named connection round-trips bytes");

    client->Close();
    server->Close();
    ::unlink(mapped.c_str());
    Check(::stat(mapped.c_str(), &st) != 0,
          "endpoint-map: mapped socket file removed at case end");
}

// Case 10 (플랜 F2 — docs/70 §6 #2): LLM stub shell round-trip — the posix
// JKLmEngine stub branch emits `echo '{"result":"stub ok","session_id":
// "stub-1"}'` with NO cmd.exe prefix (posix Spawn rides /bin/sh -c directly),
// and the reply JSON must round-trip through the adapter pipes verbatim for
// the legacy whole-buffer fallback parser. Same drain shape as case 2.
void TestLlmStubShell() {
    // Same drain shape as case 2's drainPipe: drains until an OBSERVED
    // EOF/broken verdict; a contract-(a) violation only ends the loop on the
    // safety bail and fails the check.
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
                const int got =
                    jk::process::ReadPipeData(pipe, buf, sizeof(buf));
                if (got > 0) {
                    sink->append(buf, static_cast<size_t>(got));
                    continue;
                }
                open = false;
                closed = true;
            } else if (broken == 109 || broken == 232) {
                open = false;
                closed = true;
            } else {
                usleep(20 * 1000);
            }
        }
        return closed;
    };

    constexpr uint32_t kStillActiveExit = 259;  // STILL_ACTIVE (win32 parity)

    jk::process::SpawnOptions opt;
    // The stub line EXACTLY as JKLmEngine.cpp's posix branch composes it
    // (single-quoted so /bin/sh keeps the inner double quotes verbatim) —
    // shared constant in agent/JKLlmEngine.h so the two stay in lockstep.
    opt.commandLineUtf8 = jk::agent::kStubShellCmdPosix;
    opt.hideWindow = true;
    opt.inheritedStdioPipes = true;
    const jk::process::SpawnResult sp = jk::process::Spawn(opt);
    Check(sp.ok, "llm: stub spawn ok");
    Check(sp.ok && sp.stdoutRead && sp.stderrRead,
          "llm: stub spawn returns both parent pipe read ends");
    if (!sp.ok) return;
    std::string out, errOut;
    const bool outEof = drainPipe(sp.stdoutRead, &out);
    const bool errEof = drainPipe(sp.stderrRead, &errOut);
    std::printf("  llm: stdout=\"%.200s\"\n", out.c_str());
    std::fflush(stdout);
    Check(out.find("\"result\":\"stub ok\"") != std::string::npos,
          "llm: stub JSON round-trip");
    Check(out.find("\"session_id\":\"stub-1\"") != std::string::npos,
          "llm: stub session id round-trip");
    Check(errOut.empty(), "llm: stub stderr empty");
    Check(outEof && errEof,
          "llm: EOF observed on both pipes (contract a: parent holds read "
          "ends only)");
    uint32_t code = 0;
    bool exited = false;
    for (int i = 0; i < 500; ++i) {  // 10s budget — echo exits at once
        if (jk::process::GetExitCode(sp.process, &code) &&
            code != kStillActiveExit) {
            exited = true;
            break;
        }
        usleep(20 * 1000);
    }
    Check(exited && code == 0, "llm: stub shell exits cleanly with 0");
    jk::process::CloseHandleLike(sp.process);
    jk::process::CloseHandleLike(sp.stdoutRead);
    jk::process::CloseHandleLike(sp.stderrRead);
}

// Case 11 (plan F4): jk::crt::LocaltimeS errno_t contract lock — success == 0,
// failure != 0. The FmtStamp call sites read this exact convention (docs/70
// §6 #4 inverted boolean: `if (!LocaltimeS(...))` treated success as failure,
// so the notes/files hub stamps were always empty). Also renders a stamp
// through the same snprintf shape as FmtStamp so the fix locks both the errno
// contract AND the renderable output (client-rendered, no automated probe).
// A "-1 forced failure" case is intentionally dropped: a negative time_t does
// not guarantee failure on every platform, so only the success==0 lock is real.
void TestLocaltimeS() {
    std::time_t t = std::time(nullptr);
    std::tm lt{};
    const int rc = jk::crt::LocaltimeS(&lt, &t);
    Check(rc == 0, "crt: LocaltimeS success == 0 (errno_t contract)");
    Check(lt.tm_year >= 126, "crt: LocaltimeS filled tm (year 2026+)");
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%02d-%02d %02d:%02d",
                  lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
    std::printf("  crt: stamp=\"%s\"\n", buf);
    std::fflush(stdout);
    Check(std::strlen(buf) == 11, "crt: FmtStamp shape renders 11 chars");
    Check(buf[2] == '-' && buf[5] == ' ',
          "crt: FmtStamp separators render");
}

// Case 12 (plan H — docs/70 §6 #5): posix job 복수 멤버 + 자식 stdin /dev/null
// 패리티.
//   A) 12a job multi-member: TWO children assigned to one kill-on-close job
//      must BOTH die on TerminateJobTree — the old single-pgid capture
//      overwrote its only member, so the first child survived the tree kill
//      (win32 JobObject keeps every AssignProcessToJobObject member).
//   B) 12b stdin parity: win32 never sets si.hStdInput (JKProcess_win32.cpp:77)
//      — a win32 child cannot read its parent's stdin. The posix child used to
//      inherit fd 0; now it gets /dev/null, so `read` ends instantly on EOF
//      and `echo got:$x` prints an empty value. A hang/timeout here means the
//      posix child is STILL inheriting the parent's stdin.
void TestJobMultiMemberAndStdinParity() {
    constexpr uint32_t kStillActiveExit = 259;  // STILL_ACTIVE (win32 parity)

    // Same drain shape as case 2's drainPipe: drains until an OBSERVED
    // EOF/broken verdict; a contract-(a) violation only ends the loop on the
    // safety bail and fails the check.
    auto drainPipe = [](void* pipe, std::string* sink) -> bool {
        char buf[4096];
        bool open = true;
        bool closed = false;
        int iters = 0;  // safety bail
        while (open) {
            if (++iters > 500) break;
            uint32_t avail = 0;
            int broken = 0;
            const bool data =
                jk::process::PeekPipeAvail(pipe, &avail, &broken);
            if (data && avail > 0) {
                const int got =
                    jk::process::ReadPipeData(pipe, buf, sizeof(buf));
                if (got > 0) {
                    sink->append(buf, static_cast<size_t>(got));
                    continue;
                }
                open = false;
                closed = true;
            } else if (broken == 109 || broken == 232) {
                open = false;
                closed = true;
            } else {
                usleep(20 * 1000);
            }
        }
        return closed;
    };
    auto trim = [](std::string s) -> std::string {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' ||
                              s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
        const size_t first = s.find_first_not_of(" \t\r\n");
        return first == std::string::npos ? std::string()
                                          : s.substr(first);
    };

    // A) 12a job multi-member — two sleepers, one job, one tree kill.
    //    Marker output is unnecessary: the exit path IS the observation.
    jk::process::SpawnOptions sleeper;
    sleeper.commandLineUtf8 = "sleep 30";
    sleeper.inheritedStdioPipes = true;
    const jk::process::SpawnResult m1 = jk::process::Spawn(sleeper);
    const jk::process::SpawnResult m2 = jk::process::Spawn(sleeper);
    Check(m1.ok && m1.process && m2.ok && m2.process,
          "job12: two multi-member job children spawn");
    void* job3 = jk::process::CreateKillOnCloseJob();
    Check(job3 != nullptr,
          "job12: multi-member kill-on-close job handle is non-null");
    Check(jk::process::AssignToJob(job3, m1) &&
              jk::process::AssignToJob(job3, m2),
          "job12: both children assign to the SAME job (accumulate, not "
          "overwrite)");
    Check(jk::process::AssignToJob(job3, m1),
          "job12: re-assigning the same child is idempotent and true "
          "(controller ruling)");
    Check(jk::process::TerminateJobTree(job3, 1),
          "job12: terminate kills every member tree (returns ok)");
    // Brief shape: WaitForExit(3000) on BOTH members — the old overwrite left
    // the first-assigned child running (it died 30s later, not here).
    const bool w1 = jk::process::WaitForExit(m1.process, 3000);
    const bool w2 = jk::process::WaitForExit(m2.process, 3000);
    Check(w1 && w2,
          "job12: BOTH members left after the single TerminateJobTree "
          "(WaitForExit(3000) each)");
    // 코드는 정상 종료(0) 또는 128+9(137) 둘 다 수용 — SIGKILL 도착 타이밍
    // 무관(sleep은 신호 사망 137이 상통, ok만 단정).
    uint32_t code1 = 0, code2 = 0;
    Check(jk::process::GetExitCode(m1.process, &code1) &&
              jk::process::GetExitCode(m2.process, &code2) &&
              (code1 == 0 || code1 == 137) && (code2 == 0 || code2 == 137),
          "job12: members report 0 or 128+9=137 (SIGKILL landing timing)");
    Check(code1 != kStillActiveExit && code2 != kStillActiveExit,
          "job12: reaped exit codes are stable (no 259 residue)");
    jk::process::CloseHandleLike(m1.stdoutRead);
    jk::process::CloseHandleLike(m1.stderrRead);
    jk::process::CloseHandleLike(m1.process);
    jk::process::CloseHandleLike(m2.stdoutRead);
    jk::process::CloseHandleLike(m2.stderrRead);
    jk::process::CloseHandleLike(m2.process);
    jk::process::CloseHandleLike(job3);  // close IS the kill; already dead

    // B) 12b stdin /dev/null parity — a reader child must NOT block forever.
    jk::process::SpawnOptions reader;
    reader.commandLineUtf8 = "read x; echo got:$x";
    reader.inheritedStdioPipes = true;
    const jk::process::SpawnResult rd = jk::process::Spawn(reader);
    Check(rd.ok && rd.process && rd.stdoutRead,
          "job12: stdin-parity reader spawns with pipes");
    if (!(rd.ok && rd.process)) return;
    // The parity observation: read hits /dev/null EOF instantly, echo prints
    // the empty value, sh exits 0 — all well inside a 2s wait budget. A
    // FAIL here means the posix child still holds the parent's stdin.
    const bool readerExited = jk::process::WaitForExit(rd.process, 2000);
    uint32_t code3 = 0;
    jk::process::GetExitCode(rd.process, &code3);
    std::string rdOut, rdErr;
    const bool rdEof = drainPipe(rd.stdoutRead, &rdOut);
    drainPipe(rd.stderrRead, &rdErr);
    std::printf("  job12: reader exit=%u stdin-free out=\"%.200s\"\n",
                static_cast<unsigned>(code3), rdOut.c_str());
    std::fflush(stdout);
    Check(readerExited,
          "job12: reader exits inside 2000ms (stdin is /dev/null, never "
          "the parent's stdin)");
    Check(!rdEof || trim(rdOut) == "got:",
          "job12: reader prints got: with an empty $x (EOF emptied the var)");
    Check(rdEof, "job12: reader stdout EOF observed (contract a intact)");
    Check(rdErr.empty(), "job12: reader stderr empty");
    jk::process::CloseHandleLike(rd.stdoutRead);
    jk::process::CloseHandleLike(rd.stderrRead);
    jk::process::CloseHandleLike(rd.process);
}

// Case 13 (plan H — docs/70 §6 #7): RecvAll 청크 패턴+부분읽기 timeout
// 경계+Accept 상한 실측.
//   A) 13a RecvAll 청크: 320B를 7개 불규칙 청크(13·47·64·1·128·33·34, 청크
//      사이 10-20ms)로 보내면 RecvAll(s, buf, 320)이 전부 모아 memcmp 일치 —
//      부분 recv 루프의 정확성 봉합(docs/68 승계 "수백 바이트 RecvAll 패턴";
//      케이스 3은 5바이트 단발이라 다른 의미).
//   B) 13b 부분읽기 이후 timeout 경계: posix RecvAll은 r<=0 → false로
//      fail-closed(JKNet_posix.cpp:104-118, EINTR만 재시도 — EAGAIN 포함).
//      피어가 64B 중 32B만 보내고 700ms 뒤 나머지를 보내면 SO_RCVTIMEO 400ms가
//      부분읽기 도중의 recv를 EAGAIN으로 끊고 RecvAll은 정직하게 false.
//      (케이스 3 D의 "무데이터 timeout"과 대비되는 부분읽기 경로.)
//   C) 13c Accept 무한블록 방어 실측: Linux는 listening 소켓의 SO_RCVTIMEO를
//      accept에도 적용 — 아무도 connect하지 않아도 Accept가 ~즉시
//      kInvalidSocket을 반환해야 한다. hang 방어막 alarm(20): SIGALRM 기본
//      동작=프로세스 사망이므로 hang은 조용한 붙잡힘이 아니라 요란한 적색.
void TestNetRecvAllChunksTimeoutAcceptBound() {
    // A) 13a — 320 bytes in 7 irregular chunks. Reference filler is shared
    //    with the client thread (same buffer, same formula) so memcmp is the
    //    only verdict needed.
    std::vector<char> refA(320);
    for (size_t i = 0; i < refA.size(); ++i)
        refA[i] = static_cast<char>(i * 97 + 13);
    static constexpr int kChunkSizes[7] = {13, 47, 64, 1, 128, 33, 34};
    static constexpr int kChunkDelays[6] = {15, 10, 20, 12, 18, 10};  // ms

    std::uint16_t portA = 0;
    const jk::net::Socket lpA =
        jk::net::ListenTcp("127.0.0.1", 0, 4, &portA);
    Check(lpA != jk::net::kInvalidSocket,
          "net13: chunk-test listener opens (backlog 4)");
    Check(portA != 0, "net13: chunk-test listener reports its bound port");

    struct ChunkClientResult {
        int connectErr;
        int sent;
    } ca{-1, -1};
    auto clientBodyA = [&ca, portA, &refA]() {
        const int c = socket(AF_INET, SOCK_STREAM, 0);
        if (c < 0) return;
        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_addr.s_addr = inet_addr("127.0.0.1");
        dst.sin_port = htons(portA);
        ca.connectErr = connect(c, reinterpret_cast<sockaddr*>(&dst),
                                sizeof(dst));
        if (ca.connectErr != 0) {
            ::close(c);
            return;
        }
        int sent = 0;
        int off = 0;
        for (int i = 0; i < 7; ++i) {
            const int n = static_cast<int>(send(c, refA.data() + off,
                                                kChunkSizes[i], 0));
            if (n > 0) {
                off += n;
                sent += n;
            }
            if (i < 6) NapMs(kChunkDelays[i]);
        }
        ca.sent = sent;
        sleep(1);  // hold open so the server's RecvAll sees data, not EOF
        ::close(c);
    };
    std::thread clientA(clientBodyA);

    const jk::net::Socket accA = jk::net::Accept(lpA);
    Check(accA != jk::net::kInvalidSocket,
          "net13: Accept returns the raw chunk client");
    std::vector<char> gotA(320, 0);
    Check(jk::net::RecvAll(accA, gotA.data(), 320),
          "net13: RecvAll assembles 320 bytes across 7 irregular chunks");
    Check(std::memcmp(gotA.data(), refA.data(), 320) == 0,
          "net13: assembled 320 bytes match the reference pattern");
    jk::net::Close(lpA);
    jk::net::ShutdownBoth(accA);
    jk::net::Close(accA);
    clientA.join();
    Check(ca.connectErr == 0 && ca.sent == 320,
          "net13: raw client sent all 320 bytes in 7 chunks");

    // B) 13b — partial read, then EAGAIN: RecvAll must be honest (false).
    //    The client holds the connection ~2s AFTER its second send, so a
    //    false can only come from SO_RCVTIMEO firing mid-loop — never EOF.
    std::vector<char> refB(64);
    for (size_t i = 0; i < refB.size(); ++i)
        refB[i] = static_cast<char>(i * 97 + 13);

    std::uint16_t portB = 0;
    const jk::net::Socket lpB =
        jk::net::ListenTcp("127.0.0.1", 0, 4, &portB);
    Check(lpB != jk::net::kInvalidSocket,
          "net13: timeout-test listener opens");
    Check(portB != 0, "net13: timeout-test listener reports its bound port");

    struct GapClientResult {
        int connectErr;
        int sent;
    } cb{-1, -1};
    auto clientBodyB = [&cb, portB, &refB]() {
        const int c = socket(AF_INET, SOCK_STREAM, 0);
        if (c < 0) return;
        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_addr.s_addr = inet_addr("127.0.0.1");
        dst.sin_port = htons(portB);
        cb.connectErr = connect(c, reinterpret_cast<sockaddr*>(&dst),
                                sizeof(dst));
        if (cb.connectErr != 0) {
            ::close(c);
            return;
        }
        cb.sent = static_cast<int>(send(c, refB.data(), 32, 0));
        NapMs(700);  // > SO_RCVTIMEO 400 — induces EAGAIN mid RecvAll loop
        cb.sent += static_cast<int>(send(c, refB.data() + 32, 32, 0));
        sleep(2);  // hold open far past the server's check — no EOF escape
        ::close(c);
    };
    std::thread clientB(clientBodyB);

    const jk::net::Socket accB = jk::net::Accept(lpB);
    Check(accB != jk::net::kInvalidSocket,
          "net13: Accept returns the half-send client");
    jk::net::SetTimeouts(accB, 400);
    char bufB[64] = {};
    const double tB = nowMs();
    const bool fullRead = jk::net::RecvAll(accB, bufB, 64);
    const double elapsedB = nowMs() - tB;
    std::printf("  net13: partial-read RecvAll=false after %.0f ms\n",
                elapsedB);
    std::fflush(stdout);
    Check(!fullRead,
          "net13: RecvAll reports false on a mid-read EAGAIN (fail-closed "
          "r<=0, EINTR-only retry)");
    Check(elapsedB < 2500.0,
          "net13: SO_RCVTIMEO 400 bounded the partial read (elapsed << the "
          "client's 2s hold)");

    // Teardown AFTER the client joins: its second send (at 700ms) must land
    // on a live socket, not on a closed one (raw send lacks MSG_NOSIGNAL, so
    // a racing close could SIGPIPE the harness itself).
    clientB.join();
    jk::net::Close(lpB);
    jk::net::ShutdownBoth(accB);
    jk::net::Close(accB);
    Check(cb.connectErr == 0 && cb.sent == 64,
          "net13: client sent 32+32 across the 700ms gap");

    // C) 13c — Accept must not block forever: SO_RCVTIMEO applies to accept
    //    on Linux. alarm(20) is the hang net; SIGALRM's default action ends
    //    the process loudly, so a regression reads as an abrupt RED death,
    //    not a silent parked run.
    alarm(20);
    std::uint16_t portC = 0;
    const jk::net::Socket lpC =
        jk::net::ListenTcp("127.0.0.1", 0, 4, &portC);
    Check(lpC != jk::net::kInvalidSocket, "net13: 13c listener opens");
    jk::net::SetTimeouts(lpC, 300);
    const double tC = nowMs();
    const jk::net::Socket nobody = jk::net::Accept(lpC);  // no client coming
    const double elapsedC = nowMs() - tC;
    alarm(0);  // timer off — no SIGALRM leak into later cases
    std::printf("  net13: accept with no peer returned in %.0f ms\n",
                elapsedC);
    std::fflush(stdout);
    Check(nobody == jk::net::kInvalidSocket,
          "net13: Accept with no peer returns kInvalidSocket (SO_RCVTIMEO "
          "bounds accept on Linux)");
    Check(elapsedC < 2500.0,
          "net13: unattended accept returns promptly (no infinite block)");
    jk::net::Close(lpC);
}

// Case 15 (T3 — 스펙 2026-10-08-chat-llm-promotion 설계 결정 2·3): 동기 턴
// 브리지(TurnSync) + ollama-direct leg. jkdesktop RunAppSelfTest의 1n-d 계열이
// 캐논 담당(양축)이고 이 케이스는 어댑터 축의 정독용 분신이다 — agent TU를
// 직접 링크해(빌드.sh) /bin/sh -c 스폰 경로에서 TurnSync의 본 계약을 잠근다:
//   a) echo 스터브 stdout 원문 왕복(ok=true, sessionId="", streamed=0)
//      — 실 ollama 의존 금지(환경 의존 함정): cfg의 direct_cmd로 스폰을 주입
//   b) 비영(stdout 공백) → ok=false 정직
//   c) 조립식 원문(BuildOllamaDirectCmd — 공개 계약 락) 단정
// chat.json은 exe-dir의 state에 시딩 — GetExecutablePath는 파일 경로를 주므로
// 이 하네스의 exe-dir는 engine/build이다(디테일 함정: "posix_selftest/state"
// 가 아니라 실 사용자 jkdesktop의 state/chat.json에 닿는다 — 첫 실측에서 이
// 경로 착각이 본사 chat.json을 시딩→소각으로 소멸시켰고, 복구 실측이 확인).
// 그래서 본사 파일 보존이 계약이다: 백업 원문 → 시딩 → 복원(없던 기기는 소각),
// jkdesktop RunAppSelfTest의 1n-d 블록과 같은 가드.
void TestLlmSyncOllamaDirect() {
    const std::string exePath = jk::fs::GetExecutablePath();
    const size_t sep = exePath.find_last_of('/');
    std::string dir = exePath.substr(0, sep);
    const std::string stateDir = dir + "/state";
    const std::string cfgPath = stateDir + "/chat.json";
    ::mkdir(stateDir.c_str(), 0755);
    std::string cfgBackup;
    bool hadCfg = false;
    if (std::FILE* bf = std::fopen(cfgPath.c_str(), "rb")) {
        std::fseek(bf, 0, SEEK_END);
        const long sz = std::ftell(bf);
        std::fseek(bf, 0, SEEK_SET);
        if (sz > 0) {
            cfgBackup.resize(static_cast<size_t>(sz));
            const size_t n = std::fread(&cfgBackup[0], 1, cfgBackup.size(), bf);
            cfgBackup.resize(n);
            hadCfg = n > 0;
        }
        std::fclose(bf);
    }
    auto WriteCfg = [&cfgPath](const std::string& json) -> bool {
        std::FILE* f = std::fopen(cfgPath.c_str(), "wb");
        if (!f) return false;
        const size_t w = std::fwrite(json.data(), 1, json.size(), f);
        std::fclose(f);
        return w == json.size();
    };
    // directory=/tmp — cfg 기본값은 repo 절대 경로(chdir 실패 함정, 1n-d 동일
    // 렛슨)이고 /tmp는 이 케이스의 어댑터가 이미 다녀간 실제 경로.
    auto Seed = [&WriteCfg](const std::string& directCmd) -> bool {
        return WriteCfg("{\"engine\":\"ollama-direct\",\"model\":"
                        "\"glm-test-stub:cloud\",\"directory\":\"/tmp\","
                        "\"direct_cmd\":\"" +
                        directCmd + "\"}");
    };

    Check(Seed("echo jk-ollama-direct-stub-3361"),
          "llm15: chat.json seeded (engine=ollama-direct + direct_cmd, "
          "real-ollama-free)");
    jk::agent::LlmTurnResult r;
    {
        jk::agent::JKLlmEngine eng;
        Check(eng.TurnSync("안녕", r), "llm15: echo stub turn returns true");
        Check(r.ok && r.result == "jk-ollama-direct-stub-3361",
              "llm15: stdout collected verbatim, boundary-ws trimmed");
        Check(r.sessionId.empty(),
              "llm15: sessionId empty (plain-text leg contract)");
        Check(!r.streamed,
              "llm15: no delta on the plain-text leg (streamed=false)");
    }

    Check(Seed("true"),
          "llm15: cfg re-seeded (empty-stdout stub, honest-fail 2형)");
    {
        jk::agent::JKLlmEngine eng;
        Check(!eng.TurnSync("x", r) && !r.ok,
              "llm15: empty stdout reports ok=false (spawn failure honesty)");
    }

    // 조립식 원문 — 스폰 대체 없이 컴포지션만 잠근다(kStubShellCmd* 선례).
    {
        jk::agent::ChatConfig cc;
        cc.model = "glm-test:cloud";
        const std::string cmd =
            jk::agent::BuildOllamaDirectCmd(cc, "say \"hi\" $(id)");
        Check(cmd.compare(0, 12, "ollama run \"") == 0 && cmd.back() == '"',
              "llm15: composed cmd keeps the quote spans closed");
        Check(cmd.find("say \\\"hi\\\" \\$(id)") != std::string::npos,
              "llm15: posix escaper neutralizes the sh live characters");
        Check(cmd.find("[사용자] say") != std::string::npos,
              "llm15: preamble rides the plain-text leg too");
    }

    // 본사 파일 보존 — 있던 기기는 원문 복구, 없던 기기만 소각(스크래치 없음).
    // "seeded chat.json removed"의 진실 조건은 시딩 전에도 없었다는 것.
    if (hadCfg) {
        Check(WriteCfg(cfgBackup), "llm15: chat.json restored verbatim");
    } else {
        ::unlink(cfgPath.c_str());
        Check(::access(cfgPath.c_str(), F_OK) != 0,
              "llm15: seeded chat.json removed at case end (was absent)");
    }
}


}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    TestFsGetExecutablePath();
    TestProcessAdapter();
    TestNetAdapter();
    TestPtyBridge();
    TestPipeTransport();
    TestInstanceLock();
    TestTextConv();
    TestProcessScan(argv);
    TestPipeEndpointMapping();
    TestLlmStubShell();
    TestLlmSyncOllamaDirect();
    TestLocaltimeS();
    TestJobMultiMemberAndStdinParity();
    TestNetRecvAllChunksTimeoutAcceptBound();
    std::printf("PosixSelfTest: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
