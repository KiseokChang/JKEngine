#ifndef GALLERYMODEL_H
#define GALLERYMODEL_H

// Gallery app model (spec 2026-10-09-gallery-design, T1): the pure parts of
// the photo hub — source-dir resolver over settings.json "gallery.dirs", dir
// enumeration, thumbnail fit arithmetic, and the FNV-1a cache key used by the
// T3 thumbnail disk cache (state/gallery/thumbs, filename-hash key).
//
// Header-inline on purpose: the selftest twins (engine/src/main.cpp selftest
// 2g + engine/tools/posix_selftest/main.cpp) assert these parts by direct
// link with no extra TU, and T3's texture pool consumes the same functions.
// This header must stay free of imgui/SDL/client types.

#include <agent/JKAgentJson.h>
#include <JKImageLoader.h>  // T3 MakeThumb/캐시 파이프라인 소비 — LoadedImage
                            // (plain struct — imgui/SDL/client 타입 무접촉 계약 유지)
#include <port/JKCrtShim.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace jk {
namespace gallery {

// Fixed grid cell geometry (T1 brief): thumbnail box 160x120 + one label row
// below. T3 draws the decoded thumb inside the box; T1 draws the placeholder.
inline constexpr int kCellW = 160;
inline constexpr int kCellH = 120;
// T3 in-memory thumb texture pool cap (LRU) — 스크롤 스코프 밖 재조명 재인코딩
// 방지(텍스처 재생성 무깜빡 계약). 실보이는 셀 수(격자 한 화면)보다 넉넉히
// 큰 상한이라 상한 초과는 수백 장 격자에서만 성립한다.
inline constexpr int kThumbPoolMax = 96;
// user dirs 상한 — settings 폭주 절단(방어선; 실사용은 1-3건이 현실).
inline constexpr int kMaxUserDirs = 64;

// FNV-1a 32-bit (T3 thumb disk-cache key: state/gallery/thumbs/<hash>.png).
// 결정론성 계약 — 같은 문자열은 축 무관(WSL/폰/Win) 같은 해시여야 재부팅
// 재조명 방지 캐시가 성립한다(스펙 §결정 2의 캐시 전제).
inline uint32_t Fnv1a(const std::string& s) {
    uint32_t h = 2166136261u;  // FNV-1a offset basis
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;        // FNV-1a prime
    }
    return h;
}

// Fit a w*h image into a maxW*maxH box, integer-free float result.
// s = min(maxW/w, maxH/h) 핏 배율 — 극단 종횡비(세로 4배·가로 3배)는
// 대소 관계 상관없이 min 축이 지배한다. s=1.0=원본(확대 금지 — 썸네일이
// 원본보다 커지는 왜곡 방지). 퇴화 입력(0 이하)은 {0,0} (호출부 무표시).
struct FitSize {
    float w = 0.f;
    float h = 0.f;
};
inline FitSize FitThumb(int w, int h, float maxW, float maxH) {
    if (w <= 0 || h <= 0 || maxW <= 0.f || maxH <= 0.f) return FitSize();
    float s = std::min(maxW / static_cast<float>(w), maxH / static_cast<float>(h));
    if (s > 1.0f) s = 1.0f;
    return FitSize{ static_cast<float>(w) * s, static_cast<float>(h) * s };
}

// Full-view fit (T2 brief 2g-f): s = min(vpW/w, vpH/h) — the pure ratio part
// of the full-view display (픽셀 정합). 썸네일 산치(2g-c)와 달리 s=1.0 상한이
// 없다 — 전체 보기는 뷰포트 정합(작은 원본 확대 포함; 사용자 확대·팬 조작은
// v1 백로그 YAGNI, shot의 표시 수형과 동형). 화면 배율(font_scale/scale
// 상태)과 전혀 무관한 순수 비율 계산 — fit-scale 함정 원장 존중: 렌더는 이
// 결과를 ImGui::Image 스케일 그대로 쓴다. 퇴화 입력(0 이하)은 {0,0} (호출부
// 무표시).
inline FitSize FitFull(int w, int h, float vpW, float vpH) {
    if (w <= 0 || h <= 0 || vpW <= 0.f || vpH <= 0.f) return FitSize();
    const float s = std::min(vpW / static_cast<float>(w),
                             vpH / static_cast<float>(h));
    return FitSize{ static_cast<float>(w) * s, static_cast<float>(h) * s };
}

