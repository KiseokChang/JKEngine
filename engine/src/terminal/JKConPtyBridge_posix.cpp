#include <terminal/JKConPtyBridge.h>

#ifndef _WIN32

// Inert stub (docs/68 W3): the ConPTY implementation is Windows-only. The real
// POSIX fork/execvt + openpty implementation lands in stage 2; until then every
// entry point stays inert exactly like the pre-split `#else` block of the old
// single-TU JKConPtyBridge.cpp.

#include <string>

namespace jk {

JKConPtyBridge::~JKConPtyBridge() = default;

bool JKConPtyBridge::Start(const std::string&, int, int) { return false; }
bool JKConPtyBridge::ProcessExited() const { return false; }
void JKConPtyBridge::DrainOutput(std::string&) {}
void JKConPtyBridge::WriteInput(const char*, size_t) {}
void JKConPtyBridge::Resize(int, int) {}
void JKConPtyBridge::Stop() {}

// Header-declared private member, only launched from the win32 Start().
// Defined as a no-op here so a future POSIX Start() (and anything else that
// might reference it) still links; Start() returning false never reaches it.
void JKConPtyBridge::ReaderThread() {}

} // namespace jk

#endif // !_WIN32