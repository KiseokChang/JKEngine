#include <apps/TerminalHangulInput.h>
#include <JKHangulUtil.h>

namespace jk {

namespace {

// 단일 KSSM 조합형 코드 → UTF-8. 코드가 늘 0x8000 이상 2바이트 쌍이라 NUL
// 절단 없다(docs/61 §8의 {0x00,XX}는 슬롯 코드 직기록에서만 발생).
std::string KssmCodeToUtf8(uint16_t code) {
    if (code < 0x8000) return {};
    char kssm[3] = { static_cast<char>(code >> 8),
                     static_cast<char>(code & 0xFF), '\0' };
    return KssmToUtf8(kssm);
}

} // anonymous namespace

TerminalHangulInput::Result TerminalHangulInput::Toggle() {
    Result r;
    if (hangulMode_) r = Commit();   // 모드 해제 시 진행 조합 확정
    hangulMode_ = !hangulMode_;
    return r;
}

TerminalHangulInput::Result TerminalHangulInput::Letter(SDL_Keycode key, SDL_Keymod mod) {
    Result r;
    if (!hangulMode_) return r;
    const bool letter = (key >= SDLK_a && key <= SDLK_z) ||
                        (key >= 'A' && key <= 'Z');
    if (!letter) return r;
    if (mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) return r;
    uint16_t converted = hangul_.ConvertKey(static_cast<uint16_t>(key),
                                            (mod & KMOD_SHIFT) ? 0x0040 : 0);
    bool complete = hangul_.Automata(converted);
    if (complete) {
        for (uint16_t i = 0; i < hangul_.outSP; ++i)
            r.send += KssmCodeToUtf8(hangul_.outStack[i]);
        hangul_.outSP = 0;
        // End1/End2는 트리거 키를 새 조합의 씨앗으로 심는다 — 시드 유지.
        if (!hangul_.curHanState || hangul_.charCode == 0x8441)
            hangul_.InitAutomata();
    }
    if (hangul_.curHanState && hangul_.charCode != 0x8441)
        r.preEdit = KssmCodeToUtf8(hangul_.charCode);
    return r;
}

TerminalHangulInput::Result TerminalHangulInput::Backspace() {
    Result r;
    if (!Composing()) {
        r.send = "\x7f";   // 조합 없음 — pty 삭제
        return r;
    }
    uint16_t restored = 0;
    BackspaceResult res = hangul_.BackspaceJamo(restored);
    if (res == BackspaceResult::Reattach) {
        // 받침 넘김 재부착(docs/65 O3): 넘김 직전에 송출된 받침-없는 음절(하)을
        // pty에서 삭제하고 오버레이는 재부착 음절(학) — 화면상 MS IME와 동일.
        r.send    = "\x7f";
        r.preEdit = KssmCodeToUtf8(restored);
    } else if (res == BackspaceResult::Jamo) {
        r.preEdit = KssmCodeToUtf8(restored);
    }
    // Empty: r 그대로 — 조합 종료, 다음 Backspace가 pty DEL을 보낸다.
    return r;
}

TerminalHangulInput::Result TerminalHangulInput::Commit() {
    Result r;
    if (hangul_.curHanState && hangul_.charCode != 0x8441) {
        r.send = KssmCodeToUtf8(hangul_.charCode);
        hangul_.InitAutomata();
    }
    return r;
}

} // namespace jk