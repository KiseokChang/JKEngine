// probe_hanja_dict5 — Phase A 보충 4: HanjaDic COM (imkrhjd.dll) 실측.
// IHanjaDic {AD75F3AC-18CD-48C6-A27D-F1E9A7DCE432}, CLSID {4c870c20-...}
// (HKCR\imkrhjd.hanjadic, InprocServer32 = System32\IME\IMEKR\DICTS\imkrhjd.dll).
// 서명(TypeLib 덤프 확정): GetHanjaChars(VT_UI2 한글1자, BSTR* 한자들,
// VARIANT_BOOL* ret) / GetHanjaWords(BSTR 단어, SAFEARRAY*, VARIANT_BOOL*).
// GO 기준은 probe_hanja_dict와 동일: "한"→漢(0x6F22)/韓(0x97D3) 포함+결정론.
#include <windows.h>
#include <objbase.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static const CLSID kCLSID_HanjaDic =
    { 0x4c870c20, 0x4ed3, 0x4235, { 0x9a, 0x2a, 0x71, 0x85, 0xf8, 0xa8, 0x7d, 0x06 } };
static const IID kIID_IHanjaDic =
    { 0xad75f3ac, 0x18cd, 0x48c6, { 0xa2, 0x7d, 0xf1, 0xe9, 0xa7, 0xdc, 0xe4, 0x32 } };
static const IID kIID_IHanjaDic2 =
    { 0xda341716, 0xf254, 0x4797, { 0xa8, 0x4f, 0xe0, 0x0c, 0xf6, 0xc2, 0x94, 0x40 } };

// IUnknown(3) + OpenMainDic + CloseMainDic + GetHanjaWords + GetHanjaChars
struct IHanjaDicP : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE OpenMainDic() = 0;
    virtual HRESULT STDMETHODCALLTYPE CloseMainDic() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHanjaWords(BSTR bstrHangulWord,
                                                    SAFEARRAY** ppsaHanjaWords,
                                                    VARIANT_BOOL* pvRet) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHanjaChars(USHORT wHangulChar,
                                                    BSTR* pbstrHanjaChars,
                                                    VARIANT_BOOL* pvRet) = 0;
};

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                       \
    do {                                       \
        if (cond) { ++g_pass; }                \
        else { ++g_fail; std::printf("FAIL: %s\n", msg); } \
    } while (0)

static std::vector<uint32_t> HanjaCodepoints(const std::wstring& ws) {
    std::vector<uint32_t> cps;
    for (wchar_t c : ws) cps.push_back(static_cast<uint32_t>(c));
    return cps;
}

