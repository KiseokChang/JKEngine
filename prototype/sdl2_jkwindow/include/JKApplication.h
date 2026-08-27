#ifndef JKAPPLICATION_H
#define JKAPPLICATION_H

#include <JKWindow.h>
#include <JKMessageQue.h>
#include <JKHangulManager.h>
#include <JKDC.h>
#include <SDL.h>
#include <memory>
#include <string>

namespace jk {

class JKApplication {
public:
    JKApplication();
    virtual ~JKApplication();

    bool Init(const std::string& title, int width, int height);
    void Close();
    int Run();

    void SetMainWindow(std::unique_ptr<JKWindow> window);
    JKWindow* GetMainWindow() const;

    void SetModalWindow(JKWindow* window);
    JKWindow* GetModalWindow() const { return modalWindow_; }

    JKControl* FindControlById(uint32_t winId);
    JKControl* FindControlByControlId(uint16_t controlId);

protected:
    virtual void OnInit();
    virtual void OnClose();
    virtual bool PreProcessMessage(const JKEvent& ev);
    virtual void RouteMessage(const JKEvent& ev);

    void Render();
    JKEvent TranslateSDLEvent(const SDL_Event& sdl);
    void UpdateScale();

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<JKWindow> mainWindow_;
    JKMessageQue msgQue_;
    JKDC dc_;
    std::unique_ptr<HangulManager> hangulManager_;
    bool running_ = false;

    // SDL 논리 좌표 -> 물리 픽셀 변환 배율 (SDL_RenderSetScale()으로 렌더링만 변환).
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;

    // Periodic timer tick generation (milliseconds).
    uint32_t timerInterval_ = 1000;
    uint32_t lastTimerTick_ = 0;

    // Modal dialog support.
    JKWindow* modalWindow_ = nullptr;
};

extern JKApplication* g_currentJKApp;

} // namespace jk

#endif // JKAPPLICATION_H
