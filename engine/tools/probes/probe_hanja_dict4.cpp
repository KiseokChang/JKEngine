// probe_hanja_dict4 — Phase A 보충 3: imkrapi.dll (ImeCommonAPI_KOR) TypeLib 덤프.
// 한국 IME가 자체 노출하는 COM 변환 API 채널 측정 — 인터페이스/메서드/이산형 열거.
#include <windows.h>
#include <oleauto.h>
#include <comdef.h>
#include <cstdint>
#include <cstdio>

static void DumpInterface(ITypeInfo* ti, const TYPEATTR* attr) {
    BSTR name = nullptr, doc = nullptr;
    ti->GetDocumentation(-1, &name, &doc, nullptr, nullptr);
    OLECHAR g[64] = {};
    StringFromGUID2(attr->guid, g, 64);
    std::printf("  iface %ls kind=%d guid=%ls funcs=%d\n", name ? name : L"?",
                static_cast<int>(attr->typekind), g,
                static_cast<int>(attr->cFuncs));
    if (doc) SysFreeString(doc);
    for (int f = 0; f < attr->cFuncs; ++f) {
        FUNCDESC* fd = nullptr;
        if (FAILED(ti->GetFuncDesc(f, &fd)) || !fd) continue;
        BSTR fn = nullptr;
        ti->GetDocumentation(fd->memid, &fn, nullptr, nullptr, nullptr);
        std::printf("    [%2d] %ls vtableOff=%d params=%d ret=%d\n", f, fn ? fn : L"?",
                    static_cast<int>(fd->oVft),
                    static_cast<int>(fd->cParams),
                    static_cast<int>(fd->elemdescFunc.tdesc.vt));
        const ELEMDESC* es = fd->lprgelemdescParam;
        for (short p = 0; es && p < fd->cParams; ++p) {
            const TYPEDESC& td = es[p].tdesc;
            int vt = td.vt;
            int vt2 = -1;
            if (vt == VT_PTR || vt == VT_SAFEARRAY) {
                auto* lptd = static_cast<TYPEDESC*>(td.lptdesc);
                if (lptd) vt2 = lptd->vt;
            }
            std::printf("      p%d vt=%d%s%d flags=0x%X\n", p, vt,
                        vt2 >= 0 ? "->" : "", vt2,
                        static_cast<unsigned>(es[p].paramdesc.wParamFlags));
        }
        if (fn) SysFreeString(fn);
        ti->ReleaseFuncDesc(fd);
    }
    if (name) SysFreeString(name);
}

static void DumpEnum(ITypeInfo* ti, const TYPEATTR* attr) {
    BSTR name = nullptr;
    ti->GetDocumentation(-1, &name, nullptr, nullptr, nullptr);
    std::printf("  enum %ls (%d vars)\n", name ? name : L"?", static_cast<int>(attr->cVars));
    for (int v = 0; v < attr->cVars; ++v) {
        VARDESC* vd = nullptr;
        if (FAILED(ti->GetVarDesc(v, &vd)) || !vd) continue;
        BSTR vn = nullptr;
        ti->GetDocumentation(vd->memid, &vn, nullptr, nullptr, nullptr);
        if (vd->lpvarValue) {
            VARIANT* val = static_cast<VARIANT*>(vd->lpvarValue);
            if (val->vt == VT_I4)
                std::printf("    %ls = %ld (0x%lX)\n", vn ? vn : L"?",
                            static_cast<long>(val->lVal),
                            static_cast<unsigned long>(val->lVal));
        }
        if (vn) SysFreeString(vn);
        ti->ReleaseVarDesc(vd);
    }
    if (name) SysFreeString(name);
}

int main(int argc, char** argv) {
    const char* dllPath = argc > 1 ? argv[1] : "C:\\Windows\\System32\\IME\\IMEKR\\imkrapi.dll";
    std::printf("=== probe_hanja_dict4 (TypeLib 덤프: %s) ===\n", dllPath);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    wchar_t wide[512] = {};
    MultiByteToWideChar(CP_ACP, 0, dllPath, -1, wide, 512);
    ITypeLib* tl = nullptr;
    HRESULT hr = LoadTypeLibEx(wide, REGKIND_NONE, &tl);
    std::printf("LoadTypeLibEx=0x%08lX\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !tl) return 1;
    UINT n = tl->GetTypeInfoCount();
    std::printf("types=%u\n", n);
    for (UINT i = 0; i < n; ++i) {
        ITypeInfo* ti = nullptr;
        if (FAILED(tl->GetTypeInfo(i, &ti)) || !ti) continue;
        TYPEATTR* attr = nullptr;
        if (SUCCEEDED(ti->GetTypeAttr(&attr)) && attr) {
            if (attr->typekind == TKIND_INTERFACE || attr->typekind == TKIND_DISPATCH)
                DumpInterface(ti, attr);
            else if (attr->typekind == TKIND_ENUM)
                DumpEnum(ti, attr);
            else {
                BSTR name = nullptr;
                tl->GetDocumentation(i, &name, nullptr, nullptr, nullptr);
                std::printf("  type %ls kind=%d\n", name ? name : L"?",
                            static_cast<int>(attr->typekind));
                if (name) SysFreeString(name);
            }
            ti->ReleaseTypeAttr(attr);
        }
        ti->Release();
    }
    tl->Release();
    CoUninitialize();
    std::printf("=== 덤프 끝 ===\n");
    return 0;
}