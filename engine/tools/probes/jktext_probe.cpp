// jktext_probe — 데스크탑 벡터 폰트(docs/63) 로직 단위 프로브.
// T1: KSSM 코드포인트 → 유니코드 변환. T2+: JKTextAtlas / JKDC 배선 (Task 2-3).
#include <JKHangulUtil.h>
#include <cstdint>
#include <cstdio>
#include <string>

using namespace jk;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                       \
    do {                                       \
        if (cond) { ++g_pass; }                \
        else { ++g_fail; std::printf("FAIL: %s\n", msg); } \
    } while (0)

int main() {
    // T1: KSSM→cp. 2바이트 쌍은 Utf8ToKssm 라운드트립으로 구한다.
    const std::string kHan = Utf8ToKssm("한");   // 완성형 음절
    const std::string kGa  = Utf8ToKssm("가");
    const std::string kHanja = Utf8ToKssm("漢"); // 한자 영역
    CHECK(kHan.size() == 2 && kGa.size() == 2 && kHanja.size() == 2,
          "T1 fixture: kssm pairs are 2 bytes");
    CHECK(KssmCodepointToUnicode(static_cast<uint8_t>(kHan[0]),
                                 static_cast<uint8_t>(kHan[1])) == 0xD55C,
          "T1 한 -> U+D55C");
    CHECK(KssmCodepointToUnicode(static_cast<uint8_t>(kGa[0]),
                                 static_cast<uint8_t>(kGa[1])) == 0xAC00,
          "T1 가 -> U+AC00");
    CHECK(KssmCodepointToUnicode(static_cast<uint8_t>(kHanja[0]),
                                 static_cast<uint8_t>(kHanja[1])) == 0x6F22,
          "T1 漢 -> U+6F22");
    // 매핑 없는 쌍(예약 영역) — 0 반환으로 폴백 신호.
    CHECK(KssmCodepointToUnicode(0xFF, 0xFE) == 0, "T1 unmapped pair -> 0");

    std::printf("PASS %d FAIL %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}