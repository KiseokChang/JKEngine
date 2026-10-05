#ifndef JKPROCESS_H
#define JKPROCESS_H
// jk::process — process/pipe boundary adapter (docs/68 W4 stage-1).
// Win32 impl JKProcess_win32.cpp; posix impl JKProcess_posix.cpp (stage 2, plan D:
// fork+exec via sh -c + pgid tree kill — the posix_spawn+poll sketch was not used).
// Design contracts carried from the absorbed call sites (do not change):
//  (a) InheritedStdioPipes: parent keeps READ ends only — the write ends are
//      closed immediately after spawn, else the child's stdout never EOFs.
//  (b) JobHandle is RAII-bound to "handle close == tree death"
//      (JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE) — CloseJob IS the kill.

#include <cstdint>
#include <string>
#include <vector>

namespace jk::process {

// STILL_ACTIVE — the reserved win32 "process still running" poll code
// (winbase.h). Both impls carry the 259 convention (win32 GetExitCodeProcess
// verbatim; posix heap ProcState reports 259 from a WNOHANG poll that read
// nothing — JKProcess_posix.cpp). Consumers: JKLlmEngine turn loop and
// JKWindowServer's crash classifier (CleanupDisconnectedClients). The
// per-TU static copy JKWindowServer.cpp carried (docs/68 W4 stage-1
// marking) moved here at stage-3 task 5 so the common-code classifier
// compiles platform-uniform.
inline constexpr uint32_t kStillActiveExit = 259;

struct SpawnOptions {
    std::string commandLineUtf8;   // full command line, UTF-8 (adapter widens)
    std::string workingDir;        // empty = inherit
    bool hideWindow = false;       // CREATE_NO_WINDOW + STARTF_USESHOWWINDOW/SW_HIDE
    bool inheritedStdioPipes = false;  // create stdout/stderr parent-read pipes
};

struct SpawnResult {
    bool ok = false;
    std::string error;
    void* process = nullptr;       // PROCESS_INFORMATION::hProcess (opaque)
    uint32_t pid = 0;              // dwProcessId
    void* stdoutRead = nullptr;    // parent read end — valid iff inheritedStdioPipes
    void* stderrRead = nullptr;
};

SpawnResult Spawn(const SpawnOptions& options);

// Pipe — PeekPipeAvail fills available; ReadPipeData returns bytes read
// (-1 error). Peer-closed detection is the caller's job via read==0/err —
// same observation as ERROR_BROKEN_PIPE today.
bool  PeekPipeAvail(void* pipe, uint32_t* available, int* brokenError);
int   ReadPipeData(void* pipe, char* buffer, int cap);
void  CloseHandleLike(void* handle);

// Job (KILL_ON_CLOSE contract) — LlmEngine only consumer. Interface only;
// win32 impl keeps JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, posix: heap pgid
// handle, kill(-pgid, SIGKILL) on close (plan D).
void* CreateKillOnCloseJob();
bool  AssignToJob(void* job, const SpawnResult& proc);
bool  TerminateJobTree(void* job, uint32_t exitCode);

// Single-process kill (JKWindowServer::KillServerHolders consumer — the
// "TerminateProcessTree" docs/68 wording was a survey miscount: 1 real tree
// site + this single-site consumer).
bool KillProcess(void* process, uint32_t exitCode);
// Exit classification (JKWindowServer crash path; kStillActiveExit owned by
// this header now).
bool GetExitCode(void* process, uint32_t* exitCode);

// Bounded wait for a spawned child to exit (JKLlmEngine turn-reap consumer —
// a 5s WaitForSingleObject before CloseHandleLike). true = exit observed (and
// the exit code is readable via GetExitCode); false = still running after
// timeoutMs, wait setup failed, or the handle is not a live process handle.
// The caller's next GetExitCode/CloseHandleLike keeps today's poll semantics.
bool WaitForExit(void* process, uint32_t timeoutMs);

// Whole-system process image scan (JKWindowServer::ScanServerCandidates
// consumer — the Toolhelp32 snapshot block absorbed verbatim on win32;
// docs/68 W4 stage-1 marking named this site win32-residue). Posix impl
// scans /proc/<pid>/cmdline (argv[0] basename) with a /proc/<pid>/comm
// fallback for kernel threads that have no cmdline.
//
// Image-name MATCHING CONTRACT (extracted from the original scan loop, exact
// semantics — NOT LIKE, NOT substring, NOT prefix): the consumer lowercases
// everything ASCII and compares the FULL image name with == ("jkwinserver.exe"
// / "jkdesktop.exe"). The adapter hands out raw names — win32 narrows UTF-16
// per char to ASCII with a '?' fallback for non-ASCII (original conversion
// kept verbatim); posix returns argv[0]/comm bytes as-is. Lowercasing stays
// at the call site so non-ASCII bytes can never fold.
struct ProcessImageInfo {
    uint32_t pid = 0;
    std::string imageName;
};
std::vector<ProcessImageInfo> ListProcessImages();

}  // namespace jk::process

#endif  // JKPROCESS_H
