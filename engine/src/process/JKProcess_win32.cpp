#ifdef _WIN32
// jk::process — Win32 impl (docs/68 W4 stage-1). No behavior change: the
// spawn body ports the JKLlmEngine.cpp spawn/pipe/job block verbatim-in-
// contract (the inherited-stdio branch below is the same code shape as
// agent/JKLlmEngine.cpp:250-292; the job trio is the same call sequence as
// JKLlmEngine.cpp:306-311). This TU is an implementation TU, so it carries
// <windows.h> directly (JKFs_win32.cpp / JKConPtyBridge_win32.cpp precedent);
// JKWindowServer's hand-declared dllimport style stays untouched there.
#include "../../include/process/JKProcess.h"

#include <windows.h>
#include <tlhelp32.h>  // CreateToolhelp32Snapshot / Process32FirstW/NextW

#include <string>
#include <vector>

namespace jk {
namespace process {

namespace {

// UTF-8 -> UTF-16 (CP_UTF8): the W-API spawn needs UTF-16. Local copy of the
// JKLlmEngine helper shape — the shared-helper collapse is a later wave.
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                      static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                        &w[0], n);
    return w;
}

std::string LastErrorText(unsigned long err, const char* what) {
    return std::string(what) + " failed (err=" + std::to_string(err) + ")";
}

} // namespace

SpawnResult Spawn(const SpawnOptions& options) {
    SpawnResult result;
    HANDLE readOut = nullptr, writeOut = nullptr;
    HANDLE readErr = nullptr, writeErr = nullptr;
    if (options.inheritedStdioPipes) {
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        // stdout and stderr get SEPARATE pipes: engine warnings (e.g. claude
        // CLI notices) land on stderr, and merging them into stdout would
        // break the reply-JSON parse (JKLlmEngine.cpp:251-256 contract).
        if (!CreatePipe(&readOut, &writeOut, &sa, 0) ||
            !CreatePipe(&readErr, &writeErr, &sa, 0)) {
            // Partial success must not leak the first pair (opus NIT-2).
            const unsigned long err = GetLastError();
            if (readOut) CloseHandle(readOut);
            if (writeOut) CloseHandle(writeOut);
            if (readErr) CloseHandle(readErr);
            if (writeErr) CloseHandle(writeErr);
            result.error = LastErrorText(err, "CreatePipe");
            return result;
        }
        // Our read ends must NOT be inherited by the child.
        SetHandleInformation(readOut, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(readErr, HANDLE_FLAG_INHERIT, 0);
    }

    // CreateProcessW writes into lpCommandLine, so the buffer must be mutable
    // with a terminating NUL (JKLlmEngine.cpp:272-276 shape).
    const std::wstring full = Utf8ToWide(options.commandLineUtf8);
    std::vector<wchar_t> mutableCmd(full.begin(), full.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    if (options.inheritedStdioPipes) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = writeOut;
        si.hStdError = writeErr;  // hStdInput stays unset, as today
    }
    if (options.hideWindow) {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
    const std::wstring cwd =
        options.workingDir.empty() ? std::wstring()
                                   : Utf8ToWide(options.workingDir);
    DWORD creationFlags = 0;
    if (options.hideWindow) creationFlags |= CREATE_NO_WINDOW;
    PROCESS_INFORMATION pi{};
    const BOOL spawned = CreateProcessW(nullptr, mutableCmd.data(), nullptr,
                                        nullptr,
                                        options.inheritedStdioPipes ? TRUE
                                                                    : FALSE,
                                        creationFlags, nullptr,
                                        cwd.empty() ? nullptr : cwd.c_str(),
                                        &si, &pi);
    // Contract (a): the child holds its write ends now — the parent MUST let
    // go immediately after spawn, else the child's stdout never EOFs.
    if (writeOut) CloseHandle(writeOut);
    if (writeErr) CloseHandle(writeErr);

    if (!spawned) {
        const unsigned long err = GetLastError();
        CloseHandle(readOut);
        CloseHandle(readErr);
        result.error = LastErrorText(err, "CreateProcessW");
        return result;
    }
    // The primary thread handle is closed right away (JKWindowServer
    // SpawnProcess precedent) — SpawnResult intentionally exposes the
    // process handle only; the primary thread needs no separate lifetime.
    CloseHandle(pi.hThread);

    result.ok = true;
    result.process = pi.hProcess;
    result.pid = pi.dwProcessId;
    result.stdoutRead = readOut;
    result.stderrRead = readErr;
    return result;
}

bool  PeekPipeAvail(void* pipe, uint32_t* available, int* brokenError) {
    if (brokenError) *brokenError = 0;
    // SetLastError 0 first: "peek ok, no data" must not be confused with a
    // stale ERROR_BROKEN_PIPE from an earlier call (the JKLlmEngine read
    // loop's else-if predicate, made race-free).
    SetLastError(0);
    DWORD avail = 0;
    const BOOL peeked = PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr);
    const unsigned long err = GetLastError();
    if (peeked && avail > 0) {
        if (available) *available = static_cast<uint32_t>(avail);
        return true;
    }
    if (brokenError) *brokenError = static_cast<int>(err);
    return false;
}

int   ReadPipeData(void* pipe, char* buffer, int cap) {
    DWORD got = 0;
    if (!ReadFile(pipe, buffer, static_cast<DWORD>(cap > 0 ? cap : 0), &got,
                  nullptr)) {
        // Peer closed (ERROR_BROKEN_PIPE) or another failure — the caller's
        // "read==0 means done" verdict keeps today's observation; partial
        // bytes (rare non-broken failures) are still handed up first.
        return got > 0 ? static_cast<int>(got) : -1;
    }
    return static_cast<int>(got);  // 0 also reads as "this pipe is done"
}

void  CloseHandleLike(void* handle) {
    if (handle) CloseHandle(handle);  // (b) closing the job handle IS the kill
}

void* CreateKillOnCloseJob() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return nullptr;
    // Contract (b): KILL_ON_JOB_CLOSE makes "handle close == tree death" —
    // the consumer closes the handle at scope end and every descendant dies
    // (the JKLlmEngine.cpp:300-311 comment stays the ground truth).
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim{};
    lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &lim,
                            sizeof(lim));
    return job;
}

