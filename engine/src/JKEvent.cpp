#include <JKEvent.h>
#include <cstring>

// Mouse events carry the live modifier state in ev.option (docs/26 단계 3
// mouse reports): SDL_KEYDOWN/UP have keysym.mod on the event itself, but the
// mouse structs do not — SDL_GetModState() reads SDL's keyboard modifier
// state, which is maintained by the event pump on the SAME thread (the render
// thread polls SDL events and calls this translator), so it is current and
// thread-valid here. The client mode mirrors this in JKWindowServer's
// InputEventPayload construction (payload.option already exists for keyboard).

namespace jk {

JKEvent TranslateSDLEvent(const SDL_Event& sdl) {
    JKEvent ev;
    ev.targetId = 0;

    switch (sdl.type) {
        case SDL_QUIT:
            ev.type = JKEventType::Quit;
            break;

        case SDL_MOUSEMOTION:
            ev.type = JKEventType::MouseMove;
            ev.x = sdl.motion.x;
            ev.y = sdl.motion.y;
            ev.dx = sdl.motion.xrel;
            ev.dy = sdl.motion.yrel;
            ev.option = SDL_GetModState();
            break;

        case SDL_MOUSEBUTTONDOWN:
            ev.type = JKEventType::MouseDown;
            ev.x = sdl.button.x;
            ev.y = sdl.button.y;
            ev.detail = static_cast<uint32_t>(sdl.button.button);
            ev.option = SDL_GetModState();
            break;

        case SDL_MOUSEBUTTONUP:
            ev.type = JKEventType::MouseUp;
            ev.x = sdl.button.x;
            ev.y = sdl.button.y;
            ev.detail = static_cast<uint32_t>(sdl.button.button);
            ev.option = SDL_GetModState();
            break;

        case SDL_MOUSEWHEEL:
            // Mouse wheel events (docs/26 단계 3): previously dropped here —
            // single-process apps (TerminalApp/PcxApp PreProcessMessage) never
            // received them. Wheel events carry no coordinates; the view
            // reports them at its last seen mouse cell (TerminalView).
            ev.type = JKEventType::MouseWheel;
            ev.dx = sdl.wheel.x;
            ev.dy = sdl.wheel.y;
            ev.option = SDL_GetModState();
            break;

        case SDL_KEYDOWN:
            ev.type = JKEventType::KeyDown;
            ev.keyCode = static_cast<uint32_t>(sdl.key.keysym.sym);
            ev.option = static_cast<uint32_t>(sdl.key.keysym.mod);
            break;

        case SDL_KEYUP:
            ev.type = JKEventType::KeyUp;
            ev.keyCode = static_cast<uint32_t>(sdl.key.keysym.sym);
            ev.option = static_cast<uint32_t>(sdl.key.keysym.mod);
            break;

        case SDL_TEXTINPUT:
            ev.type = JKEventType::Char;
            std::strncpy(ev.text, sdl.text.text, sizeof(ev.text) - 1);
            ev.text[sizeof(ev.text) - 1] = '\0';
            if (ev.text[0]) {
                ev.keyCode = static_cast<uint32_t>(static_cast<unsigned char>(ev.text[0]));
            }
            break;

        case SDL_TEXTEDITING:
            ev.type = JKEventType::TextEditing;
            std::strncpy(ev.text, sdl.edit.text, sizeof(ev.text) - 1);
            ev.text[sizeof(ev.text) - 1] = '\0';
            ev.editStart = sdl.edit.start;
            ev.editLength = sdl.edit.length;
            break;

        default:
            ev.type = JKEventType::None;
            break;
    }

    return ev;
}

} // namespace jk
