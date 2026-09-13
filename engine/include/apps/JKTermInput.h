#ifndef JKTERMINPUT_H
#define JKTERMINPUT_H

// Pure xterm input-encoding helpers for the terminal (docs/26 단계 3,
// spec §2 mouse encoding / §4 arrow keys). Header-only (JKTermSelection.h
// precedent) with NO SDL/Windows dependencies so TerminalView — shared by the
// single-process and window-server client modes — and the exe self-test use
// one implementation. These functions only build strings; gating (is mouse
// reporting on? SGR or X10? app cursor keys?) lives in TerminalView and
// JKVtParser's DECSET accessors.

#include <algorithm>
#include <cstdint>
#include <string>

namespace jk {

// ---------------------------------------------------------------------------
// Mouse reporting (spec §2)
// ---------------------------------------------------------------------------

// Event kind for SGR mouse reports (DECSET 1006): Press/Motion terminate the
// CSI with 'M', Release with 'm' (lowercase — the only casing xterm gives a
// release event).
enum class MouseKind { Press, Motion, Release };

// xterm WIRE modifier bits for mouse reports (spec §2): +4 Shift, +8 Meta
// (Alt), +16 Ctrl. These are NOT SDL KMOD values — the caller (TerminalView)
// translates SDL_Keymod → this bitmask before calling; the header stays
// SDL-free. Combined with the button number (0/1/2 = left/middle/right) and
// the +32 motion / +64..65 wheel offsets into one wire byte.
enum class MouseMod {
    Shift = 4,
    Meta  = 8,
    Ctrl  = 16,
};

// SGR mouse report (spec §2): "\x1b[<" + b + ";" + x + ";" + y + M/m, where
// b = btn + mods + 32 for motion. Wheel passes btn 64 (up) / 65 (down) as
// `btn`. Coordinates arrive 1-based viewport cells (the caller adds 1 to its
// 0-based grid coords; the encoder just prints them) — no cap (spec §2: SGR
// coordinates stay unscaled, unlike the X10 223 cap).
inline std::string EncodeMouseSgr(int btn, int x, int y, MouseKind kind,
                                  int mods) {
    int b = btn + mods;
    if (kind == MouseKind::Motion) b += 32;   // motion while a button is held
    std::string out = "\x1b[<";
    out += std::to_string(b);
    out += ';';
    out += std::to_string(x);
    out += ';';
    out += std::to_string(y);
    out += (kind == MouseKind::Release) ? 'm' : 'M';
    return out;
}

// Classic (non-SGR) mouse encoding (spec §2, DECSET 1000/1002 without 1006):
// "\x1b[M" + (32+Cb) + (32+x) + (32+y). Cb semantics per xterm NORMAL/BUTTON
// tracking: press = Cb 0/1/2 (the button), release = Cb 3 (the classic
// protocol cannot say WHICH button was released), motion = Cb btn+32. Press
// ONLY is X10 COMPATIBILITY mode (DECSET 9) — a different, older mode we do
// not implement. Coordinates are 1-based and clamped to 1..223 — the
// 32-offset must stay inside one signed byte (255 max wire char).
// Classic MOTION (Cb btn+32) exists on the wire but v1 does not report it —
// Motion returns an empty string; the caller drops it (explicit gap,
// docs/41 §7, same reasoning as the non-SGR wheel gap). Wheel has no classic
// form here either (see EncodeWheelAlt).
inline std::string EncodeMouseX10(int btn, int x, int y, MouseKind kind) {
    const int cb = (kind == MouseKind::Release)
                       ? 3                                // generic release
                       : std::clamp(btn, 0, 2);           // plain button press
    const auto clamp = [](int v) { return v < 1 ? 1 : (v > 223 ? 223 : v); };
    std::string out;
    if (kind == MouseKind::Motion) return out;   // v1 gap — not reported
    out = "\x1b[M";
    out += static_cast<char>(32 + cb);
    out += static_cast<char>(32 + clamp(x));
    out += static_cast<char>(32 + clamp(y));
    return out;
}

// ---------------------------------------------------------------------------
// Arrow / navigation keys (spec §4)
// ---------------------------------------------------------------------------

enum class NavKey { Up, Down, Left, Right, Home, End, PgUp, PgDn };

// xterm WIRE modifier bits for CSI 1;<m> keyboard reports (spec §4):
// m = 1 + 1·Shift + 2·Alt + 4·Ctrl. Again NOT SDL KMOD values — the caller
// translates SDL_Keymod → this bitmask at the call site (the header stays
// SDL-free, unlike the mouse bits these are the CSI <m> parameter directly).
enum class NavMod {
    Shift = 1,
    Alt   = 2,
    Ctrl  = 4,
};

// Arrow/navigation encoding (spec §4):
//   mods == 0 + appCursor → SS3 ("\x1bOA" .. Up/Down/Right/Left A/B/C/D,
//                            Home/End OH/OF) — DECSET 1 (app cursor keys)
//   mods == 0 + !appCursor → CSI ("\x1b[A" .., Home/End "\x1b[H"/"\x1b[F")
//   mods != 0 → "\x1b[1;<m><char>" with char A/B/C/D/H/F (Home/End share the
//               1;~m~ family per xterm), PgUp/PgDn → "\x1b[5;<m>~"/"\x1b[6;<m>~"
// PgUp/PgDn with NO mods keep the plain "\x1b[5~"/"\x1b[6~" form (existing
// TerminalView behaviour, byte-for-byte).
inline std::string EncodeArrow(NavKey key, int mods, bool appCursor) {
    // Final char per xterm: A/B/C/D = up/down/right/left, H/F = home/end.
    char c = 0;
    switch (key) {
        case NavKey::Up:    c = 'A'; break;
        case NavKey::Down:  c = 'B'; break;
        case NavKey::Right: c = 'C'; break;
        case NavKey::Left:  c = 'D'; break;
        case NavKey::Home:  c = 'H'; break;
        case NavKey::End:   c = 'F'; break;
        default: break;   // PgUp/PgDn handled below (tilde form)
    }
    if (mods == 0) {
        if (key == NavKey::PgUp) return "\x1b[5~";
        if (key == NavKey::PgDn) return "\x1b[6~";
        if (appCursor) return std::string("\x1bO") + c;
        return std::string("\x1b[") + c;
    }
    const int m = 1 + mods;   // Shift=1 / Alt=2 / Ctrl=4, spec §4
    if (key == NavKey::PgUp)
        return "\x1b[5;" + std::to_string(m) + "~";
    if (key == NavKey::PgDn)
        return "\x1b[6;" + std::to_string(m) + "~";
    return "\x1b[1;" + std::to_string(m) + c;
}

// Wheel → arrow keys (spec §2): mouse reporting OFF + alt screen + one wheel
// notch sends n arrow keys so vim scrolls (Windows Terminal default = 3 rows
// per notch; the caller fixes n=3 in v1). up=true → Up arrows, else Down.
inline std::string EncodeWheelAlt(bool up, int n) {
    std::string out;
    const char* seq = up ? "\x1b[A" : "\x1b[B";
    for (int i = 0; i < n; ++i) out += seq;
    return out;
}

} // namespace jk

#endif // JKTERMINPUT_H
