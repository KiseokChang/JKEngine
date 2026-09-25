// Desktop vector font (docs/63). STB_TRUETYPE_IMPLEMENTATION lives HERE —
// moved from JKGlyphAtlas.cpp so jkcore-only targets (jkagentd) can still
// link while the client terminal atlas reuses the single definition.
#define STB_TRUETYPE_IMPLEMENTATION

#include <JKTextAtlas.h>
#include <JKResourceCache.h>
#include <agent/JKAgentJson.h>
#include <stb_truetype.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace jk {

namespace {

std::string ExeDir() {
#ifdef _WIN32
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : dir.substr(0, slash + 1);
#else
    return std::string();   // Linux: settings override support arrives with the port (docs/62)
#endif
}

bool FileReadable(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

// exe dir + state\settings.json (docs/54 허브) — font_path/font_fallback 두
// 리졸버가 같은 파일을 직독한다. 파일 부재/파손은 nullptr (기본값 경로).
static std::unique_ptr<jk::agent::AgentJson> LoadDesktopSettingsJson() {
#ifdef _WIN32
    const std::string kvPath = ExeDir() + "state\\settings.json";
#else
    const std::string kvPath = ExeDir() + "state/settings.json";
#endif
    std::FILE* f = std::fopen(kvPath.c_str(), "rb");
    if (!f) return nullptr;
    std::string text;
    char chunk[2048];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) text.append(chunk, n);
    std::fclose(f);
    auto json = std::make_unique<jk::agent::AgentJson>(text);
    if (!json->ok()) return nullptr;
    return json;
}

} // namespace

std::string text::ResolveDesktopFontPath() {
    // 1) settings.json override (docs/54 hub).
    if (auto json = LoadDesktopSettingsJson()) {
        std::string v;
        if (json->GetObjStr("text", "font_path", v) && !v.empty() &&
            v.size() <= 300) {
            return v;   // 오버라이드 존중 — 파손이면 Init 실패로 가시(스펙 fail-safe)
        }
    }
    // 2) platform default (docs/63 §4.1).
#ifdef _WIN32
    return "C:\\Windows\\Fonts\\malgun.ttf";
#else
    static const char* kLinuxCandidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    };
    for (const char* cand : kLinuxCandidates) {
        if (FileReadable(cand)) return cand;
    }
    return std::string();   // Linux + 폰트 부재 = 비트맵 경로 유지
#endif
}

std::string text::ResolveDesktopFallbackPath() {
    // settings 직독, **기본값 없음** — 빈 문자열 = 체인 미설정(1단계
    // ResolveDesktopFontPath의 직독 선례, docs/63 §6 2단계).
    if (auto json = LoadDesktopSettingsJson()) {
        std::string v;
        if (json->GetObjStr("text", "font_fallback", v) && !v.empty() &&
            v.size() <= 300) {
            return v;
        }
    }
    return std::string();
}

jk::text::CellMetrics jk::text::ComputeCellMetrics(float s) {
    // 허용 범위 [1.0, 3.0] 클램프 — settings_set 파싱 단계가 정문 게이트
    // (범위 밖은 bad_value 거부)이고 이 함수는 방어선. 하한 클램프 덕에
    // 0.5 같은 입력도 기본 셀 {8,16,16}으로 수렴한다.
    if (s < 1.0f) s = 1.0f;
    if (s > 3.0f) s = 3.0f;
    // hanW는 engW 유도(2×) — 독자 반올림(lround(16s))하면 소수 scale에서
    // hanW ≠ 2×engW가 돼 JKEdit의 "쌍=2셀×engW" 매핑이 셀당 최대 1px
    // 표류했다(docs/65 O4). 셀 모델의 진실은 "KSSM 쌍 = eng 셀 2개"다.
    // (주의: CellMetrics 필드 순서는 {engW, hanW, cellH} — hanW 슬롯에
    // 높이를 넣지 않도록 초기자 순서를 지킨다.)
    const int engW = std::max(4, static_cast<int>(std::lround(8.0f * s)));
    return CellMetrics{
        engW,
        2 * engW,
        std::max(8, static_cast<int>(std::lround(16.0f * s))),
    };
}

