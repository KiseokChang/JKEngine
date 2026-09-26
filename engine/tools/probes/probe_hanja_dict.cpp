// probe_hanja_dict — O1 한자 변환 Phase A 선형 조사 (docs/66 §A).
// 신형 한국어 IME에서 ImmGetConversionList가 사는가: 소스 인코딩 × 플래그 ×
// hIMC 전수 + 재호출 결정론. GO/NO-GO 게이트 — NO-GO면 수술 중단(폴백 없음).
// 레슨(docs/61 §18): 플래그 갱신류 IMM 관측은 부패한다 — 사전 조회 API는 별개
// 측정으로만 확정한다.
#include <JKHangulUtil.h>
#include <windows.h>
#include <imm.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#pragma comment(lib, "imm32")

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                       \
    do {                                       \
        if (cond) { ++g_pass; }                \
        else { ++g_fail; std::printf("FAIL: %s\n", msg); } \
    } while (0)

// CANDIDATELIST 문자열(IME 문자셋 추정)을 사람이 읽을 수 있게 디코드해 나열.
static void DumpCandidates(const CANDIDATELIST* cl, const char* tag) {
    std::printf("  [%s] dwCount=%lu dwPageSize=%lu\n", tag,
                static_cast<unsigned long>(cl->dwCount),
                static_cast<unsigned long>(cl->dwPageSize));
    for (DWORD i = 0; i < cl->dwCount && i < 12; ++i) {
        const char* s = reinterpret_cast<const char*>(
            reinterpret_cast<const BYTE*>(cl) + cl->dwOffset[i]);
        // 1) CP949(완성형 가설) 디코드
        wchar_t wbuf[64] = {};
        int wl = MultiByteToWideChar(949, 0, s, -1, wbuf, 64);
        std::printf("    cand %2lu raw=%02X%02X", static_cast<unsigned long>(i),
                    static_cast<unsigned char>(s[0]),
                    static_cast<unsigned char>(s[1]));
        if (wl > 0) {
            std::printf(" cp949->");
            for (int j = 0; j < wl - 1 && j < 8; ++j)
                std::printf("U+%04X ", static_cast<unsigned>(wbuf[j]));
        }
        std::printf("\n");
    }
}

// 2단 호출(크기 측정→실제). 크기 측정이 0을 돌려도 4096 폴백으로 시도.
static bool TryList(HKL hkl, HIMC himc, const char* srcA, UINT flag,
                    std::vector<uint8_t>* buf, CANDIDATELIST** out, DWORD* err) {
    buf->assign(4096 * 4, 0);
    *err = 0;
    // 1단: 필요 크기 측정
    DWORD need = ImmGetConversionListA(hkl, himc, srcA, nullptr, 0, flag);
    std::printf("    size-probe dwBufLen=0 -> %lu\n",
                static_cast<unsigned long>(need));
    DWORD blen = need ? need : 4096;
    if (buf->size() < blen) buf->resize(blen);
    auto* cl = reinterpret_cast<CANDIDATELIST*>(buf->data());
    // 2단: 실제 호출. 마지막 에러 채질.
    SetLastError(0xdeadbeef);
    DWORD got = ImmGetConversionListA(hkl, himc, srcA, cl, blen, flag);
    *err = GetLastError();
    if (got == 0) return false;
    *out = cl;
    return true;
}

