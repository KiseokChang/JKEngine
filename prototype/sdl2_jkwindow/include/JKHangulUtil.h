#ifndef JKHANGULUTIL_H
#define JKHANGULUTIL_H

#include <string>

namespace jk {

// Convert a UTF-8 string to the KSSM 2-byte encoding used by JKDC's bitmap
// font renderer. On Windows this uses CP949 -> KSSM via the legacy wancode
// tables; on other platforms it currently returns the input unchanged.
std::string Utf8ToKssm(const char* utf8);

} // namespace jk

#endif // JKHANGULUTIL_H
