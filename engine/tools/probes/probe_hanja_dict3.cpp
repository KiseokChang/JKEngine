// probe_hanja_dict3 — Phase A 보충 2: TSF 경로 측정.
// ImmGetConversionList 사망이 확인된 후의 현대판 채널:
// ITfFunctionProvider::GetFunction → ITfFnSearchCandidateProvider::GetSearchCandidates.
// MinGW msctf.h에 ITfFunctionProvider/ITfThreadMgr은 C++ 인터페이스로 존재 — 그대로 사용.
// 결측분(ITfFnSearchCandidateProvider/ITfCandidateList/ITfCandidateString)만 수동 선언
// (SDK ctffunc.h vtable 순서 동일).
#include <windows.h>
#include <msctf.h>
#include <objbase.h>
#include <cstdint>
#include <cstdio>

// ---- 수동 선언(MinGW msctf.h 결측분 — SDK ctffunc.h 서명/순서 동일) ----
static const IID kIID_ITfFnSearchCandidateProvider =
    { 0x87a2ad8f, 0xf27b, 0x4920, { 0x85, 0x01, 0x67, 0x60, 0x22, 0x80, 0x17, 0x5d } };

MIDL_INTERFACE("581f317e-fd9d-443f-b972-ed00467c5d40")
ITfCandidateStringP : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetString(BSTR* pbstr) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIndex(ULONG* pnIndex) = 0;
};

MIDL_INTERFACE("a3ad50fb-9bdb-49e3-a843-6c76520fbf5d")
ITfCandidateListP : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE EnumCandidates(IUnknown** ppEnum) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCandidate(ULONG nIndex,
                                                   ITfCandidateStringP** ppCand) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCandidateNum(ULONG* pnCnt) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetResult(ULONG nIndex, int imcr) = 0;
};

MIDL_INTERFACE("87a2ad8f-f27b-4920-8501-67602280175d")
ITfFnSearchCandidateProviderP : public IUnknown {
public:
    // vtable: IUnknown 3 + ITfFunction::GetDisplayName + GetSearchCandidates + SetResult
    virtual HRESULT STDMETHODCALLTYPE GetDisplayName(BSTR* pbstrName) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSearchCandidates(BSTR bstrQuery,
                                                          BSTR bstrApplicationId,
                                                          ITfCandidateListP** pplist) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetResult(BSTR bstrQuery, BSTR bstrApplicationID,
                                                BSTR bstrResult) = 0;
};

// ---- 프로브 ----
static void DumpList(ITfCandidateListP* list, const char* tag) {
    ULONG n = 0;
    if (FAILED(list->GetCandidateNum(&n))) { std::printf("    num fail\n"); return; }
    std::printf("  [%s] count=%lu\n", tag, static_cast<unsigned long>(n));
    for (ULONG i = 0; i < n && i < 12; ++i) {
        ITfCandidateStringP* cs = nullptr;
        if (FAILED(list->GetCandidate(i, &cs)) || !cs) continue;
        BSTR b = nullptr;
        if (SUCCEEDED(cs->GetString(&b)) && b) {
            std::printf("    cand %2lu U+%04X", static_cast<unsigned long>(i),
                        static_cast<unsigned>(b[0]));
            for (int j = 1; j < 8 && b[j]; ++j)
                std::printf(" %04X", static_cast<unsigned>(b[j]));
            std::printf("\n");
            SysFreeString(b);
        }
        cs->Release();
    }
}

static void ProbeProfile(ITfThreadMgr* tm, const TF_LANGUAGEPROFILE& prof);

static void ProbeProfiles(ITfThreadMgr* tm, const char* tag) {
    auto pCreateIpp = reinterpret_cast<HRESULT(WINAPI*)(ITfInputProcessorProfiles**)>(
        GetProcAddress(LoadLibraryA("msctf.dll"), "TF_CreateInputProcessorProfiles"));
    ITfInputProcessorProfiles* ipp = nullptr;
    if (!pCreateIpp || FAILED(pCreateIpp(&ipp)) || !ipp) {
        std::printf("[%s] TF_CreateInputProcessorProfiles 실패\n", tag);
        return;
    }
    IEnumTfLanguageProfiles* en = nullptr;
    if (SUCCEEDED(ipp->EnumLanguageProfiles(0x0412, &en)) && en) {
        TF_LANGUAGEPROFILE prof[16] = {};
        ULONG got = 0;
        en->Next(16, prof, &got);
        std::printf("[%s] korean profiles=%lu\n", tag, static_cast<unsigned long>(got));
        for (ULONG i = 0; i < got && i < 16; ++i) ProbeProfile(tm, prof[i]);
        en->Release();
    } else {
        std::printf("[%s] EnumLanguageProfiles(0x0412) 실패\n", tag);
    }
    ipp->Release();
}

