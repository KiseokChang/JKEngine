#ifndef JKEVENT_H
#define JKEVENT_H

#include <JKTypes.h>
#include <stdint.h>

namespace jk {

enum class JKEventType : uint16_t {
    None,
    Quit,
    MouseMove,
    MouseDown,
    MouseUp,
    KeyDown,
    KeyUp,
    Char,
    TextEditing,
    Paint,
    Timer,
    Command,
    SizeChanged,
    DpiChanged,
    User
};

struct JKEvent {
    JKEventType type = JKEventType::None;
    uint32_t    targetId = 0;   // Target window/control composite id (legacy; prefer winId+controlId).
    uint32_t    winId = 0;      // Target window id.
    uint16_t    controlId = 0;  // Target control id within the window.
    int32_t     x = 0;
    int32_t     y = 0;
    int32_t     dx = 0;
    int32_t     dy = 0;
    uint32_t    keyCode = 0;
    uint32_t    detail = 0;
    uint32_t    option = 0;
    int32_t     editStart = 0;
    int32_t     editLength = 0;
    char        text[64] = {};
};

JKEvent TranslateSDLEvent(const SDL_Event& sdl);

} // namespace jk

#endif // JKEVENT_H
