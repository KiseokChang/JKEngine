#include <terminal/JKConPtyBridge.h>

#ifdef _WIN32

#include <windows.h>
#include <cstdio>
#include <vector>

namespace jk {

namespace {

// kernel32 ConPTY entry points, loaded dynamically so no ConPTY SDK header is
// needed. Signature per MS docs ("pseudoconsole.h"): HPCON is void* here.
using PseudoConsole = void*;
using FnCreatePseudoConsole = HRESULT(WINAPI*)(COORD, HANDLE, HANDLE, DWORD, PseudoConsole*);
using FnResizePseudoConsole = HRESULT(WINAPI*)(PseudoConsole, COORD);
using FnClosePseudoConsole = void(WINAPI*)(PseudoConsole);

constexpr DWORD kProcThreadAttributePseudoConsole = 0x00020016;  // SDK fallback

FnCreatePseudoConsole LoadCreatePseudoConsole() {
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    return reinterpret_cast<FnCreatePseudoConsole>(
        reinterpret_cast<void*>(GetProcAddress(k32, "CreatePseudoConsole")));
}
FnResizePseudoConsole LoadResizePseudoConsole() {
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    return reinterpret_cast<FnResizePseudoConsole>(
        reinterpret_cast<void*>(GetProcAddress(k32, "ResizePseudoConsole")));
}
FnClosePseudoConsole LoadClosePseudoConsole() {
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    return reinterpret_cast<FnClosePseudoConsole>(
        reinterpret_cast<void*>(GetProcAddress(k32, "ClosePseudoConsole")));
}

} // namespace

JKConPtyBridge::~JKConPtyBridge() {
    Stop();
}

bool JKConPtyBridge::Start(const std::string& commandLine, int cols, int rows) {
    if (started_) return false;

    auto create = LoadCreatePseudoConsole();
    if (!create) {
        std::fprintf(stderr, "JKConPtyBridge: CreatePseudoConsole unavailable\n");
        return false;
    }

    // Pipe pair 1: our input → pty stdin. Pipe pair 2: pty stdout → us.
    HANDLE inRead = nullptr, inWrite = nullptr;
    HANDLE outRead = nullptr, outWrite = nullptr;
    if (!CreatePipe(&inRead, &inWrite, nullptr, 0) ||
        !CreatePipe(&outRead, &outWrite, nullptr, 0)) {
        std::fprintf(stderr, "JKConPtyBridge: CreatePipe failed\n");
        if (inRead) CloseHandle(inRead);
        if (inWrite) CloseHandle(inWrite);
        if (outRead) CloseHandle(outRead);
        if (outWrite) CloseHandle(outWrite);
        return false;
    }

    PseudoConsole hpcon = nullptr;
    const COORD size{ static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
    const HRESULT hr = create(size, inRead, outWrite, 0, &hpcon);
    // conhost owns its ends now; close our copies regardless.
    CloseHandle(inRead);
    CloseHandle(outWrite);
    if (FAILED(hr)) {
        std::fprintf(stderr, "JKConPtyBridge: CreatePseudoConsole failed 0x%08lX\n",
                     static_cast<unsigned long>(hr));
        CloseHandle(inWrite);
        CloseHandle(outRead);
        return false;
    }

    // Spawn the shell with the pseudo console attached via the attribute list.
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    auto attrList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(
        GetProcessHeap(), 0, attrSize));
    bool spawnOk = false;
    STARTUPINFOEXA si{};
    PROCESS_INFORMATION pi{};
    if (attrList &&
        InitializeProcThreadAttributeList(attrList, 1, 0, &attrSize) &&
        UpdateProcThreadAttribute(attrList, 0, kProcThreadAttributePseudoConsole,
                                  hpcon, sizeof(hpcon), nullptr, nullptr)) {
        ZeroMemory(&si, sizeof(si));
        si.StartupInfo.cb = sizeof(si);
        si.lpAttributeList = attrList;   // EXTENDED_STARTUPINFO_PRESENT reads this
        // Console-subsystem children copy the parent's std handles even with
        // bInheritHandles=FALSE (Windows console-to-console inheritance), so a
        // redirected parent (window-server client writing logs) makes the shell
        // think its output is redirected and bypass the pty console. Null std
        // handles with STARTF_USESTDHANDLES force console-device handles, i.e.
        // the pseudoconsole. Verified: child reports IsOutputRedirected=False.
        si.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
        si.StartupInfo.hStdInput = nullptr;
        si.StartupInfo.hStdOutput = nullptr;
        si.StartupInfo.hStdError = nullptr;
        std::vector<char> cmd(commandLine.begin(), commandLine.end());
        cmd.push_back('\0');
        if (CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                           EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                           &si.StartupInfo, &pi)) {
            spawnOk = true;
        } else {
            std::fprintf(stderr, "JKConPtyBridge: CreateProcess failed (%lu)\n",
                         GetLastError());
        }
    } else {
        std::fprintf(stderr, "JKConPtyBridge: attribute list setup failed (%lu)\n",
                     GetLastError());
    }
    if (attrList) {
        DeleteProcThreadAttributeList(attrList);
        HeapFree(GetProcessHeap(), 0, attrList);
    }
    if (!spawnOk) {
        auto close = LoadClosePseudoConsole();
        if (close) close(hpcon);
        CloseHandle(inWrite);
        CloseHandle(outRead);
        return false;
    }

