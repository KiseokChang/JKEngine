// JKHanjaDict — 한자 사전 채널 격리 모듈 (docs/66 O1 Phase B1).
//
// 시스템 채널은 한국어 IME 자체 한자 사전 COM 서버(imkrhjd.dll, IHanjaDic)다 —
// 구판 IMM(ImmGetConversionList)과 현판 TSF(SearchCandidateProvider)는 신형
// 한국어 IME에서 실측 사망(docs/66 §2-§3). 인코딩 파이프라인:
//   조회: KSSM 쌍 → KssmCodepointToUnicode → COM(Unicode 1자)
//   후보: BSTR UTF-16 1자 → UTF-8 → Utf8ToKssm — 쌍 복원 불가(? 치환) 후보는
//         렌더 불가 한계와 일치시켜 탈락(docs/66 위험 2).
// COM STA + 사전 캐시 모두 UI 스레드 전용 — 헤더 계약 참조.
#include "JKHanjaDict.h"

#include "JKHangulUtil.h"

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#include <string>
#include <unordered_map>

namespace jk::hanja {
namespace {

// 타입라이브러리 덤프로 확정한 값·서명(docs/66 §3.5). MinGW 헤더에
// IHanjaDic이 없어 수동 선언 — vtable 순서는 IUnknown(3) + OpenMainDic +
// CloseMainDic + GetHanjaWords + GetHanjaChars(조회에 쓰는 슬롯만 선언,
// 위치는 표 전체 순서와 동일하다).
const CLSID kHanjaDicClsid = { 0x4c870c20, 0x4ed3, 0x4235,
                               { 0x9a, 0x2a, 0x71, 0x85, 0xf8, 0xa8, 0x7d, 0x06 } };
const IID kIHanjaDicIid = { 0xad75f3ac, 0x18cd, 0x48c6,
                            { 0xa2, 0x7d, 0xf1, 0xe9, 0xa7, 0xdc, 0xe4, 0x32 } };

struct IHanjaDic : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE OpenMainDic() = 0;
    virtual HRESULT STDMETHODCALLTYPE CloseMainDic() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHanjaWords(BSTR bstrHangulWord,
                                                    SAFEARRAY** ppsaHanjaWords,
                                                    VARIANT_BOOL* pvRet) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHanjaChars(USHORT wHangulChar,
                                                    BSTR* pbstrHanjaChars,
                                                    VARIANT_BOOL* pvRet) = 0;
};

struct State {
    Provider testProvider = nullptr;
    bool initialized = false;
    bool available = false;
    IHanjaDic* dic = nullptr;
    std::unordered_map<uint16_t, std::vector<uint16_t>> cache;

    ~State() {
        if (dic) dic->Release();
    }
};

State& S() {
    static State s;
    return s;
}

// UTF-16(BMP 1자) → UTF-8. 사전 후보는 KS 한자(BMP CJK) 도메인 — 서러게이트는
// 나오지 않는 것으로 문서화한다.
int Utf16ToUtf8(wchar_t wc, char out[4]) {
    int len = 0;
    if (wc < 0x80) {
        out[len++] = static_cast<char>(wc);
    } else if (wc < 0x800) {
        out[len++] = static_cast<char>(0xC0 | (wc >> 6));
        out[len++] = static_cast<char>(0x80 | (wc & 0x3F));
    } else {
        out[len++] = static_cast<char>(0xE0 | (wc >> 12));
        out[len++] = static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
        out[len++] = static_cast<char>(0x80 | (wc & 0x3F));
    }
    return len;
}

// 캐시 포함 시스템 채널 조회. EnsureInit의 채질도 쓰므로 초기화를 다시
// 요구하지 않는다(재귀 방지 — dic 멤버를 직접 쓴다).
bool QueryCom(uint16_t syllable, std::vector<uint16_t>* out) {
    out->clear();
    auto it = S().cache.find(syllable);
    if (it != S().cache.end()) {
        *out = it->second;
        return !out->empty();
    }
    // KSSM 쌍 → 유니코드 음절 코드. 매핑 없는 쌍은 후보 없음.
    const uint32_t cp = KssmCodepointToUnicode(static_cast<uint8_t>(syllable >> 8),
                                               static_cast<uint8_t>(syllable & 0xFF));
    std::vector<uint16_t> cands;
    if (cp != 0 && cp <= 0xFFFF && S().dic) {
        BSTR b = nullptr;
        VARIANT_BOOL ok = VARIANT_FALSE;
        if (SUCCEEDED(S().dic->GetHanjaChars(static_cast<USHORT>(cp), &b, &ok)) &&
            ok != VARIANT_FALSE && b) {
            const UINT n = SysStringLen(b);
            for (UINT i = 0; i < n; ++i) {
                char utf8[4] = {};
                const int len = Utf16ToUtf8(b[i], utf8);
                utf8[len] = '\0';
                // 한자 1자 → KSSM 쌍. 표 밖 글자는 '?' 1바이트로 치환되므로
                // 쌍(2바이트)만 후보로 남긴다.
                const std::string kssm = Utf8ToKssm(utf8);
                if (kssm.size() == 2)
                    cands.push_back(static_cast<uint16_t>((static_cast<uint16_t>(
                                            static_cast<uint8_t>(kssm[0])))
                                                          << 8 |
                                       static_cast<uint8_t>(kssm[1])));
            }
            SysFreeString(b);
        }
    }
    S().cache.emplace(syllable, cands);
    *out = cands;
    return !cands.empty();
}

bool EnsureInit() {
    State& s = S();
    if (s.initialized) return s.available;
    s.initialized = true;
    // COM STA. 이미 다른 모드로 초기화된 스레드(CHANGED_MODE)에서도 시도 —
    // in-proc 서버는 어차피 같은 스레드에서 호출된다.
    const HRESULT hco = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hco) && hco != RPC_E_CHANGED_MODE) return false;
    HRESULT hr = CoCreateInstance(kHanjaDicClsid, nullptr, CLSCTX_INPROC_SERVER,
                                  kIHanjaDicIid, reinterpret_cast<void**>(&s.dic));
    if (FAILED(hr) || !s.dic) return false;
    if (FAILED(s.dic->OpenMainDic())) return false;
    // 채질 1회: '한'(KSSM D065) — 프로브 실측과 동일 기준(docs/66 §3.5).
    std::vector<uint16_t> probe;
    s.available = QueryCom(0xD065, &probe);
    return s.available;
}

} // namespace

bool Candidates(uint16_t kssmSyllable, std::vector<uint16_t>* out) {
    if (!out) return false;
    out->clear();
    if (S().testProvider) return S().testProvider(kssmSyllable, out);
    if (!EnsureInit()) return false;
    return QueryCom(kssmSyllable, out);
}

bool Available() {
    if (S().testProvider) return true;
    return EnsureInit();
}

void SetProviderForTest(Provider p) {
    S().testProvider = p;
}

} // namespace jk::hanja

#else // !_WIN32

namespace jk::hanja {

// 비윈도우 플랫폼: 시스템 사전 채널이 없다 — 전 경로 graceful no-op
// (테스트 주입 프로바이더만 유효).
bool Candidates(uint16_t kssmSyllable, std::vector<uint16_t>* out) {
    if (!out) return false;
    out->clear();
    return false;
}

bool Available() {
    return false;
}

void SetProviderForTest(Provider p) {
    (void)p;
}

} // namespace jk::hanja

#endif // _WIN32