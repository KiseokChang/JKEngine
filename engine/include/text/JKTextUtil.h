// include/text/JKTextUtil.h — 문자열 트림 단일 진실원 (docs/106 §2-5 잔여)
//
// 왜: Trim 복사본이 5벌 있었고 두 계열로 갈렸다 —
//   A) std::isspace 기준(공백·\f·\v 등 전부)
//   B) " \t\r\n" 문자집합 기준
// 둘 다 "앞뒤 공백 제거"지만 \f/\v가 붙은 입력에서 결과가 다르다. 계약을
// 이름으로 고정한다(Trim = B, TrimSpace = A). 헤더 inline — CMake 무변.
#ifndef JK_TEXT_UTIL_H
#define JK_TEXT_UTIL_H

#include <cctype>
#include <string>

namespace jk {
namespace text {

// B) " \t\r\n" 만 제거 — 설정·경로·프롬프트 원문 계열에서 쓰던 계약.
inline std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// A) std::isspace 기준(공백·\f·\v 포함) — UI 입력(팔레트·파일명 검색) 계약.
inline std::string TrimSpace(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

}  // namespace text
}  // namespace jk

#endif  // JK_TEXT_UTIL_H
