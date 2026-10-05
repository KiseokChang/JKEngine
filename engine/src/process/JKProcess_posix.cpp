#ifndef _WIN32
// jk::process — real posix impl (docs/68 stage-2 plan D, task 2). Replaces the
// stage-1 stubs; every signature stays exactly as in JKProcess.h. The mapping
// was fixed by the task brief (R-D2): commandLineUtf8 -> "/bin/sh -c", pipes
// via pipe()+fork()+dup2 with the parent keeping the READ ends only (contract
// a), jobs as heap-captured pgids with kill(-pgid, SIGKILL) as the tree kill
// and handle close == tree death (contract b), and STILL_ACTIVE 259 carried
// over via heap-captured child state polled with waitpid(WNOHANG) — the
// win32 exit-code convention survives because the pid and the reaped exit
// code live in the process handle itself, not in the kernel handle table.
#include "../../include/process/JKProcess.h"

#include <cerrno>
#include <csignal>
#include <cstdio>   // std::snprintf
#include <cstdlib>  // std::atol
#include <cstring>  // std::memchr
#include <new>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace jk {
namespace process {

namespace {

// Hand-carried Win32 constants the contract names, for posix parity.
// STILL_ACTIVE(259)는 헤더가 소유로 이동(stage-3 task 5) — 여기의 복각은
// 소각(헤더 상수와 모호성 충돌). kErrBrokenPipe만 이 TU에 남는다.
constexpr int kErrBrokenPipe = 109;         // ERROR_BROKEN_PIPE family sentinel

// Every void* handle this TU hands out is a heap block tagged with a magic so
// CloseHandleLike can dispatch without changing the opaque-handle signature.
// A real malloc'd address can never collide with these tags.
enum : uint32_t {
    kMagicProc = 0x4A4B4334u,  // "JKC4" — process state (pid + exit capture)
    kMagicPipe = 0x4A4B4335u,  // "JKC5" — pipe state (parent read fd)
    kMagicJob = 0x4A4B4336u,   // "JKC6" — job state (heap-captured pgid)
};

struct HandleState {
    uint32_t magic;
};

struct PipeState : HandleState {
    int fd;  // the parent's READ end; never handed to the child
};

struct ProcState : HandleState {
    pid_t pid;
    bool reaped;
    bool killed;
    uint32_t exitCode;
};

struct JobState : HandleState {
    pid_t pgid;   // literal pgid captured on AssignToJob (pgid == pid by setpgid)
    bool bound;
    bool killed;
};

HandleState* AsHandle(void* handle) {
    return handle ? static_cast<HandleState*>(handle) : nullptr;
}
ProcState* AsProc(void* handle) {
    HandleState* h = AsHandle(handle);
    return (h && h->magic == kMagicProc) ? static_cast<ProcState*>(handle)
                                         : nullptr;
}
PipeState* AsPipe(void* handle) {
    HandleState* h = AsHandle(handle);
    return (h && h->magic == kMagicPipe) ? static_cast<PipeState*>(handle)
                                         : nullptr;
}
JobState* AsJob(void* handle) {
    HandleState* h = AsHandle(handle);
    return (h && h->magic == kMagicJob) ? static_cast<JobState*>(handle)
                                        : nullptr;
}

// Fail-closed error strings stay in the stub's "jk::process:" family, with the
// same "<Op> failed (err=N)" shape the win32 TU uses (LastErrorText).
std::string FailText(int err, const char* what) {
    return std::string("jk::process: ") + what + " failed (errno=" +
           std::to_string(err) + ")";
}

const char* ShellPath() { return "/bin/sh"; }

}  // namespace

SpawnResult Spawn(const SpawnOptions& options) {
    SpawnResult result;
    int outPipe[2] = {-1, -1}, errPipe[2] = {-1, -1};

    if (options.inheritedStdioPipes) {
        // stdout and stderr get SEPARATE pipes: engine warnings land on stderr
        // and merging them into stdout would break the reply-JSON parse — the
        // same separation the win32 CreatePipe pair carries.
        if (pipe(outPipe) != 0 || pipe(errPipe) != 0) {
            // Partial success must not leak the first pair (opus NIT-2 parity).
            const int err = errno;
            for (int fd : outPipe)
                if (fd >= 0) close(fd);
            for (int fd : errPipe)
                if (fd >= 0) close(fd);
            result.error = FailText(err, "pipe");
            result.errorCode = static_cast<uint32_t>(err);
            return result;
        }
    }

    // inheritStdioHandles (task 8, jkctl absorb) needs no posix work: fork/exec
    // inherits the parent's stdio fds naturally — the child already shares the
    // console/redirected stdout the caller had. Only record the failure codes,
    // same "(err=N)/(errno=N)" observation the consumers print.
    const pid_t pid = fork();
    if (pid < 0) {
        const int err = errno;
        for (int fd : outPipe)
            if (fd >= 0) close(fd);
        for (int fd : errPipe)
            if (fd >= 0) close(fd);
        result.error = FailText(err, "fork");
        result.errorCode = static_cast<uint32_t>(err);
        return result;
    }

    if (pid == 0) {
        // Child: wire the WRITE ends to fd1/fd2, then exec the shell. No
        // CLOEXEC games needed — the fds cross the execve on purpose; the
        // parent's copies are closed in the parent branch below (contract a).
        if (options.inheritedStdioPipes) {
            if (dup2(outPipe[1], STDOUT_FILENO) < 0) _exit(126);
            if (dup2(errPipe[1], STDERR_FILENO) < 0) _exit(126);
            close(outPipe[0]);
            close(outPipe[1]);
            close(errPipe[0]);
            close(errPipe[1]);
        }
        if (!options.workingDir.empty() &&
            chdir(options.workingDir.c_str()) != 0) {
            _exit(127);
        }
        // Own process group so tree kills (contract b) work without racing
        // the shell's own children back into our group.
        setpgid(0, 0);
        execl(ShellPath(), "sh", "-c", options.commandLineUtf8.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);  // exec failed — exit classification sees 127, not 0
    }

    // Parent: join the pgroup fight on our side (whoever gets there first
    // wins; both set the same value). Benign when the child already exec'd.
    setpgid(pid, pid);

    // Contract (a): the child holds its write ends now — the parent MUST let
    // go immediately after spawn, else the child's stdout never EOFs. The
    // closed slots are cleared (-1) so a later failure-path cleanup loop can
    // never double-close them.
    if (options.inheritedStdioPipes) {
        close(outPipe[1]);
        close(errPipe[1]);
        outPipe[1] = -1;
        errPipe[1] = -1;
    }

    // Fail-closed allocation like the win32 partial-pipe guard: anything left
    // after this point must unwind the whole spawn without throwing out of a
    // void*-ABI adapter.
    auto closePipeFds = [](int (&fds)[2]) {
        for (int& fd : fds) {
            if (fd >= 0) {
                close(fd);
                fd = -1;
            }
        }
    };

    ProcState* proc = new (std::nothrow) ProcState{
        HandleState{kMagicProc}, pid, false, false, 0};
    if (!proc) {
        const int err = ENOMEM;
        closePipeFds(outPipe);
        closePipeFds(errPipe);
        waitpid(pid, nullptr, 0);  // reap ourselves, we created the child
        result.error = FailText(err, "alloc");
        result.errorCode = static_cast<uint32_t>(err);
        return result;
    }

    // Same nothrow/fail-closed discipline as ProcState — a throwing new here
    // would unwind through the C-style void* handle contract.
    PipeState* outRead = nullptr;
    PipeState* errRead = nullptr;
    if (options.inheritedStdioPipes) {
        outRead = new (std::nothrow)
            PipeState{HandleState{kMagicPipe}, outPipe[0]};
        errRead = new (std::nothrow)
            PipeState{HandleState{kMagicPipe}, errPipe[0]};
        if (!outRead || !errRead) {
            const int err = ENOMEM;
            delete outRead;
            delete errRead;
            closePipeFds(outPipe);
            closePipeFds(errPipe);
            waitpid(pid, nullptr, 0);  // reap ourselves, we created the child
            result.error = FailText(err, "alloc");
            result.errorCode = static_cast<uint32_t>(err);
            return result;
        }
    }

    result.ok = true;
    result.process = proc;
    result.pid = static_cast<uint32_t>(pid);
    result.stdoutRead = outRead;  // null iff !inheritedStdioPipes, like win32
    result.stderrRead = errRead;
    return result;
}

bool  PeekPipeAvail(void* pipe, uint32_t* available, int* brokenError) {
    if (brokenError) *brokenError = 0;
    // "peek ok, no data" must not be confused with a stale broken-pipe verdict
    // from an earlier call — brokenError stays 0 unless the peer is observed
    // closed (the win32 SetLastError(0) discipline).
    PipeState* p = AsPipe(pipe);
    if (!p || p->fd < 0) {
        if (brokenError) *brokenError = kErrBrokenPipe;
        return false;
    }
    struct pollfd pf{p->fd, static_cast<short>(POLLIN | POLLHUP | POLLERR), 0};
    if (poll(&pf, 1, 0) < 0) {
        if (brokenError) *brokenError = errno;
        return false;
    }
    const bool readable = (pf.revents & POLLIN) != 0;
    if (readable) {
        int bytes = 0;
        if (ioctl(p->fd, FIONREAD, &bytes) == 0 && bytes > 0) {
            if (available) *available = static_cast<uint32_t>(bytes);
            return true;
        }
    }
    if ((pf.revents & (POLLHUP | POLLERR)) && !readable) {
        // Peer write end closed with nothing buffered — the drained/empty
        // pipe's verdict lands in the ERROR_BROKEN_PIPE family the callers
        // already recognize (109/232).
        if (brokenError) *brokenError = kErrBrokenPipe;
    }
    return false;
}

int   ReadPipeData(void* pipe, char* buffer, int cap) {
    PipeState* p = AsPipe(pipe);
    if (!p || p->fd < 0) return -1;
    if (cap <= 0) return 0;  // win32 parity: ReadFile clamps cap to 0
    ssize_t got = 0;
    do {  // EINTR is retryable — a stale signal must not read as a broken pipe
        got = read(p->fd, buffer, static_cast<size_t>(cap));
    } while (got < 0 && errno == EINTR);
    // got==0 -> peer closed: "this pipe is done" (same observation the win32
    // caller keeps); an error is -1 — the caller's read==0/-1 verdict holds.
    return static_cast<int>(got);
}

void  CloseHandleLike(void* handle) {
    HandleState* h = AsHandle(handle);
    if (!h) return;
    switch (h->magic) {
        case kMagicPipe: {
            PipeState* p = static_cast<PipeState*>(handle);
            if (p->fd >= 0) close(p->fd);
            delete p;
            return;
        }
        case kMagicProc: {
            ProcState* p = static_cast<ProcState*>(handle);
            // Parity: closing a PROCESS handle does not kill the child (that
            // is the job handle's contract-b duty). We only try one cheap
            // WNOHANG reap so an already-exited child does not zombie.
            if (!p->reaped) {
                int st = 0;
                const pid_t w = waitpid(p->pid, &st, WNOHANG);
                if (w == p->pid) {
                    p->reaped = true;
                    p->exitCode = WIFEXITED(st)
                                      ? static_cast<uint32_t>(
                                            WEXITSTATUS(st))
                                      : static_cast<uint32_t>(128 +
                                                              WTERMSIG(st));
                }
            }
            delete p;
            return;
        }
        case kMagicJob: {
            JobState* j = static_cast<JobState*>(handle);
            // Contract (b): "handle close == tree death" — closing the job
            // handle IS the kill, even without a preceding TerminateJobTree.
            if (j->bound && !j->killed) {
                if (kill(-j->pgid, SIGKILL) == 0) j->killed = true;
            }
            delete j;
            return;
        }
        default:
            return;  // unknown pointer: nothing owned here (stage-1 no-op)
    }
}

void* CreateKillOnCloseJob() {
    JobState* job = new (std::nothrow) JobState{
        HandleState{kMagicJob}, 0 /*pgid*/, false /*bound*/, false};
    return job;  // pgid gets captured at AssignToJob time
}

bool  AssignToJob(void* job, const SpawnResult& proc) {
    JobState* j = AsJob(job);
    ProcState* p = AsProc(proc.process);
    if (!j || !p) return false;
    // setpgid(0,0) at spawn made the child its own group leader, so the pgid
    // literal is the pid — captured in heap state exactly like the win32 job
    // object captures process membership at AssignProcessToJobObject time.
    j->pgid = p->pid;
    j->bound = true;
    return true;
}

bool  TerminateJobTree(void* job, uint32_t exitCode) {
    (void)exitCode;  // posix signal death cannot be forced to a specific code;
                     // the child reports 128+SIGKILL(9)=137, still non-zero.
    JobState* j = AsJob(job);
    if (!j || !j->bound || j->killed) return false;
    const bool ok = kill(-j->pgid, SIGKILL) == 0;
    if (ok) j->killed = true;
    return ok;
}

bool  KillProcess(void* process, uint32_t exitCode) {
    (void)exitCode;  // SIGKILL carries no exit code on posix; the child
                     // reports 128+9, still non-zero.
    ProcState* p = AsProc(process);
    if (!p) return false;
    // Reap-check BEFORE any kill: if the child is already dead (or a WNOHANG
    // poll just reaped it), the pid may have been recycled — killing it would
    // hit an innocent process. Fail-closed, like TerminateProcess on a dead
    // handle; the cached exit code stays readable via GetExitCode.
    if (!p->reaped) {
        int st = 0;
        const pid_t w = waitpid(p->pid, &st, WNOHANG);
        if (w == p->pid) {
            p->reaped = true;
            p->exitCode = WIFEXITED(st)
                              ? static_cast<uint32_t>(WEXITSTATUS(st))
                              : static_cast<uint32_t>(128 + WTERMSIG(st));
        }
    }
    if (p->reaped) return false;
    return kill(p->pid, SIGKILL) == 0;
}

bool  GetExitCode(void* process, uint32_t* exitCode) {
    ProcState* p = AsProc(process);
    if (!p) return false;
    if (!p->reaped) {
        int st = 0;
        const pid_t w = waitpid(p->pid, &st, WNOHANG);
        if (w == 0) {
            // Still running: keep the win32 259 convention — the caller's
            // polling loop (engine/src/main.cpp:2889 shape) treats 259 as
            // "not exited yet".
            if (exitCode) *exitCode = kStillActiveExit;
            return true;
        }
        if (w < 0) return false;  // fail-closed (ECHILD etc.)
        p->reaped = true;
        // Shell-parity classification: clean exit keeps the code; signal
        // death reports 128+signo (what a shell would print), non-zero.
        p->exitCode = WIFEXITED(st)
                          ? static_cast<uint32_t>(WEXITSTATUS(st))
                          : static_cast<uint32_t>(128 + WTERMSIG(st));
    }
    if (exitCode) *exitCode = p->exitCode;
    return true;
}

bool  WaitForExit(void* process, uint32_t timeoutMs) {
    ProcState* p = AsProc(process);
    if (!p) return false;
    // WNOHANG polling with a bounded budget (win32 WaitForSingleObject ms
    // semantics): true = exit observed (GetExitCode reads the cached code),
    // false = still running at budget end or wait setup failed. A cached
    // reap short-circuits immediately.
    while (!p->reaped) {
        int st = 0;
        const pid_t w = waitpid(p->pid, &st, WNOHANG);
        if (w == p->pid) {
            p->reaped = true;
            p->exitCode = WIFEXITED(st)
                              ? static_cast<uint32_t>(WEXITSTATUS(st))
                              : static_cast<uint32_t>(128 + WTERMSIG(st));
            return true;
        }
        if (w < 0) return false;  // ECHILD etc. — fail-closed
        if (timeoutMs == 0) return false;
        const uint32_t step = timeoutMs > 20 ? 20 : timeoutMs;
        usleep(static_cast<useconds_t>(step) * 1000u);
        timeoutMs -= step;
    }
    return true;
}

// Whole-system image scan — posix leg. Each /proc/<pid>/cmdline's argv[0]
// basename is the closest analogue of the win32 szExeFile image name, with a
// /proc/<pid>/comm fallback (kernel-truncated to 15 chars) for entries with
// an empty cmdline. Name-less entries are skipped — matching is exact
// equality at the consumer (header contract); posix bytes are returned
// as-is (no ASCII clamp/non-ASCII '?' substitution here, no lowercasing).
std::vector<ProcessImageInfo> ListProcessImages() {
    std::vector<ProcessImageInfo> out;
    DIR* const d = ::opendir("/proc");
    if (!d) return out;
    struct dirent* de = nullptr;
    while ((de = ::readdir(d)) != nullptr) {
        const char* const s = de->d_name;
        if (s[0] < '0' || s[0] > '9') continue;  // numeric pid dirs only
        const long pid = std::atol(s);
        if (pid <= 0) continue;
        char pathBuf[64];
        std::snprintf(pathBuf, sizeof(pathBuf), "/proc/%ld/cmdline", pid);
        std::string name;
        const int fd = ::open(pathBuf, O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            char buf[2048] = {};
            const ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
            ::close(fd);
            if (n > 0) {  // argv[0] = bytes up to the first NUL
                const char* const nul =
                    static_cast<const char*>(std::memchr(buf, '\0', (size_t)n));
                name.assign(buf, nul ? (size_t)(nul - buf) : (size_t)n);
                const size_t slash = name.find_last_of('/');
                if (slash != std::string::npos) name = name.substr(slash + 1);
            }
        }
        if (name.empty()) {  // kernel thread — comm fallback
            std::snprintf(pathBuf, sizeof(pathBuf), "/proc/%ld/comm", pid);
            const int cfd = ::open(pathBuf, O_RDONLY | O_CLOEXEC);
            if (cfd >= 0) {
                char buf[128] = {};
                const ssize_t n = ::read(cfd, buf, sizeof(buf) - 1);
                ::close(cfd);
                if (n > 0) {
                    name.assign(buf, (size_t)n);
                    while (!name.empty() && (name.back() == '\n' ||
                                             name.back() == '\0'))
                        name.pop_back();
                }
            }
        }
        if (name.empty()) continue;
        out.push_back(
            {static_cast<uint32_t>(pid), std::move(name)});
    }
    ::closedir(d);
    return out;
}

}  // namespace process
}  // namespace jk

#endif  // !_WIN32 — win32 TU keeps the real implementation
