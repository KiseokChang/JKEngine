#ifndef JKPROCESS_H
#define JKPROCESS_H
// jk::process — process/pipe boundary adapter (docs/68 W4 stage-1).
// Win32 impl JKProcess_win32.cpp; posix stubs (stage 2: posix_spawn+poll).
// Design contracts carried from the absorbed call sites (do not change):
//  (a) InheritedStdioPipes: parent keeps READ ends only — the write ends are
//      closed immediately after spawn, else the child's stdout never EOFs.
//  (b) JobHandle is RAII-bound to "handle close == tree death"
//      (JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE) — CloseJob IS the kill.

#include <cstdint>
#include <string>

namespace jk::process {

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
// win32 impl keeps JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, posix returns nullptr.
void* CreateKillOnCloseJob();
bool  AssignToJob(void* job, const SpawnResult& proc);
bool  TerminateJobTree(void* job, uint32_t exitCode);

// Single-process kill (JKWindowServer::KillServerHolders consumer — the
// "TerminateProcessTree" docs/68 wording was a survey miscount: 1 real tree
// site + this single-site consumer).
bool KillProcess(void* process, uint32_t exitCode);
// Exit classification (JKWindowServer crash path; kStillActiveExit=259 stays).
bool GetExitCode(void* process, uint32_t* exitCode);

}  // namespace jk::process

#endif  // JKPROCESS_H