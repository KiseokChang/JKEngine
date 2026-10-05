#include <terminal/JKConPtyBridge.h>

#ifndef _WIN32

// jk::JKConPtyBridge — real posix impl (docs/68 stage-2 plan D, task 4).
// R-D2 mapping: Start(commandLine) executes `/bin/sh -c <commandLine>` on a
// glibc openpty() master; the child does setsid() (+TIOCSCTTY, so the pty is
// its controlling terminal) + dup2(0/1/2) + execl("/bin/sh","sh","-c",...),
// which makes pid == pgid for the later tree kill. ReaderThread reads the
// master fd and mirrors the win32 ReadFile loop's verdict: any terminal read
// outcome (0 EOF on BSD-ish ptys, EIO on Linux once the last slave fd is
// closed, EBADF after teardown) becomes the ShellExited signal; only EINTR is
// retried. Byte streams pass through raw — no ConPTY-style re-rendering, so
// DrainOutput is a plain buffer dump under the same 1 MiB cap.
//
// Stop follows the docs/22 §8.2 close order as carried by the win32 body:
// pseudoconsole close → bounded shell wait → terminate → cleanup, mapped to
// posix by the controller ruling: master close → bounded waitpid(WNOHANG)
// polling (~2s) → SIGKILL pgid → final reap/cleanup. The consumer-observable
// order is that same one — the pty is closed before the bounded shell wait.
//
// VERIFIED DESIGN POINT (this file's one big divergence from the win32 shape):
// on Linux, closing the pty master does NOT wake a thread blocked in read()
// on it — measured probe: close(master) returned in 0 ms but the blocked
// reader stayed parked and its read() only returned -EIO after the child's
// 30s sleep expired. The win32 premise "ClosePseudoConsole unblocks the
// reader" therefore has no direct posix analogue (POSIX leaves close() vs a
// concurrent blocked read() undefined, and Linux does not interrupt it). The
// wakeup mechanism here is instead: (a) the reader polls the master with a
// 100 ms timeout re-checking started_ on every iteration, and (b) Stop sets
// started_ false FIRST, so the reader exits within one poll period — the
// flag ALONE bounds the join, close-before-join buys nothing (close wakes no
// Linux sleeper). Stop therefore JOINs the reader FIRST and closes the master
// only afterwards: with the reader thread gone before close(), the
// close-vs-blocked-read fd-reuse window is eliminated entirely (no reader can
// hold a stale fd number), not merely narrowed by a re-check. The bounded
// waitpid window still gives the shell its graceful-death chance: closing the
// last master fd hangs the pty up (SIGHUP to the foreground group), which
// typically kills the shell before any SIGKILL is needed.

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <new>
#include <string>

#include <fcntl.h>
#include <poll.h>
#include <pty.h>       // openpty (glibc; -lutil in the WSL build script)
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>      // nanosleep (Stop's bounded waitpid polling)
#include <unistd.h>

