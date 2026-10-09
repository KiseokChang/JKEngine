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
#include <port/JKCrtShim.h>

#include <algorithm>
#include <cstdint>
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