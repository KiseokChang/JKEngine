#ifndef TERMINALHANGULINPUT_H
#define TERMINALHANGULINPUT_H

// Terminal internal Hangul composition input (docs/61 §22). The server SDL
// window has its IME context deliberately detached (JKWindowServer Init →
// JKPlatform::DetachIme — with an IME attached, Enter/Esc/letters arrive as
// VK_PROCESSKEY and clients would never see those raw keydowns). That design
// leaves the terminal — which used to rely on OS IME commit (docs/26 단계 5)
// — unable to type Hangul at all in the desktop. This class owns the 2-set
// composition state instead: letter keydowns feed HangulAutomata, completed
// syllables go to the pty as UTF-8 bytes, and the in-progress syllable is
// returned as a pre-edit overlay string for the cursor cell. TerminalView
// knows no composition logic; the probe drives this class directly.

#include <JKHangulAutomata.h>
#include <SDL.h>
#include <string>

namespace jk {

class TerminalHangulInput {
public:
    // send = bytes to forward to the pty (committed syllables); preEdit =
    // cursor overlay string (UTF-8, empty = no overlay). The syllable being
    // composed has NOT reached the pty yet — backspace pops jamo locally.
    struct Result {
        std::string send;
        std::string preEdit;
    };

    bool HangulMode() const { return hangulMode_; }
    bool Composing() const { return hangul_.curHanState != 0; }

    void Reset() { hangul_.InitAutomata(); }

    // 한/영 토글(서버 LL 훅 ImeToggle, LANG1 키코드). 꺼지는 방향에서는
    // 진행 조합을 확정한다. 기본은 영문 모드.
    Result Toggle();

    // 영문 자모 키다운. 모드가 꺼져 있거나, ctrl/alt/gui가 붙었거나, 문자가
    // 아니면 빈 Result — 호출자는 기존 입력 경로를 계속한다. 겹자모 플래그는
    // 물리 Shift만 따른다(docs/61 §21 — 캡스락은 tolower 정규화가 흡수).
    Result Letter(SDL_Keycode key, SDL_Keymod mod);

    // 조합 중이면 자소 1개 되돌림(HangulAutomata::BackspaceJamo), 아니면
    // \x7f(pty 삭제) 반환.
    Result Backspace();

    // 진행 중 조합을 확정해 pty 바이트로 돌려준다(조합 없으면 빈 send).
    Result Commit();

private:
    HangulAutomata hangul_;
    bool hangulMode_ = false;
};

} // namespace jk
#endif // TERMINALHANGULINPUT_H