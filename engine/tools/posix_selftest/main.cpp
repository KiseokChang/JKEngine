// posix_selftest/main.cpp — WSL-verified selftest harness for posix adapter
// bodies (stage-2 plan D, task 1). Win32 has no counterpart; this exe is built
// and run only under Ubuntu-24.04 WSL via engine/tools/posix_selftest/build.sh.
// Convention mirrors the win32 in-app selftest (engine/src/main.cpp
// RunAppSelfTest): one "[PASS]/[FAIL] <case>" line per check, the total as
// "PosixSelfTest: <n> failure(s)", exit non-zero on any failure.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>  // std::abs (2g-c 산치)
#include <cstdio>
#include <cstring>  // std::memcmp
#include <ctime>  // nanosleep (D-T3 hygiene: nanosleep, not usleep)
#include <filesystem>  // 2g-e 폴더 열거 실측(임시 폴더)
#include <fstream>  // 2g-e 스텁 이미지 생성
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>  // stat, S_ISSOCK (socket-file liveness triage below)
#include <unistd.h>  // access, unlink, getpid, R_OK, close, sleep

#include <arpa/inet.h>  // inet_addr, htons (raw client below — this TU is
#include <netinet/in.h>  // posix-only, so it may include POSIX socket headers
#include <sys/socket.h>  // directly; no windows.h-cleanliness constraint here)

