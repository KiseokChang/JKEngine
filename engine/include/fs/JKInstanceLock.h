#ifndef JKINSTANCELOCK_H
#define JKINSTANCELOCK_H
// jk::fs — single-instance guard adapter (docs/62 §8 flock 봉쇄; stage-2 plan D
// task 6). Win32 impl JKInstanceLock_win32.cpp (CreateMutexA named mutex — the
// guard acquisition absorbed from JKWindowServer.cpp:414); posix impl
// JKInstanceLock_posix.cpp (flock(LOCK_EX|LOCK_NB) on the /tmp/<name>.lock
// file). Gives the posix port the same acquire/refuse contract instead of a
// no-op guard while the win32 call site keeps its identical observable
// behavior (refusal messages, takeover logic, lifetime).

// Single slot: one guard is held per process. A second AcquireInstanceLock
// while holding — even with the same name — returns false (win32 parity:
// re-opening a named mutex you already own reports ERROR_ALREADY_EXISTS),
// instead of overwriting the slot and leaking the first handle. Call
// ReleaseInstanceLock first. Not thread-safe — the absorbed call site is
// main-thread-only, the same constraint the original local-mutex code carried.

#include <string>

namespace jk::fs {
// Acquire: true=owned (process exits release it — Win32 mutex / flock
// lifetime); false=another instance holds it. Release: drop early (optional).
//
// Win32 diagnostic contract (posix side never touches last-error — there it
// is meaningless): when the call actually attempted the acquisition (slot
// empty at entry), a false return leaves GetLastError() carrying the
// underlying CreateMutexA verdict — ERROR_ALREADY_EXISTS (183) means another
// instance holds the guard, anything else is the acquisition hard-failure
// code. JKWindowServer's hard-failure branch prints it verbatim.
bool AcquireInstanceLock(const std::string& lockName);
void ReleaseInstanceLock();
}  // namespace jk::fs

#endif  // JKINSTANCELOCK_H
