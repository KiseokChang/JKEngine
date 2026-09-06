#include <ipc/JKSharedMemory.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

namespace jk {
namespace ipc {

struct JKSharedMemory::Impl {
    HANDLE hMapFile = nullptr;
    void* view = nullptr;
    size_t size = 0;
};

JKSharedMemory::JKSharedMemory() : impl_(std::make_unique<Impl>()) {
}

JKSharedMemory::~JKSharedMemory() {
    Close();
}

bool JKSharedMemory::Create(const std::string& name, size_t size) {
    if (size == 0 || impl_->hMapFile) return false;

    impl_->hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(size),
        name.c_str());

    if (!impl_->hMapFile) {
        std::fprintf(stderr, "JKSharedMemory::Create('%s') failed: %lu\n",
                     name.c_str(), GetLastError());
        return false;
    }

    impl_->view = MapViewOfFile(impl_->hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!impl_->view) {
        std::fprintf(stderr, "JKSharedMemory::Create MapViewOfFile failed: %lu\n",
                     GetLastError());
        Close();
        return false;
    }

    impl_->size = size;
    return true;
}

bool JKSharedMemory::Open(const std::string& name, size_t size) {
    if (size == 0 || impl_->hMapFile) return false;

    impl_->hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    if (!impl_->hMapFile) {
        std::fprintf(stderr, "JKSharedMemory::Open('%s') failed: %lu\n",
                     name.c_str(), GetLastError());
        return false;
    }

    impl_->view = MapViewOfFile(impl_->hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!impl_->view) {
        std::fprintf(stderr, "JKSharedMemory::Open MapViewOfFile failed: %lu\n",
                     GetLastError());
        Close();
        return false;
    }

    impl_->size = size;
    return true;
}

void JKSharedMemory::Close() {
    if (impl_) {
        if (impl_->view) {
            UnmapViewOfFile(impl_->view);
            impl_->view = nullptr;
        }
        if (impl_->hMapFile) {
            CloseHandle(impl_->hMapFile);
            impl_->hMapFile = nullptr;
        }
        impl_->size = 0;
    }
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

#endif // _WIN32
