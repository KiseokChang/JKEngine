/* #126 [#126 — docs/103 실측 승계] — iconv 헤더 (폐기 코드).
 * 실측 진전: NDK r28(API 28) libc.so에 iconv 심볼이 실물로 존재한다(링커
 * "defined in …/28/libc.so" 원문 — docs/78 TX2 "bionic iconv 부재"는 Termux
 * 시절 단정). 헤더는 extern "C" 선언만 — 정의는 실물 bionic iconv가 갖고,
 * CP949 호환 여부는 iconv_open 실패(fail-closed 룰링)로 정직 관측된다. */
#ifndef SPIKE_B_ICONV_HEADER_H
#define SPIKE_B_ICONV_HEADER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* iconv_t;
extern iconv_t iconv_open(const char* tocode, const char* fromcode);
extern size_t iconv(iconv_t cd, char** inbuf, size_t* inbytesleft,
                    char** outbuf, size_t* outbytesleft);
extern int iconv_close(iconv_t cd);

#ifdef __cplusplus
}
#endif

#endif