#include <ipc/JKSharedMemory.h>

#ifndef _WIN32

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

namespace jk {
namespace ipc {

// docs/78 TX2 ARM regression: bionic has no POSIX shm (shm_open/shm_unlink
// absent) — Android falls back to a NAMED FILE under $TMPDIR while glibc
// keeps /dev/shm. Named-file semantics survive (Open(name) by a second
// process); flash-backed $PREFIX/tmp (not tmpfs) is a TX5 perf-measure item.
#if defined(__ANDROID__)
static std::string ShmPath(const std::string& name) {
    const char* tmp = std::getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    // shm naming uses a leading '/' ("/jkshm-name" style) — strip it for a
    // real path component.
    std::string base = name;
    if (!base.empty() && base[0] == '/') base.erase(0, 1);
    return std::string(tmp) + "/jkshm_" + base;
}
static int ShmOpen(const char* name, int flags, int) {
    return open(ShmPath(name).c_str(), flags, 0644);
}
static int ShmUnlink(const char* name) {
    return unlink(ShmPath(name).c_str());
}
#define JK_SHM_OPEN ShmOpen
#define JK_SHM_UNLINK ShmUnlink
#else
#define JK_SHM_OPEN shm_open
#define JK_SHM_UNLINK shm_unlink
#endif

struct JKSharedMemory::Impl {
    int fd = -1;
    void* view = nullptr;
    size_t size = 0;
    std::string name;
};

JKSharedMemory::JKSharedMemory() : impl_(std::make_unique<Impl>()) {
}

JKSharedMemory::~JKSharedMemory() {
    Close();
}

bool JKSharedMemory::Create(const std::string& name, size_t size) {
    if (size == 0 || impl_->fd >= 0) return false;

    int fd = JK_SHM_OPEN(name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        std::fprintf(stderr, "JKSharedMemory::Create('%s') failed\n", name.c_str());
        return false;
    }

    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
        std::fprintf(stderr, "JKSharedMemory::Create ftruncate failed\n");
        close(fd);
        JK_SHM_UNLINK(name.c_str());
        return false;
    }

    void* view = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED) {
        std::fprintf(stderr, "JKSharedMemory::Create mmap failed\n");
        close(fd);
        JK_SHM_UNLINK(name.c_str());
        return false;
    }

    impl_->fd = fd;
    impl_->view = view;
    impl_->size = size;
    impl_->name = name;
    return true;
}

bool JKSharedMemory::Open(const std::string& name, size_t size) {
    if (size == 0 || impl_->fd >= 0) return false;

    int fd = JK_SHM_OPEN(name.c_str(), O_RDWR, 0644);
    if (fd < 0) {
        std::fprintf(stderr, "JKSharedMemory::Open('%s') failed\n", name.c_str());
        return false;
    }

    void* view = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED) {
        std::fprintf(stderr, "JKSharedMemory::Open mmap failed\n");
        close(fd);
        return false;
    }

    impl_->fd = fd;
    impl_->view = view;
    impl_->size = size;
    impl_->name = name;
    return true;
}

void JKSharedMemory::Close() {
    if (!impl_) return;
    if (impl_->view) {
        munmap(impl_->view, impl_->size);
        impl_->view = nullptr;
    }
    if (impl_->fd >= 0) {
        close(impl_->fd);
        impl_->fd = -1;
    }
    if (!impl_->name.empty()) {
        JK_SHM_UNLINK(impl_->name.c_str());
        impl_->name.clear();
    }
    impl_->size = 0;
}

uint8_t* JKSharedMemory::Data() const {
    return impl_ ? static_cast<uint8_t*>(impl_->view) : nullptr;
}

size_t JKSharedMemory::Size() const {
    return impl_ ? impl_->size : 0;
}

bool JKSharedMemory::IsValid() const {
    return impl_ && impl_->view != nullptr;
}

} // namespace ipc
} // namespace jk

#endif // !_WIN32