const jk::text::CellMetrics& jk::text::GetCellMetrics() {
    // 함수 로컬 static — C++11 스레드 안전 초기화, **프로세스당 1회** 산출.
    // MeasureText가 static이라 모든 텍스트 경로(MeasureText/TextOut/위젯)가
    // 이 경유하며, 설정 반영 시점은 재시작(스펙: text.font_scale 옵트인).
    static const CellMetrics m = []() -> CellMetrics {
        float s = 1.0f;   // 미설정/파싱 실패/범위 밖 = 기본 1.0 (기각, 픽셀동일)
        if (auto json = LoadDesktopSettingsJson()) {
            std::string v;
            if (json->GetObjStr("text", "font_scale", v) && !v.empty()) {
                char* end = nullptr;
                const double d = std::strtod(v.c_str(), &end);
                // 전체 소비 + 숫자 + 허용 범위만 수용 — "1.5x"·"-2"·"abc"류는
                // 전부 기각해 기본 1.0(비트맵 셀)으로 폴백한다.
                if (end != v.c_str() && *end == '\0' && d >= 1.0 && d <= 3.0) {
                    s = static_cast<float>(d);
                }
            }
        }
        return ComputeCellMetrics(s);
    }();
    return m;
}

JKTextAtlas::JKTextAtlas() = default;
JKTextAtlas::~JKTextAtlas() = default;

int JKTextAtlas::StrideOf(uint32_t cp) const {
    return cp < 0x80 ? engCellW_ : hanCellW_;   // KSSM 전진 규칙과 동일
}

bool JKTextAtlas::Init(const std::string& fontPath, int engCellW, int cellH,
                       int hanCellW) {
    engCellW_ = std::max(4, engCellW);
    hanCellW_ = std::max(8, hanCellW);
    cellH_ = std::max(8, cellH);
    primary_ = Face{};    // 재 Init은 보조 체인까지 해제 — 호스트가 재시도한다
    fallback_ = Face{};
    // 최종리뷰 NIT: registered_.clear()는 추적만 지운다 — 이전 (fg,cp) 글리프
    // 텍스처는 호스트 캐시에 남는다. Init은 cache 핸들을 받지 않아 UnloadImage가
    // 불가하지만, 프로덕션에선 호스트가 Init/InitFallback을 수명당 1회만 호출
    // (설정은 apply_on_restart)해 도달 불가 + CreateImageFromRGBA가 같은 키를
    // 먼저 UnloadImage하므로 재등록 시 자가 치유. 핫스왑 경로 신설 시 정리 필요.
    registered_.clear();
    return LoadFace(fontPath, &primary_, engCellW_, cellH_, hanCellW_);
}

bool JKTextAtlas::InitFallback(const std::string& fontPath) {
    if (fontPath.empty() || primary_.info == nullptr) return false;
    // 재진입 사각 방지(리뷰 NIT): 이전 보조 면의 등록 기록(fb=1 엔트리)을
    // 제거한다 — 남겨두면 GlyphSrc(...,true)가 새 면의 PageKey와 어긋나는
    // 스테일 rect를 보고한다. (1차 엔트리는 보조 면과 무관해 유지.)
    // 위 Init의 NIT 주석과 동일 — fb 텍스처의 캐시 UnloadImage는 없다.
    registered_.erase(std::remove_if(registered_.begin(), registered_.end(),
                                     [](uint64_t k) { return (k & 1u) != 0; }),
                      registered_.end());
    fallback_ = Face{};
    return LoadFace(fontPath, &fallback_, engCellW_, cellH_, hanCellW_);
}

bool JKTextAtlas::LoadFace(const std::string& fontPath, Face* face,
                           int engCellW, int cellH, int hanCellW) {
    face->info.reset();
    face->data.clear();
    face->engScale = face->hanScale = 0.0f;
    face->engBaseline = face->hanBaseline = 0;

    // 폰트 파일 로드 — JKGlyphAtlas::Init와 동일 규약.
    std::FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, fontPath.c_str(), "rb");
#else
    f = std::fopen(fontPath.c_str(), "rb");