namespace jk {

namespace {

// The win32 TU stores HANDLEs/HPCON in the header's opaque void* fields. Posix
// has no separate pipe ends (the master fd is bidirectional) and no thread
// handle to keep, so: hpcon_ carries the master fd, and proc_ points at a
// heap ChildState — needed because waitpid(WNOHANG) REAPS the child on the
// first poll, and ProcessExited() is const but must stay true on every later
// call (the win32 kernel handle keeps signaling for free; posix needs the
// cached verdict).
struct ChildState {
    pid_t pid;
    bool reaped;
    uint32_t exitCode;  // shell-parity: status, or 128+signo on signal death
};

constexpr int kStopPollMs = 100;     // reader wake budget (Stop join bound)
constexpr int kStopWaitMs = 2000;    // R-D2: bounded waitpid before SIGKILL

// hpcon_/proc_ are plain void* (not atomic) — reads/writes are confined to
// Start (before the reader thread exists), the UI thread's
// ProcessExited/WriteInput, and Stop (the reader is joined in between).
int MasterFd(const void* slot) {
    return static_cast<int>(reinterpret_cast<intptr_t>(slot));
}

void SetMasterFd(void** slot, int fd) {
    *slot = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
}

int ProcessId(const void* proc) {
    const ChildState* state = static_cast<const ChildState*>(proc);
    return state ? state->pid : -1;
}

// Shell-parity exit classification, same convention JKProcess_posix.cpp uses
// (clean exit keeps the code; signal death reports 128+signo, non-zero).
uint32_t Classifier(const int status) {
    return WIFEXITED(status) ? static_cast<uint32_t>(WEXITSTATUS(status))
                             : static_cast<uint32_t>(128 + WTERMSIG(status));
}

}  // namespace

JKConPtyBridge::~JKConPtyBridge() {
    Stop();
}

bool JKConPtyBridge::Start(const std::string& commandLine, int cols, int rows) {
    if (started_) return false;

    int master = -1;
    int slave = -1;
    if (openpty(&master, &slave, nullptr, nullptr, nullptr) != 0 ||
        master < 0 || slave < 0) {
        std::fprintf(stderr, "JKConPtyBridge: openpty failed (errno=%d)\n",
                     errno);
        if (master >= 0) close(master);
        if (slave >= 0) close(slave);
        return false;
    }
    if (master == 0) {
        // fd 0 was closed in this process and openpty filled the slot: move
        // the master out so the fd never doubles as a zero/invalid encoding.
        const int moved = fcntl(master, F_DUPFD, 1);
        if (moved < 0) {
            const int err = errno;
            std::fprintf(stderr, "JKConPtyBridge: master fd move failed "
                                 "(errno=%d)\n", err);
            close(master);
            close(slave);
            return false;
        }
        close(master);
        master = moved;
    }

    // Size the pty before the child sees it. Field-order note: struct winsize
    // stores ws_row BEFORE ws_col, but the header's argument order follows the
    // win32 COORD convention the win32 body uses (first param = cols, second
    // = rows) — assigned by name, never by aggregate position.
    winsize ws{};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    if (ioctl(master, TIOCSWINSZ, &ws) != 0) {
        const int err = errno;
        std::fprintf(stderr, "JKConPtyBridge: TIOCSWINSZ failed (errno=%d)\n",
                     err);
        close(master);
        close(slave);
        return false;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        const int err = errno;
        std::fprintf(stderr, "JKConPtyBridge: fork failed (errno=%d)\n", err);
        close(master);
        close(slave);
        return false;
    }
    if (pid == 0) {
        // Child: new session (setsid ⇒ own process group ⇒ pid==pgid, so the
        // later kill(-pgid) sweeps the shell's children too), the pty becomes
        // the controlling terminal, and all of stdio is the slave. No UTF-8
        // conversion step exists here: argv is byte-transparent on posix, so
        // the win32 CP_UTF8→UTF-16 conversion has no counterpart.
        // Every hard-failure exit first scribbles a one-line note onto the
        // SLAVE fd — the parent's reader sees exactly why the child went
        // away, even though Start() itself keeps returning per the win32
        // contract (the child's _exit code stays the real signal).
        const auto die = [slave](const char* what, int code) {
            char note[96];
            const int n = std::snprintf(note, sizeof(note),
                                        "JKConPtyBridge: child %s\r\n", what);
            if (n > 0) {
                // Best effort: if even the note is undeliverable, the exit
                // code below is the only signal left. write()'s unused
                // result is consumed to keep -Wall quiet.
                const ssize_t sent = write(slave, note,
                                           static_cast<size_t>(n));
                (void)sent;
            }
            _exit(code);
        };
        close(master);
        if (setsid() < 0) die("setsid failed", 126);
        if (ioctl(slave, TIOCSCTTY, nullptr) < 0) die("TIOCSCTTY failed", 126);
        if (dup2(slave, STDIN_FILENO) < 0) die("dup2 stdin failed", 126);
        if (dup2(slave, STDOUT_FILENO) < 0) die("dup2 stdout failed", 126);
        if (dup2(slave, STDERR_FILENO) < 0) die("dup2 stderr failed", 126);
        if (slave > STDERR_FILENO) close(slave);
        execl("/bin/sh", "sh", "-c", commandLine.c_str(),
              static_cast<char*>(nullptr));
        die("exec /bin/sh failed", 127);  // same convention the process adapter
                                          // uses (_exit 127 on exec failure)
    }

    // The child owns the slave now — the parent lets go immediately, exactly
    // like the win32 body closing conhost-owned pipe ends after the pseudo
    // console took them.
    close(slave);
    slave = -1;

    // Fail-closed unwind: anything failing from here must reap the child and
    // close the master, never leaking either side out of a Start() that
    // reports false.
    ChildState* child = new (std::nothrow) ChildState{pid, false, 0};
    if (!child) {
        std::fprintf(stderr, "JKConPtyBridge: child state alloc failed\n");
        close(master);
        const int w = waitpid(pid, nullptr, 0);
        if (w < 0) {
            // Child unreachable (unlikely): leave no zombie without looping.
            std::fprintf(stderr, "JKConPtyBridge: waitpid failed (errno=%d)\n",
                         errno);
        }
        return false;
    }

    hpcon_ = reinterpret_cast<void*>(static_cast<intptr_t>(master));
    proc_ = child;
    procThread_ = nullptr;   // win32-only: posix spawns no proc thread
    inWrite_ = nullptr;      // win32-only: no pipe pair on posix
    outRead_ = nullptr;
    exited_ = false;
    started_ = true;         // the reader starts ONLY after the guard is set
    reader_ = std::thread(&JKConPtyBridge::ReaderThread, this);
    return true;
}

bool JKConPtyBridge::ProcessExited() const {
    if (!started_ || !proc_) return false;
    ChildState* state = static_cast<ChildState*>(proc_);
    if (state->reaped) return true;
    int status = 0;
    // Non-blocking reap of OUR child — the win32 body polls the process
    // handle with a zero timeout, posix polls waitpid with WNOHANG.
    const pid_t w = waitpid(state->pid, &status, WNOHANG);
    if (w == 0) return false;  // running
    if (w < 0) return false;   // fail-closed (ECHILD should not happen here)
    state->reaped = true;
    state->exitCode = Classifier(status);
    return true;
}

void JKConPtyBridge::ReaderThread() {
    const int master = MasterFd(hpcon_);
    if (master < 0) {
        exited_ = true;
        return;
    }
    char buf[4096];
    for (;;) {
        if (!started_.load()) return;  // Stop() asked for teardown — THE exit
                                       // mechanism (close wakes no sleeper)
        // 100 ms-timeout poll instead of a bare blocking read: Linux does not
        // wake read() sleepers when the master fd is closed (probe-verified,
        // see the header comment at top), so a bare blocking read could never
        // observe the teardown flag — Stop() would strand it forever. The
        // timeout re-enters the started_ check.
        pollfd pfd{master, static_cast<short>(POLLIN | POLLHUP | POLLERR), 0};
        const int pr = poll(&pfd, 1, kStopPollMs);
        if (pr < 0) {
            if (errno == EINTR) continue;
            exited_ = true;  // the pty read is done, however it failed
            return;
        }
        if (pr == 0) continue;  // quiet period — re-check started_, poll again
        // Second flag check before touching the fd. Stop joins BEFORE closing
        // the master, so no close can land under this thread's feet today;
        // the check is belt-and-braces, keeping the fd touched only while the
        // flag still says the bridge is live.
        if (!started_.load()) return;
        // Mirror of the win32 ReadFile loop: data appends, and ANY terminal
        // outcome (0 EOF, -EIO after the last slave closed, -EBADF after
        // teardown) marks the shell side exited. Only EINTR retries.
        const ssize_t got = read(master, buf, sizeof(buf));
        if (got <= 0) {
            if (got < 0 && errno == EINTR) continue;
            exited_ = true;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(outMutex_);
            outBuf_.append(buf, static_cast<size_t>(got));
            // Cap buffered output; drop oldest half if the UI falls behind.
            if (outBuf_.size() > (1u << 20)) {
                outBuf_.erase(0, outBuf_.size() / 2);
            }
        }
    }
}

void JKConPtyBridge::DrainOutput(std::string& out) {
    std::lock_guard<std::mutex> lock(outMutex_);
    out.append(outBuf_);
    outBuf_.clear();
}

void JKConPtyBridge::WriteInput(const char* data, size_t len) {
    if (!started_ || len == 0) return;
    const int master = MasterFd(hpcon_);
    size_t done = 0;
    while (done < len) {
        const ssize_t n = write(master, data + done, len - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;  // best effort, like the win32 body: shell gone / no room
        }
        done += static_cast<size_t>(n);
    }
}

void JKConPtyBridge::Resize(int cols, int rows) {
    if (!started_) return;
    const int master = MasterFd(hpcon_);
    // Same cols-first argument order the win32 COORD body uses; on the tty
    // side only TIOCSWINSZ speaks, and the shell hears about it through its
    // own SIGWINCH (posix has ResizePseudoConsole's notify built in).
    winsize ws{};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ioctl(master, TIOCSWINSZ, &ws);
}

void JKConPtyBridge::Stop() {
    if (!started_) return;
    started_ = false;  // FIRST: the flag alone bounds the reader within one
                       // poll period (verified: close() wakes no Linux sleeper)

    // (1) Join the reader BEFORE touching the master fd. The reader exits via
    // the started_ re-check ≤100 ms after the flag flip — no fd interaction
    // needed — and joining first means no thread can ever hold a stale master
    // fd number afterwards, eliminating the close-vs-blocked-read fd-reuse
    // window entirely (the earlier close-before-join only narrowed it via a
    // pre-read re-check; the reviewer ruling removes even that).
    if (reader_.joinable()) {
        reader_.join();
    }

    const int master = MasterFd(hpcon_);
    // (2) docs/22 §8.2: pseudoconsole close — conhost analogue. Closing the
    // last master fd hangs the pty up (SIGHUP to the shell's foreground
    // group), which gives the shell its graceful-death chance in (3).
    // Consumer-observable close order is unchanged: pty closed before the
    // bounded shell wait.
    if (master >= 0) {
        close(master);
        SetMasterFd(&hpcon_, -1);
    }

    // (3) Bounded shell wait: ~2s of waitpid(WNOHANG) polling per R-D2 (the
    // win32 body waits 3s on the handle, then falls back to TerminateProcess).
    const pid_t pid = ProcessId(proc_);
    ChildState* state = static_cast<ChildState*>(proc_);
    if (pid > 0) {
        bool reaped = false;
        int status = 0;
        const timespec tick{0, 10 * 1000 * 1000};  // 10 ms — nanosleep, not
                                                   // the deprecated usleep
        for (int waited = 0; waited < kStopWaitMs; waited += 10) {
            const pid_t w = waitpid(pid, &status, WNOHANG);
            if (w == pid) {
                reaped = true;
                break;
            }
            if (w < 0 && errno == ECHILD) {
                // Already reaped on a ProcessExited() call — keep that verdict
                // (the cached exit code stays readable, parity with a win32
                // handle that keeps signaling).
                reaped = true;
                break;
            }
            if (nanosleep(&tick, nullptr) != 0) {
                // EINTR shortens one tick; the loop's own budget keeps the
                // window bounded regardless.
            }
        }
        if (!reaped) {
            // (4) TerminateProcess analogue: kill the whole tree. setsid() at
            // spawn made pid==pgid; if the group vanished (child exec'd away
            // from it), fall back to the lone pid.
            if (kill(-pid, SIGKILL) != 0) {
                kill(pid, SIGKILL);
            }
            waitpid(pid, &status, 0);  // (5) final reap — SIGKILL lands fast
        }
    }
    delete state;
    proc_ = nullptr;
    procThread_ = nullptr;
    inWrite_ = nullptr;
    outRead_ = nullptr;
    exited_ = true;
}

}  // namespace jk

#endif  // !_WIN32
