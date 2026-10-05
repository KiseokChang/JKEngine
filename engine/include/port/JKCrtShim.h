#ifndef JKCRTSHIM_H
#define JKCRTSHIM_H
// jk::crt — MSVC 전용 CRT 함수 셈(shim) 집합 (linux stage 3 task 2). Win32에선
// CRT 원본 위임(패리티 100% — 위임이라 호출부 텍스트만 바뀌고 관측 동일),
// 그 외(Linux/glibc v1 계획)는 표준 CRT 조합으로 같은 errno_t 계약을 흉내 낸다.
// 헤더 온리(inline) — 세 함수 모두 원본이 인라인 가능한 얇은 위임이라 TU 1개
// 추가 없이 include만으로 소비자 치환이 끝난다.
//
// 계약: 각 함수는 MSVC CRT 원본의 시그니처·반환 규약(errno_t — 성공 0/실패
// nonzero)을 그대로 이어받는다. 실패 시의 errno 값을 그대로 쓰지 않는 곳은
// EIO 등으로 0이 아님을 보장(nonzero 계약 유지).

#ifdef _WIN32
#include <cstdio>   // ::fopen_s
#include <ctime>    // ::localtime_s
#include <cstring>  // ::_stricmp
#else
// Linux 전용 v1 계획: glibc/POSIX 조합. strings.h(strcasecmp)는 POSIX 헤더.
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <strings.h>
#endif

namespace jk::crt {

// fopen_s(&f, path, mode) → ::fopen_s / (posix) fopen + errno
// 성공 0·실패 nonzero, 실패 시 *file=nullptr 호출부 보장. 모드 문자열 그대로.
inline int FopenS(std::FILE** file, const char* path, const char* mode) {
#ifdef _WIN32
    return ::fopen_s(file, path, mode);
#else
    if (file == nullptr) return EINVAL;
    *file = std::fopen(path, mode);
    if (*file != nullptr) return 0;
    const int saved = errno;  // fopen은 실패 시 errno 세트
    return saved != 0 ? saved : EIO;
#endif
}

// localtime_s(&tm, &time_t) → ::localtime_s / (posix) localtime_r
// MSVC errno_t 규약 위임. posix localtime_r은 실패 시 nullptr (errno는
// 대체로 세트되나 규약상 null 판정이 1차 — errno 보조).
inline int LocaltimeS(std::tm* out, const std::time_t* t) {
#ifdef _WIN32
    return ::localtime_s(out, t);
#else
    std::tm* const r = ::localtime_r(t, out);
    if (r != nullptr) return 0;
    const int saved = errno;
    return saved != 0 ? saved : 1;
#endif
}

// _stricmp(a, b) → ::_stricmp / (posix) strcasecmp — ASCII 대소문자 무시 비교
// (로캘 독립 비교는 원본도 아님 — 관측 동형).
inline int Stricmp(const char* a, const char* b) {
#ifdef _WIN32
    return ::_stricmp(a, b);
#else
    return ::strcasecmp(a, b);
#endif
}

}  // namespace jk::crt

#endif  // JKCRTSHIM_H
