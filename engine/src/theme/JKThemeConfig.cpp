// src/theme/JKThemeConfig.cpp — theme.json 프리셋 1키 로더 (P2 단계 2 스펙 §1b)
// terminal.json과 달리 quickjs를 띄우지 않는다: 키가 하나뿐이므로 문자열
// 스캔이면 충분. 파일 없음/깨짐 → false, 활성 테마 불변 (기본값=다크).
#include "theme/JKTheme.h"

#include <cstdio>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

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
    std::string dir;
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        dir = exePath;
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir.resize(slash + 1);
    }
#endif
    return dir + "theme.json";
}

// P3 hot-swap: poll theme.json mtime (the caller owns the cadence, 500ms).
// Missing file counts as mtime 0, so deleting the file also registers and
// re-runs the loader, which is fail-open on missing (keeps preset — docs/45).
static long long s_lastThemeMtime = -1;

static long long ThemeFileMtime(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fa))
        return 0;
    return ((long long)fa.ftLastWriteTime.dwHighDateTime << 32) |
            fa.ftLastWriteTime.dwLowDateTime;
}

bool PollPresetFile() {
    const std::string path = DefaultThemePath();
    const long long m = ThemeFileMtime(path);
    if (m == s_lastThemeMtime) return false;
    s_lastThemeMtime = m;
    if (s_lastThemeMtime == 0) return false;  // vanished: nothing to load
    return loadPresetFromFile(path);          // idempotent on same preset
}

void WriteThemePresetFile(const std::string& preset) {
    const std::string path = DefaultThemePath();
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "{\"preset\":\"%s\"}\n", preset.c_str());
    std::fclose(f);
    // Register the write immediately so the next poll tick doesn't
    // re-run the loader for our own write.
    s_lastThemeMtime = ThemeFileMtime(path);
    std::printf("[theme] preset '%s' -> %s (hot-swap)\n", preset.c_str(),
                path.c_str());
    std::fflush(stdout);
}

} } // namespace jk::theme