// Full-view prev/next with wrap-around (T2 brief): 끝 지점에서 순환 — index에
// delta(±1=이전/다음)를 더한 뒤 count로 정규화한다. count<=0(빈 목록)은 0을
// 돌려주고 아무것도 하지 않는다(호출부 no-op 계약).
inline int WrapStep(int index, int delta, int count) {
    if (count <= 0) return 0;
    int i = index % count;
    if (i < 0) i += count;
    int r = (i + delta) % count;
    if (r < 0) r += count;
    return r;
}

// Path normalization (selftest 2g-b): generic separator form ('\\'→'/' — Win
// 원문과 폰/WSL의 '/' 쓰기를 한 표기로 묶는다), duplicate trailing separators
// stripped, and empty components ("", "/", "//") dropped. Dedupe is byte-exact
// keeping the first occurrence — case-insensitive dedupe would wrongly fold
// distinct entries on the case-sensitive phone/WSL filesystems, so v1 honors
// the settings author's spelling.
inline std::vector<std::string> NormalizeDirs(std::vector<std::string> dirs) {
    std::vector<std::string> out;
    out.reserve(dirs.size());
    for (std::string& raw : dirs) {
        // fs::path 정규형(generic_string) — '\'→'/' (Win 원문과 폰/WSL의 '/'
        // 쓰기를 한 표기로 묶는다). 부재 경로도 path 구성은 실패하지 않는다.
        std::string p = std::filesystem::path(raw).generic_string();
        // 뒤 구분자 중복 — 몇 겹이어도 한 번에 벗긴다.
        while (!p.empty() && p.back() == '/') p.pop_back();
        if (p.empty()) continue;  // 빈 성분 제거(원래 빈값·"/"·"//" 전부)
        if (std::find(out.begin(), out.end(), p) == out.end())
            out.push_back(p);
    }
    return out;
}

// settings.json "gallery"."dirs" — a JSON array of path strings (spec 결정 2;
// 폰에서는 /sdcard/Pictures 등을 폰 settings에 직접 기록해 켠다). 비문자열
// 성분은 스킵, 부재/파손/빈 배열 = 빈 목록(리졸버가 기본 폴더만 남긴다 —
// fail-safe 계약).
// AgentJson의 배열 리더는 root가 **객체**일 때만 열리므로(GetArraySize/
// GetArrValStr 계약) GetObjRaw의 raw 배열 "[..]"를 {"dirs":raw} 래퍼로 감싸
// 재파싱한다 — 원문 보존(파손 재해석 없이 문자열 그대로 소비).
inline std::vector<std::string> DirsFromSettings(const std::string& settingsText) {
    std::vector<std::string> out;
    if (settingsText.empty()) return out;
    const agent::AgentJson root(settingsText);
    if (!root.ok()) return out;
    std::string raw;
    if (!root.GetObjRaw("gallery", "dirs", raw)) return out;
    const agent::AgentJson arr("{\"dirs\":" + raw + "}");
    if (!arr.ok()) return out;
    int n = 0;
    if (!arr.GetArraySize("dirs", n) || n <= 0) return out;
    if (n > kMaxUserDirs) n = kMaxUserDirs;  // 폭주 절단
    for (int i = 0; i < n; ++i) {
        std::string v;
        if (arr.GetArrValStr("dirs", i, v) && !v.empty())
            out.push_back(std::move(v));
    }
    return out;
}

// Source-dir resolver (selftest 2g-a) — the app's single truth for which
// folders the gallery scans. Pure: no file I/O here; the caller hands in the
// raw settings.json text it read from disk. Order contract: the default
// capture dir <exeDir>/state/screenshots (shot과 같은 원전, 읽기 전용) first,
// then user dirs in settings order, duplicates and empty components removed
// (NormalizeDirs). settings 부재/파손 = 기본 1건(fail-safe).
inline std::vector<std::string> GalleryDirList(const std::string& exeDir,
                                               const std::string& settingsText) {
    std::vector<std::string> dirs;
    // fs::path 합성 + generic_string — exeDir 끝 구분자 유무 무관, 폰/WSL의
    // '/' 표기와 같은 형태로 합쳐진다. exeDir가 빈값(jk::fs 계약상 실패)이면
    // 상대경로 "state/screenshots"가 남는다 — 여전히 1건 기본(fail-safe).
    dirs.push_back((std::filesystem::path(exeDir) / "state" / "screenshots")
                       .generic_string());
    for (std::string& user : DirsFromSettings(settingsText))
        dirs.push_back(std::move(user));
    return NormalizeDirs(std::move(dirs));
}

