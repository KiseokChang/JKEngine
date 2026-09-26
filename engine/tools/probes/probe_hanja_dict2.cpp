// probe_hanja_dict2 — Phase A 보충 조사: 전 조합 0 반환의 원인 분해.
// (a) ImmGetConversionListW 유니코드판 (b) 구형 IME KLID 00000412 로드
// (c) 실창+ImmAssociateContext+열림/변환상태 설정 (d) 4u.
// GO/NO-GO 판정은 probe_hanja_dict와 동일 기준(漢/韓 포함+결정론).
#include <JKHangulUtil.h>
#include <windows.h>
#include <imm.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                       \
    do {                                       \
        if (cond) { ++g_pass; }                \
        else { ++g_fail; std::printf("FAIL: %s\n", msg); } \
    } while (0)

typedef LRESULT (WINAPI *PFN_ImmGetConversionListW)(HKL, HIMC, LPCWSTR,
                                                    LPCANDIDATELIST, DWORD, UINT);

static void DumpCands(const CANDIDATELIST* cl, const char* tag, bool wide) {
    std::printf("  [%s] dwCount=%lu\n", tag, static_cast<unsigned long>(cl->dwCount));
    for (DWORD i = 0; i < cl->dwCount && i < 12; ++i) {
        const BYTE* base = reinterpret_cast<const BYTE*>(cl) + cl->dwOffset[i];
        std::printf("    cand %2lu raw=%02X%02X", static_cast<unsigned long>(i),
                    base[0], base[1]);
        if (wide) {
            const wchar_t* w = reinterpret_cast<const wchar_t*>(base);
            std::printf(" wide->U+%04X\n", static_cast<unsigned>(w[0]));
        } else {
            wchar_t wbuf[64] = {};
            if (MultiByteToWideChar(949, 0, reinterpret_cast<const char*>(base),
                                    -1, wbuf, 64) > 0)
                std::printf(" cp949->U+%04X\n", static_cast<unsigned>(wbuf[0]));
            else
                std::printf(" (cp949 decode fail)\n");
        }
    }
}

static bool TryA(HKL hkl, HIMC himc, const char* srcA, UINT flag,
                 CANDIDATELIST** out, DWORD* err) {
    static std::vector<uint8_t> buf(1 << 16);
    *err = 0;
    SetLastError(0xdeadbeef);
    DWORD got = ImmGetConversionListA(hkl, himc, srcA,
                                      reinterpret_cast<CANDIDATELIST*>(buf.data()),
                                      static_cast<DWORD>(buf.size()), flag);
    *err = GetLastError();
    if (got == 0) return false;
    *out = reinterpret_cast<CANDIDATELIST*>(buf.data());
    return true;
}

static bool TryW(PFN_ImmGetConversionListW pfn, HKL hkl, HIMC himc,
                 const wchar_t* srcW, UINT flag, CANDIDATELIST** out, DWORD* err) {
    static std::vector<uint8_t> buf(1 << 16);
    *err = 0;
    SetLastError(0xdeadbeef);
    DWORD got = pfn(hkl, himc, srcW,
                    reinterpret_cast<CANDIDATELIST*>(buf.data()),
                    static_cast<DWORD>(buf.size()), flag);
    *err = GetLastError();
    if (got == 0) return false;
    *out = reinterpret_cast<CANDIDATELIST*>(buf.data());
    return true;
}

