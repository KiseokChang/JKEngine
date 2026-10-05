#ifndef JKTEXTCONV_H
#define JKTEXTCONV_H
// jk::text — 문자셋 변환 경계 어댑터 (linux stage 3 full-build task 4). Win32 impl
// in JKTextConv_win32.cpp (MultiByteToWideChar/WideCharToMultiByte 원문 위임 —
// JKHangulUtil.cpp가 수기 선언하던 두 kernel32 API의 소유 TU 이동), posix impl
// in JKTextConv_posix.cpp (iconv; glibc CP949 실측 확인 — 완성형 테 왕복 spike).
//
// 유효성 계약(어댑터 단일 관측): 부적합 시퀀스/변환 실패 = 빈 문자열 fail-closed.
// win32는 MBTW MB_ERR_INVALID_CHARS 관측을 승계(JKHangulUtil.cpp 원문의
// "wlen <= 0 -> {}" 계약), posix iconv는 E2BIG/EINVAL/EILSEQ 등 어떤 실패와
// 부분 변환도 빈 문자열로 봉쇄한다. 소비자는 기존 빈 문자열 폴백 분기를 그대로 쓴다.

#include <string>
#include <string_view>

namespace jk::text {

// UTF-8 -> UTF-16 (std::wstring). 실패/부적합 = 빈 wstring. posix에서
// std::wstring은 4바이트 wchar_t(Linux)이므로 UTF-16LE 유닛열을 코드포인트로
// 재포장한다(surrogate 쌍 결합 — 유일한 쌍 미스는 fail-closed).
std::wstring Utf8ToUtf16(std::string_view utf8);

// UTF-16 (std::wstring) -> UTF-8. posix는 코드포인트를 UTF-16LE 유닛열로
// 역포장한 뒤 iconv한다(0x10000+는 surrogate 쌍 분해, surrogate scalar는
// 부적합 = 빈 문자열).
std::string Utf16ToUtf8(std::wstring_view utf16);

// UTF-8 -> CP949 (EUC-KR 완성형+UHC) 바이트열 — KS X 1001 행/한자/한글 전부.
// 부적합 UTF-8 = 빈 문자열. CP949로 표현불가 코드포인트는 win32 WCTM(949,0)
// 이 '?'로 치환하고, posix iconv는 실패(빈 문자열)한다 — 관측 차이의 유일 지점
// (이모지 등; win32 원문 계약상 치환, posix 계약상 fail-closed — 컨트롤러 룰링).
std::string Utf8ToCp949(std::string_view utf8);

// CP949 -> UTF-8. 부적합 CP949 바이트열 = 빈 문자열(win32 MBTW(949,0)의 '?'
// 치환과 달리 posix는 실패 — 위 Utf8ToCp949 역방향, 동일 이유).
std::string Cp949ToUtf8(std::string_view cp949);

}  // namespace jk::text

#endif  // JKTEXTCONV_H