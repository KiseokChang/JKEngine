#ifndef _WIN32
// jk::text posix impl (linux stage 3 full-build task 4). iconv legs —
// glibc CP949 (EUC-KR 완성형 + UHC) was verified by the plan's spike (테
// round-trip converts correctly). The TU takes no windows-adjacent header and
// owns iconv.h, so jkcore consumers stay windows.h-clean.
//
// Fail-closed ruling (controller, stage-3 T4): ANY iconv failure —
// E2BIG/EINVAL/EILSEQ or otherwise — and ANY partial conversion (iconv
// consumed some input before erroring, or bytes remain unconsumed) yields the
// EMPTY string, the exact win32 MBTW(MB_ERR_INVALID_CHARS) observation. The
// adapter never hands back '?'-substituted or truncated bytes.
#include "../../include/text/JKTextConv.h"

#include <iconv.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace jk::text {
namespace {

// One-shot iconv conversion, whole-buffer in / whole-buffer out. E2BIG cannot
// happen for these directions (output buffer is 4x the input + slack — UTF-8
// worst case is 3 bytes per UTF-16 unit / 2 bytes per CP949, and back), but
// the ruling is unconditional: any error or leftover input -> empty string.
// Handles are opened/closed per call — these conversions run at human text
// rates (IME strings, terminal cells, edit buffers), not per-frame bulk, so
// the open/close overhead is noise against keeping the code stateless and
// thread-safe (no shared saved-charset state).
bool IconvConvert(const char* from, const char* to, const char* src,
                  size_t srcBytes, std::string* out) {
    iconv_t cd = iconv_open(to, from);
    if (cd == reinterpret_cast<iconv_t>(-1)) return false;  // unknown charset

    const size_t cap = srcBytes * 4 + 8;
    out->assign(cap, '\0');
    char* inPtr = const_cast<char*>(src);  // iconv takes char** (pre-POSIX-2008)
    size_t inLeft = srcBytes;
    char* outPtr = &(*out)[0];
    size_t outLeft = cap;
    const bool ok =
        iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft) !=
            static_cast<size_t>(-1) &&
        inLeft == 0;  // whole buffer consumed — no partial conversion accepted
    const size_t produced = cap - outLeft;
    iconv_close(cd);
    if (!ok) {
        out->clear();
        return false;
    }
    out->resize(produced);
    return true;
}

inline uint16_t LeUnit(const std::string& u16, size_t at) {
    return static_cast<uint16_t>(static_cast<uint8_t>(u16[at])) |
           static_cast<uint16_t>(
               static_cast<uint16_t>(static_cast<uint8_t>(u16[at + 1])) << 8);
}

}  // namespace

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) return L"";
    std::string u16;
    if (!IconvConvert("UTF-8", "UTF-16LE", utf8.data(), utf8.size(), &u16)) {
        return L"";
    }
    // std::wstring is 4-byte wchar_t on Linux: repack UTF-16LE code units into
    // scalar codepoints. A surrogate pair in the byte stream is folded back
    // into one scalar; a lone or unpaired surrogate means the source UTF-8 was
    // itself surrogate-encoded — the win32 leg (MBTW MB_ERR_INVALID_CHARS)
    // REJECTS such input outright and returns EMPTY, never U+FFFD (U+FFFD
    // belongs only to the flag-0 lenient path, which this adapter never runs),
    // so every branch below fails closed to "" as the parity observation.
    if (u16.size() % 2 != 0) return L"";  // stray trailing byte — not UTF-16
    std::wstring out;
    out.reserve(u16.size() / 2);
    for (size_t i = 0; i < u16.size(); i += 2) {
        const uint16_t unit = LeUnit(u16, i);
        if (unit >= 0xD800 && unit < 0xDC00) {  // high surrogate: need a pair
            if (i + 4 > u16.size()) return L"";
            const uint16_t low = LeUnit(u16, i + 2);
            if (low < 0xDC00 || low >= 0xE000) return L"";
            out.push_back(static_cast<wchar_t>(
                0x10000 + ((static_cast<uint32_t>(unit) - 0xD800) << 10) +
                (static_cast<uint32_t>(low) - 0xDC00)));
            i += 2;
        } else if (unit >= 0xDC00 && unit < 0xE000) {
            return L"";  // low surrogate without a high — fail-closed
        } else {
            out.push_back(static_cast<wchar_t>(unit));
        }
    }
    return out;
}

std::string Utf16ToUtf8(std::wstring_view utf16) {
    if (utf16.empty()) return {};
    // Repack wchar_t scalars (4 bytes on Linux) into UTF-16LE code units, then
    // hand the byte stream to iconv. Surrogate scalar values (0xD800..0xDFFF,
    // which cannot appear from a valid UTF-16 string on Windows either) and
    // anything above U+10FFFF are invalid sequences — fail-closed.
    std::string u16;
    u16.reserve(utf16.size() * 4);
    auto pushLe = [&u16](uint16_t unit) {
        u16.push_back(static_cast<char>(unit & 0xFF));
        u16.push_back(static_cast<char>((unit >> 8) & 0xFF));
    };
    for (const wchar_t wc : utf16) {
        const uint32_t cp = static_cast<uint32_t>(wc);
        if (cp < 0x10000) {
            if (cp >= 0xD800 && cp < 0xE000) return {};
            pushLe(static_cast<uint16_t>(cp));
        } else if (cp <= 0x10FFFF) {
            pushLe(static_cast<uint16_t>(
                0xD800 + ((cp - 0x10000) >> 10)));          // high surrogate
            pushLe(static_cast<uint16_t>(
                0xDC00 + ((cp - 0x10000) & 0x3FF)));        // low surrogate
        } else {
            return {};
        }
    }
    std::string out;
    if (!IconvConvert("UTF-16LE", "UTF-8", u16.data(), u16.size(), &out)) {
        return {};
    }
    return out;
}

std::string Utf8ToCp949(std::string_view utf8) {
    if (utf8.empty()) return {};
    std::string out;
    if (!IconvConvert("UTF-8", "CP949", utf8.data(), utf8.size(), &out)) {
        return {};
    }
    return out;
}

std::string Cp949ToUtf8(std::string_view cp949) {
    if (cp949.empty()) return {};
    std::string out;
    if (!IconvConvert("CP949", "UTF-8", cp949.data(), cp949.size(), &out)) {
        return {};
    }
    return out;
}

// #94 경로 인코딩 사슬 (JKTextConv.h 원문 좌표): posix paths are byte strings
// whose encoding the FILESYSTEM owns (UTF-8) — the native view IS the UTF-8
// bytes, so both legs are the identity (no validation: the fail-closed UTF-8
// gates above are UTF-8→something directions, while these two must keep the
// consumer's bytes untouched on both sides; selftest 2q pins the round-trip).
std::string Utf8ToAnsi(std::string_view utf8) { return std::string(utf8); }

std::string AnsiToUtf8(std::string_view ansi) { return std::string(ansi); }

}  // namespace jk::text

#endif  // _WIN32