static void ProbeProfile(ITfThreadMgr* tm, const TF_LANGUAGEPROFILE& prof) {
    OLECHAR cs[64] = {}, pr[64] = {};
    StringFromGUID2(prof.clsid, cs, 64);
    StringFromGUID2(prof.guidProfile, pr, 64);
    std::printf("  profile clsid=%ls profile=%ls active=%d\n", cs, pr,
                static_cast<int>(prof.fActive));

    ITfFunctionProvider* fp = nullptr;
    HRESULT hf = tm->GetFunctionProvider(prof.clsid, &fp);
    std::printf("    GetFunctionProvider=0x%08lX\n", static_cast<unsigned long>(hf));
    if (FAILED(hf) || !fp) return;

    IUnknown* fn = nullptr;
    // rguid 변형 1: 함수 GUID = IID 자체(문서상 전용 함수 GUID 부재 — ctffunc.h 확인).
    HRESULT hg = fp->GetFunction(kIID_ITfFnSearchCandidateProvider,
                                 kIID_ITfFnSearchCandidateProvider, &fn);
    std::printf("    GetFunction(SCP as rguid)=0x%08lX\n", static_cast<unsigned long>(hg));
    if (FAILED(hg) || !fn) {
        // rguid 변형 2: GUID_NULL
        GUID nullGuid = {};
        hg = fp->GetFunction(nullGuid, kIID_ITfFnSearchCandidateProvider, &fn);
        std::printf("    GetFunction(GUID_NULL)=0x%08lX\n", static_cast<unsigned long>(hg));
    }
    if (SUCCEEDED(hg) && fn) {
        auto* q = static_cast<ITfFnSearchCandidateProviderP*>(fn);
        BSTR name = nullptr;
        if (SUCCEEDED(q->GetDisplayName(&name)) && name) {
            std::printf("    DisplayName=%ls\n", name);
            SysFreeString(name);
        }
        ITfCandidateListP* list = nullptr;
        BSTR query = SysAllocString(L"한");
        HRESULT hs = q->GetSearchCandidates(query, nullptr, &list);
        std::printf("    GetSearchCandidates(appId=NULL)=0x%08lX\n",
                    static_cast<unsigned long>(hs));
        if (FAILED(hs)) {
            if (list) { list->Release(); list = nullptr; }
            wchar_t empty[] = L"";
                            BSTR query2 = SysAllocString(L"한");
            hs = q->GetSearchCandidates(query2, empty, &list);
            SysFreeString(query2);
            std::printf("    GetSearchCandidates(appId=\"\")=0x%08lX\n",
                        static_cast<unsigned long>(hs));
        }
        if (SUCCEEDED(hs) && list) DumpList(list, "tsf/han");
        if (list) list->Release();
        SysFreeString(query);
        fn->Release();
    }
    fp->Release();
}

