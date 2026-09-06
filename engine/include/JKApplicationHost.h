#ifndef JKAPPLICATIONHOST_H
#define JKAPPLICATIONHOST_H

#include <JKAudioCommand.h>

struct SDL_Window;

namespace jk {

class JKWindow;
class JKControl;
class JKWindowManager;
class JKResourceCache;

// Host services shared by the two application hosts: JKApplication (single-
// process path) and JKClientApplication (window-server client process).
//
// Controls and shared dialogs (JKMessageBox, JKMenu, JKDialog, JKFileDialog,
// apputil::ShowModalMessage) must behave identically in both paths, so they
// reach the active host through g_jkAppHost. The client host is NOT a
// JKApplication — it must not own SDL video/audio threads — which is why the
// old g_currentJKApp-based calls silently no-op'd in client processes (e.g.
// a game-over message box never became modal and never painted).
class JKApplicationHost {
public:
    virtual ~JKApplicationHost() = default;

    // Modal window stack (JKMessageBox / JKMenu / JKDialog / JKFileDialog).
    // A modal window receives all input and is painted on top of the main
    // window; closing it restores the previous focus.
    virtual void SetModalWindow(JKWindow* window) = 0;
    virtual JKWindow* GetModalWindow() const = 0;

    // Mouse capture — the control that keeps receiving motion/release while a
    // button is held, even outside its bounds.
    virtual void SetCapture(JKControl* control) = 0;
    virtual void ReleaseCapture() = 0;
    virtual JKControl* GetCapture() const = 0;

    // Keyboard/IME focus target. GetSdlWindow returns the native window IME
    // composition attaches to (the visible window in single-process mode, the
    // hidden renderer window in client processes).
    virtual void SetInputWindow(JKWindow* window) = 0;
    virtual JKWindow* GetInputWindow() const = 0;
    virtual SDL_Window* GetSdlWindow() const = 0;

    virtual JKWindowManager* GetWindowManager() const = 0;
    virtual JKResourceCache* GetResourceCache() const = 0;

    // Audio playback proxy (JKSoundManager). Single-process: the local audio
    // thread. Client: forwarded to the window server over IPC.
    virtual void PostAudioCommand(const AudioCommand& cmd) = 0;
};

// The active host: the JKApplication in single-process mode, the
// JKClientApplication in client processes. Null in hostless utility modes
// (e.g. the `test` self-test), so every access must be null-guarded.
extern JKApplicationHost* g_jkAppHost;

} // namespace jk

#endif // JKAPPLICATIONHOST_H