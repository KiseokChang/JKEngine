#ifndef JKHANGULUTIL_H
#define JKHANGULUTIL_H

#include <cstdint>
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

// KSSM 2바이트 조합형 쌍을 유니코드 코드포인트로. 매핑 없는 쌍은 0.
// (docs/63 데스크탑 벡터 폰트 — JKDC::HanPutCh가 글리프 조회에 쓴다.)
uint32_t KssmCodepointToUnicode(uint8_t first, uint8_t second);

// KSSM 글자 경계 스캔 (docs/61 §23). 조합형 코드는 둘째 바이트가 0x80+인
// 것이 실재한다(wCodeTable 0x88a1류) — "바이트 >= 0x80 = 첫 바이트"
// 휴리스틱은 둘째 바이트를 첫 바이트로 오판해 커서가 쌍 중간에 진입하고
// 삭제가 쌍을 쪼갠다. 유효성 역인덱스(KssmToUtf8이 쓰는 표)로 판정한다.
//
// KssmCharLenAt: 위치 i의 글자 바이트 수. (b_i,b_i+1)이 유효 KSSM 쌍이면 2,
// 아니면(ASCII/무효 바이트/범위 밖 둘째 바이트) 1.
// KssmPrevBoundary: i 이전(엄밀히 < i) 글자 경계. 경계 스캔은 순방향만
// 결정적이라 0부터 스캔해 마지막 경계를 돌려준다(i > len이면 len).
// KssmSnapBoundary: i가 경계면 그대로, 아니면 첫 경계(쌍 끝)로 스냅 —
// PixelToPos의 "쌍 통째 전진" 관례와 같다.
size_t KssmCharLenAt(const char* s, size_t len, size_t i);
size_t KssmPrevBoundary(const char* s, size_t len, size_t i);
size_t KssmSnapBoundary(const char* s, size_t len, size_t i);

} // namespace jk

#endif // JKHANGULUTIL_H
