#ifndef JKAPPLICATION_H
#define JKAPPLICATION_H

#include <JKWindow.h>
#include <JKMessageQue.h>
#include <JKHangulManager.h>
#include <JKDC.h>
#include <JKRenderBackend.h>
#include <JKResourceCache.h>
#include <JKWindowManager.h>
#include <SDL.h>
#include <memory>
#include <string>
#include <set>
#include <vector>

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
    SDL_Window* GetSdlWindow() const { return window_; }

    void SetModalWindow(JKWindow* window);
    JKWindow* GetModalWindow() const { return windowManager_ ? windowManager_->GetModalWindow() : nullptr; }

    void SetCapture(JKControl* control);
    void ReleaseCapture();
    JKControl* GetCapture() const { return windowManager_ ? windowManager_->GetCapture() : nullptr; }

    void SetInputWindow(JKWindow* window);
    JKWindow* GetInputWindow() const { return windowManager_ ? windowManager_->GetInputWindow() : nullptr; }

    JKControl* FindControlById(uint32_t winId);
    JKControl* FindControlByControlId(uint16_t controlId);
    JKWindow*  FindWindowById(uint32_t winId);

    JKResourceCache* GetResourceCache() const { return resourceCache_.get(); }
    JKWindowManager* GetWindowManager() const { return windowManager_.get(); }

    // Legacy single timer interval. Prefer AddTimer/RemoveTimer for per-window
    // or per-control timers. SetTimerInterval configures the implicit global
    // timer (winId 0) for backward compatibility.
    void SetTimerInterval(uint32_t ms);

    // Register a repeating or one-shot timer. Returns a handle that can be passed
    // to RemoveTimer. When fired, a JKEventType::Timer event is posted with the
    // given winId as the target.
    uint64_t AddTimer(uint32_t winId, uint32_t intervalMs, bool repeat);
    void RemoveTimer(uint64_t handle);
    void RemoveTimersForWindow(uint32_t winId);

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
    void RenderDirtyRegions(JKWindow* dirtyWindow);
    JKEvent TranslateSDLEvent(const SDL_Event& sdl);
    void UpdateScale();

    // 모니터 이동/DPI 전환 대응 헬퍼 (14_sdl2_window_dpi.md 참조)
    void ReapplyPlacement();
    void SynchronizeWindowOnDisplayChanged(int displayIndex);

    // Debounced resize/DPI handling.
    void OnSDLWindowEvent(const SDL_WindowEvent& winEv);
    void ProcessPendingResize(uint32_t now);

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
    std::unique_ptr<JKRenderBackend> renderBackend_;
    JKRenderBackend::TextureHandle backBuffer_ = JKRenderBackend::InvalidTexture;
    int backBufferW_ = 0;
    int backBufferH_ = 0;

    std::unique_ptr<JKWindow> mainWindow_;
    std::unique_ptr<JKWindowManager> windowManager_;
    JKMessageQue msgQue_;
    JKDC dc_;
    std::unique_ptr<HangulManager> hangulManager_;
    std::unique_ptr<JKResourceCache> resourceCache_;
    bool running_ = false;

    // SDL 논리 좌표 -> 물리 픽셀 변환 배율 (SDL_RenderSetScale()으로 렌더링만 변환).
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;

    // 앱 논리 좌표계. Init 요청 크기로 시작하며, 창이 생성된 후에는
    // SDL_GetWindowSize()가 보고하는 실제 창 논리 포인트 크기로 동기화한다.
    // 내부 레이아웃/hit-test/드래그는 이 좌표계를 그대로 사용하고, 렌더링/입력만
    // HiDPI 물리 픽셀에 맞춰 스케일한다.
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

    // Deadline-based timer scheduler.
    struct DeadlineTimer {
        uint64_t handle = 0;
        uint32_t winId = 0;
        uint32_t intervalMs = 0;
        uint32_t deadline = 0;
        bool repeat = false;

        bool operator<(const DeadlineTimer& other) const {
            return deadline < other.deadline;
        }
    };
    std::multiset<DeadlineTimer> timers_;
    uint64_t nextTimerHandle_ = 1;
    uint32_t legacyTimerInterval_ = 1000;
    uint32_t lastLegacyTimerTick_ = 0;

    // Debounced resize/DPI change state.
    static constexpr uint32_t kResizeDebounceMs = 200;
    int pendingResizeW_ = 0;
    int pendingResizeH_ = 0;
    int pendingDpiDisplay_ = -1;
    uint32_t pendingResizeDeadline_ = 0;
    bool resizePending_ = false;

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
