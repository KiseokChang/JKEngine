// End-to-end check for InstallCrashHandler: install the filter, dereference
// null, and confirm a .dmp lands in state/logs (run from engine/build).
#include <JKCrashHandler.h>

int main() {
    jk::InstallCrashHandler("state/logs", "crashprobe");
    int* p = nullptr;
    *p = 42;  // deliberate access violation
    return 0; // unreachable
}