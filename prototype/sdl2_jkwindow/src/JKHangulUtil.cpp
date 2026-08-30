#include <JKHangulUtil.h>
#include <wancode.h>
#include <cstring>
#include <cstdint>
#include <vector>

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
#ifndef MB_ERR_INVALID_CHARS
#define MB_ERR_INVALID_CHARS 0x00000008
#endif

// Avoid pulling in <windows.h> in this translation unit; declare the two
// conversion APIs (kernel32) directly. They are linked implicitly on Windows.
extern "C" __stdcall int MultiByteToWideChar(unsigned int CodePage,
                                             unsigned long dwFlags,
                                             const char* lpMultiByteStr,
                                             int cbMultiByte,
                                             wchar_t* lpWideCharStr,
                                             int cchWideChar);
extern "C" __stdcall int WideCharToMultiByte(unsigned int CodePage,
                                             unsigned long dwFlags,
                                             const wchar_t* lpWideCharStr,
                                             int cchWideChar,
                                             char* lpMultiByteStr,
                                             int cbMultiByte,
                                             const char* lpDefaultChar,
                                             int* lpUsedDefaultChar);

namespace jk {

std::string Utf8ToKssm(const char* utf8) {
#ifdef _WIN32
    if (!utf8 || !utf8[0]) return {};

    // 1. UTF-8 -> EUC-KR (CP949) byte stream.
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
    if (wlen <= 0) return {};
    std::vector<wchar_t> wbuf(static_cast<size_t>(wlen));
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wbuf.data(), wlen);
    int elen = WideCharToMultiByte(949, 0, wbuf.data(), -1, nullptr, 0, nullptr, nullptr);
    if (elen <= 0) return {};
    std::string euc(static_cast<size_t>(elen) - 1, '\0');
    WideCharToMultiByte(949, 0, wbuf.data(), -1, &euc[0], elen, nullptr, nullptr);

    // 2. EUC-KR completion form -> KSSM combination form via wCodeTable.
    std::string out;
    out.reserve(euc.size());
    for (size_t i = 0; i < euc.size(); ) {
        unsigned char c1 = static_cast<unsigned char>(euc[i]);
        if (c1 < 0x80) {
            out.push_back(static_cast<char>(c1));
            ++i;
            continue;
        }
        if (i + 1 >= euc.size()) {
            out.push_back('?');
            ++i;
            continue;
        }
        unsigned char c2 = static_cast<unsigned char>(euc[i + 1]);
        uint16_t kssm = 0;
        if (c1 >= 0xB0 && c1 <= 0xC8) {
            int idx = (c1 - 0xB0) * 94 + (c2 - 0xA1);
            if (idx >= 0 && idx < NUMHANGUL) kssm = wCodeTable[idx];
        } else if (c1 == 0xA4 && c2 >= 0xA1 && c2 < 0xA1 + SINGLEHAN) {
            kssm = SingleHan[c2 - 0xA1];
        } else if (c1 >= 0xCA && c1 <= 0xFD && c2 >= 0xA1 && c2 <= 0xFE) {
            // EUC-KR Hanja -> KSSM Hanja (inverse of KSSM2KS).
            int tmp = (c1 - 0xCA) * 94 + (c2 - 0xA1);
            uint8_t kc1 = static_cast<uint8_t>(0xE0 + tmp / 188);
            uint8_t raw = static_cast<uint8_t>(0x31 + (tmp % 188));
            uint8_t kc2 = raw + (raw > 0x7E ? 18 : 0);
            kssm = static_cast<uint16_t>((kc1 << 8) | kc2);
        }
        if (kssm) {
            out.push_back(static_cast<char>(kssm >> 8));
            out.push_back(static_cast<char>(kssm & 0xFF));
        } else {
            out.push_back('?');
        }
        i += 2;
    }
    return out;
#else
    return utf8 ? utf8 : "";
#endif
}

} // namespace jk
