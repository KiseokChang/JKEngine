#ifndef JKTERMINALCONFIG_H
#define JKTERMINALCONFIG_H

// terminal.json — docs/26 단계 5 (설정 파일) delivered through docs/27 단계 4:
// the config is JSON and is parsed with the vendored quickjs runtime's
// JS_ParseJSON — one throwaway JSContext per load, no new dependency. Missing
// file, malformed JSON, and wrong-typed keys all fall back per key to the
// defaults below; nothing ever fails the app startup.

#include <cstdint>
#include <string>

namespace jk {

struct JKTerminalConfig {
    std::string shell = "powershell.exe -NoLogo";
    std::string font = "C:\\Windows\\Fonts\\consola.ttf";
    std::string fontFallback = "C:\\Windows\\Fonts\\malgun.ttf";
    int scrollback = 1000;              // JKTerminalGrid::kScrollbackMax
    uint32_t themeBg = 0x0C0C0C;        // TerminalView defaults
    uint32_t themeFg = 0xCCCCCC;

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