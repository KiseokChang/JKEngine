#ifndef APPS_JKAPPMODULE_H
#define APPS_JKAPPMODULE_H

// C ABI contract for dynamically loaded app modules (Phase B).
//
// A game app is built as a shared library (jkapp_<name>.dll) that statically
// contains the jkcore code it needs. The client host (main.cpp --client)
// loads the DLL with LoadLibraryA and drives it exclusively through the two
// C functions below — no C++ objects cross the module boundary, so each
// module may own its own copy of the core and its own CRT state safely.
//
// Exports per module:
//   jk_app_meta()       -> static JKAppMeta describing the app
//   jk_app_run_client() -> constructs the app, Init(meta)+Run(), returns exit code
//
// The .jkx container (Phase C) reuses this ABI: the manifest's entry field
// names the DLL to extract and load.

#include <cstdint>

#if defined(_WIN32)
  #if defined(JKAPP_MODULE_BUILD)
    #define JKAPP_EXPORT extern "C" __declspec(dllexport)
  #else
    #define JKAPP_EXPORT extern "C"
  #endif
#else
  #if defined(JKAPP_MODULE_BUILD)
    #define JKAPP_EXPORT extern "C" __attribute__((visibility("default")))
  #else
    #define JKAPP_EXPORT extern "C"
  #endif
#endif

namespace jk {

// Describes one app module. Kept POD/plain so it can later be produced from
// a .jkx manifest without ABI drift.
struct JKAppMeta {
    const char* name;   // spawn key, e.g. "minesweeper"
    const char* title;  // window/surface title
    int32_t width;      // initial surface width (logical px)
    int32_t height;     // initial surface height (logical px)
};

} // namespace jk

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta();
JKAPP_EXPORT int jk_app_run_client(const char* pipeName);

#endif // APPS_JKAPPMODULE_H