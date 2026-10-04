#ifndef _WIN32
// Posix stubs of jk::process (docs/68 W4 stage-1). No Linux spawn machinery
// yet — stage 2 lands posix_spawn + poll(); until then every call reports
// failure / null / empty and implies no behavior.
#include "../../include/process/JKProcess.h"

namespace jk::process {

SpawnResult Spawn(const SpawnOptions&) {
    SpawnResult r;
    r.error =
        "jk::process: posix spawn arrives with stage 2 (posix_spawn+poll)";
    return r;
}

// brokenError stays 0 — there is no pipe machinery to observe yet.
bool  PeekPipeAvail(void*, uint32_t*, int* brokenError) {
    if (brokenError) *brokenError = 0;
    return false;
}

int   ReadPipeData(void*, char*, int) { return -1; }
void  CloseHandleLike(void*) {}           // nothing is ever owned here yet
void* CreateKillOnCloseJob() { return nullptr; }  // no job object on posix yet
bool  AssignToJob(void*, const SpawnResult&) { return false; }
bool  TerminateJobTree(void*, uint32_t) { return false; }
bool  KillProcess(void*, uint32_t) { return false; }
bool  GetExitCode(void*, uint32_t*) { return false; }

}  // namespace jk::process

#endif  // !_WIN32 — stage 2 will replace these stubs, not their signatures