#include <agent/JKLlmEngine.h>  // kStubShellCmdPosix (case 10), TurnSync (case 15)
#include <client/JKActivityGate.h>  // case 2i (T1) — 클라 활동 게이트 순수 부품
#include <apps/ChatRouter.h>  // 자연어 승격 배선 (T4 — case 16 twin)
#include <apps/GalleryModel.h>  // case 2g (T1) — 갤러리 순수 부품 직링크
#include <JKTextAtlas.h>  // case 19 (T2) — 텍스트 배율 결선 순수 부품 단정
#include <ipc/JKPipeTransport.h>
#include <ipc/JKWireEndpoints.h>
#include <server/JKFrameDirty.h>  // case 17 (T1) — 더티 계산기 순수 단정
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
// JKLlmEngine stub branch emits `echo '{"result":"stub ok","session_id":
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
    // The stub line EXACTLY as JKLlmEngine.cpp's posix branch composes it
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

    // F3-1 posix 대응 단정(T3 fix r1, 2026-10-08) — win의 cmd 토글 수리는
    // posix 분기 원문 유지(계약 c)와 별개로, sh 지역 걷기(동형 단정)와 라이브
    // sh 관측으로 posix leg의 주입 방어를 대등하게 증명한다. /bin/sh -c 규약:
    // 이중 따옴표 지역 안 `\\x`는 리터럴 x(지역 유지)이므로 ShellDqEscape의
    // `\ $ 백틱` 이스케이프가 컨텐츠 메타문자를 사망시킨다.
    //
    // 알려진 잔여(win과 같은 원장 줄): 인용 안 `%VAR%` 확장은 batch 컨텍스트
    // 전용(cmd가 확장이지만 sh는 무관) — posix leg에는 % 확장 자체가 없다.
    {
        jk::agent::ChatConfig cc;
        cc.model = "glm-test:cloud";
        const std::string hostile =
            "she said \"hi & echo INJECTED-MARKER-3361 <in|out> $(id)";
        const std::string cmd =
            jk::agent::BuildOllamaDirectCmd(cc, hostile);

        // sh 지역 걷기 — 지역 안 백슬래시 짝을 한 스텝으로 소화하고, 지역 외
        // 메타문자(& | < > ;)가 하나라도 착지하면 방어 실패. 종료 시 지역
        // 닫힘까지 단정한다.
        auto MetaShielded = [](const std::string& composed) {
            int state = 0;
            for (size_t i = 0; i < composed.size(); ++i) {
                const char ch = composed[i];
                if (ch == '\\' && state == 1 && i + 1 < composed.size()) {
                    ++i;  // sh dq 지역 안 \x — 리터럴화
                    continue;
                }
                if (ch == '"') state ^= 1;
                else if (state == 0 && std::strchr("&|<>;", ch) != nullptr)
                    return false;
            }
            return state == 0;
        };
        Check(MetaShielded(cmd),
              "llm15: composed region shields the hostile metacharacters "
              "(sh walk)");
        Check(cmd.find("she said \\\"hi & echo INJECTED-MARKER-3361") !=
                      std::string::npos,
              "llm15: posix keeps the sh bkslash-quote form (leg unchanged)");

        // 라이브 sh — echo 자식(추가 바이너리 금지; /bin/sh -c 접두는 어댑터
        // 실 계약). 양성 대조(불균형 맨따옴표 — 지역 조기 닫힘 → 마커 별행)로
        // 검출기 자체를 증명한 뒤, 조립식 지역의 무주입을 실 sh에서 증명.
        auto MarkerOwnLine = [](const std::string& out) {
            size_t at = 0;
            while ((at = out.find("INJECTED-MARKER-3361", at)) !=
                   std::string::npos) {
                const size_t bol = out.find_last_of('\n', at);
                size_t next = out.find('\n', at);
                if (next == std::string::npos) next = out.size();
                const size_t lineBegin =
                    bol == std::string::npos ? 0 : bol + 1;
                if (out.compare(lineBegin, 20, "INJECTED-MARKER-3361") == 0)
                    return true;
                at += 20;
            }
            return false;
        };
        auto RunSh = [](const std::string& cmdline,
                        std::string* out) -> bool {
            jk::process::SpawnOptions o;
            o.commandLineUtf8 = cmdline;
            o.inheritedStdioPipes = true;
            const jk::process::SpawnResult r = jk::process::Spawn(o);
            if (!r.ok) return false;
            // TestProcessAdapter의 drainPipe 동형 — 관측 EOF/브로큰 판정.
            char buf[4096];
            bool open = true;
            while (open) {
                uint32_t avail = 0;
                int broken = 0;
                if (jk::process::PeekPipeAvail(r.stdoutRead, &avail,
                                               &broken) &&
                    avail > 0) {
                    const int got = jk::process::ReadPipeData(
                        r.stdoutRead, buf, sizeof(buf));
                    if (got > 0) {
                        out->append(buf, static_cast<size_t>(got));
                        continue;
                    }
                    open = false;
                } else if (broken == 109 || broken == 232) {
                    open = false;
                } else {
                    usleep(10 * 1000);
                }
            }
            uint32_t code = 0;
            bool exited = false;
            for (int i = 0; i < 500 && !exited; ++i) {  // 10s budget
                if (jk::process::GetExitCode(r.process, &code) &&
                    code != 259)
                    exited = true;
                else
                    usleep(20 * 1000);
            }
            jk::process::CloseHandleLike(r.stdoutRead);
            jk::process::CloseHandleLike(r.stderrRead);
            jk::process::CloseHandleLike(r.process);
            return exited && code == 0;
        };

        std::string vuln;
        // 함정: sh(dash)는 닫히지 않은 인용에서 파싱 오류로 별행 개통 없이
        // 즉사한다 — 양성 대조는 인용수를 짝수로 맞춰 실 주입 모양을 유지한다
        // (개선 전 수형의 cmd와 같은 모양을 sh 규약으로 재현).
        Check(RunSh("echo \"she said \"hi & echo INJECTED-MARKER-3361\"\"",
                    &vuln) &&
                  MarkerOwnLine(vuln),
              "llm15: live sh: bare-quote shape injects (positive control)");
        std::printf("  llm15: vuln out=[%s]\n", vuln.c_str());
        std::fflush(stdout);

        const std::string modelSpan = "\"glm-test:cloud\" ";
        const std::string region =
            cmd.substr(cmd.find(modelSpan) + modelSpan.size());
        std::string fixedOut;
        Check(RunSh("echo " + region, &fixedOut) &&
                  !MarkerOwnLine(fixedOut) && fixedOut.find("she said \"") !=
                                                   std::string::npos &&
                  fixedOut.find("INJECTED-MARKER-3361") !=
                      std::string::npos,
              "llm15: live sh: composed region carries the marker as "
              "content, no injection");
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

// Case 16 (T4 — 스펙 2026-10-08-chat-llm-promotion 설계 결정 3·4): 자연어
// 승격 배선. jkdesktop RunAppSelfTest의 1n-f 계열이 캐논 담당(양축)이고 이
// 케이스는 어댑터 축의 분신 — /bin/sh -c 스폰 경로에서 배선 순서를 잠근다:
//   ① 정확 트리거 매치 = LLM 우회(TurnSync 0) ② 비매치 + cfg 구성 =
//   TurnSync + 행동 JSON 파싱 ③ LLM 실패/파싱 불가·cfg 미구성 = stub
//   안내문 폴백. 스폰은 direct_cmd echo 스터브 주입(실 ollama 의존 금지 —
//   llm15의 같은 계약; sh는 이중 따옴표를 벗기므로 시딩은 인용 단일화).
//   chat.json은 llm15와 같은 exe-dir 계약(engine/build/state — 본사 파일
//   보존: 백업 원문 → 시딩 → 복원/소각).
void TestChatPromoteWiring() {
    using jk::agent::ChatConfig;
    // (a) 순수 파서+게이트 — 플랫폼 무관 로직의 어댑터 축 도표.
    {
        jk::agent::ChatConfig mc;
        mc.engine = "ollama-direct";
        mc.fileKnown = true;
        Check(jk::ChatLlmEngineConfigured(mc), "llm16: configured = ollama-direct");
        ChatConfig ms;
        ms.engine = "stub";
        ms.fileKnown = true;
        Check(!jk::ChatLlmEngineConfigured(ms), "llm16: configured = false for stub (미구성)");
        ChatConfig mf;
        Check(!jk::ChatLlmEngineConfigured(mf),
              "llm16: default cfg (no file) = unconfigured — fileKnown gate "
              "(NR4-1, opt-in 가법)");
        jk::ChatAction a;
        std::string note;
        Check(jk::ChatLlmActionParse(
                  "```json\n{\"action\":\"launch\",\"app\":\"minesweeper\","
                  "\"text\":\"지뢰찾기를 실행합니다.\"}\n```", a, note) &&
                  a.kind == jk::ChatAction::Launch &&
                  a.app == "minesweeper",
              "llm16: code-fenced action JSON parses (fence strip)");
        Check(jk::ChatLlmActionParse(
                  "{\"action\":\"talk\",\"text\":\"TALK-GUIDE-3361\"}", a,
                  note) && a.kind == jk::ChatAction::Info &&
                  note == "TALK-GUIDE-3361",
              "llm16: talk = informational turn (Info, text rides the guide)");
        Check(!jk::ChatLlmActionParse("행동 JSON 누락", a, note),
              "llm16: no JSON at all → parse fail (honest fallback feed)");
        Check(!jk::ChatLlmActionParse(
                  "{\"action\":\"launch\"}", a, note),
              "llm16: launch without app → parse fail");
        const std::string body = jk::ChatLlmTurnPrompt("지뢰찾기 좀 띄워줘");
        Check(body.find("action: launch / close / focus / list / talk") !=
                          std::string::npos &&
                      body.find('"') == std::string::npos &&
                      body.find('|') == std::string::npos &&
                      body.find("켜줘") != std::string::npos &&
                      body.find("[발화] 지뢰찾기 좀 띄워줘") !=
                          std::string::npos,
              "llm16: prompt body carries quote·pipe-free schema+trigger "
              "table+utterance (ollama leg re-quote 헤지)");
        Check(body.find("[시스템 지시]") == std::string::npos,
              "llm16: preamble NOT duplicated in the prompt body (엔진 접두 "
              "단일 출처)");
    }

    // (b) wiring — chat.json 시딩(llm15의 exe-dir 계약 그대로) + echo 스터브.
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
    auto Seed = [&WriteCfg](const std::string& directCmd) -> bool {
        // /tmp directory(chdir 실패 함정 — llm15 동일) + 인용·백슬래시 이중
        // 이스케이프(수기 인게스트 경고 원문의 코드판).
        std::string cmdEsc;
        for (char ch : directCmd) {
            if (ch == '"' || ch == '\\') { cmdEsc += '\\'; cmdEsc += ch; }
            else cmdEsc += ch;
        }
        return WriteCfg("{\"engine\":\"ollama-direct\",\"model\":"
                        "\"glm-test-stub:cloud\",\"directory\":\"/tmp\","
                        "\"direct_cmd\":\"" +
                        cmdEsc + "\"}");
    };
    // sh는 이중 따옴표를 벗기므로(JSON에 다시 필요) 인용 전체를 홑따옴표로.
    const std::string focusSeed =
        "echo '{\"action\":\"focus\",\"text\":\"LLM-TOOK-THE-TURN-3361\"}'";
    Check(Seed(focusSeed),
          "llm16: chat.json seeded (llm focus marker, ollama-direct)");
    {
        jk::agent::ChatConfig mc;
        mc.engine = "ollama-direct";
        mc.fileKnown = true;
        mc.directCmd = focusSeed;
        jk::ChatAction a;
        bool used = true;
        const std::string guide =
            jk::ChatRouteTurn("지뢰찾기 켜줘", a, mc, &used);
        Check(!used && a.kind == jk::ChatAction::Launch &&
                  a.app == "minesweeper",
              "llm16: exact trigger match bypasses the LLM turn (usedLlm=false)");
        jk::ChatAction ar;
        Check(guide == jk::ChatRouterRoute("지뢰찾기 켜줘", ar),
              "llm16: bypass guide is the legacy router guide verbatim");
    }

    const std::string launchSeed =
        "echo '{\"action\":\"launch\",\"app\":\"tetris\","
        "\"text\":\"TETRIS-LLM-GUIDE-3361\"}'";
    Check(Seed(launchSeed), "llm16: cfg re-seeded (launch JSON stub)");
    {
        jk::agent::ChatConfig mc;
        mc.engine = "ollama-direct";
        mc.fileKnown = true;
        jk::ChatAction a;
        bool used = false;
        const std::string guide =
            jk::ChatRouteTurn("테트리스 좀 부탁할게", a, mc, &used);
        Check(used && a.kind == jk::ChatAction::Launch && a.app == "tetris",
              "llm16: non-match + configured cfg → LLM turn parses into a "
              "Launch action (sh echo stub)");
        Check(guide == "TETRIS-LLM-GUIDE-3361",
              "llm16: model text becomes the guide");
    }

    // (b+) NR4-1 (T4 fix r1) — 파일 부재·engine 키 누락 = 미구성(opt-in
    // 가법 — 컨트롤러 룰링). 파일 소각 → LoadChatConfig → 표지 false·engine
    // 기본값 무변(jkbridge/jkchat 원계약) → 비매치 발화 스폰 0건 + stub
    // 안내문. engine 키 누락 파일도 동일. 구성 파일은 표지 true → TurnSync
    // 시도(스터브 마커 회신이 영수증).
    {
        ::unlink(cfgPath.c_str());
        const jk::agent::ChatConfig absentF = jk::agent::LoadChatConfig();
        Check(!absentF.fileKnown && absentF.engine == "ollama",
              "llm16: chat.json absent → fileKnown=false (engine 기본값 무변 "
              "— jkbridge/jkchat 소비자 영향 0)");
        Check(!jk::ChatLlmEngineConfigured(absentF),
              "llm16: file-absent cfg = unconfigured (승격 opt-in 가법)");
        jk::ChatAction a;
        bool used = true;   // 오염 증거 — 스폰하면 지워지지 않는다
        const std::string guide = jk::ChatRouteTurn(
            "세상엔 채팅이 이렇게 어려웠나", a, absentF, &used);
        Check(!used && a.kind == jk::ChatAction::Info,
              "llm16: file-absent non-match utterance spawns 0 LLM turns");
        jk::ChatAction ar;
        Check(guide == jk::ChatRouterRoute("세상엔 채팅이 이렇게 어려웠나", ar),
              "llm16: file-absent fallback guide is the legacy router guide "
              "verbatim");
        Check(WriteCfg("{\"model\":\"glm-mute:cloud\"}"),
              "llm16: chat.json seeded without engine key");
        const jk::agent::ChatConfig noKeyF = jk::agent::LoadChatConfig();
        Check(!noKeyF.fileKnown && !jk::ChatLlmEngineConfigured(noKeyF),
              "llm16: engine key missing = unconfigured (engine 키 실제 구성 "
              "계약)");
        jk::ChatAction b;
        bool usedB = true;
        (void)jk::ChatRouteTurn("뜬금없는 말 3361", b, noKeyF, &usedB);
        Check(!usedB && b.kind == jk::ChatAction::Info,
              "llm16: engine-key-missing non-match spawns 0 LLM turns");
        Check(Seed(focusSeed),
              "llm16: cfg re-seeded (engine key back — opt-in half of NR4-1)");
        const jk::agent::ChatConfig fromFileF = jk::agent::LoadChatConfig();
        Check(fromFileF.fileKnown && jk::ChatLlmEngineConfigured(fromFileF),
              "llm16: LoadChatConfig raises fileKnown for an engine-keyed "
              "file (gate opens)");
        jk::ChatAction c;
        bool usedC = false;
        const std::string guideC = jk::ChatRouteTurn(
            "아무 말 3361", c, fromFileF, &usedC);
        Check(usedC && c.kind == jk::ChatAction::Focus && guideC ==
                  "LLM-TOOK-THE-TURN-3361",
              "llm16: configured-from-file non-match attempts the TurnSync "
              "(stub marker = attempt receipt)");
    }

    Check(Seed(jk::agent::kStubShellCmdPosix),
          "llm16: cfg re-seeded (stub-echo JSON, no action schema)");
    {
        // T4 fix r2 (NR4R-1): 표지 미보정 재발 수리 — 이 수형의 단정 목표는
        // "턴을 시도했는데 응답 파싱이 실패해 폴백"이다(게이트 차단이 아님).
        // 시딩 파일(kStubShellCmdPosix echo)은 engine 키를 갖는 유효 JSON이라
        // fileKnown=true가 참이고, 스키마 밖 회신은 **응답**만의 결함 —
        // win 1n-f24의 posix 쌍둥이(NR4-1 원장 concern 1 동일 함정의 마지막
        // 잔여 — 구성 mc 전수 재검토로 이 1건만 남았다).
        jk::agent::ChatConfig mc;
        mc.engine = "ollama-direct";
        mc.fileKnown = true;   // 표지 — 개통돼야 턴 시도·파싱 실패가 성립
        jk::ChatAction a;
        bool used = true;
        const std::string guide =
            jk::ChatRouteTurn("뜬금없는 발화 3361", a, mc, &used);
        Check(!used && a.kind == jk::ChatAction::Info,
              "llm16: unparsable engine reply falls back honestly");
        jk::ChatAction ar;
        Check(guide == jk::ChatRouterRoute("뜬금없는 발화 3361", ar),
              "llm16: fallback guide is the legacy router guide verbatim");
        Check(guide.find("인식하지 못했습니다") != std::string::npos,
              "llm16: fallback is the stub InfoGuide");
    }

    // 본사 파일 보존 — llm15와 같은 마무리(없던 기기는 소각).
    if (hadCfg) {
        Check(WriteCfg(cfgBackup), "llm16: chat.json restored verbatim");
    } else {
        ::unlink(cfgPath.c_str());
        Check(::access(cfgPath.c_str(), F_OK) != 0,
              "llm16: seeded chat.json removed at case end (was absent)");
    }
}


// Case 17 (T1 — 스펙 2026-10-08-dirty-present 설계 결정 4): 프레임 더티
// 계산기의 순수 로직 단정. jkdesktop RunAppSelfTest의 1p 계열이 캐논 담당
// (양축)이고 이 케이스는 어댑터 축의 분신 — 렌더러 생성 0(SDL_Rect 타입뿐)으로
// 표면→화면 매핑(스케일·반올림·클램프)·합집합 병합·역치(40% full 전환)·빈
// TakeDirty·AddLayerMove 이전∪새·ForceFull을 g++ 링크에서 잠근다. 케이스
// 6종(plan verbatim)은 1p 캐논과 동일한 수형이다.
void TestFrameDirty() {
    using jk::server::FrameDirtyAccumulator;

    // 1p-1) 매핑(+스케일 2.0): 표면 100x50, 스케일 2.0, 레이어 화면 원점
    // (40, 60) — 표면 rect {10,20,20,10} → 화면 {60,100,40,20}.
    {
        FrameDirtyAccumulator acc(800, 600);
        acc.AddSurfaceRect(7, 100, 50, 2.0f, 2.0f, 40, 60,
                           jk::ipc::DirtyRect{10, 20, 20, 10});
        std::vector<SDL_Rect> out;
        Check(acc.TakeDirty(out) && out.size() == 1 && out[0].x == 60 &&
                  out[0].y == 100 && out[0].w == 40 && out[0].h == 20,
              "1p-1 매핑(+스케일 2.0) 표면rect→화면rect");
        // 오버플레이·화면 경계 클램프: 표면을 초과하는 dirty는 레이어 dst까지
        // 절단(레이어 바깥 화면 면적을 부채질하지 않는다).
        FrameDirtyAccumulator over(800, 600);
        over.AddSurfaceRect(7, 100, 50, 2.0f, 2.0f, 40, 60,
                            jk::ipc::DirtyRect{0, 0, 200, 200});
        Check(over.TakeDirty(out) && out.size() == 1 && out[0].x == 40 &&
                  out[0].y == 60 && out[0].w == 200 && out[0].h == 100,
              "1p-1b 오버플레이·화면 경계 클램프(dst 절단)");
        // 반올림 좌표(스펙: 스케일 매핑 = 반올림) — 스케일 1.5, 원점 (0,0):
        // {3,5,4,6} → {lround(4.5)=5, lround(7.5)=8, 6, 9}(half-away 반올림).
        FrameDirtyAccumulator rnd(800, 600);
        rnd.AddSurfaceRect(7, 100, 50, 1.5f, 1.5f, 0, 0,
                           jk::ipc::DirtyRect{3, 5, 4, 6});
        Check(rnd.TakeDirty(out) && out.size() == 1 && out[0].x == 5 &&
                  out[0].y == 8 && out[0].w == 6 && out[0].h == 9,
              "1p-1c 반올림 좌표 매핑(스케일 1.5, lround)");
    }

    // 1p-2) 합집합 병합: 인접+중첩은 하나로 뭉치고, 떨어진 rect는 유지.
    {
        FrameDirtyAccumulator merge(800, 600);
        merge.AddDirtyLayerRect(1, {0, 0, 100, 100});
        merge.AddDirtyLayerRect(2, {100, 0, 100, 100});  // 인접(1px 접촉)
        merge.AddDirtyLayerRect(3, {50, 50, 100, 100});  // 중첩
        merge.AddDirtyLayerRect(4, {500, 500, 10, 10});  // 떨어짐
        std::vector<SDL_Rect> out;
        Check(merge.TakeDirty(out) && out.size() == 2 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 200 && out[0].h == 150 &&
                  out[1].x == 500 && out[1].y == 500 && out[1].w == 10 &&
                  out[1].h == 10,
              "1p-2 합집합 병합(인접·중첩 정리, 이격 유지)");
    }

    // 1p-3) 역치(full 전환): 40% 경계 — 미만은 부분 rect 제시, 도달은 full.
    {
        FrameDirtyAccumulator thr(100, 100);
        thr.AddDirtyLayerRect(1, {0, 0, 39, 99});  // 3861/10000 = 38.6% < 40%
        std::vector<SDL_Rect> out;
        Check(!thr.IsFull(), "1p-3a 역치 미만 = IsFull false");
        Check(thr.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 39 && out[0].h == 99,
              "1p-3b 역치 미만 = 부분 rect 제시(제시 유지)");
        thr.AddDirtyLayerRect(1, {0, 0, 50, 80});  // 4000/10000 = 정확 40%
        Check(thr.IsFull(), "1p-3c 누적 40% 도달 = full 전환(>= 경계)");
        Check(thr.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 100 && out[0].h == 100,
              "1p-3d full 전환 = 화면 전체 rect 단건");
        Check(!thr.TakeDirty(out), "1p-3e TakeDirty 후 재초기화(빈=false)");
    }

    // 1p-4) 빈=TakeDirty false(제시 스킵 원료) — 오염된 out도 비운다.
    {
        FrameDirtyAccumulator none(800, 600);
        std::vector<SDL_Rect> out{SDL_Rect{9, 9, 1, 1}};
        Check(!none.TakeDirty(out) && out.empty(),
              "1p-4 빈 계산기 = TakeDirty false(out 비움)");
        none.ForceFull();
        Check(none.TakeDirty(out) && out.size() == 1 && out[0].w == 800 &&
                  out[0].h == 600,
              "1p-4b 사건 0 + ForceFull = full rect(강제는 사건 무관)");
    }

    // 1p-5) AddLayerMove = 이전∪새(이동 궤적 양쪽 착지).
    {
        FrameDirtyAccumulator mv(800, 600);
        mv.AddLayerMove(5, {0, 0, 10, 10}, {50, 50, 10, 10});
        std::vector<SDL_Rect> out;
        bool sawOld = false, sawNew = false;
        Check(mv.TakeDirty(out) && out.size() == 2,
              "1p-5a AddLayerMove = 이전∪새 두 후보 rect");
        for (const SDL_Rect& r : out) {
            if (r.x == 0 && r.y == 0 && r.w == 10 && r.h == 10) sawOld = true;
            if (r.x == 50 && r.y == 50 && r.w == 10 && r.h == 10) sawNew = true;
        }
        Check(sawOld && sawNew,
              "1p-5b 이전 dst·새 dst 모두 이동 사건에 있음");
        // 인접 dst 무브는 합집합 정리로 한 rect에 뭉친다.
        FrameDirtyAccumulator mv2(800, 600);
        mv2.AddLayerMove(5, {0, 0, 16, 16}, {16, 0, 16, 16});
        Check(mv2.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 32 && out[0].h == 16,
              "1p-5c 인접 dst 무브 = 단건 병합(이전∪새)");
    }

    // 1p-6) ForceFull: 포커스 재정렬/오버레이 훅 — 작은 rect에도 전체로.
    {
        FrameDirtyAccumulator ff(800, 600);
        std::vector<SDL_Rect> out;
        Check(!ff.IsFull(), "1p-6a 초기 IsFull false");
        ff.AddDirtyLayerRect(1, {0, 0, 4, 4});
        ff.ForceFull();
        Check(ff.IsFull(), "1p-6b ForceFull = IsFull true");
        Check(ff.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 800 && out[0].h == 600,
              "1p-6c ForceFull = 화면 전체 rect 단건(사건 상쇄 정리)");
    }
}

// case 18 (T2): 커밋 rect 수집 배선의 계산기 단면 + T1 리뷰 F1 승계 케이스.
// SDL 렌더러를 요구하지 않는 형태(brief Step 1 원문) — 배서 계산기(T1)에
// CommitSurface → 보류 큐 → Composite 소비의 AddSurfaceRect 매핑을 재용해
// 배선 모양의 산치를 단정한다. SDL 의존 본체(SW 부분 업로드)는 기계 실측.
void TestFrameDirtyWiring() {
    using jk::server::FrameDirtyAccumulator;  // case 17과 같은 사용 규약
    // 1p-7) 커밋 배선 수집(T2 — 스펙 결정 2 "와이어 원용"의 계산기 수형):
    // CommitSurface 핸들러가 폐기하던 DirtyRect[]를 보류 큐로 전달하고
    // Composite 소비 시점에 AddSurfaceRect로 매핑하는 배선의 계산기 단면
    // SDL 렌더러 없이 단정한다(레이어 표면→화면 매핑 = T1 계산기 재용 —
    // SDL 렌더러 의존 본체는 수기 실측, brief Step 1 원문).
    {
        // 한 레이어의 커밋 1건 — 서로 겹치는 3 rect가 매핑+합집합 정리로
        // 소송 병합 목록이 된다(제시 rect 개수의 원료 = TakeDirty out.size()).
        // 레이어: 표면 120x80, 스케일 1.0, 화면 원점 (10, 20).
        FrameDirtyAccumulator wired(800, 600);
        const jk::ipc::DirtyRect commitRects[3] = {
            {0, 0, 10, 10}, {5, 5, 10, 10}, {30, 40, 20, 15}};
        for (const jk::ipc::DirtyRect& dr : commitRects) {
            wired.AddSurfaceRect(11, 120, 80, 1.0f, 1.0f, 10, 20, dr);
        }
        std::vector<SDL_Rect> out;
        Check(wired.TakeDirty(out) && out.size() == 2 &&
                  out[0].x == 10 && out[0].y == 20 && out[0].w == 15 &&
                  out[0].h == 15 &&
                  out[1].x == 40 && out[1].y == 60 && out[1].w == 20 &&
                  out[1].h == 15,
              "1p-7 커밋 DirtyRect[] 수집→매핑→병합 목록(rect 개수 = out.size())");
        // 배선 불변의 반쪽: 커밋 rect가 없는 dirty 레이어는 dst 전체 봉합
        // (AddDirtyLayerRect = 이미 화면 좌표를 받는 계약 — dst 산식 원용).
        FrameDirtyAccumulator fallback(800, 600);
        fallback.AddDirtyLayerRect(11, {10, 20, 120, 80});
        Check(fallback.TakeDirty(out) && out.size() == 1 &&
                  out[0].x == 10 && out[0].y == 20 && out[0].w == 120 &&
                  out[0].h == 80,
              "1p-7b 커밋 rect 없는 dirty = dst 전체 봉합(배선 불변 반쪽)");
        // 복수 레이어 커밋이 같은 프레임에 섞여도(스케일·원점이 레이어별) 각자
        // 자기 매핑을 찍는다 — 큐 누적 모양(커밋 사이드별 스케일 2.0 대비).
        FrameDirtyAccumulator two(800, 600);
        two.AddSurfaceRect(11, 120, 80, 1.0f, 1.0f, 10, 20,
                           jk::ipc::DirtyRect{0, 0, 10, 10});
        two.AddSurfaceRect(12, 120, 80, 2.0f, 2.0f, 100, 100,
                           jk::ipc::DirtyRect{0, 0, 10, 10});
        Check(two.TakeDirty(out) && out.size() == 2,
              "1p-7c 레이어 2종의 커밋 rect가 같은 프레임에 누적(레이어별 매핑)");
    }

    // 1p-8) T1 리뷰 F1 승계: 역치 면적은 병합 목록 총합 — 중첩 중복 가산 아니다
    // (컨트롤러 룰링 "역치 면적 = 무중첩 병합 총합"). 원장 산치(fix r1 NT2-2
    // 표기 정정 — 케이스 로직 무변경): 화면 100x100·가산 5000(50x50 x2) ≥
    // 4000 = full이어야 답하나 **병합 목록 총합 3600**(rect 2건이 병합되어
    // 목록에 오르는 것은 merged-bbox {0,0,60,60} = 3600 / **정확 합집합
    // 3400보다 bbox 과대 — full 조기 보수 방향**) < 4000 = IsFull false.
    // 정확 합집합 3400 = 5000 - 중첩 1600 — 중첩 40x40이 되려면 오프셋
    // (10,10)의 {10,10,50,50}이다(원장 표기 {30,30,50,50}은 중첩 400·합집합
    // 4600이어서 산치와 어긋남 — T2 리포트 concern 원장 정정).
    {
        FrameDirtyAccumulator f1(100, 100);
        f1.AddDirtyLayerRect(1, {0, 0, 50, 50});
        f1.AddDirtyLayerRect(2, {10, 10, 50, 50});
        Check(!f1.IsFull(),
              "1p-8a 가산 5000이어도 병합 목록 총합 3600 = IsFull false(F1 승계)");
        std::vector<SDL_Rect> out;
        Check(f1.TakeDirty(out) && out.size() == 1 && out[0].x == 0 &&
                  out[0].y == 0 && out[0].w == 60 && out[0].h == 60,
              "1p-8b 병합 총합 <역치 = 부분 제시(bbox 병합 목록 유지)");
        // 가교: 중첩 없는 2 rect — 합집합 = 가산 ≥ 역치면 full 전환(산법
        // 경계 — 가약이 아니라 진짜 합집합이 넘을 때만).
        FrameDirtyAccumulator f2(100, 100);
        f2.AddDirtyLayerRect(1, {0, 0, 50, 50});
        f2.AddDirtyLayerRect(2, {0, 50, 50, 50});  // 인접·합집합 5000
        Check(f2.IsFull(), "1p-8c 병합 총합 5000(=합집합) = full 전환(중첩 없음)");
    }
}

// Case 19 (T2 — 스펙 2026-10-09-phone-text-scale): 텍스트 배율 결선 산치.
// jkdesktop RunAppSelfTest의 2t 계열이 캐논 담당(양축)이고 이 케이스는 어댑터
// 축의 분신 — STL/SDL 렌더러 없이 순수 부품만: ComputeCellMetrics 산술,
// 미설정 소자 기본 배율 플랫폼 상수(posix 축 기대 1.5 — Win 쌍둥이는 1.0),
// 비트맵 폴백 확대 매핑 헬퍼(s=1.0 항등·1.5·2.0 샘플).
// GetCellMetrics 단정 금지(환경 의존 — exe-dir settings.json 직독 진실원).
void TestTextScaleFix() {
    // 2t-a) ComputeCellMetrics(1.5f) 산술 — posix 소자 기본 배율의 셀 격자
    // (hanW=2×engW 불변식 — docs/65 O4).
    const jk::text::CellMetrics c15 = jk::text::ComputeCellMetrics(1.5f);
    Check(c15.engW == 12 && c15.hanW == 24 && c15.cellH == 24,
          "2t-a ComputeCellMetrics(1.5) == {12,24,24} (hanW=2×engW)");

    // 2t-b/2t-c) 미설정 기본 배율 플랫폼 단정 — 컴파일타임 상수 분기.
    Check(jk::text::DefaultFontScale() == 1.5f,
          "2t-b posix 미설정 기본 배율 = 1.5 (스펙 사용자 확정)");
    const jk::text::CellMetrics cd =
        jk::text::ComputeCellMetrics(jk::text::DefaultFontScale());
    Check(cd.engW == 12 && cd.hanW == 24 && cd.cellH == 24,
          "2t-c posix 미설정 셀 = {12,24,24} (글리프도 함께 1.5)");

    // 2t-d) 확대 매핑 항등 — 소스 스팬 == 목표 스팬 = 모든 샘플 자신
    // (Windows 폴백 픽셀동일 산치 — 쌍둥이와 동일 수형).
    Check(jk::text::StretchNearestIndex(0, 8, 8) == 0 &&
              jk::text::StretchNearestIndex(7, 8, 8) == 7 &&
              jk::text::StretchNearestIndex(15, 16, 16) == 15,
          "2t-d 확대 매핑 항등(스팬 동일 = 샘플 그대로, s=1.0 픽셀동일)");

    // 2t-e) 1.5 확대 샘플: src 8 → dst 12 = idx*8/12.
    Check(jk::text::StretchNearestIndex(0, 8, 12) == 0 &&
              jk::text::StretchNearestIndex(3, 8, 12) == 2 &&
              jk::text::StretchNearestIndex(5, 8, 12) == 3 &&
              jk::text::StretchNearestIndex(11, 8, 12) == 7 &&
              jk::text::StretchNearestIndex(23, 8, 24) == 7,
          "2t-e 1.5 확대 샘플 = idx*src/dst (nearest 격자)");

    // 2t-f) 2.0 확대 샘플 + 병적 입력 방어선.
    Check(jk::text::StretchNearestIndex(0, 8, 16) == 0 &&
              jk::text::StretchNearestIndex(15, 8, 16) == 7 &&
              jk::text::StretchNearestIndex(1, 8, 1) == 7 &&
              jk::text::StretchNearestIndex(-1, 8, 8) == 0,
          "2t-f 2.0 확대 샘플 + 병적 입력 첫 샘플 수렴(방어선)");

    // KSSM 쌍 폴백은 반올림 좌표에서 4/9px 오차(8→15px 등 홀수 폭)를 허용한다
    // — 비트맵 폴백 한계(스펙 fail-safe 명시; 벡터 아틀라스가 정상 경로).
    // 단정치 않고 수용 계약만 여기에 기록한다.

    // 3b) 크롬 타이틀 밴드 높이 산식 (T3 — 스펙 2026-10-09-phone-text-scale
    // 결정 1): 현행 상수 24 = 비트맵 셀 16 + 여백 8의 합이라는 원문 실측의
    // 산치. 소비처 양축 — JKWindow.cpp kTitle(클라 표면 안의 밴드)·
    // JKWindowServer.cpp 히트테스트 존+승인 배너 밴드 두께(서버 크롬 —
    // "MUST stay in sync"). 쌍둥이(engine/src/main.cpp 3b)와 동일 수형.
    Check(jk::text::ComputeChromeTitleBarHeight(16) == 24,
          "3b-a s=1.0 셀 16 → 밴드 24 (현행 상수와 정확 등호 — "
          "Windows 창 타이틀 픽셀동일 산치)");
    Check(jk::text::ComputeChromeTitleBarHeight(24) == 32,
          "3b-b posix 기본 1.5 셀 24 → 밴드 32 (1.5 타이틀 글리프 클립 방지)");
    Check(jk::text::ComputeChromeTitleBarHeight(8) == 24 &&
              jk::text::ComputeChromeTitleBarHeight(48) == 56,
          "3b-c 최소 현행값 24 보장(하단 방어선) + 상단 s=3.0 셀 48 → 56");
    // (T3 fix r1) 앱 본문 상단 오프셋 = 밴드 산식 + 본문 여백 6 — 고정
    // 리터럴(topY 30)의 분해가 소비 소스로 모였다. 쌍둥이(engine/src/main.cpp
    // 3b-d/3b-e)와 동일 수형.
    Check(jk::text::ComputeAppContentTopOffset(16) == 30,
          "3b-d s=1.0 앱 상단 오프셋 = 밴드 24 + 여백 6 = 30 "
          "(기존 ImGui topY 리터럴과 정확 등호 — Windows 무변 산치)");
    Check(jk::text::ComputeAppContentTopOffset(24) == 38,
          "3b-e posix 기본 1.5 앱 상단 오프셋 38 — 본문이 밴드 32와 "
          "겹치지 않는다(I-2 상단 사각지대 해소 산치)");
}

// Gallery 모델 순수 부품 (T1 — 스펙 2026-10-09-gallery-design). jkdesktop
// RunAppSelfTest의 2g 계열이 캐논 담당(양축)이고 이 케이스는 어댑터 축의
// 분신 — 헤더 inline(apps/GalleryModel.h, jk::gallery)을 g++ 직링크로
// 단정한다(2t 선례). backslash 정규화 수형은 Win 원문 계약이라 posix 쌍은
// '/'-만 수형을 단정한다(exeDir 빈값 승계 수형 포함).
void TestGalleryModel() {
    using jk::gallery::GalleryDirList;
    // 2g-a) 순수 리졸버 — settings 없음 = 기본 1건(fail-safe), dirs 2건 =
    // 기본이 앞+유저 순서 보존, 중복 경로 제거, 파손/비배열 = 기본 1건.
    const std::vector<std::string> noSettings = GalleryDirList("X:/exe", "");
    Check(noSettings.size() == 1 &&
              noSettings[0] == "X:/exe/state/screenshots",
          "2g-a settings 없음 = 기본 1건(fail-safe)");
    const std::vector<std::string> withDirs = GalleryDirList(
        "X:/exe", R"({"gallery":{"dirs":["P:/pics","Q:/cam"]}})");
    Check(withDirs.size() == 3 &&
              withDirs[0] == "X:/exe/state/screenshots" &&
              withDirs[1] == "P:/pics" && withDirs[2] == "Q:/cam",
          "2g-a dirs 2건 = 기본이 앞+유저 순서 보존");
    const std::vector<std::string> dup = GalleryDirList(
        "X:/exe",
        R"({"gallery":{"dirs":["X:/exe/state/screenshots","P:/pics"]}})");
    Check(dup.size() == 2 && dup[0] == "X:/exe/state/screenshots" &&
              dup[1] == "P:/pics",
          "2g-a 기본 중복 유저 경로 제거(첫 등장 유지)");
    const std::vector<std::string> broken =
        GalleryDirList("X:/exe", R"json({"gallery":{"dirs":[)json");
    Check(broken.size() == 1 && broken[0] == "X:/exe/state/screenshots",
          "2g-a 파손 settings = 기본 1건(fail-safe)");
    const std::vector<std::string> notArray =
        GalleryDirList("X:/exe", R"({"gallery":{"dirs":"P:/pics"}})");
    Check(notArray.size() == 1 && notArray[0] == "X:/exe/state/screenshots",
          "2g-a dirs 비배열 = 무시, 기본 1건(원문 보존 소비)");
    const std::vector<std::string> emptyExe = GalleryDirList("", "");
    Check(emptyExe.size() == 1 && emptyExe[0] == "state/screenshots",
          "2g-a exeDir 빈값 = 상대 기본 1건(jk::fs 빈값 계약 승계)");

    // 2g-b) 경로 정규화 — 뒤 구분자 중복·빈 성분 제거·첫 등장 유지(posix 수형;
    // backslash 수형은 Win 쌍둥이 소유 — '\'는 posix에서 성분 문자).
    const std::vector<std::string> norm = jk::gallery::NormalizeDirs(
        {"srv/share//", "", "/", "srv/share"});
    Check(norm.size() == 1 && norm[0] == "srv/share",
          "2g-b 뒤 구분자 중복+빈 성분 제거+첫 등장 유지(posix)");

    // 2g-c) 썸네일 박스 산치 — FitThumb = min 축 지배 + s=1.0 상한(확대 금지).
    const jk::gallery::FitSize same =
        jk::gallery::FitThumb(160, 120, 160, 120);
    Check(same.w == 160.f && same.h == 120.f, "2g-c 정합 입력 = s 1.0(원본)");
    const jk::gallery::FitSize small =
        jk::gallery::FitThumb(80, 60, 160, 120);
    Check(small.w == 80.f && small.h == 60.f,
          "2g-c 작은 원본 = 확대 금지(s 1.0 상한)");
    const jk::gallery::FitSize big =
        jk::gallery::FitThumb(3200, 2400, 160, 120);
    Check(big.w == 160.f && big.h == 120.f, "2g-c 큰 원본 = 박스 정합 축소");
    const jk::gallery::FitSize wide =
        jk::gallery::FitThumb(10000, 10, 160, 120);
    Check(wide.w == 160.f && std::abs(wide.h - 0.16f) < 1e-3f,
          "2g-c 극단 가로 종횡비 = min 축 지배(비율 유지)");
    const jk::gallery::FitSize tall =
        jk::gallery::FitThumb(10, 10000, 160, 120);
    Check(std::abs(tall.w - 0.12f) < 1e-3f && tall.h == 120.f,
          "2g-c 극단 세로 종횡비 = min 축 지배(비율 유지)");
    const jk::gallery::FitSize deg = jk::gallery::FitThumb(0, 100, 160, 120);
    Check(deg.w == 0.f && deg.h == 0.f, "2g-c 퇴화 입력 = {0,0}");

    // 2g-d) FNV-1a 캐시 키 — 결정론성+충돌 부재 3쌍+basis(구조 상수).
    Check(jk::gallery::Fnv1a("a.png") == jk::gallery::Fnv1a("a.png"),
          "2g-d 결정론성(같은 입력 = 같은 해시)");
    Check(jk::gallery::Fnv1a("shot_1.png") != jk::gallery::Fnv1a("shot_2.png") &&
              jk::gallery::Fnv1a("p1.jpg") != jk::gallery::Fnv1a("p1.jpeg") &&
              jk::gallery::Fnv1a("a/b.png") != jk::gallery::Fnv1a("a/b.png "),
          "2g-d 충돌 부재 3쌍(대쉬 1문자·형제 확장자·꼬리 공백)");
    Check(jk::gallery::Fnv1a("") == 2166136261u,
          "2g-d 빈 문자열 = offset basis(구조 상수)");

    // 2g-e) 폴더 열거 — 없는 폴더/빈 폴더=목록 비움(ec 중립형), 실열거 =
    // 최신순(mtime desc, 명시 세트 — 시계 분해능 무관)+비이미지/서브디렉터리
    // 성분 스킵. 임시 폴더 실측(사후 소각 — 엔진 selftest 계약).
    const std::filesystem::path gdir =
        std::filesystem::temp_directory_path() / "jk_gallery_selftest";
    std::filesystem::remove_all(gdir);
    std::vector<std::string> out;
    bool ok = true;
    out = jk::gallery::ListImageFiles((gdir / "missing").string(), &ok);
    Check(out.empty() && !ok, "2g-e 없는 폴더 = 목록 비움+ok false");
    std::filesystem::create_directories(gdir);
    out = jk::gallery::ListImageFiles(gdir.string(), &ok);
    Check(out.empty() && ok, "2g-e 빈 폴더 = 목록 비움+ok true");

    { std::ofstream f(gdir / "b.png"); f.put('x'); }
    { std::ofstream f(gdir / "a.png"); f.put('x'); }
    { std::ofstream f(gdir / "c.txt"); f.put('x'); }
    std::filesystem::create_directories(gdir / "sub");
    { std::ofstream f(gdir / "sub" / "in.png"); f.put('x'); }
    const auto later =
        std::filesystem::file_time_type::clock::now() + std::chrono::hours(1);
    std::filesystem::last_write_time(gdir / "b.png", later);
    out = jk::gallery::ListImageFiles(gdir.string(), &ok);
    Check(ok && out.size() == 2 && out[0] == "b.png" && out[1] == "a.png",
          "2g-e 열거 = 최신순(mtime desc)+비이미지/서브디렉터리 스킵");
    std::filesystem::remove_all(gdir);

    // 2g-f) 전체 보기 핏 산치 (T2 — 스펙 결정 3 "핏 표시"): FitFull(w,h,
    // vpW,vpH) = min 축 지배 순수 비율 — 썸네일 산치(2g-c)와 달리 s=1.0 상한
    // 부재(작은 원본 확대 허용 — shot 표시 수형 동형), 화면 배율 상태와 무관
    // (fit-scale 함정 원장 존중). 쌍둥이(engine/src/main.cpp)와 동일 수형.
    const jk::gallery::FitSize half = jk::gallery::FitFull(1920, 1080, 960, 540);
    Check(half.w == 960.f && half.h == 540.f,
          "2g-f 절반 축소 = 뷰포트 정합(s=min 축 지배)");
    const jk::gallery::FitSize up = jk::gallery::FitFull(80, 60, 160, 120);
    Check(up.w == 160.f && up.h == 120.f,
          "2g-f 작은 원본 = 확대 허용(s 상한 부재 — 2g-c와 반대 수형)");
    const jk::gallery::FitSize fwide = jk::gallery::FitFull(10000, 10, 160, 120);
    Check(fwide.w == 160.f && std::abs(fwide.h - 0.16f) < 1e-3f,
          "2g-f 극단 가로 종횡비 = min 축 지배(비율 유지)");
    const jk::gallery::FitSize ftall = jk::gallery::FitFull(10, 10000, 160, 120);
    Check(std::abs(ftall.w - 0.12f) < 1e-3f && ftall.h == 120.f,
          "2g-f 극단 세로 종횡비 = min 축 지배(비율 유지)");
    const jk::gallery::FitSize fdeg = jk::gallery::FitFull(0, 100, 160, 120);
    const jk::gallery::FitSize fdegVp = jk::gallery::FitFull(100, 100, 0.f, 120.f);
    Check(fdeg.w == 0.f && fdeg.h == 0.f && fdegVp.w == 0.f && fdegVp.h == 0.f,
          "2g-f 퇴화 입력(이미지·뷰포트) = {0,0}");
    // 2g-f 이전/다음 wrap-around — 끝 지점 순환(전체 보기 이동 계약).
    Check(jk::gallery::WrapStep(0, -1, 3) == 2 &&
              jk::gallery::WrapStep(2, 1, 3) == 0,
          "2g-f wrap = 끝 지점 순환(이전·다음)");
    Check(jk::gallery::WrapStep(1, 1, 3) == 2 &&
              jk::gallery::WrapStep(1, -1, 3) == 0,
          "2g-f wrap = 범위 내 이동");
    Check(jk::gallery::WrapStep(5, 0, 3) == 2 &&
              jk::gallery::WrapStep(7, 1, 3) == 2,
          "2g-f wrap = 범위 밖 인덱스 수렴(모듈로 정규화)");
    Check(jk::gallery::WrapStep(0, 1, 0) == 0 &&
              jk::gallery::WrapStep(0, -1, 0) == 0,
          "2g-f wrap = 빈 목록 무접촉(무변)");

    // 2g-g) nearest 박스 축소 (T3 — T2 폴백 확대 동형 기법 역방향): dst
    // (x,y) = src(x*srcW/dstW, y*srcH/dstH) 소스 샘플 그대로. s=1.0 항등
    // 등호·0.5 축소 격자·1.5 요청(상한 눌림 = 항등)·퇴화 입력 방어선.
    {
        // 4x4 소스 — 픽셀 R채널 = 샘플 인덱스(x + y*4)로 식별 가능하게.
        jk::LoadedImage src;
        src.w = 4;
        src.h = 4;
        src.rgba.resize(16 * 4);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                uint8_t* px = src.rgba.data() + (y * 4 + x) * 4;
                px[0] = static_cast<uint8_t>(x + y * 4);
                px[1] = 0;
                px[2] = 0;
                px[3] = 255;
            }
        const jk::LoadedImage id = jk::gallery::MakeThumb(src, 4, 4);
        Check(id.w == 4 && id.h == 4 &&
                  std::memcmp(id.rgba.data(), src.rgba.data(), 16 * 4) == 0,
              "2g-g s=1.0 = 항등(치수+픽셀 정확 등호)");
        const jk::LoadedImage half = jk::gallery::MakeThumb(src, 2, 2);
        const uint8_t* hp = half.rgba.data();
        Check(half.w == 2 && half.h == 2 && hp[0] == 0 && hp[4] == 2 &&
                  hp[8] == 8 && hp[12] == 10,
              "2g-g 0.5 축소 샘플 = x*src/dst 격자(사분면 0·2·8·10)");
        const jk::LoadedImage up = jk::gallery::MakeThumb(src, 6, 6);
        Check(up.w == 4 && up.h == 4 &&
                  std::memcmp(up.rgba.data(), src.rgba.data(), 16 * 4) == 0,
              "2g-g 1.5 요청 = s 1.0 상한 눌림 항등(확대 금지 — 2g-c 계약)");
        const jk::LoadedImage degBox = jk::gallery::MakeThumb(src, 0, 4);
        const jk::LoadedImage degEmpty =
            jk::gallery::MakeThumb(jk::LoadedImage(), 4, 4);
        Check(degBox.rgba.empty() && degBox.w == 0 && degBox.h == 0 &&
                  degEmpty.rgba.empty(),
              "2g-g 퇴화 입력(박스 0·빈 픽셀) = 빈 LoadedImage(placeholder 유지)");
    }

    // 2g-h) 캐시 키 (T3 — 2g-d Fnv1a 소비): 경로+size+mtime 조합. 같은
    // 이름·내용 변화(크기 또는 mtime) = 키 변화(스메리 캐시 방지), 스탬프
    // 실패(부재) = 빈 키(열외), 캐시 파일 경로 합성 계약.
    const std::string kBase = jk::gallery::ThumbKey("P:/pics/a.png", 100, 5);
    Check(!kBase.empty() && kBase.size() == 8 &&
              kBase == jk::gallery::ThumbKey("P:/pics/a.png", 100, 5),
          "2g-h 결정론성+8자리 hex 형식(빈 키 부재)");
    Check(jk::gallery::ThumbKey("P:/pics/a.png", 101, 5) != kBase &&
              jk::gallery::ThumbKey("P:/pics/a.png", 100, 6) != kBase,
          "2g-h size 변화·mtime 변화 = 키 변화(내용 변화 반영)");
    Check(jk::gallery::ThumbKey("P:/pics/b.png", 100, 5) != kBase,
          "2g-h 경로 변화 = 키 변화(이름·내용 같아도)");
    Check(jk::gallery::GalleryThumbPath("X:/exe", "0a1b2c3d") ==
                      "X:/exe/state/gallery/thumbs/0a1b2c3d.png" &&
              jk::gallery::GalleryThumbPath("X:/exe", "").empty(),
          "2g-h 캐시 경로 합성 = thumbs/<key>.png (빈 키 = 빈 경로 방어선)");
    {
        // 실측 한쌍 — 생존 파일은 키가 성립하고 2회차 조명도 같은 키(재부팅
        // 재조명 방지의 가교), 부재 파일은 ec 중립형 열외(빈 키).
        const std::filesystem::path tdir =
            std::filesystem::temp_directory_path() / "jk_gallery_key";
        std::filesystem::remove_all(tdir);
        std::filesystem::create_directories(tdir);
        {
            std::ofstream out(tdir / "a.png");
            for (int i = 0; i < 10; ++i) out.put('a');
        }
        const std::string live =
            jk::gallery::ThumbKeyFor((tdir / "a.png").string());
        const std::string missed =
            jk::gallery::ThumbKeyFor((tdir / "no.png").string());
        Check(missed.empty() && !live.empty() &&
                  live == jk::gallery::ThumbKeyFor((tdir / "a.png").string()),
              "2g-h 실측 = 생존 파일 키 성립+재조명 동일, 부재 = 빈 키(열외)");
        std::filesystem::remove_all(tdir);
    }

    // 2g-i) LRU 퇴출 산치 (T3 fix r1 — 동일 프레임 기록 텍스처 파괴 방지):
    // 이번 프레임(useFrame == curFrame) 접촉 슬롯은 후보에서 **전부** 제외,
    // 후보 중 최소 세대, 동세대 동률 = 앞 인덱스, 후보 0 = -1(placeholder
    // 유지). 예전 가드(`tick == cur` 1건)의 C1 결함 수형을 직단정한다.
    Check(jk::gallery::PickLruVictim({9, 9, 9}, 9) == -1 &&
              jk::gallery::PickLruVictim({9, 3, 9, 7}, 9) == 1 &&
              jk::gallery::PickLruVictim({9, 7, 7, 9}, 9) == 1 &&
              jk::gallery::PickLruVictim({}, 9) == -1,
          "2g-i 퇴출 산치 = 동일 프레임 세대 전부 제외+최소 세대(동률 앞 인덱스)+후보 0 = -1");

    // 2g-j) 컬 산치 (T3 fix r2 — C2 영구 기아 봉합): 셀 박스 수직 스팬이
    // 클립 스팬과 **교차**할 때만 풀에 요청한다. 포함·위 절반·아래 절반
    // 교차 = 가시(요청), 전체 위/아래 = 비가시(요청 없음 — 0 높이 교차는
    // 픽셀이 없으므로 경계 접촉도 비가시). 이상 경계(가시 셀 > 96 = 풀
    // 상한)는 동세대 접촉만 남아 후보 0 = -1 → placeholder 유지 계약이라
    // 2g-i의 후보 0 수형과 연결(파괴 없음, 기아는 컬이 이미 봉합 — 가시>
    // 96 극단만 placeholder).
    Check(jk::gallery::ThumbRowVisible(10.f, 130.f, 0.f, 200.f) &&
              jk::gallery::ThumbRowVisible(-50.f, 100.f, 0.f, 200.f) &&
              jk::gallery::ThumbRowVisible(100.f, 300.f, 0.f, 200.f) &&
              !jk::gallery::ThumbRowVisible(200.f, 300.f, 0.f, 200.f) &&
              !jk::gallery::ThumbRowVisible(-10.f, 0.f, 0.f, 200.f),
          "2g-j 컬 산치 = 클립 교차 셀만 요청(경계 접촉 = 비가시 — 0 높이 교차 금지)");
    Check(jk::gallery::PickLruVictim({9, 9, 9, 9}, 9) == -1,
          "2g-j 이상 경계(가시 셀 > 96 = 풀 상한) = 후보 0 → placeholder 유지(파괴 없음)");
}

