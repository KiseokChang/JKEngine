#ifdef _WIN32
// jk::text win32 impl (linux stage 3 full-build task 4). This TU OWNS
// windows.h — windows.h-first convention: it is the very first include, before
// anything that could drag another windows-adjacent header (the mirror image
// of JKNet_win32.cpp's winsock2.h-first rule). Consumers stay windows.h-clean:
// they see only jk::text and std:: string types, so no hand-written dllimport
// declarations remain in them (JKHangulUtil.cpp's extern "C" __stdcall block
// moved here and became the real API includes).
//
// The conversion bodies delegate VERBATIM to the two kernel32 conversion APIs
// with the exact flag combinations the original JKHangulUtil.cpp legs used
// (codepage constants owned here, no longer defines in the consumer):
//   Utf8 side MBTW:   CP_UTF8 + MB_ERR_INVALID_CHARS  (invalid utf8 -> {}, the
//                     fail-closed contract this adapter carries)
//   CP949 side MBTW:  949, flag 0                      ('?' default-char
//                     substitution for stray bytes, as the original)
//   -> UTF-8 WCTM:    CP_UTF8, flag 0
//   -> CP949 WCTM:    949, flag 0                     ('?' substitution for
//                     chars not representable in CP949, e.g. emoji — the
//                     docs/65 O5 observation, now owned by the adapter)
#include <windows.h>

#include "../../include/text/JKTextConv.h"

#include <string>
#include <string_view>

namespace jk::text {
namespace {

// MBTW two-call shape (query + fill) with explicit lengths — the original legs
// used -1 (NUL scan); the adapter takes length-delimited string_views, so
// embedded-NUL truncation is no longer possible (more correct, input never
// has embedded NULs at the call sites).
std::wstring MbToWide(unsigned int codepage, unsigned long flags,
                      std::string_view in) {
    if (in.empty()) return L"";
    const int cch = ::MultiByteToWideChar(
        codepage, flags, in.data(), static_cast<int>(in.size()), nullptr, 0);
    if (cch <= 0) return L"";  // fail-closed contract (invalid sequence)
    std::wstring w(static_cast<size_t>(cch), L'\0');
    ::MultiByteToWideChar(codepage, flags, in.data(), static_cast<int>(in.size()),
                          &w[0], cch);
    return w;
}

// WCTM two-call shape (query + fill), no default-char override — the same
// lpDefaultChar=nullptr as every original call site.
std::string WideToMb(unsigned int codepage, unsigned long flags,
                     std::wstring_view in) {
    if (in.empty()) return {};
    const int len = ::WideCharToMultiByte(
        codepage, flags, in.data(), static_cast<int>(in.size()), nullptr, 0,
        nullptr, nullptr);
    if (len <= 0) return {};  // fail-closed contract
    std::string out(static_cast<size_t>(len), '\0');
    ::WideCharToMultiByte(codepage, flags, in.data(), static_cast<int>(in.size()),
                          &out[0], len, nullptr, nullptr);
    return out;
}

}  // namespace

std::wstring Utf8ToUtf16(std::string_view utf8) {
    return MbToWide(CP_UTF8, MB_ERR_INVALID_CHARS, utf8);
}

std::string Utf16ToUtf8(std::wstring_view utf16) {
    return WideToMb(CP_UTF8, 0, utf16);
}

std::string Utf8ToCp949(std::string_view utf8) {
    const std::wstring w = MbToWide(CP_UTF8, MB_ERR_INVALID_CHARS, utf8);
    if (w.empty()) return {};
    return WideToMb(949, 0, w);
}

std::string Cp949ToUtf8(std::string_view cp949) {
    const std::wstring w = MbToWide(949, 0, cp949);
    if (w.empty()) return {};
    return WideToMb(CP_UTF8, 0, w);
}

}  // namespace jk::text

#endif  // _WIN32