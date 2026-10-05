#ifndef JKTERMINALCONFIG_H
#define JKTERMINALCONFIG_H

// terminal.json — docs/26 단계 5 (설정 파일) delivered through docs/27 단계 4:
// the config is JSON and is parsed with the vendored quickjs runtime's
// JS_ParseJSON — one throwaway JSContext per load, no new dependency. Missing
// file, malformed JSON, and wrong-typed keys all fall back per key to the
// defaults below; nothing ever fails the app startup.

#include <cstdint>
#include <cstdlib>  // std::getenv — posix 셸 기본값 (docs/78 TX5)
#include <string>

// posix 셸 기본값: Win32 리터럴 "powershell.exe -NoLogo"는 posix에서 존재하지
// 않는 바이너리다 (docs/78 TX5 폰 실측 — mksh -c "powershell.exe -NoLogo" =
// not found → 자식 127 → 터미널 클라 즉사. WSL은 interop으로 powershell.exe가
// 실존해 또 우연히 살아 있던 케이스). SHELL 환경변수가 정직한 기본 (Termux는
// 항상 설정; 미설정이면 /bin/sh로 glibc 관행 폴백).
#if !defined(_WIN32)
namespace detail {
inline std::string TerminalShellDefault() {
    const char* s = std::getenv("SHELL");
    return (s && *s) ? std::string(s) : std::string("/bin/sh");
}
}  // namespace detail
#endif  // !_WIN32

namespace jk {

struct JKTerminalConfig {
#if defined(_WIN32)
    std::string shell = "powershell.exe -NoLogo";
#else
    std::string shell = detail::TerminalShellDefault();
#endif
    std::string font = "C:\\Windows\\Fonts\\consola.ttf";
    std::string fontFallback = "C:\\Windows\\Fonts\\malgun.ttf";
    int scrollback = 1000;              // JKTerminalGrid::kScrollbackMax
    uint32_t themeBg = 0x0C0C0C;        // TerminalView defaults
    uint32_t themeFg = 0xCCCCCC;
    bool themeBgSet = false;            // terminal.json에 themeBg 키가 있으면 true (P2 단계 3)
    bool themeFgSet = false;            // terminal.json에 themeFg 키가 있으면 true (P2 단계 3)

    // Applies overrides from `path` (JSON). Returns false when the file could
    // not be opened (defaults retained); a malformed file logs and keeps
    // defaults for the bad keys only.
    bool Load(const std::string& path);

    // terminal.json next to the running exe (the user-editable deployment
    // spot, same directory the apps/*.jkx packages live in).
    static std::string DefaultPath();
};

} // namespace jk

#endif // JKTERMINALCONFIG_H