#endif
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) { std::fclose(f); return false; }
    face->data.resize(static_cast<size_t>(size));
    const size_t read = std::fread(face->data.data(), 1, face->data.size(), f);
    std::fclose(f);
    if (read != face->data.size()) return false;

    face->info = std::make_unique<stbtt_fontinfo>();
    if (!stbtt_InitFont(face->info.get(), face->data.data(), 0)) {
        face->info.reset();
        return false;
    }

    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(face->info.get(), &ascent, &descent, &lineGap);
    if (ascent - descent <= 0) { face->info.reset(); return false; }

    // 영문 스케일: 'M' 전진 = engCellW (터미널 primary와 동일 레시피).
    int advW = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(face->info.get(), 'M', &advW, &lsb);
    if (advW > 0) {
        face->engScale = static_cast<float>(engCellW) / static_cast<float>(advW);
    } else {
        face->engScale = static_cast<float>(cellH) /
                         static_cast<float>(ascent - descent);
    }
    // 한글 스케일: 음절(0xAC00) 전진 = hanCellW 스트라이드 (InitFallback 레시피).
    // 한글 글리프가 없는 폰트(consola 등)는 전진 0 → 영문 스케일 2배 폴백.
    stbtt_GetCodepointHMetrics(face->info.get(), 0xAC00, &advW, &lsb);
    if (advW > 0) {
        face->hanScale = static_cast<float>(hanCellW) / static_cast<float>(advW);
    } else {
        face->hanScale = face->engScale * 2.0f;
    }
    // em 박스가 셀을 넘으면 축소 (맑은 고딕이 여기 걸린다 — InitFallback 코멘트).
    float emPx = static_cast<float>(ascent - descent) * face->hanScale;
    if (emPx > static_cast<float>(cellH)) {
        face->hanScale *= static_cast<float>(cellH) / emPx;
        emPx = static_cast<float>(ascent - descent) * face->hanScale;
    }
    face->hanBaseline = static_cast<int>((static_cast<float>(cellH) - emPx) * 0.5f +
                                         static_cast<float>(ascent) * face->hanScale);
    const float emPxEng = static_cast<float>(ascent - descent) * face->engScale;
    face->engBaseline = static_cast<int>(
        (static_cast<float>(cellH) - emPxEng) * 0.5f +
        static_cast<float>(ascent) * face->engScale);
    return true;
}

bool JKTextAtlas::RasterizeGlyph(uint32_t fg, uint32_t cp,
                                 std::vector<uint8_t>* rgba,
                                 bool useFallbackPage) {
    const Face* face = useFallbackPage ? &fallback_ : &primary_;
    if (face->info == nullptr) return false;
    const int stride = StrideOf(cp);
    const float scale = cp < 0x80 ? face->engScale : face->hanScale;
    const int baseline = cp < 0x80 ? face->engBaseline : face->hanBaseline;

    // 폰트에 글리프 없으면 false — stb는 미커버 cp를 글리프 0(.notdef)로
    // 돌려 tofu 박스로 래스터라이즈하므로, 먼저 커버리지를 본다(폴백 계약).
    // useFallbackPage=true 경로는 1차 EnsureGlyph가 실패한 뒤에만 온다
    // (JKDC 체인) — 보조 폰트도 미커버면 false = 비트맵 폴백.
    if (cp != 0x20 &&
        stbtt_FindGlyphIndex(face->info.get(), static_cast<int>(cp)) == 0) {
        return false;
    }
    int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
    stbtt_GetCodepointBitmapBox(face->info.get(), static_cast<int>(cp),
                                scale, scale, &ix0, &iy0, &ix1, &iy1);
    int bw = ix1 - ix0;
    int bh = iy1 - iy0;
    rgba->assign(static_cast<size_t>(stride) * cellH_ * 4, 0);
    if (bw <= 0 || bh <= 0) {
        // 폰트에 글리프 없음(공백 포함 — 공백은 잉크 0으로 "있음"이 맞다).
        // cp==0x20은 성공(빈 글리프), 그 외 잉크 없음도 좌표만으로 등록 허용.
        return cp == 0x20;
    }
    bw = std::min(bw, stride);
    bh = std::min(bh, cellH_);
    std::vector<uint8_t> cov(static_cast<size_t>(bw) * bh, 0);
    stbtt_MakeCodepointBitmap(face->info.get(), cov.data(), bw, bh, bw,
                              scale, scale, static_cast<int>(cp));

    const uint8_t cr = static_cast<uint8_t>((fg >> 16) & 0xFF);
    const uint8_t cg = static_cast<uint8_t>((fg >> 8) & 0xFF);
    const uint8_t cb = static_cast<uint8_t>(fg & 0xFF);
    // 왼쪽 위 음수 시작은 잘라낸다(1단계: 클립 허용 — 시프트보다 단순).
    const int px0 = std::max(0, ix0);
    const int py0 = std::max(0, baseline + iy0);
    for (int y = 0; y < bh; ++y) {
        const int py = py0 + y;
        if (py >= cellH_) break;
        for (int x = 0; x < bw; ++x) {
            const int px = px0 + x;
            if (px >= stride) break;
            const uint8_t a = cov[static_cast<size_t>(y) * bw + x];
            if (a == 0) continue;
            uint8_t* dst = &(*rgba)[(static_cast<size_t>(py) * stride + px) * 4];
            dst[0] = cr;
            dst[1] = cg;
            dst[2] = cb;
            dst[3] = a;
        }
    }
    return true;
}