int main() {
    std::printf("=== probe_hanja_dict5 (HanjaDic COM 실측) ===\n");
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IHanjaDicP* dic = nullptr;
    HRESULT hr = CoCreateInstance(kCLSID_HanjaDic, nullptr, CLSCTX_INPROC_SERVER,
                                  kIID_IHanjaDic, reinterpret_cast<void**>(&dic));
    std::printf("[1] CoCreateInstance(IHanjaDic)=0x%08lX dic=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(dic));
    CHECK(SUCCEEDED(hr) && dic, "C1 HanjaDic COM 생성");
    if (!dic) return 1;

    IUnknown* d2 = nullptr;
    HRESULT h2 = dic->QueryInterface(kIID_IHanjaDic2, reinterpret_cast<void**>(&d2));
    std::printf("[1] QI(IHanjaDic2)=0x%08lX\n", static_cast<unsigned long>(h2));
    if (d2) d2->Release();

    hr = dic->OpenMainDic();
    std::printf("[2] OpenMainDic=0x%08lX\n", static_cast<unsigned long>(hr));
    CHECK(SUCCEEDED(hr), "C2 OpenMainDic 성공");

    // "한" 1자 → 한자 문자열
    BSTR chars = nullptr;
    VARIANT_BOOL vb = VARIANT_FALSE;
    hr = dic->GetHanjaChars(static_cast<USHORT>(0xD55C), &chars, &vb);
    std::printf("[3] GetHanjaChars('한')=0x%08lX vb=%d bstr=%p\n",
                static_cast<unsigned long>(hr), static_cast<int>(vb),
                static_cast<void*>(chars));
    CHECK(SUCCEEDED(hr) && vb != VARIANT_FALSE, "C3 GetHanjaChars('한') 성공");
    std::vector<uint32_t> cps;
    if (SUCCEEDED(hr) && chars) {
        std::wstring ws(chars, SysStringLen(chars));
        std::printf("    len=%u cps:", static_cast<unsigned>(ws.size()));
        for (uint32_t c : HanjaCodepoints(ws)) std::printf(" %04X", c);
        std::printf("\n");
        bool hasHanzi = false;
        for (uint32_t c : HanjaCodepoints(ws))
            if (c == 0x6F22 || c == 0x97D3) hasHanzi = true;
        CHECK(hasHanzi, "C4 '한' 후보에 漢/韓 포함");
        cps = HanjaCodepoints(ws);
        SysFreeString(chars);
    }

    // 재호출 결정론 ×2
    if (!cps.empty()) {
        bool deterministic = true;
        for (int r = 0; r < 2; ++r) {
            BSTR c2 = nullptr;
            VARIANT_BOOL v2 = VARIANT_FALSE;
            if (FAILED(dic->GetHanjaChars(static_cast<USHORT>(0xD55C), &c2, &v2)) ||
                !c2 || HanjaCodepoints(std::wstring(c2, SysStringLen(c2))) != cps)
                deterministic = false;
            if (c2) SysFreeString(c2);
        }
        CHECK(deterministic, "C5 재호출 2회 결정론");
    }

    // 대조군: "가"와 자음 코드(0x1111 범위 밖) — 존재하지 않는 음절은 실패인가
    BSTR c3 = nullptr;
    VARIANT_BOOL v3 = VARIANT_TRUE;
    HRESULT h3 = dic->GetHanjaChars(static_cast<USHORT>(0x1100), &c3, &v3);
    std::printf("[4] GetHanjaChars(0x1100 조합자모)=0x%08lX vb=%d bstr=%p\n",
                static_cast<unsigned long>(h3), static_cast<int>(v3),
                static_cast<void*>(c3));
    if (c3) SysFreeString(c3);

    // 단어 경로 참고: "한국"
    SAFEARRAY* psa = nullptr;
    VARIANT_BOOL vw = VARIANT_FALSE;
    BSTR word = SysAllocString(L"한국");
    HRESULT h4 = dic->GetHanjaWords(word, &psa, &vw);
    std::printf("[5] GetHanjaWords('한국')=0x%08lX vb=%d psa=%p\n",
                static_cast<unsigned long>(h4), static_cast<int>(vw),
                static_cast<void*>(psa));
    if (SUCCEEDED(h4) && psa) {
        BSTR* data = nullptr;
        SafeArrayAccessData(psa, reinterpret_cast<void**>(&data));
        LONG lo = 0, hi = 0;
        SafeArrayGetLBound(psa, 1, &lo);
        SafeArrayGetUBound(psa, 1, &hi);
        std::printf("    words=%ld:", static_cast<long>(hi - lo + 1));
        for (LONG i = lo; i <= hi && i < lo + 8; ++i) {
            std::wstring w(data[i], SysStringLen(data[i]));
            for (uint32_t c : HanjaCodepoints(w)) std::printf(" %04X", c);
            std::printf(" |");
        }
        std::printf("\n");
        SafeArrayUnaccessData(psa);
        SafeArrayDestroy(psa);
    }
    SysFreeString(word);

    dic->CloseMainDic();
    dic->Release();
    CoUninitialize();
    std::printf("GO/NO-GO: %s\n", g_fail == 0 ? "GO" : "NO-GO");
    std::printf("PASS %d FAIL %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}