// src/theme/JKThemeConfig.cpp — theme.json 프리셋 1키 로더 (P2 단계 2 스펙 §1b)
// terminal.json과 달리 quickjs를 띄우지 않는다: 키가 하나뿐이므로 문자열
// 스캔이면 충분. 파일 없음/깨짐 → false, 활성 테마 불변 (기본값=다크).
#include "theme/JKTheme.h"
#include <fs/JKFs.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

namespace jk { namespace theme {

bool loadPresetFromFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::string buf;
    char chunk[4096];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) buf.append(chunk, n);
    std::fclose(f);

    const size_t key = buf.find("\"preset\"");
    if (key == std::string::npos) return false;
    const size_t colon = buf.find(':', key + 8);
    if (colon == std::string::npos) return false;
    const size_t q1 = buf.find('"', colon + 1);
    if (q1 == std::string::npos) return false;
    const size_t q2 = buf.find('"', q1 + 1);
    if (q2 == std::string::npos) return false;
    const std::string preset = buf.substr(q1 + 1, q2 - q1 - 1);

    if (preset == "light")       setTheme(&kLight);
    else if (preset == "classic") setTheme(&kClassic);
    else if (preset == "dark")    setTheme(&kDefault);
    else return false;
    std::printf("[theme] preset '%s' from %s\n", preset.c_str(), path.c_str());
    std::fflush(stdout);
    return true;
}

std::string DefaultThemePath() {
    // jk::fs::GetExecutablePath 흡수 (docs/68 W5). 원문 규약: 추출 실패 시
    // 빈 dir(dir + "theme.json"에서 상대 경로 폴백), 성공 시 후행 '\' 유지.
    std::string dir;
#ifdef _WIN32
    dir = jk::fs::GetExecutablePath();
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir.resize(slash + 1);
#endif
    return dir + "theme.json";
}

// P3 hot-swap: poll theme.json mtime (the caller owns the cadence, 500ms).
// Missing file counts as mtime 0, so deleting the file also registers and
// re-runs the loader, which is fail-open on missing (keeps preset — docs/45).
// stage-3 task 5: GetFileAttributesExA(WIN32_FILE_ATTRIBUTE_DATA) 수기 →
// std::filesystem::last_write_time. 원문 반환은 FILETIME(1601 기준 100ns
// ticks)을 64비트로 합친 것 — 변경 감지는 값 비교라 표현은 규약일 뿐, 같은
// 100ns-ticks-since-1601 수 형태를 clock_cast(system_clock ns)→/100→+1601
// 오프셋으로 유지한다(msvc FILETIME과 같은 수로 온다). 실패(무파일 포함)=0
// 관측 동형.
static long long s_lastThemeMtime = -1;

static long long ThemeFileMtime(const std::string& path) {
    std::error_code ec;
    const auto ftw = std::filesystem::last_write_time(
        std::filesystem::path(path), ec);
    if (ec) return 0;
    const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(ftw);
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        sys.time_since_epoch()).count();
    // 1970-01-01 이전 파일은 음수 절단이 아닌 0 (원문의 "실패/과거=0" 규약).
    if (ns < 0) return 0;
    return static_cast<long long>(ns / 100) + 116444736000000000LL;
}

bool PollPresetFile() {
    const std::string path = DefaultThemePath();
    const long long m = ThemeFileMtime(path);
    if (m == s_lastThemeMtime) return false;
    s_lastThemeMtime = m;
    if (s_lastThemeMtime == 0) return false;  // vanished: nothing to load
    return loadPresetFromFile(path);          // idempotent on same preset
}

bool WriteThemePresetFile(const std::string& preset) {
    const std::string path = DefaultThemePath();
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "{\"preset\":\"%s\"}\n", preset.c_str());
    std::fclose(f);
    // Register the write immediately so the next poll tick doesn't
    // re-run the loader for our own write.
    s_lastThemeMtime = ThemeFileMtime(path);
    std::printf("[theme] preset '%s' -> %s (hot-swap)\n", preset.c_str(),
                path.c_str());
    std::fflush(stdout);
    return true;
}

} } // namespace jk::theme