// ---- T3 thumbnail pipeline (brief 2026-10-09-gallery/task-3) ----

// 파일 스탬프 (T3 캐시 키 원료) — ec 중립형(last_write_time·file_size의
// error_code 오버로드만, throwing 오버로드 금지 — 2g-e 계약 승계). 부재/읽기
// 실패 = ok false (호출부는 키를 못 세우므로 실패 슬롯으로 열외).
struct ThumbStamp {
    bool ok = false;
    long long size = 0;    // file_size (byte)
    long long mtime = 0;   // last_write_time의 time_since_epoch().count() 정수열
};
inline ThumbStamp ThumbStampOf(const std::string& fullPath) {
    ThumbStamp st;
    std::error_code ec;
    const std::filesystem::file_time_type t =
        std::filesystem::last_write_time(std::filesystem::path(fullPath), ec);
    if (ec) return ThumbStamp();  // ok=false (부재·권한·소멸 전부)
    const auto n = t.time_since_epoch().count();
    st.mtime = static_cast<long long>(n);  // 시계 정수열 — 같은 기기 안에서 안정
    const uintmax_t sz = std::filesystem::file_size(
        std::filesystem::path(fullPath), ec);
    if (ec) return ThumbStamp();
    st.size = static_cast<long long>(sz);
    st.ok = true;
    return st;
}

// T3 disk-cache key (selftest 2g-h): 전체 경로+size+mtime을 '|' 구분 원문으로
// 묶어 Fnv1a(2g-d 소비) → 8자리 소문자 hex. 파일명만으로는 이름 같고 내용이
// 다른 파일(재촬영 덮어쓰기)을 구분 못 하므로 크기+mtime을 키에 묶는다 —
// 내용 변화 = 키 변화 = 새 캐시 파일(스메리 캐시 방지). mtime 시계 epoch는
// 플랫폼 몫이지만 캐시 dir는 기기 귀속이라 같은 기기 안 결정론만 계약(2g-d의
// 축 무관 결정론은 Fnv1a 원문 계약이 계속 소유).
inline std::string ThumbKey(const std::string& fullPath, long long size,
                            long long mtime) {
    const std::string seed = fullPath + "|" + std::to_string(size) + "|" +
                             std::to_string(mtime);
    char buf[16] = {};
    std::snprintf(buf, sizeof(buf), "%08x", Fnv1a(seed));
    return buf;
}

// T3 disk-cache key of a live file — 스탬프 실패(부재 등) = 빈 문자열.
inline std::string ThumbKeyFor(const std::string& fullPath) {
    const ThumbStamp st = ThumbStampOf(fullPath);
    if (!st.ok) return {};
    return ThumbKey(fullPath, st.size, st.mtime);
}

// Disk-cache file path (brief 계약명): <exeDir>/state/gallery/thumbs/<key>.png.
// empty key = 방어선(empty string — 호출부 무접촉). fs::path 합성+generic —
// GalleryDirList와 같은 수형(폰/WSL의 '/' 표기).
inline std::string GalleryThumbPath(const std::string& exeDir,
                                    const std::string& cacheKey) {
    if (cacheKey.empty()) return {};
    return (std::filesystem::path(exeDir) / "state" / "gallery" / "thumbs" /
            (cacheKey + ".png"))
        .generic_string();
}

// Nearest downscale (T3 — T2 폴백 확대 동형 기법, 역방향): dst 픽셀 (x,y)는
// sx=x*srcW/dstW·sy=y*srcH/dstH 소스 샘플 그대로(박스 축소 = 최근접 샘플).
// dst 치수는 FitThumb 산치(s=min 축 지배, s>1.0 → 1.0 확대 금지)를 내림해
// 1 이상 보정 — 1.5 요청도 상한에 눌려 s=1.0 항등(확대 아님)이 된다. 퇴화
// 입력(빈 픽셀·치수 0 이하·박스 0 이하) = 빈 LoadedImage(호출부 placeholder
// 유지 계약).
inline LoadedImage MakeThumb(const LoadedImage& src, int maxW, int maxH) {
    if (src.w <= 0 || src.h <= 0 || maxW <= 0 || maxH <= 0 ||
        src.rgba.empty())
        return LoadedImage();
    const float s = std::min(
        std::min(static_cast<float>(maxW) / static_cast<float>(src.w),
                 static_cast<float>(maxH) / static_cast<float>(src.h)),
        1.0f);
    const int dstW = std::max(1, static_cast<int>(src.w * s));
    const int dstH = std::max(1, static_cast<int>(src.h * s));
    LoadedImage out;
    out.w = dstW;
    out.h = dstH;
    out.rgba.assign(static_cast<size_t>(dstW) * dstH * 4, 0);
    for (int y = 0; y < dstH; ++y) {
        const int sy =
            std::min(src.h - 1, y * src.h / dstH);  // nearest 역사상 (T2 동형)
        const uint8_t* srcRow =
            src.rgba.data() + static_cast<size_t>(sy) * src.w * 4;
        uint8_t* dstRow =
            out.rgba.data() + static_cast<size_t>(y) * dstW * 4;
        for (int x = 0; x < dstW; ++x) {
            const int sx = std::min(src.w - 1, x * src.w / dstW);
            std::memcpy(dstRow + static_cast<size_t>(x) * 4,
                        srcRow + static_cast<size_t>(sx) * 4, 4);
        }
    }
    return out;
}