int main() {
    std::printf("=== probe_hanja_dict (Phase A 선형 조사, docs/66 §A) ===\n");

    // [1] HKL 열거 — 한국어(0x0412) 존재?
    UINT n = GetKeyboardLayoutList(0, nullptr);
    std::vector<HKL> hcls(n ? n : 0);
    if (n) n = GetKeyboardLayoutList(n, hcls.data());
    std::printf("[1] layouts=%lu thread=%08lX\n",
                static_cast<unsigned long>(n),
                static_cast<unsigned long>(reinterpret_cast<uintptr_t>(GetKeyboardLayout(0))));
    HKL korean = nullptr;
    for (UINT i = 0; i < n; ++i) {
        const WORD lang = LOWORD(hcls[i]);
        std::printf("    hkl[%u]=%08lX lang=%04X\n", i,
                    static_cast<unsigned long>(reinterpret_cast<uintptr_t>(hcls[i])),
                    static_cast<unsigned>(lang));
        if (lang == 0x0412) korean = hcls[i];
    }
    CHECK(korean != nullptr, "A1 한국어 HKL(0x0412) 존재");
    if (!korean) {
        std::printf("NO-GO: 한국어 HKL 부재 — 전 경로 graceful no-op 환경\n");
        std::printf("PASS %d FAIL %d\n", g_pass, g_fail);
        return 1;
    }

    // 소스 준비: "한" EUC-KR(C7 D1) / KSSM( Utf8ToKssm 경유), "가" 대조군,
    // "하나" 2음절.
    const char kHanEuc[3] = { static_cast<char>(0xC7), static_cast<char>(0xD1), 0 };
    const std::string kHanKssm = jk::Utf8ToKssm("\xED\x95\x9C");   // 한
    const char kGaEuc[3] = { static_cast<char>(0xB0), static_cast<char>(0xA1), 0 };
    const char kHanaEuc[5] = { static_cast<char>(0xC7), static_cast<char>(0xB3),
                               static_cast<char>(0xB3), static_cast<char>(0xAA), 0 };
    std::printf("[src] han.euc=%02X%02X han.kssm=%02X%02X ga.euc=%02X%02X\n",
                static_cast<unsigned char>(kHanEuc[0]), static_cast<unsigned char>(kHanEuc[1]),
                static_cast<unsigned char>(kHanKssm[0]), static_cast<unsigned char>(kHanKssm[1]),
                static_cast<unsigned char>(kGaEuc[0]), static_cast<unsigned char>(kGaEuc[1]));

    HIMC ctx = ImmCreateContext();
    std::printf("[2] hIMC: NULL + 실컨텍스트(%p)\n", static_cast<void*>(ctx));

    struct Combo { const char* src; UINT flag; HIMC himc; const char* tag; };
    std::vector<Combo> combos;
    for (UINT f : { GCL_CONVERSION, GCL_REVERSECONVERSION }) {
        for (HIMC h : { static_cast<HIMC>(nullptr), ctx }) {
            combos.push_back({ kHanEuc, f, h, "han.euc" });
            combos.push_back({ kHanKssm.c_str(), f, h, "han.kssm" });
        }
    }
    combos.push_back({ kGaEuc, GCL_CONVERSION, nullptr, "ga.euc" });
    combos.push_back({ kHanaEuc, GCL_CONVERSION, nullptr, "hana.euc" });

    std::vector<uint8_t> buf;
    CANDIDATELIST* cl = nullptr;
    DWORD err = 0;
    // 승리 조합 기록 — JKHanjaDict의 고정 상수가 된다.
    const Combo* win = nullptr;
    std::vector<uint32_t> winCands;
    for (const Combo& c : combos) {
        std::printf("  TRY %s flag=%u himc=%p\n", c.tag,
                    static_cast<unsigned>(c.flag), static_cast<void*>(c.himc));
        if (TryList(korean, c.himc, c.src, c.flag, &buf, &cl, &err)) {
            DumpCandidates(cl, c.tag);
            std::vector<uint32_t> cps;
            for (DWORD i = 0; i < cl->dwCount && i < 12; ++i) {
                const char* s = reinterpret_cast<const char*>(
                    reinterpret_cast<const BYTE*>(cl) + cl->dwOffset[i]);
                wchar_t wbuf[64] = {};
                if (MultiByteToWideChar(949, 0, s, -1, wbuf, 64) > 0)
                    cps.push_back(static_cast<uint32_t>(wbuf[0]));
            }
            // GO 조건: "한" 소스 + GCL_CONVERSION + dwCount>=3 + 漢/韓 포함
            if (c.src == kHanEuc && c.flag == GCL_CONVERSION && cl->dwCount >= 3) {
                bool hasHanzi = false;
                for (uint32_t cp : cps)
                    if (cp == 0x6F22 || cp == 0x97D3) hasHanzi = true;
                if (hasHanzi && !win) {
                    win = &c;
                    winCands = cps;
                }
            }
        } else {
            std::printf("    -> failed (got=0, err=0x%08lX)\n",
                        static_cast<unsigned long>(err));
        }
    }
    if (ctx) ImmDestroyContext(ctx);

    // [3] GO 판정
    CHECK(win != nullptr, "A3 GO 조합 존재 (han.euc + GCL_CONVERSION, 漢/韓 포함)");
    if (win) {
        std::printf("[3] WINNER: src=%s flag=%u himc=%s count=%zu\n",
                    win->tag, static_cast<unsigned>(win->flag),
                    win->himc ? "ctx" : "NULL", winCands.size());

        // [4] 재호출 결정론 — 승리 조합 3회.
        bool deterministic = true;
        std::vector<uint8_t> b2;
        CANDIDATELIST* c2 = nullptr;
        for (int r = 0; r < 3; ++r) {
            if (!TryList(korean, win->himc, win->src, win->flag, &b2, &c2, &err)) {
                deterministic = false;
                break;
            }
            std::vector<uint32_t> cps;
            for (DWORD i = 0; i < c2->dwCount && i < 12; ++i) {
                const char* s = reinterpret_cast<const char*>(
                    reinterpret_cast<const BYTE*>(c2) + c2->dwOffset[i]);
                wchar_t wbuf[64] = {};
                if (MultiByteToWideChar(949, 0, s, -1, wbuf, 64) > 0)
                    cps.push_back(static_cast<uint32_t>(wbuf[0]));
            }
            if (cps != winCands) deterministic = false;
        }
        CHECK(deterministic, "A4 승리 조합 3회 재호출 결정론");
        std::printf("GO/NO-GO: %s\n", (g_fail == 0) ? "GO" : "NO-GO");
    } else {
        std::printf("GO/NO-GO: NO-GO\n");
    }

    std::printf("PASS %d FAIL %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}