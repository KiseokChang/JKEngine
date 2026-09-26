// hanja_dict_live_probe — docs/66: jk::hanja EnsureInit(실제 COM 채널) 실사.
// 콘솔 게이트는 가짜 프로바이더만 주입돼 EnsureInit의 실사전 경로가 무검증
// 이었다. '한'(조합형 D065) 조회 결과를 그대로 프린트한다.
#include <JKHanjaDict.h>
#include <cstdio>
#include <vector>

int main() {
    std::printf("Available() = %d\n", jk::hanja::Available() ? 1 : 0);
    for (const uint16_t syl : {static_cast<uint16_t>(0xD065)}) {
        std::vector<uint16_t> out;
        const bool ok = jk::hanja::Candidates(syl, &out);
        std::printf("Candidates(0x%04X) = %d count=%zu\n", syl, ok ? 1 : 0,
                    out.size());
        for (size_t i = 0; i < out.size() && i < 5; ++i)
            std::printf("  [%zu] pair=0x%04X\n", i, out[i]);
    }
    // 재호출 결정론(캐시 경로).
    std::vector<uint16_t> again;
    jk::hanja::Candidates(0xD065, &again);
    std::printf("second call count=%zu\n", again.size());
    return 0;
}