// LRU eviction arbiter (T3 fix r1 — selftest 2g-i): the pool's pure side of the
// "same-frame destruction is forbidden" contract. useFrames[i] is slot i's
// last-used frame generation. Slots touched in the *current* frame are
// excluded from candidacy **entirely** — their textures are already recorded in
// this frame's draw list, so destroying them before RenderDrawData is a
// freed-texture render (the C1 defect — an earlier per-request tick guard only
// excluded one entry, not the whole frame's touches). Among the candidates the
// oldest generation wins; ties keep the first index (determinism). No
// candidate → -1: the caller leaves the cell as a placeholder instead of
// destroying a texture that is about to be drawn.
inline int PickLruVictim(const std::vector<long long>& useFrames,
                         long long curFrame) {
    int victim = -1;
    for (int i = 0; i < static_cast<int>(useFrames.size()); ++i) {
        if (useFrames[i] == curFrame) continue;  // 이번 프레임 접촉분 — 전부 제외
        if (victim < 0 || useFrames[i] < useFrames[victim]) victim = i;
    }
    return victim;
}

// Enumerate one dir's image files, newest first (mtime desc; tie = name desc,
// the shot_<epoch-ms>_ lexical order precedent). ec-neutral throughout (error
// codes, no throwing overloads — 이 TU 무 try/catch, shot 앱 동일 계약):
// a missing dir returns an empty list with *ok=false, an existing-but-empty
// dir returns an empty list with *ok=true. Only what LoadImageFile decodes
// counts as an image here (png/jpg/jpeg); subdirectories are skipped.
inline std::vector<std::string> ListImageFiles(const std::string& dir,
                                               bool* ok) {
    if (ok) *ok = false;
    std::error_code ec;
    const std::filesystem::directory_iterator it(std::filesystem::path(dir), ec);
    if (ec || it == std::filesystem::directory_iterator()) {
        if (!ec && ok) *ok = true;  // 존재하지만 빈 폴더 = ok (목록만 비움)
        return {};
    }

    using Stamp = std::pair<std::filesystem::file_time_type, std::string>;
    std::vector<Stamp> stamps;
    for (const std::filesystem::directory_entry& entry : it) {
        std::error_code entryEc;
        const bool isDir = entry.is_directory(entryEc);
        if (entryEc || isDir) continue;  // 열거 스캔 중 소멸 성분은 스킵
        const std::string ext = entry.path().extension().string();
        if (jk::crt::Stricmp(ext.c_str(), ".png") != 0 &&
            jk::crt::Stricmp(ext.c_str(), ".jpg") != 0 &&
            jk::crt::Stricmp(ext.c_str(), ".jpeg") != 0)
            continue;
        std::error_code te;
        const std::filesystem::file_time_type t = entry.last_write_time(te);
        if (te) continue;  // mtime을 못 읽는 성분은 열외(순서 단정 보존)
        stamps.emplace_back(t, entry.path().filename().string());
    }
    std::sort(stamps.begin(), stamps.end(),
              [](const Stamp& a, const Stamp& b) {
                  if (a.first != b.first) return a.first > b.first;
                  return a.second > b.second;
              });
    std::vector<std::string> out;
    out.reserve(stamps.size());
    for (const Stamp& s : stamps) out.push_back(s.second);
    if (ok) *ok = true;
    return out;
}

} // namespace gallery
} // namespace jk

#endif // GALLERYMODEL_H