bool JKTextAtlas::RasterizeGlyphForTest(uint32_t fg, uint32_t cp,
                                        std::vector<uint8_t>* rgba,
                                        int* w, int* h, bool useFallbackPage) {
    if (useFallbackPage ? fallback_.info == nullptr : primary_.info == nullptr) {
        return false;
    }
    if (!RasterizeGlyph(fg, cp, rgba, useFallbackPage)) return false;
    if (w) *w = StrideOf(cp);
    if (h) *h = cellH_;
    return true;
}

std::string JKTextAtlas::PageKey(uint32_t fg, uint32_t cp,
                                 bool useFallbackPage) const {
    char buf[32];
    // 보조 폰트 페이지는 별도 접두어 — 같은 (fg,cp)의 1차 텍스처와 충돌 봉쇄.
    std::snprintf(buf, sizeof(buf),
                  useFallbackPage ? "desktextf_%08x_%06x"
                                  : "desktext_%08x_%06x",
                  static_cast<unsigned>(fg), cp);
    return buf;
}

JKRect JKTextAtlas::GlyphSrc(uint32_t fg, uint32_t cp,
                             bool useFallbackPage) const {
    const uint64_t key = (static_cast<uint64_t>(fg) << 34) |
                         (static_cast<uint64_t>(cp) << 1) |
                         (useFallbackPage ? 1u : 0u);
    for (uint64_t r : registered_) {
        if (r == key) return JKRect{ 0, 0, StrideOf(cp), cellH_ };
    }
    return JKRect{ 0, 0, 0, 0 };
}

bool JKTextAtlas::EnsureGlyph(JKResourceCache* cache, uint32_t fg, uint32_t cp,
                              bool useFallbackPage) {
    if (!cache) return false;
    if (useFallbackPage ? fallback_.info == nullptr : primary_.info == nullptr) {
        return false;
    }
    const uint64_t key = (static_cast<uint64_t>(fg) << 34) |
                         (static_cast<uint64_t>(cp) << 1) |
                         (useFallbackPage ? 1u : 0u);
    for (size_t i = 0; i < registered_.size(); ++i) {
        if (registered_[i] == key) {
            // LRU 터치: 기존 히트는 MRU(끝)로 이동 — 폐기 순서가 실사용을 따른다.
            if (i + 1 != registered_.size()) {
                registered_.erase(registered_.begin() + static_cast<ptrdiff_t>(i));
                registered_.push_back(key);
            }
            return true;
        }
    }
    std::vector<uint8_t> rgba;
    if (!RasterizeGlyph(fg, cp, &rgba, useFallbackPage)) return false;
    const std::string cacheKey = PageKey(fg, cp, useFallbackPage);
    if (!cache->CreateImageFromRGBA(cacheKey, StrideOf(cp), cellH_, rgba)) {
        return false;
    }
    registered_.push_back(key);
    // 상한(docs/63 §6): 초과분은 가장 오래된(최근 미사용) 글리프부터 폐기 —
    // 필요 시 같은 (fg,cp,face) 재요청이 다시 등록한다.
    while (registered_.size() > kMaxGlyphTextures) {
        EvictOldest(cache);
    }
    return true;
}

void JKTextAtlas::EvictOldest(JKResourceCache* cache) {
    if (!cache || registered_.empty()) return;
    const uint64_t key = registered_.front();
    const uint32_t fg = static_cast<uint32_t>(key >> 34);
    // uint32 캐스트가 fg 고위 비트(fg<<33는 비트 33..)를 절단하고 fb는 >>1로
    // 떨어진다 — 마스크 불요(리뷰 NIT; 마스크는 cp 비트 31을 잘랐다).
    const uint32_t cp = static_cast<uint32_t>(key >> 1);
    const bool fb = (key & 1u) != 0;
    cache->UnloadImage(PageKey(fg, cp, fb));
    registered_.erase(registered_.begin());
}

} // namespace jk