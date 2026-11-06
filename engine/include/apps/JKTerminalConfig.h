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
//
// [T9 수형 — 2026-11-04, "터미널 dark void"의 본 수형]: APK에서 $SHELL 미설정
// → "/bin/sh" → pty 계약(`/bin/sh -c <commandLine>`)이 중첩 비대화형 sh를
// 만들어 PS1이 안 달린다(폰 부팅 캡처 원문 — 빈 화면). 수형 = Android 기본
// "/system/bin/sh -i" — -c 래퍼 안에서도 -i가 대화형을 강제해 mksh 기본
// 프롬프트 "$ "가 뜬다. overrides(--shell/terminal.json shell)는 무접촉.
#if !defined(_WIN32)
namespace detail {
inline std::string TerminalShellDefault() {
#if defined(__ANDROID__)
    return std::string("/system/bin/sh -i");
#endif
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
#if defined(__ANDROID__)
    // T9 (2026-11-04 — 폰 부팅 "dotted placeholder" 원문의 수형): 폰의 폰트
    // 기본 = 시스템 사다리(벡터 atlas 기본 leg와 같은 glyf 단면 축 — CFF
    // 거부 계약 동일). Consolas는 폰에 부재 — 뷰가 placeholder를 그린다.
    // [T9 수형 — 원문: 폰 부팅 프롬프트 row의 cmap-why gi(':')=0 전멸 — OneUI
    // Sans KR VF의 cmap 3서브테이블 136바이트 = ASCII 미커버(파이썬 cmap 파스
    // 원문 {58:0,65:0}) — every cell = .notdef tofu로 그려졌던 것. 수형 =
    // ASCII·등폭이 확인된 DroidSansMono(gi(':')=27·gi(A)=34 — 파스 원문)를
    // primary로, 한글 커버(가/나 실측 폭 11px — PC probe)인 OneUI를
    // fallback으로 둔다(폴백 체인 계약 원존).]
    std::string font = "/system/fonts/DroidSansMono.ttf";
    std::string fontFallback = "/system/fonts/OneUISansKR-VF.ttf";
#else
    std::string font = "C:\\Windows\\Fonts\\consola.ttf";
    std::string fontFallback = "C:\\Windows\\Fonts\\malgun.ttf";
#endif
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