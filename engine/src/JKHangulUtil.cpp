#include <JKHangulUtil.h>
#include <wancode.h>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
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

std::string KssmToUtf8(const char* kssm) {
#ifdef _WIN32
    if (!kssm || !kssm[0]) return {};

    // 역인덱스 지연 1회 구축: KSSM 코드 → EUC-KR 2바이트. Utf8ToKssm이 쓰는
    // wCodeTable/SingleHan/한자 산술 매핑을 순방향 도메인 순회로 뒤집으므로
    // 왕복 일치가 보장된다.
    static const std::unordered_map<uint16_t, uint16_t> inverse = [] {
        std::unordered_map<uint16_t, uint16_t> m;
        m.reserve(NUMHANGUL + SINGLEHAN + 512);
        for (int idx = 0; idx < NUMHANGUL; ++idx) {
            const uint16_t k = wCodeTable[idx];
            if (k) {
                const uint16_t euc = static_cast<uint16_t>(
                    ((0xB0 + idx / 94) << 8) | (0xA1 + idx % 94));
                m[k] = euc;
            }
        }
        for (int i = 0; i < SINGLEHAN; ++i) {
            const uint16_t k = SingleHan[i];
            if (k) m[k] = static_cast<uint16_t>((0xA4 << 8) | (0xA1 + i));
        }
        // 한자: Utf8ToKssm의 산술 매핑 역 — 순방향과 같은 tmp 순회로 채운다.
        for (int tmp = 0; tmp < 52 * 94; ++tmp) {
            const uint8_t kc1 = static_cast<uint8_t>(0xE0 + tmp / 188);
            const uint8_t raw = static_cast<uint8_t>(0x31 + tmp % 188);
            const uint8_t kc2 = static_cast<uint8_t>(raw + (raw > 0x7E ? 18 : 0));
            const uint16_t euc = static_cast<uint16_t>(
                ((0xCA + tmp / 94) << 8) | (0xA1 + tmp % 94));
            m[static_cast<uint16_t>((kc1 << 8) | kc2)] = euc;
        }
        return m;
    }();

    std::string euc;
    euc.reserve(std::strlen(kssm));
    for (size_t i = 0; kssm[i]; ) {
        const unsigned char c1 = static_cast<unsigned char>(kssm[i]);
        if (c1 < 0x80) {
            euc.push_back(static_cast<char>(c1));
            ++i;
            continue;
        }
        if (!kssm[i + 1]) break;  // 잘린 리드 바이트 — 폐기
        const uint16_t k =
            static_cast<uint16_t>((c1 << 8) | static_cast<unsigned char>(kssm[i + 1]));
        const auto it = inverse.find(k);
        if (it != inverse.end()) {
            euc.push_back(static_cast<char>(it->second >> 8));
            euc.push_back(static_cast<char>(it->second & 0xFF));
        } else {
            euc.push_back('?');
        }
        i += 2;
    }

    // EUC-KR(CP949) -> UTF-8.
    int wlen = MultiByteToWideChar(949, 0, euc.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return {};
    std::vector<wchar_t> wbuf(static_cast<size_t>(wlen));
    MultiByteToWideChar(949, 0, euc.c_str(), -1, wbuf.data(), wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, nullptr, 0,
                                   nullptr, nullptr);
    if (ulen <= 0) return {};
    std::string out(static_cast<size_t>(ulen) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, &out[0], ulen, nullptr,
                        nullptr);
    return out;
#else
    return kssm ? kssm : "";
#endif
}

} // namespace jk

namespace {

// UTF-8 첫 코드포인트 디코드. 기형이면 0. (KssmToUtf8 산출은 정상 UTF-8.)
uint32_t Utf8FirstCodepoint(const char* s) {
    const auto* u = reinterpret_cast<const unsigned char*>(s);
    if (!u[0]) return 0;
    if (u[0] < 0x80) return u[0];
    uint32_t cp = 0;
    int len = 0;
    if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; len = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; len = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; len = 4; }
    else return 0;
    for (int i = 1; i < len; ++i) {
        if ((u[i] & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | static_cast<uint32_t>(u[i] & 0x3F);
    }
    return cp;
}

} // namespace

namespace {
// 실계산부 — 기존 검증된 역인덱스(KssmToUtf8)를 글자 단위로 재사용. 신규 매핑
// 테이블 금지(docs/60 §7: 산술 매핑 이중 유지는 결함의 온상). 캐시 채움 경로.
uint32_t ComputeKssmCodepoint(uint8_t first, uint8_t second) {
#ifdef _WIN32
    const char bytes[3] = { static_cast<char>(first), static_cast<char>(second), 0 };
    const std::string utf8 = jk::KssmToUtf8(bytes);
    if (utf8.empty()) return 0u;
    // KssmToUtf8은 매핑 없는 쌍을 '?'로 치환하므로, 왕복(Utf8ToKssm)이 원래
    // 쌍으로 돌아오는 경우만 실제 매핑으로 인정한다 — 그 외는 0으로 폴백 신호.
    if (jk::Utf8ToKssm(utf8.c_str()) != std::string(bytes)) return 0u;
    return Utf8FirstCodepoint(utf8.c_str());
#else
    // 비(非)Windows 포트 함정(최종리뷰 MINOR-1): Utf8ToKssm이 항등이라 왕복 가드가
    // 항상 통과해 raw KSSM 바이트를 쓰레기 UTF-8 코드포인트로 해독한다. 포트
    // 시점에는 0(비트맵 폴백)을 반환하도록 봉쇄 — 변환기를 이식할 때 함께 고친다.
    (void)first;
    (void)second;
    return 0u;
#endif
}
} // namespace

uint32_t jk::KssmCodepointToUnicode(uint8_t first, uint8_t second) {
    // 메모이즈(최종리뷰 IMP-2): 같은 KSSM 쌍은 리페인트마다 반복 호출되므로 쌍 단위
    // 캐시 — 캐시 히트면 문자열 할당 2건+왕복 변환을 모두 건너뛴다. 0(매핑 없음)도
    // 결과로 캐시 — 계약(0=폴백 신호) 불변.
    static std::mutex mu;
    static std::unordered_map<uint16_t, uint32_t> memo;
    const uint16_t key =
        static_cast<uint16_t>((static_cast<uint16_t>(first) << 8) | second);
    {
        std::lock_guard<std::mutex> lock(mu);
        const auto it = memo.find(key);
        if (it != memo.end()) return it->second;
    }
    const uint32_t cp = ComputeKssmCodepoint(first, second);
    {
        std::lock_guard<std::mutex> lock(mu);
        memo[key] = cp;
    }
    return cp;
}
