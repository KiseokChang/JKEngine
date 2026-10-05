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

// Plan H1 (docs/70 §6 #5): win32 JobObject takes MULTIPLE members via
// AssignProcessToJobObject and TerminateJobObject kills them all — the
// single-pgid capture was an overwrite (a second AssignToJob silently
// dropped the first member from the tree kill). Heap state accumulates
// pgids instead, matching the win32 observation.
struct JobState : HandleState {
    std::vector<pid_t> pgids;  // 복수 멤버 — win32 JobObject 복수 계약 패리티
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
        // win32 parity (JKProcess_win32.cpp:77 hStdInput 미설정): 자식 stdin은
        // 데이터 원이 없다 — 부모 stdin 상속은 posix 전용 이탈이었다(플랜 E 잔여
        // docs/70 §6 #5: 패리티 원하면 open("/dev/null")+dup2(0)). /dev/null EOF는
        // win32 NULL stdin 즉시 오류와 같은 관측 — 자식이 read에 영원히 블록하지
        // 않고 부모(agentd 등)의 stdin을 훔치는 사고가 구조적으로 불가능해진다.
        // 룰링(플랜 H1): inheritedStdioPipes와 무관하게 무조건 적용 — win32
        // 스폰의 자식 stdin은 두 계열(pipes/handles) 모두 데이터 원 없음; terminal
        // 접두 pty는 이 어댑터가 아니라 JKConPtyBridge 소관이라 영향 0.
        // fd-0 재사용 엣지(R1): 부모 fd 0이 이미 닫힌 컨텍스트(데몬화된 서버,
        // agentd 연쇄 스폰 — 플랜 G fd 위생의 대표 코너)에서 open은 fd 0 자신을
        // 돌려준다 — dup2는 equal-fd 부작용 없는 no-op이고(FD_CLOEXEC 미소거),
        // 이어지는 close(nullFd)가 자식 fd 0을 통째로 닫아 stdin=/dev/null 계약이
        // EBADF로 붕괴한다. O_CLOEXEC는 곧 닫을 원본 fd에 무의미 — 제거하고
        // nullFd==0일 때는 dup/close 모두 생략한다.
        {
            const int nullFd = ::open("/dev/null", O_RDONLY);
            if (nullFd >= 0 && nullFd != STDIN_FILENO) {
                ::dup2(nullFd, STDIN_FILENO);
                ::close(nullFd);
            }
        }
        // 위에서 닫힌 nullFd 슬롯은 아래 스윕 범위(3+)에 순차 재사용될 수 있고
        // 스윕의 close는 EBADF여도 무해하므로 순서 보장이 충분 — 불변명(스윕은
        // 0/1/2를 건드리지 않는다)은 그대로 유지된다.
        // fork 상속 fd 소각 (플랜 G3 WSLg 실측): fork는 exec 대상과 무관하게
        // 열린 fd 전부를 복사한다 — 대표 사고는 서버 acceptor의 unix listener:
        // taskbar 자동 스폰 직후 자식이 listener를 물려쥐어 소켓 파일이
        // "살아" 보이고, 서버의 다음 acceptor 이터레이션은 "a live server
        // already holds that socket"로 영원히 실패 + 이후 클라 connect는
        // 상속된 listener의 만석 백로그에서 블록(agentd 타임아웃 실측).
        // stdio 0/1/2 유지 — 스윕은 STDERR_FILENO+1(=3)부터 (R2: 첫 판의
        // STDOUT_FILENO+1(=2)는 stderr를 희생 — dup2로 배선된 stderr 파이프
        // 뒤에 close(2)가 먼저 닫아끊어 posix_selftest 2 FAIL 실측: 자식
        // stderr 공백 + `echo >&2`가 EBADF로 sh exit 1). 불변명: 스윕은
        // 0/1/2를 건드리지 않는다 — 위 dup2로 배선된 파이프 포함.
        // EBADF는 정상 경로(close는 fd마다 정확히 닫힌다). 상한은
        // getdtablesize()(리뷰 R1-2, <unistd.h>, _GNU_SOURCE 불요) — 고정
        // 4096은 RLIMIT_NOFILE 소프트 상한이 더 큰 환경에서 그 바깥의 상속
        // fd(대표: acceptor listener)를 계속 누출한다.
        const int fdUpper = getdtablesize();
        for (int fd = STDERR_FILENO + 1; fd < fdUpper; ++fd) close(fd);
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
            // Plan H1: same loop as TerminateJobTree (every member pgid, ESRCH
            // = already dead = success; `bound`는 소멸 — `killed`만이 게이트).
            if (!j->killed) {
                bool ok = true;
                for (pid_t pgid : j->pgids) {
                    if (kill(-pgid, SIGKILL) != 0 && errno != ESRCH) ok = false;
                }
                if (ok) j->killed = true;  // failed member stays retryable
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
        HandleState{kMagicJob}, {} /*pgids: empty job*/, false};
    return job;  // pgids get accumulated at AssignToJob time
}

bool  AssignToJob(void* job, const SpawnResult& proc) {
    JobState* j = AsJob(job);
    ProcState* p = AsProc(proc.process);
    if (!j || !p) return false;
    // setpgid(0,0) at spawn made the child its own group leader, so the pgid
    // literal is the pid — captured in heap state exactly like the win32 job
    // object captures process membership at AssignProcessToJobObject time.
    // Plan H1: win32 JobObject takes MULTIPLE members and keeps them all —
    // ACCUMULATE, never overwrite. Re-assigning the same proc is an
    // idempotent no-op that still reports success (controller ruling).
    for (pid_t pgid : j->pgids)
        if (pgid == p->pid) return true;  // duplicate assign — already a member
    j->pgids.push_back(p->pid);
    return true;
}

bool  TerminateJobTree(void* job, uint32_t exitCode) {
    (void)exitCode;  // posix signal death cannot be forced to a specific code;
                     // the child reports 128+SIGKILL(9)=137, still non-zero.
    JobState* j = AsJob(job);
    if (!j || j->killed) return false;
    // Plan H1: kill EVERY member's process group, not just the last assigned.
    // kill()==ESRCH means the member is already dead — win32 TerminateJobObject
    // on an empty/job-with-dead-members job still succeeds (controller
    // ruling), so ESRCH is treated as success and ok stays true; only a kill
    // failing with any other errno fails the call.
    // An EMPTY job (no member ever assigned) succeeds just like win32's
    // TerminateJobObject on a job with no processes — ok=true, killed=true.
    bool ok = true;
    for (pid_t pgid : j->pgids) {
        if (kill(-pgid, SIGKILL) != 0 && errno != ESRCH) ok = false;
    }
    j->killed = ok;  // full success: no member left to re-kill (이중 킬 방지);
                     // a real error keeps killed=false so CloseHandleLike's
                     // close-kill can retry the failed member.
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
