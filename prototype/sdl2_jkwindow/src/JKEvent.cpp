#include <JKEvent.h>
#include <cstring>

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
            break;

        case SDL_MOUSEBUTTONDOWN:
            ev.type = JKEventType::MouseDown;
            ev.x = sdl.button.x;
            ev.y = sdl.button.y;
            ev.detail = static_cast<uint32_t>(sdl.button.button);
            break;

        case SDL_MOUSEBUTTONUP:
            ev.type = JKEventType::MouseUp;
            ev.x = sdl.button.x;
            ev.y = sdl.button.y;
            ev.detail = static_cast<uint32_t>(sdl.button.button);
            break;

        case SDL_KEYDOWN:
            ev.type = JKEventType::KeyDown;
            ev.keyCode = static_cast<uint32_t>(sdl.key.keysym.sym);
            break;

        case SDL_KEYUP:
            ev.type = JKEventType::KeyUp;
            ev.keyCode = static_cast<uint32_t>(sdl.key.keysym.sym);
            break;

        case SDL_TEXTINPUT:
            ev.type = JKEventType::Char;
            std::strncpy(ev.text, sdl.text.text, sizeof(ev.text) - 1);
            ev.text[sizeof(ev.text) - 1] = '\0';
            if (ev.text[0]) {
                ev.keyCode = static_cast<uint32_t>(static_cast<unsigned char>(ev.text[0]));
            }
            break;

        default:
            ev.type = JKEventType::None;
            break;
    }

    return ev;
}

} // namespace jk
