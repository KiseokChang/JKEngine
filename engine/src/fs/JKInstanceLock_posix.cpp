#ifndef _WIN32
// Posix impl of jk::fs::InstanceLock (stage-2 plan D task 6, docs/62 §8 flock
// 봉쇄). Brief mapping: lockName → /tmp/<name>.lock, open(O_CREAT|O_RDWR,
// 0600), flock(LOCK_EX|LOCK_NB). Single TU-static slot mirroring the win32
// TU's guard-handle pattern (the same state the absorbed JKWindowServer call
// site used to keep in its local m / serverGuardMutex_).
#include "../../include/fs/JKInstanceLock.h"
#include "../../include/fs/JKFs.h"  // TempDir — docs/78 TX3 폰 /tmp 부재 폴백

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include <fcntl.h>     // open, O_CREAT, O_RDWR, O_CLOEXEC
#include <sys/file.h>  // flock, LOCK_EX, LOCK_NB, LOCK_UN
#include <unistd.h>    // close

namespace jk {
namespace fs {

namespace {

// Single-slot guard state — one guard per process, held until process exit or
// an explicit ReleaseInstanceLock (flock lifetime exactly mirrors the Win32
// named-mutex lifetime: ownership dies with the last open reference).
int g_fd = -1;

}  // namespace

// lockName maps to <TempDir>/<name>.lock. Original brief rule was /tmp/ —
// docs/78 TX3 폰 실측: Android has no writable /tmp for an app uid (open
// fails EACCES), so the adapter owns the base: jk::fs::TempDir() — $TMPDIR
// (Termux: /data/data/com.termux/files/usr/tmp), else /tmp/ (glibc parity),
// with the win32-parity "." fail-soft as the last rung. Backslashes in the
// consumer name ("Local\\jkdesktop-server-<pipe>") stay legal filename
// bytes, but any '/' folds to '_' so a path-shaped name can never escape
// the base or create subdirectories ("../" collapses to ".._").
// O_CLOEXEC matches the win32 mutex handle's non-inheritance: a spawned child
// keeping the lock fd open after its parent dies would hold the flock and
// block the parent's restart forever.
//
// The lock file is NEVER unlinked: unlinking while another process still
// holds the flock lets a second acquirer open+create a fresh inode and hold a
// disjoint "lock" (the classic unlink+flock race). Stale files are harmless —
// flock dies with the holding fd, and the next acquirer reuses the file.
// (Consequence: the file's lifetime is not the lock's lifetime — the same
// observable as the Win32 named object going away while a stale local name
// remains.)
bool AcquireInstanceLock(const std::string& lockName) {
    if (g_fd >= 0) return false;  // single slot — win32 parity: re-opening a
    // named mutex we already own reports ERROR_ALREADY_EXISTS, i.e. refusal.
    std::string path = TempDir();
    if (path == ".") path = "./";  // TempDir fail-soft — needs the separator
    for (const char c : lockName) path += (c == '/') ? '_' : c;
    path += ".lock";

    const int fd = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0) {
        // Hard failure (never happens for names under /tmp in practice) —
        // leave a child-side diagnostic; this is fail-closed, never "owned".
        const int err = errno;
        std::fprintf(
            stderr, "jk::fs::AcquireInstanceLock('%s'): open failed (errno=%d %s)\n",
            path.c_str(), err, strerror(err));
        return false;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const int err = errno;
        // EWOULDBLOCK/EAGAIN = another instance holds it — contract false,
        // silent (the win32 side is silent here too; the refusal message is
        // the call site's). Any other errno is a hard failure worth a
        // diagnostic note.
        if (err != EWOULDBLOCK && err != EAGAIN) {
            std::fprintf(
                stderr,
                "jk::fs::AcquireInstanceLock('%s'): flock failed (errno=%d %s)\n",
                path.c_str(), err, strerror(err));
        }
        close(fd);  // fail-closed: no fd leak on a refused acquire
        return false;
    }
    g_fd = fd;
    return true;
}

// Early release — flock LOCK_UN then close, mirroring the win32 pairing of
// ReleaseMutex + CloseHandle. (flock is also released by the bare close; the
// explicit LOCK_UN keeps the pairing self-evident.) Nothing held = no-op, so
// double release is safe.
void ReleaseInstanceLock() {
    if (g_fd < 0) return;
    flock(g_fd, LOCK_UN);
    close(g_fd);
    g_fd = -1;
}

}  // namespace fs
}  // namespace jk

#endif  // _WIN32
