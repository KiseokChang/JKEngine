#ifndef JKWINDOW_H
#define JKWINDOW_H

#include <JKControl.h>
#include <JKTypes.h>
#include <string>

namespace jk {

class JKWindow : public JKControl {
public:
    enum class WindowRegion { None, TitleBar, Border, Client };

    JKWindow();
    explicit JKWindow(const std::string& title);

    void SetTitle(const std::string& title);
    const std::string& GetTitle() const;

    void SetWindowRect(const JKRect& rect);
    void MoveWindow(int32_t dx, int32_t dy);
    void MoveTo(int32_t x, int32_t y);
    void ResizeWindow(int32_t dx, int32_t dy);

    WindowRegion HitTestRegion(int32_t screenX, int32_t screenY) const;
    JKControl* HitTest(int32_t screenX, int32_t screenY) override;

    // Client area within the window rect (title bar/border already excluded):
    // {kBorder, kTitle, w-2*kBorder, h-kBorder-kTitle} in window-local coords.
    // Child control rects are interpreted relative to the client area origin.
    JKRect GetClientRect() const { return clientRect_; }

    void SetFocusChild(JKControl* child);
    JKControl* GetFocusChild() { return focusChild_; }
    const JKControl* GetFocusChild() const { return focusChild_; }

    // Drops focusChild_ first when it (or one of its ancestors) is
    // close-requested, so the destruction pass cannot leave a dangling
    // focus reference (e.g. the combobox dropdown popup owns focus when the
    // user picks an item and it is removed).
    void RemoveClosedChildren() override;

    void FocusFirstChild();
    void FocusNextChild();
    void FocusPrevChild();

    void PaintWindow(JKDC& dc) override;
    void OnPaintClient(JKDC& dc) override;
    void OnClose() override;
    void RespondMessage(const JKEvent& ev) override;
    // 타이틀 바 스트립 (docs/67 단 2): 클라이언트 좌표 사각형(음수 y = 타이틀 바
    // 안). 이 안의 자식은 프레임 위에 그려지고 히트테스트된다. 비어 있으면 기존
    // 동작 그대로 — 타 앱 영향 0.
    void SetFrameStripRect(const JKRect& rect) { frameStripRect_ = rect; }
    const JKRect& GetFrameStripRect() const { return frameStripRect_; }
    // 와이어용 surface 로컬 좌표(메인 창이 surface를 채우므로 창 로컬 == surface
    // 로컬). clientRect_ 오프셋(kBorder/kTitle)을 여기서 더한다 — 상수 포킹 금지.
    JKRect GetFrameStripSurfaceRect() const;
    // JKControl::PaintClient는 screen CLIENT rect로 클립한다 — 음수 y 스트립
    // 자식이 타이틀 바에서 잘린다. 스트립 선언 시에만 클립을 창 전체로 넓힌다.
    void PaintClient(JKDC& dc) override;

    // Dirty-region management for partial redraw.
    void AddDirtyRect(const JKRect& screenRect);
    bool HasDirtyRects() const { return !dirtyRects_.empty(); }
    const std::vector<JKRect>& GetDirtyRects() const { return dirtyRects_; }
    void ClearDirtyRects();

protected:
    std::string title_;
    JKControl* focusChild_ = nullptr;
    // 프레임 스트립 클라이언트 좌표(음수 y 가능). 비어 있으면 패스스루/클립/
    // 히트테스트 확장 전부 무효(docs/67 단 2).
    JKRect frameStripRect_{};

    // 스크린 좌표계의 스트립 사각형 — HitTest/HitTestRegion/PaintWindow가
    // 같은 산식을 쓰므로 한 곳에서 뽑는다.
    JKRect FrameStripScreenRect() const;

    // 닫기 버튼 호버 (I4 소박): MouseMove 전이 감지 → 전이 시에만 더티.
    // 컴포지터 오버레이(JKCompositor.cpp:326)는 서버 측 복제 상태라 별도
    // 원장(눈확인 대기) — 클라 페인트만 먼저 봉합.
    bool closeHover_ = false;

    bool dragging_ = false;
    JKPoint dragStartMouse_;
    JKRect dragStartRect_;

    bool resizing_ = false;
    JKPoint resizeStartMouse_;
    JKRect resizeStartRect_;

    // Recompute the client area for border/title and relayout children.
    void OnRectChanged(const JKRect& rect) override;

    // No-op: OnRectChanged (fired by every SetRect) already lays children out
    // with the dock-aware passes; the blanket recursion would clobber it
    // (docs/64 §8).
    void LayoutChildren() override;

    JKRect GetCloseButtonRect() const;

    JKRect GetScreenClientRect() const override;

private:
    std::vector<JKRect> dirtyRects_;
};

} // namespace jk

#endif // JKWINDOW_H