// Case 2i (T1 — 스펙 2026-10-09-client-idle): 클라 활동 게이트 순수 부품
// (client/JKActivityGate.h). Run() 루프의 wantRender 판정이 렌더러·서버·SDL
// 접촉 없이 단정된다. 근거 = .superpowers/sdd/2026-10-09-clt-spin/
// spike-report.md §1a — Run()(JKClientApplication.cpp)이 타이머 채널 소비
// 수>0을 활동으로 계수해 docs/78 게이트를 매 16ms 틱마다 무력화, 폰 갤러리
// 클라 무변화 19fps 풀코어 100% 실측. 계약: **타이머 틱 = 배송일 뿐 활동이
// 아니다** — 입력·에이전트·툴콜·테마만 활동 계수, 폴백 1s(장면 더티 게이트)
// 안전망 유지. 캐논 계보(기존 Win 569/WSL 546/posix 277 — 2g 계열 다음 신설
// 2i, 캐논 축 = engine/src/main.cpp RunAppSelfTest 2i).
void TestActivityGate() {
    // 2i-a) 타이머 틱 단독 = 렌더 유발 안 함(T1 핵심 수형)과 폴백/부팅/더티
    // 구조 원문. 더티 조회 프레디케이트는 폴백 비도달(또는 게이트 선행 참)
    // 동안 열람 0회(HasDirtyWindows 매 이터레이션 열람 방지 — Run() 원문).
    const auto neverDirty = [] { return false; };
    Check(!jk::client::GateWantRender(
              /*timerDelivered=*/true, /*inputDrained=*/false,
              /*agentEvent=*/false, /*toolCall=*/false,
              /*themeChanged=*/false, /*frameDirty=*/false,
              /*renderedOnce=*/true, /*fallback=*/false, neverDirty),
          "2i-a 타이머 틱 단독 = 렌더 유발 안 함(활동 게이트 원문 수형)");

    int probeCount = 0;
    {
        const auto probeAbsent = [&probeCount] {
            ++probeCount;
            return false;
        };
        const bool decided = jk::client::GateWantRender(
            true, false, false, false, false, false, true, /*fallback=*/true,
            probeAbsent);
        Check(!decided && probeCount == 1,
              "2i-a 폴백 도달+더티 부재 = 스킵(더티 조회 1회 원문)");
    }
    {
        probeCount = 0;
        const auto probePresent = [&probeCount] {
            ++probeCount;
            return true;
        };
        const bool decided = jk::client::GateWantRender(
            true, false, false, false, false, false, true, /*fallback=*/true,
            probePresent);
        Check(decided && probeCount == 1,
              "2i-a 폴백 도달+장면 더티 = 렌더(1s 폴백 안전망 유지)");
    }
    {
        const bool firstFrame = jk::client::GateWantRender(
            false, false, false, false, false, /*frameDirty=*/false,
            /*renderedOnce=*/false, /*fallback=*/false, neverDirty);
        Check(firstFrame, "2i-a 부팅 첫 프레임 = 이벤트 없이 즉시 렌더");
        const bool dirtyFrame = jk::client::GateWantRender(
            false, false, false, false, false, /*frameDirty=*/true,
            /*renderedOnce=*/true, /*fallback=*/false, neverDirty);
        Check(dirtyFrame, "2i-a frameDirty = 게이트 첫 항 원문 유지");
    }

    // 2i-b) 입력/에이전트/툴콜/테마 = 활동 유지 회귀(T1이 활동 계수를 죽이지
    // 않음 — 각 채널 단독으로도 렌더).
    const auto channel = [](bool i, bool a, bool t, bool th) {
        return jk::client::GateWantRender(
            /*timerDelivered=*/false, i, a, t, th, /*frameDirty=*/false,
            /*renderedOnce=*/true, /*fallback=*/false, [] { return false; });
    };
    Check(channel(/*input=*/true, false, false, false),
          "2i-b 입력 이벤트 = 활동(렌더 유지)");
    Check(channel(false, /*agent=*/true, false, false),
          "2i-b 에이전트 이벤트 = 활동(렌더 유지)");
    Check(channel(false, false, /*toolCall=*/true, false),
          "2i-b 에이전트 툴콜 = 활동(렌더 유지)");
    Check(channel(false, false, false, /*theme=*/true),
          "2i-b 테마 변경 = 활동(렌더 유지)");
    Check(jk::client::GateWantRender(
              /*timerDelivered=*/true, /*inputDrained=*/true,
              /*agentEvent=*/false, /*toolCall=*/false,
              /*themeChanged=*/false, /*frameDirty=*/false,
              /*renderedOnce=*/true, /*fallback=*/false, neverDirty),
          "2i-b 타이머 배송+입력 공존 = 활동(타이머 불참여가 활동을 누르지 않음)");
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
    TestChatPromoteWiring();
    TestLocaltimeS();
    TestJobMultiMemberAndStdinParity();
    TestNetRecvAllChunksTimeoutAcceptBound();
    TestFrameDirty();
    TestActivityGate();
    TestFrameDirtyWiring();
    TestTextScaleFix();
    TestGalleryModel();
    std::printf("PosixSelfTest: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
