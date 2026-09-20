// windows.h-clean TU (JKPlatform_win32.cpp precedent): this file includes
// <windows.h> FIRST, before any engine header, so wingdi macros can't poison
// engine declarations. Nothing else engine-side lives here.
#include <JKCrashHandler.h>

#ifdef _WIN32

#include <windows.h>
#include <dbghelp.h>
#include <io.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>
#include <thread>

#pragma comment(lib, "dbghelp")

namespace jk {
namespace {

std::string g_dumpDir;
std::string g_tag;

void TimeStamp(char* buf, size_t n) {
    const time_t now = time(nullptr);
    struct tm tmv;
    localtime_s(&tmv, &now);
    std::snprintf(buf, n, "%04d%02d%02d_%02d%02d%02d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
}

void EnsureDir(const std::string& dir) {
    CreateDirectoryA(dir.c_str(), nullptr);  // exists → ERROR_ALREADY_EXISTS, fine
}

// The filter must be tiny and allocation-free-ish: only dbghelp + CRT file IO.
LONG WINAPI JkUnhandledFilter(EXCEPTION_POINTERS* ep) {
    char ts[32];
    TimeStamp(ts, sizeof(ts));
    const std::string path = g_dumpDir + "\\" + g_tag + "_" + ts + ".dmp";
    HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;
        // MiniDumpNormal is small and fast; the crash may be mid-heap-corrupt.
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
                          MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

// abort() in the UCRT raises fast-fail and skips the SEH filter — park a
// marker so an abort death is distinguishable from "process vanished".
void JkAbortMarker(int) {
    char ts[32];
    TimeStamp(ts, sizeof(ts));
    const std::string path = g_dumpDir + "\\" + g_tag + "_abort_" + ts + ".log";
    HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w = 0;
        WriteFile(f, "SIGABRT — abort() called", 24, &w, nullptr);
        CloseHandle(f);
    }
}

// Mirror daemon: drain the pipe stdout/stderr were dup2'd into, write every
// chunk to the saved console handle AND the log file (row-flushed — the file
// must survive a crash with the last lines in it).
struct MirrorCtx {
    HANDLE pipeRead;
    HANDLE console;
    std::string path;
    FILE* file;
};

DWORD WINAPI MirrorThread(LPVOID param) {
    MirrorCtx* ctx = static_cast<MirrorCtx*>(param);
    char buf[4096];
    DWORD n = 0;
    for (;;) {
        if (!ReadFile(ctx->pipeRead, buf, sizeof(buf), &n, nullptr) || n == 0) {
            break;  // write end closed (process dying) or pipe broken
        }
        DWORD w = 0;
        if (ctx->console)
            WriteFile(ctx->console, buf, n, &w, nullptr);
        if (ctx->file) {
            fwrite(buf, 1, n, ctx->file);
            std::fflush(ctx->file);  // crash-truth: never lose the tail
        }
    }
    if (ctx->file) std::fclose(ctx->file);
    CloseHandle(ctx->pipeRead);
    delete ctx;
    return 0;
}

} // namespace

void InstallCrashHandler(const std::string& dumpDir, const std::string& tag) {
    g_dumpDir = dumpDir;
    g_tag = tag;
    EnsureDir(dumpDir);
    SetUnhandledExceptionFilter(JkUnhandledFilter);
    signal(SIGABRT, JkAbortMarker);
}

bool MirrorLogToFiles(const std::string& logDir, const std::string& tag) {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    // Take a PRIVATE duplicate of the stdout handle before touching any fds.
    // (docs/60 §13) _dup2(wfd, 1) closes fd1's underlying original handle and
    // the freed handle-table slot gets reused by the pipe-write duplicates —
    // the saved value then aliases a pipe write end, and the mirror writing
    // to it feeds its own pipe (measured: 150MB/s self-echo flood, the probe
    // string came back through the pipe). A private dup owns its slot.
    HANDLE consolePriv = nullptr;
    if (console && console != INVALID_HANDLE_VALUE &&
        !DuplicateHandle(GetCurrentProcess(), console, GetCurrentProcess(),
                         &consolePriv, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        consolePriv = nullptr;
    }
    HANDLE pipeRead = nullptr, pipeWrite = nullptr;
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    if (!CreatePipe(&pipeRead, &pipeWrite, &sa, 0)) return false;

    char ts[32];
    TimeStamp(ts, sizeof(ts));
    EnsureDir(logDir);
    const std::string path = logDir + "\\" + tag + "_" + ts + ".log";

    FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "wb") != 0) file = nullptr;

    // stdout + stderr → pipe write end (fd level, so printf/fprintf flow).
    const int wfd = _open_osfhandle(reinterpret_cast<intptr_t>(pipeWrite), 0);
    if (wfd < 0) {
        if (file) std::fclose(file);
        CloseHandle(pipeRead);
        return false;
    }
    _dup2(wfd, _fileno(stdout));
    _dup2(wfd, _fileno(stderr));
    _close(wfd);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    MirrorCtx* ctx = new MirrorCtx{ pipeRead, consolePriv, path, file };
    HANDLE t = CreateThread(nullptr, 0, MirrorThread, ctx, 0, nullptr);
    if (t) CloseHandle(t);  // daemon: we only need the handle to not leak
    return true;
}

} // namespace jk

#else // Non-Windows: no-ops (crash evidence is a live-host concern)

#include <cstring>
#include <cstdio>

namespace jk {

void InstallCrashHandler(const std::string&, const std::string&) {}
bool MirrorLogToFiles(const std::string&, const std::string&) { return false; }

} // namespace jk

#endif // _WIN32