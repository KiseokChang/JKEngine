#ifndef _WIN32
// Posix stub of jk::fs (stage 2: readlink("/proc/self/exe")).
#include "../../include/fs/JKFs.h"

namespace jk::fs {
std::string GetExecutablePath() { return std::string(); }
}
#endif