    hpcon_ = hpcon;
    proc_ = pi.hProcess;
    procThread_ = pi.hThread;
    inWrite_ = inWrite;
    outRead_ = outRead;
    exited_ = false;
    started_ = true;

    reader_ = std::thread(&JKConPtyBridge::ReaderThread, this);
    return true;
}

void JKConPtyBridge::ReaderThread() {
    HANDLE outRead = static_cast<HANDLE>(outRead_);
    char buf[4096];
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(outRead, buf, sizeof(buf), &got, nullptr) || got == 0) {
            // Pipe closed: conhost exited or ClosePseudoConsole ran.
            exited_ = true;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(outMutex_);
            outBuf_.append(buf, got);
            // Cap buffered output; drop oldest half if the UI falls behind.
            if (outBuf_.size() > (1u << 20)) {
                outBuf_.erase(0, outBuf_.size() / 2);
            }
        }
    }
}

void JKConPtyBridge::DrainOutput(std::string& out) {
    std::lock_guard<std::mutex> lock(outMutex_);
    out.append(outBuf_);
    outBuf_.clear();
}

void JKConPtyBridge::WriteInput(const char* data, size_t len) {
    if (!started_ || len == 0) return;
    HANDLE inWrite = static_cast<HANDLE>(inWrite_);
    DWORD written = 0;
    // Best effort: a failed write means the shell is gone.
    WriteFile(inWrite, data, static_cast<DWORD>(len), &written, nullptr);
}

void JKConPtyBridge::Resize(int cols, int rows) {
    if (!started_) return;
    auto resize = LoadResizePseudoConsole();
    if (!resize) return;
    const COORD size{ static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
    resize(static_cast<PseudoConsole>(hpcon_), size);
}

void JKConPtyBridge::Stop() {
    if (!started_) return;
    started_ = false;

    auto close = LoadClosePseudoConsole();
    if (close) {
        close(static_cast<PseudoConsole>(hpcon_));   // unblocks the reader
    }
    if (reader_.joinable()) {
        reader_.join();
    }

    HANDLE proc = static_cast<HANDLE>(proc_);
    if (proc) {
        if (WaitForSingleObject(proc, 3000) != WAIT_OBJECT_0) {
            TerminateProcess(proc, 0);
            WaitForSingleObject(proc, 3000);
        }
        CloseHandle(proc);
        proc_ = nullptr;
    }
    if (procThread_) {
        CloseHandle(static_cast<HANDLE>(procThread_));
        procThread_ = nullptr;
    }
    if (inWrite_) {
        CloseHandle(static_cast<HANDLE>(inWrite_));
        inWrite_ = nullptr;
    }
    if (outRead_) {
        CloseHandle(static_cast<HANDLE>(outRead_));
        outRead_ = nullptr;
    }
    hpcon_ = nullptr;
    exited_ = true;
}

} // namespace jk

#else  // !_WIN32 — non-Windows builds get an inert stub

namespace jk {

JKConPtyBridge::~JKConPtyBridge() = default;
bool JKConPtyBridge::Start(const std::string&, int, int) { return false; }
void JKConPtyBridge::DrainOutput(std::string&) {}
void JKConPtyBridge::WriteInput(const char*, size_t) {}
void JKConPtyBridge::Resize(int, int) {}
void JKConPtyBridge::Stop() {}

} // namespace jk

#endif // _WIN32