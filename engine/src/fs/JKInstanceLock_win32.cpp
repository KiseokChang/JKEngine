#ifdef _WIN32
// Win32 impl of jk::fs::InstanceLock (docs/68 W6, plan D task 6). The
// CreateMutexA named-mutex acquisition + ERROR_ALREADY_EXISTS refusal absorbed
// from JKWindowServer.cpp:414 verbatim: CreateMutexA(nullptr,
// 1 /* TRUE: initial owner */), acquired iff a handle comes back and
// GetLastError() != ERROR_ALREADY_EXISTS — the controller's WAIT_OBJECT_0|
// WAIT_ABANDONED "counts as acquired" clause is moot on this path because the
// bInitialOwner create takes ownership outright, no wait is involved (a fresh
// or abandoned name is always acquired). Hand-declared dllimports — the
// windows.h-clean TU convention JKWindowServer.cpp carries.
#include "../../include/fs/JKInstanceLock.h"

#include <string>

extern "C" __declspec(dllimport) void* __stdcall CreateMutexA(
    void* lpMutexAttributes, int bInitialOwner, const char* lpName);
extern "C" __declspec(dllimport) int __stdcall ReleaseMutex(void* hMutex);
extern "C" __declspec(dllimport) int __stdcall CloseHandle(void* hObject);
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
extern "C" __declspec(dllimport) void __stdcall SetLastError(unsigned long dwErrCode);
constexpr unsigned long kErrorAlreadyExists = 183;  // winbase.h

namespace jk {
namespace fs {

namespace {

// Single-slot state — the guard handle the absorbed call site used to juggle
// in its local `m` (plus the serverGuardMutex_ lifetime). One guard per
// process; a second acquire over this slot returns false instead of
// overwriting it and leaking the first handle. Process exit releases the
// mutex the same way the original call site relied on (kernel handle table).
void* g_mutex = nullptr;

}  // namespace

bool AcquireInstanceLock(const std::string& lockName) {
    if (g_mutex) return false;  // single slot — release first. Win32 parity for
    // the same-name case: re-Creating an owned named mutex reports
    // ERROR_ALREADY_EXISTS, i.e. refusal. (WindowServer never re-acquires
    // while owning anyway — StartAcceptor gates on its held marker.)
    void* m = CreateMutexA(nullptr, 1 /* TRUE: initial owner */,
                           lockName.c_str());
    const unsigned long err = GetLastError();
    if (!m) {
        // Hard failure (name collides with a non-mutex object, ACL denial…).
        // GetLastError stays untouched since CreateMutexA — the absorbed call
        // site's hard-failure branch prints it verbatim.
        return false;
    }
    if (err == kErrorAlreadyExists) {
        // Another instance holds it. The handle refers to the existing mutex
        // WITHOUT owning it — close it at once (the absorbed call site did),
        // then restore the value the call site used to read straight after
        // CreateMutexA: CloseHandle must not garble the caller-visible
        // diagnostic contract (JKInstanceLock.h).
        CloseHandle(m);
        SetLastError(kErrorAlreadyExists);
        return false;
    }
    g_mutex = m;
    return true;
}

void ReleaseInstanceLock() {
    if (!g_mutex) return;  // nothing held — idempotent no-op (double release
    // is safe, same as the absorbed refusal path's ReleaseMutex+CloseHandle)
    ReleaseMutex(g_mutex);
    CloseHandle(g_mutex);
    g_mutex = nullptr;
}

}  // namespace fs
}  // namespace jk

#endif  // _WIN32