int main() {
    std::printf("=== probe_hanja_dict3 (TSF 경로) ===\n");
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::printf("[0] CoInit=0x%08lX\n", static_cast<unsigned long>(hr));

    ITfThreadMgr* tm = nullptr;
    auto pCreateTm = reinterpret_cast<HRESULT(WINAPI*)(ITfThreadMgr**)>(
        GetProcAddress(LoadLibraryA("msctf.dll"), "TF_CreateThreadMgr"));
    std::printf("[0] TF_CreateThreadMgr=%p\n", reinterpret_cast<void*>(pCreateTm));
    if (pCreateTm && SUCCEEDED(pCreateTm(&tm)) && tm) {
        TfClientId cid = 0;
        std::printf("[1] Initialize=0x%08lX\n", static_cast<unsigned long>(tm->Activate(&cid)));
    }
    if (!tm) { std::printf("NO-GO(측정 불능): ThreadMgr 부재\n"); return 1; }

    // [2] 포커스 없는 상태 — 컨텍스트 없이 프로바이더가 등록되는가
    ProbeProfiles(tm, "nofocus");

    // [3] TSF 정식 포커스 셋업: 실창 + DocumentMgr + Context(NULL 텍스트스토어)
    // + SetFocus — IME 텍스트 서비스가 활성화되면 FunctionProvider가 등록된다.
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.lpszClassName = "jk_probe_hanja3";
    wc.hInstance = GetModuleHandleA(nullptr);
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, "jk_probe_hanja3", "", 0, 0, 0, 10, 10,
                                nullptr, nullptr, wc.hInstance, nullptr);
    std::printf("[3] hwnd=%p\n", static_cast<void*>(hwnd));
    if (hwnd) {
        ShowWindow(hwnd, SW_HIDE);
        ITfDocumentMgr* dm = nullptr;
        if (SUCCEEDED(tm->CreateDocumentMgr(&dm)) && dm) {
            TfClientId cid2 = 0;
            tm->Activate(&cid2);
            ITfContext* ctx = nullptr;
            TfEditCookie ec = 0;
            HRESULT hc = dm->CreateContext(cid2, 0, nullptr, &ctx, &ec);
            std::printf("[3] CreateContext=0x%08lX\n", static_cast<unsigned long>(hc));
            if (SUCCEEDED(hc) && ctx) {
                std::printf("[3] Push=0x%08lX\n",
                            static_cast<unsigned long>(dm->Push(ctx)));
                std::printf("[3] SetFocus=0x%08lX\n",
                            static_cast<unsigned long>(tm->SetFocus(dm)));
                // 입력 프로세서가 문맥에 붙을 시간(메시지 펌프 1회)
                MSG msg;
                while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageA(&msg);
                }
                // [4] 포커스 있는 상태에서 재측정
                ProbeProfiles(tm, "focus");

                // [5] 시스템 전체 FunctionProvider 열거 — 어느 프로바이더도
                // SearchCandidateProvider를 노출하지 않는지 확정.
                IEnumTfFunctionProviders* pen = nullptr;
                if (SUCCEEDED(tm->EnumFunctionProviders(&pen)) && pen) {
                    ITfFunctionProvider* fps[16] = {};
                    ULONG gotf = 0;
                    pen->Next(16, fps, &gotf);
                    std::printf("[5] function providers=%lu\n",
                                static_cast<unsigned long>(gotf));
                    for (ULONG i = 0; i < gotf && i < 16; ++i) {
                        GUID ftype = {};
                        BSTR desc = nullptr;
                        fps[i]->GetType(&ftype);
                        fps[i]->GetDescription(&desc);
                        OLECHAR ft[64] = {};
                        StringFromGUID2(ftype, ft, 64);
                        std::printf("  fp[%lu] type=%ls desc=%ls\n",
                                    static_cast<unsigned long>(i), ft,
                                    desc ? desc : L"(null)");
                        if (desc) SysFreeString(desc);
                        IUnknown* fn = nullptr;
                        HRESULT hgf = fps[i]->GetFunction(
                            kIID_ITfFnSearchCandidateProvider,
                            kIID_ITfFnSearchCandidateProvider, &fn);
                        std::printf("    GetFunction(SCP)=0x%08lX\n",
                                    static_cast<unsigned long>(hgf));
                        if (SUCCEEDED(hgf) && fn) {
                            auto* q = static_cast<ITfFnSearchCandidateProviderP*>(fn);
                            ITfCandidateListP* list = nullptr;
                            BSTR query = SysAllocString(L"한");
                            HRESULT hs = q->GetSearchCandidates(query, nullptr, &list);
                            SysFreeString(query);
                            std::printf("    GetSearchCandidates=0x%08lX\n",
                                        static_cast<unsigned long>(hs));
                            if (SUCCEEDED(hs) && list) DumpList(list, "anyfp/han");
                            if (list) list->Release();
                            fn->Release();
                        }
                        fps[i]->Release();
                    }
                    pen->Release();
                } else {
                    std::printf("[5] EnumFunctionProviders 실패\n");
                }
                ctx->Release();
            }
            dm->Release();
        }
        DestroyWindow(hwnd);
    }

    tm->Release();
    CoUninitialize();
    std::printf("=== TSF 조사 끝 ===\n");
    return 0;
}