int main() {
    std::printf("=== probe_hanja_dict2 (보충 조사) ===\n");
    HKL korean = reinterpret_cast<HKL>(static_cast<uintptr_t>(0x04120412));
    const char kHanEuc[3] = { static_cast<char>(0xC7), static_cast<char>(0xD1), 0 };
    wchar_t kHanW[2] = { 0xD55C, 0 };

    // (a) 유니코드판은 imm32에 수출되어 있나?
    HMODULE imm = GetModuleHandleA("imm32.dll");
    auto pfnW = reinterpret_cast<PFN_ImmGetConversionListW>(
        imm ? GetProcAddress(imm, "ImmGetConversionListW") : nullptr);
    std::printf("[a] imm32=%p ImmGetConversionListW=%p\n", static_cast<void*>(imm),
                reinterpret_cast<void*>(pfnW));

    CANDIDATELIST* cl = nullptr;
    DWORD err = 0;
    if (pfnW) {
        if (TryW(pfnW, korean, nullptr, kHanW, GCL_CONVERSION, &cl, &err))
            DumpCands(cl, "W/han/CONV/NULL", true);
        else
            std::printf("  W/han/CONV/NULL -> 0 (err=0x%08lX)\n",
                        static_cast<unsigned long>(err));
    }

    // (b) 구형 IME KLID 00000412 로드 시도 — HKL high word가 달라진다.
    HKL old = LoadKeyboardLayoutA("00000412", KLF_NOTELLSHELL);
    std::printf("[b] LoadKeyboardLayout(00000412)=%08lX\n",
                static_cast<unsigned long>(reinterpret_cast<uintptr_t>(old)));
    if (old) {
        if (TryA(old, nullptr, kHanEuc, GCL_CONVERSION, &cl, &err))
            DumpCands(cl, "old/han/CONV/NULL", false);
        else
            std::printf("  old/han/CONV/NULL -> 0 (err=0x%08lX)\n",
                        static_cast<unsigned long>(err));
        // 활성화 후 재시도
        ActivateKeyboardLayout(old, KLF_ACTIVATE);
        if (TryA(old, nullptr, kHanEuc, GCL_CONVERSION, &cl, &err))
            DumpCands(cl, "old/act/han/CONV", false);
        else
            std::printf("  old/act/han/CONV -> 0 (err=0x%08lX)\n",
                        static_cast<unsigned long>(err));
    }

    // (c) 실창+컨텍스트 부착+열림/변환상태 — IMM 사전 조회가 창 문맥을
    // 요구하는가?
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.lpszClassName = "jk_probe_hanja2";
    wc.hInstance = GetModuleHandleA(nullptr);
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, "jk_probe_hanja2", "", 0, 0, 0, 10, 10,
                                nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd) {
        HIMC ctx = ImmCreateContext();
        HIMC prev = ImmAssociateContext(hwnd, ctx);
        ImmSetOpenStatus(ctx, TRUE);
        DWORD conv = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE, sent = IME_SMODE_NONE;
        ImmSetConversionStatus(ctx, conv, sent);
        HIMC ghimc = ImmGetContext(hwnd);
        std::printf("[c] hwnd=%p ctx=%p ImmGetContext=%p open=%d\n",
                    static_cast<void*>(hwnd), static_cast<void*>(ctx),
                    static_cast<void*>(ghimc), ImmGetOpenStatus(ghimc) ? 1 : 0);
        if (TryA(korean, ghimc, kHanEuc, GCL_CONVERSION, &cl, &err))
            DumpCands(cl, "ctx/han/CONV", false);
        else
            std::printf("  ctx/han/CONV -> 0 (err=0x%08lX)\n",
                        static_cast<unsigned long>(err));
        if (pfnW) {
            if (TryW(pfnW, korean, ghimc, kHanW, GCL_CONVERSION, &cl, &err))
                DumpCands(cl, "ctxW/han/CONV", true);
            else
                std::printf("  ctxW/han/CONV -> 0 (err=0x%08lX)\n",
                            static_cast<unsigned long>(err));
        }
        if (ghimc) ImmReleaseContext(hwnd, ghimc);
        ImmAssociateContext(hwnd, prev);
        if (ctx) ImmDestroyContext(ctx);
        DestroyWindow(hwnd);
    }

    // (d) 4u(4) 플래그 변형.
    {
        HIMC ctx = ImmCreateContext();
        if (TryA(korean, ctx, kHanEuc, 4u, &cl, &err))
            DumpCands(cl, "preconv/han", false);
        else
            std::printf("  preconv/han -> 0 (err=0x%08lX)\n",
                        static_cast<unsigned long>(err));
        if (ctx) ImmDestroyContext(ctx);
    }

    std::printf("=== 보충 조사 끝 (판정은 상기 조합의 漢/韓 유무로) ===\n");
    std::printf("PASS %d FAIL %d\n", g_pass, g_fail);
    return 0;
}