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

} } // namespace jk::theme
