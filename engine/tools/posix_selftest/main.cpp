// posix_selftest/main.cpp — WSL-verified selftest harness for posix adapter
// bodies (stage-2 plan D, task 1). Win32 has no counterpart; this exe is built
// and run only under Ubuntu-24.04 WSL via engine/tools/posix_selftest/build.sh.
// Convention mirrors the win32 in-app selftest (engine/src/main.cpp
// RunAppSelfTest): one "[PASS]/[FAIL] <case>" line per check, the total as
// "PosixSelfTest: <n> failure(s)", exit non-zero on any failure.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>  // nanosleep (D-T3 hygiene: nanosleep, not usleep)
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>  // stat, S_ISSOCK (socket-file liveness triage below)
#include <unistd.h>  // access, unlink, getpid, R_OK, close, sleep

#include <arpa/inet.h>  // inet_addr, htons (raw client below — this TU is
#include <netinet/in.h>  // posix-only, so it may include POSIX socket headers
#include <sys/socket.h>  // directly; no windows.h-cleanliness constraint here)

#include <ipc/JKPipeTransport.h>
#include <fs/JKInstanceLock.h>
#include <net/JKNet.h>
#include <process/JKProcess.h>
#include <terminal/JKConPtyBridge.h>

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

}  // namespace

int main() {
    TestFsGetExecutablePath();
    TestProcessAdapter();
    TestNetAdapter();
    TestPtyBridge();
    TestPipeTransport();
    TestInstanceLock();
    std::printf("PosixSelfTest: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
