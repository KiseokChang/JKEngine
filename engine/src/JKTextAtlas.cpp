// Desktop vector font (docs/63). STB_TRUETYPE_IMPLEMENTATION lives HERE —
// moved from JKGlyphAtlas.cpp so jkcore-only targets (jkagentd) can still
// link while the client terminal atlas reuses the single definition.
#define STB_TRUETYPE_IMPLEMENTATION

#include <JKTextAtlas.h>
#include <JKResourceCache.h>
#include <agent/JKAgentJson.h>
#include <stb_truetype.h>
#include <algorithm>
#include <cstdio>

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

} // namespace

std::string text::ResolveDesktopFontPath() {
    // 1) settings.json override (docs/54 hub): exe dir + state\settings.json.
#ifdef _WIN32
    const std::string kvPath = ExeDir() + "state\\settings.json";
#else
    const std::string kvPath = ExeDir() + "state/settings.json";
#endif
    std::FILE* f = std::fopen(kvPath.c_str(), "rb");
    if (f) {
        std::string text;
        char chunk[2048];
        size_t n;
        while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) text.append(chunk, n);
        std::fclose(f);
        jk::agent::AgentJson json(text);
        std::string v;
        if (json.ok() && json.GetObjStr("text", "font_path", v) && !v.empty() &&
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

JKTextAtlas::JKTextAtlas() = default;
JKTextAtlas::~JKTextAtlas() = default;

int JKTextAtlas::StrideOf(uint32_t cp) const {
    return cp < 0x80 ? engCellW_ : hanCellW_;   // KSSM 전진 규칙과 동일
}
float JKTextAtlas::ScaleOf(uint32_t cp) const {
    return cp < 0x80 ? engScale_ : hanScale_;
}
int JKTextAtlas::BaselineOf(uint32_t cp) const {
    return cp < 0x80 ? engBaseline_ : hanBaseline_;
}

bool JKTextAtlas::Init(const std::string& fontPath, int engCellW, int cellH,
                       int hanCellW) {
    engCellW_ = std::max(4, engCellW);
    hanCellW_ = std::max(8, hanCellW);
    cellH_ = std::max(8, cellH);
    info_.reset();
    registered_.clear();

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
    fontData_.resize(static_cast<size_t>(size));
    const size_t read = std::fread(fontData_.data(), 1, fontData_.size(), f);
    std::fclose(f);
    if (read != fontData_.size()) return false;

    info_ = std::make_unique<stbtt_fontinfo>();
    if (!stbtt_InitFont(info_.get(), fontData_.data(), 0)) {
        info_.reset();
        return false;
    }

    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(info_.get(), &ascent, &descent, &lineGap);
    if (ascent - descent <= 0) { info_.reset(); return false; }

    // 영문 스케일: 'M' 전진 = engCellW_ (터미널 primary와 동일 레시피).
    int advW = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(info_.get(), 'M', &advW, &lsb);
    if (advW > 0) {
        engScale_ = static_cast<float>(engCellW_) / static_cast<float>(advW);
    } else {
        engScale_ = static_cast<float>(cellH_) /
                    static_cast<float>(ascent - descent);
    }
    // 한글 스케일: 음절(0xAC00) 전진 = hanCellW_ 스트라이드 (InitFallback 레시피).
    stbtt_GetCodepointHMetrics(info_.get(), 0xAC00, &advW, &lsb);
    if (advW > 0) {
        hanScale_ = static_cast<float>(hanCellW_) / static_cast<float>(advW);
    } else {
        hanScale_ = engScale_ * 2.0f;
    }
    // em 박스가 셀을 넘으면 축소 (맑은 고딕이 여기 걸린다 — InitFallback 코멘트).
    float emPx = static_cast<float>(ascent - descent) * hanScale_;
    if (emPx > static_cast<float>(cellH_)) {
        hanScale_ *= static_cast<float>(cellH_) / emPx;
        emPx = static_cast<float>(ascent - descent) * hanScale_;
    }
    hanBaseline_ = static_cast<int>((static_cast<float>(cellH_) - emPx) * 0.5f +
                                    static_cast<float>(ascent) * hanScale_);
    const float emPxEng = static_cast<float>(ascent - descent) * engScale_;
    engBaseline_ = static_cast<int>(
        (static_cast<float>(cellH_) - emPxEng) * 0.5f +
        static_cast<float>(ascent) * engScale_);
    return true;
}

bool JKTextAtlas::RasterizeGlyph(uint32_t fg, uint32_t cp,
                                 std::vector<uint8_t>* rgba) {
    if (!info_) return false;
    const int stride = StrideOf(cp);
    const float scale = ScaleOf(cp);

    // 폰트에 글리프 없으면 false — stb는 미커버 cp를 글리프 0(.notdef)로
    // 돌려 tofu 박스로 래스터라이즈하므로, 먼저 커버리지를 본다(폴백 계약).
    if (cp != 0x20 && stbtt_FindGlyphIndex(info_.get(), static_cast<int>(cp)) == 0) {
        return false;
    }
    int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
    stbtt_GetCodepointBitmapBox(info_.get(), static_cast<int>(cp),
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
    stbtt_MakeCodepointBitmap(info_.get(), cov.data(), bw, bh, bw,
                              scale, scale, static_cast<int>(cp));

    const uint8_t cr = static_cast<uint8_t>((fg >> 16) & 0xFF);
    const uint8_t cg = static_cast<uint8_t>((fg >> 8) & 0xFF);
    const uint8_t cb = static_cast<uint8_t>(fg & 0xFF);
    // 왼쪽 위 음수 시작은 잘라낸다(1단계: 클립 허용 — 시프트보다 단순).
    const int px0 = std::max(0, ix0);
    const int py0 = std::max(0, BaselineOf(cp) + iy0);
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
                                        int* w, int* h) {
    if (!info_) return false;
    if (!RasterizeGlyph(fg, cp, rgba)) return false;
    if (w) *w = StrideOf(cp);
    if (h) *h = cellH_;
    return true;
}

std::string JKTextAtlas::PageKey(uint32_t fg, uint32_t cp) const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "desktext_%08x_%06x",
                  static_cast<unsigned>(fg), cp);
    return buf;
}

JKRect JKTextAtlas::GlyphSrc(uint32_t fg, uint32_t cp) const {
    const uint64_t key = (static_cast<uint64_t>(fg) << 32) | cp;
    for (uint64_t r : registered_) {
        if (r == key) return JKRect{ 0, 0, StrideOf(cp), cellH_ };
    }
    return JKRect{ 0, 0, 0, 0 };
}

bool JKTextAtlas::EnsureGlyph(JKResourceCache* cache, uint32_t fg, uint32_t cp) {
    if (!cache || !info_) return false;
    const uint64_t key = (static_cast<uint64_t>(fg) << 32) | cp;
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
    if (!RasterizeGlyph(fg, cp, &rgba)) return false;
    const std::string cacheKey = PageKey(fg, cp);
    if (!cache->CreateImageFromRGBA(cacheKey, StrideOf(cp), cellH_, rgba)) {
        return false;
    }
    registered_.push_back(key);
    // 상한(docs/63 §6): 초과분은 가장 오래된(최근 미사용) 글리프부터 폐기 —
    // 필요 시 같은 (fg,cp) 재요청이 다시 등록한다.
    while (registered_.size() > kMaxGlyphTextures) {
        EvictOldest(cache);
    }
    return true;
}

void JKTextAtlas::EvictOldest(JKResourceCache* cache) {
    if (!cache || registered_.empty()) return;
    const uint64_t key = registered_.front();
    const uint32_t fg = static_cast<uint32_t>(key >> 32);
    const uint32_t cp = static_cast<uint32_t>(key & 0xFFFFFFFFu);
    cache->UnloadImage(PageKey(fg, cp));
    registered_.erase(registered_.begin());
}

} // namespace jk