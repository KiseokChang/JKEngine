/* #125 스파이크-B — iconv 정직 스텁 (폐기 코드). 항상 실패 — 엔진 fail-closed. */
#include "iconv.h"
#include <errno.h>

iconv_t iconv_open(const char* tocode, const char* fromcode)
{ (void)tocode; (void)fromcode; errno = EINVAL; return (iconv_t)-1; }

size_t iconv(iconv_t cd, char** inbuf, size_t* inbytesleft,
             char** outbuf, size_t* outbytesleft)
{ (void)cd; (void)inbuf; (void)inbytesleft; (void)outbuf; (void)outbytesleft;
  errno = EILSEQ; return (size_t)-1; }

int iconv_close(iconv_t cd) { (void)cd; return 0; }