bool  AssignToJob(void* job, const SpawnResult& proc) {
    if (!job || !proc.process) return false;
    // Signature is (hJob, hProcess) — JOB first (winbase.h:2926). The
    // absorbed JKLlmEngine.cpp:311 call passes them swapped and ignores the
    // return value (latently no-op'd job binding); the adapter carries the
    // contract as WRITTEN in that call site's design intent, not its bug.
    return AssignProcessToJobObject(job, proc.process) != FALSE;
}

bool  TerminateJobTree(void* job, uint32_t exitCode) {
    if (!job) return false;
    return TerminateJobObject(job, exitCode) != FALSE;
}

bool  KillProcess(void* process, uint32_t exitCode) {
    if (!process) return false;
    return TerminateProcess(process, exitCode) != FALSE;
}

bool  GetExitCode(void* process, uint32_t* exitCode) {
    if (!process) return false;
    DWORD code = 0;
    if (!GetExitCodeProcess(process, &code)) return false;
    if (exitCode) *exitCode = static_cast<uint32_t>(code);
    return true;
}

bool  WaitForExit(void* process, uint32_t timeoutMs) {
    if (!process) return false;
    // WAIT_OBJECT_0 / WAIT_ABANDONED both mean "exited, status readable";
    // WAIT_TIMEOUT fails (callers keep their poll loops).
    const DWORD w = WaitForSingleObject(process, timeoutMs);
    return w == WAIT_OBJECT_0 || w == WAIT_ABANDONED;
}

// Whole-system image scan — the Toolhelp32 block moved verbatim from
// JKWindowServer.cpp:317-336 (stage-3 task 5): snapshot, first/next walk,
// CloseHandle. The wchar→ASCII narrowing of szExeFile keeps the original
// per-char clamp (0-127 → char, else '?'); lowercasing stays at the call
// site (see the header's matching contract). PROCESSENTRY32W comes from
// <tlhelp32.h> in this TU (it owns windows.h by convention — the call site's
// hand-carried struct was only windows.h-avoidance, same layout).
std::vector<ProcessImageInfo> ListProcessImages() {
    std::vector<ProcessImageInfo> out;
    void* snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!snap || snap == (void*)(long long)-1) return out;  // INVALID_HANDLE_VALUE
    PROCESSENTRY32W e{};
    e.dwSize = sizeof(e);
    if (Process32FirstW(snap, &e)) {
        do {
            std::string exe;
            for (const wchar_t* p = e.szExeFile; *p; ++p) {
                const char c = (*p >= 0 && *p < 128) ? static_cast<char>(*p) : '?';
                exe.push_back(c);
            }
            out.push_back(
                {static_cast<uint32_t>(e.th32ProcessID), std::move(exe)});
        } while (Process32NextW(snap, &e));
    }
    CloseHandle(snap);
    return out;
}

}  // namespace process
}  // namespace jk

#endif  // _WIN32
