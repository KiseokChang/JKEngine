#ifndef JKAPPLICATION_H
#define JKAPPLICATION_H

#include <JKWindow.h>
#include <JKMessageQue.h>
#include <JKHangulManager.h>
#include <JKDC.h>
#include <JKRenderBackend.h>
#include <JKResourceCache.h>
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

    void SetCapture(JKControl* control);
    void ReleaseCapture();
    JKControl* GetCapture() const { return captureControl_; }

    void SetInputWindow(JKWindow* window);
    JKWindow* GetInputWindow() const { return inputWindow_; }

    JKControl* FindControlById(uint32_t winId);
    JKControl* FindControlByControlId(uint16_t controlId);

    JKResourceCache* GetResourceCache() const { return resourceCache_.get(); }

    // Change the periodic timer tick interval (milliseconds). Default is 1000.
    void SetTimerInterval(uint32_t ms) { timerInterval_ = ms; }

#ifdef _WIN32
    // 디버깅용: 마우스 이벤트 로그 핸들(JKButton 등에서 사용).
    FILE* GetMouseLog() const { return mouseLog_; }
#endif

protected:
    virtual void OnInit();
    virtual void OnClose();
    virtual bool PreProcessMessage(const JKEvent& ev);
    virtual void RouteMessage(const JKEvent& ev);

    void Render();
    JKEvent TranslateSDLEvent(const SDL_Event& sdl);
    void UpdateScale();

    // 모니터 이동/DPI 전환 대응 헬퍼 (14_sdl2_window_dpi.md 참조)
    void ReapplyPlacement();
    void SynchronizeWindowOnDisplayChanged(int displayIndex);

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
    std::unique_ptr<JKRenderBackend> renderBackend_;
    JKRenderBackend::TextureHandle backBuffer_ = JKRenderBackend::InvalidTexture;
    int backBufferW_ = 0;
    int backBufferH_ = 0;

    std::unique_ptr<JKWindow> mainWindow_;
    JKMessageQue msgQue_;
    JKDC dc_;
    std::unique_ptr<HangulManager> hangulManager_;
    std::unique_ptr<JKResourceCache> resourceCache_;
    bool running_ = false;

    // SDL 논리 좌표 -> 물리 픽셀 변환 배율 (SDL_RenderSetScale()으로 렌더링만 변환).
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;

    // 앱 논리 좌표계(Init에 요청한 크기). 창이 화면 작업 영역에 맞춰 줄어들어도
    // 앱 레이아웃 좌표계는 이 크기로 고정되고, 렌더링/입력만 스케일된다.
    int logicalWidth_ = 0;
    int logicalHeight_ = 0;

    // 등비(레터박스) 배율에서 앱 내용 주변의 물리 픽셀 여백(좌상단 오프셋).
    int letterboxX_ = 0;
    int letterboxY_ = 0;

    // 창 좌표(SDL 논리 포인트) -> 물리 픽셀 배율(DPI). 마우스 좌표 변환에 사용.
    float ptToPhysX_ = 1.0f;
    float ptToPhysY_ = 1.0f;

    // "안정 pt": Init 직후의 창(논리 포인트) 크기. 모니터 전이 시 SDL의
    // WM_DPICHANGED 제안 rect 재해석(stale dpiScale)으로 pt가 x0.8/x0.64로
    // 튀는 것을 되돌리는 기준값으로 쓴다(불변값이라 연쇄 축소 없음).
    int stablePtW_ = 0;
    int stablePtH_ = 0;

    // Periodic timer tick generation (milliseconds).
    uint32_t timerInterval_ = 1000;
    uint32_t lastTimerTick_ = 0;

    // Modal dialog support.
    JKWindow* modalWindow_ = nullptr;

    // Mouse capture and the window that owns the current keyboard focus.
    JKControl* captureControl_ = nullptr;
    JKWindow* inputWindow_ = nullptr;

#ifdef _WIN32
    // Win32 물리 픽셀 마우스 좌표 추적 (MouseMove delta 계산용).
    int lastMousePhysX_ = 0;
    int lastMousePhysY_ = 0;
    bool hasLastMousePhys_ = false;

    // 디버깅용 마우스 이벤트 로그 파일 핸들.
    FILE* mouseLog_ = nullptr;
#endif

    void CreateOrResizeBackBuffer();
    void DestroyBackBuffer();
};

extern JKApplication* g_currentJKApp;

} // namespace jk

#endif // JKAPPLICATION_H
