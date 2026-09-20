#ifndef JKHANGULUTIL_H
#define JKHANGULUTIL_H

#include <string>

namespace jk {

// Convert a UTF-8 string to the KSSM 2-byte encoding used by JKDC's bitmap
// font renderer. On Windows this uses CP949 -> KSSM via the legacy wancode
// tables; on other platforms it currently returns the input unchanged.
std::string Utf8ToKssm(const char* utf8);

// Inverse of Utf8ToKssm: KSSM combination-form codes back to UTF-8, via a
// lazily built inverse of the same wancode tables (docs/60: the script app
// getText roundtrip — JKEdit stores KSSM, JS strings are UTF-8).
std::string KssmToUtf8(const char* kssm);

} // namespace jk

#endif // JKHANGULUTIL_H
