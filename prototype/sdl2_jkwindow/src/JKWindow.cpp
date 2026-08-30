#include <JKWindow.h>
#include <JKApplication.h>
#include <algorithm>
#include <cstdio>

namespace jk {

namespace {

void CollectFocusableControls(JKControl* root, std::vector<JKControl*>& out) {
    if (!root || !root->IsVisible()) return;
    if (root->IsFocusable()) out.push_back(root);
    for (const auto& child : root->GetChildren()) {
        CollectFocusableControls(child.get(), out);
    }
}

} // anonymous namespace

JKWindow::JKWindow() = default;

JKWindow::JKWindow(const std::string& title) : title_(title) {
}

void JKWindow::SetTitle(const std::string& title) {
    title_ = title;
}

const std::string& JKWindow::GetTitle() const {
    return title_;
}

void JKWindow::OnRectChanged(const JKRect& rect) {
    // JKControl::SetRect은 padding을 기반으로 clientRect_를 계산하지만,
    // JKWindow는 테두리/타이틀 크기를 유지한 클라이언트 영역을 직접 계산한다.
    constexpr int32_t kBorder = 2;
    constexpr int32_t kTitle  = 24;
    JKRect client;
    client.x = kBorder;
    client.y = kTitle;
    client.w = rect.w - kBorder * 2;
    if (client.w < 0) client.w = 0;
    client.h = rect.h - kTitle - kBorder;
    if (client.h < 0) client.h = 0;
    SetClientRect(client);

    // Anchored / autosized children are relaid out when the window resizes.
    // JKWindow는 자기 자신을 PerformLayout() 대상으로 삼으면
    // JKControl::PerformLayout -> SetRect -> PerformLayout 무한 재귀에 빠진다.
    // 직접 자식들만 재배치한다.
    const JKRect& clientRect = GetClientRect();
    for (auto& child : children_) {
        child->PerformLayout(clientRect);
    }
}

void JKWindow::SetWindowRect(const JKRect& rect) {
    // SDL 논리 좌표를 그대로 사용: 테두리 2pt, 타이틀 24pt를 고정한다.
    // SDL_RenderSetScale()이 물리 픽셀로 확대/축소한다.
    SetRect(rect);

#ifdef DEBUG
    const JKRect& client = GetClientRect();
    std::printf("[SetWindowRect] title='%s' border=%d titleH=%d "
                "rect=(%d,%d %dx%d) client=(%d,%d %dx%d)\n",
                title_.c_str(), 2, 24,
                rect.x, rect.y, rect.w, rect.h,
                client.x, client.y, client.w, client.h);
#endif
}

void JKWindow::MoveWindow(int32_t dx, int32_t dy) {
    JKRect r = GetRect();
    r.x += dx;
    r.y += dy;
    SetRect(r);  // OnRectChanged hook이 clientRect_를 재계산한다.
}

void JKWindow::MoveTo(int32_t x, int32_t y) {
    JKRect r = GetRect();
    r.x = x;
    r.y = y;
    SetRect(r);  // OnRectChanged hook이 clientRect_를 재계산한다.
}

void JKWindow::ResizeWindow(int32_t dx, int32_t dy) {
    JKRect r = GetRect();
    r.w += dx;
    r.h += dy;
    if (r.w < 64) r.w = 64;
    if (r.h < 48) r.h = 48;
    SetWindowRect(r);
}

JKWindow::WindowRegion JKWindow::HitTestRegion(int32_t screenX, int32_t screenY) const {
    const JKRect screenRect = GetScreenRect();
    if (!screenRect.Contains(screenX, screenY)) {
        return WindowRegion::None;
    }
    const JKRect screenClient = GetScreenClientRect();
    if (screenClient.Contains(screenX, screenY)) {
        return WindowRegion::Client;
    }
    if (screenY < screenRect.y + clientRect_.y) {
        return WindowRegion::TitleBar;
    }
    return WindowRegion::Border;
}

JKControl* JKWindow::HitTest(int32_t x, int32_t y) {
    const JKRect screenClient = GetScreenClientRect();
    if (screenClient.Contains(x, y)) {
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            if (!(*it)->IsVisible()) continue;
            JKControl* found = (*it)->HitTest(x, y);
            if (found) return found;
        }
    }
    return this;
}

JKRect JKWindow::GetScreenClientRect() const {
    const JKRect screen = GetScreenRect();
    return JKRect{ screen.x + clientRect_.x, screen.y + clientRect_.y,
                   clientRect_.w, clientRect_.h };
}

JKRect JKWindow::GetCloseButtonRect() const {
    if (!parent_) {
        return JKRect{ 0, 0, 0, 0 };
    }
    const JKRect screenRect = GetScreenRect();
    constexpr int32_t kButtonSize = 20;
    constexpr int32_t kMargin = 2;
    return JKRect{
        screenRect.x + screenRect.w - kButtonSize - kMargin,
        screenRect.y + kMargin,
        kButtonSize,
        kButtonSize
    };
}

void JKWindow::PaintWindow(JKDC& dc) {
    const JKRect screenRect = GetScreenRect();

    // 윈도우 프레임(타이틀 바, 테두리)은 윈도우 화면 영역으로 클립한다.
    dc.PushClipRect(screenRect);

    // 타이틀 바 (classic blue). SDL 논리 높이 clientRect_.y를 사용하며,
    // SDL_RenderSetScale()이 HiDPI 물리 픽셀로 변환한다.
    JKRect titleBar = screenRect;
    titleBar.h = clientRect_.y;
    dc.SetColor(0, 0, 128, 255);
    dc.FillRect(titleBar);

    // 닫기 버튼은 부모가 있는 떠 있는 윈도우에만 표시한다.
    const JKRect closeBtn = GetCloseButtonRect();
    const int32_t closeReserve = closeBtn.IsEmpty() ? 0 : closeBtn.w + 4;

    // 타이틀 텍스트 (bitmap font).
    if (!title_.empty()) {
        dc.SetTextColor(255, 255, 255);
        dc.SetBackColor(0, 0, 128);
        JKRect textRect = titleBar;
        // 안쪽 여백 4px, 닫기 버튼이 있으면 우측 여유를 추가한다.
        textRect.x += 4;
        textRect.w -= 8 + closeReserve;
        dc.TextOutX(textRect, title_.c_str(), ADJ_YCENTER | ADJ_LEFT, false);
    }

    // 닫기 버튼: 회색 배경에 흰색 ×.
    if (!closeBtn.IsEmpty()) {
        dc.SetColor(192, 192, 192, 255);
        dc.FillRect(closeBtn);
        dc.SetColor(0, 0, 0, 255);
        dc.DrawRect(closeBtn);

        constexpr int32_t kPad = 5;
        dc.SetColor(255, 255, 255, 255);
        dc.DrawLine(closeBtn.x + kPad, closeBtn.y + kPad,
                    closeBtn.x + closeBtn.w - kPad - 1,
                    closeBtn.y + closeBtn.h - kPad - 1);
        dc.DrawLine(closeBtn.x + closeBtn.w - kPad - 1, closeBtn.y + kPad,
                    closeBtn.x + kPad,
                    closeBtn.y + closeBtn.h - kPad - 1);
    }

    // 테두리
    dc.SetColor(192, 192, 192, 255);
    dc.DrawRect(screenRect);

    dc.PopClipRect();
}

void JKWindow::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();

    // 클라이언트 배경
    dc.SetColor(240, 240, 240, 255);
    dc.FillRect(client);

    // 자식 컨트롤 그리기. 자식이 JKWindow이면 프레임(PaintWindow)도 함께 그립니다.
    for (auto& child : children_) {
        if (!child->IsVisible()) continue;
        JKWindow* childWin = dynamic_cast<JKWindow*>(child.get());
        if (childWin) {
            childWin->PaintWindow(dc);
        }
        child->PaintClient(dc);
    }
}

void JKWindow::FocusFirstChild() {
    std::vector<JKControl*> candidates;
    CollectFocusableControls(this, candidates);
    if (!candidates.empty()) {
        candidates[0]->SetFocus();
    }
}

void JKWindow::FocusNextChild() {
    std::vector<JKControl*> candidates;
    CollectFocusableControls(this, candidates);
    if (candidates.empty()) return;
    size_t idx = 0;
    if (focusChild_) {
        auto it = std::find(candidates.begin(), candidates.end(), focusChild_);
        if (it != candidates.end()) idx = static_cast<size_t>(it - candidates.begin());
    }
    idx = (idx + 1) % candidates.size();
    candidates[idx]->SetFocus();
}

void JKWindow::FocusPrevChild() {
    std::vector<JKControl*> candidates;
    CollectFocusableControls(this, candidates);
    if (candidates.empty()) return;
    size_t idx = 0;
    if (focusChild_) {
        auto it = std::find(candidates.begin(), candidates.end(), focusChild_);
        if (it != candidates.end()) idx = static_cast<size_t>(it - candidates.begin());
    }
    idx = (idx + candidates.size() - 1) % candidates.size();
    candidates[idx]->SetFocus();
}

void JKWindow::SetFocusChild(JKControl* child) {
    focusChild_ = child;
}

void JKWindow::RespondMessage(const JKEvent& ev) {
    // 키/문자 입력은 포커스를 가진 자식 컨트롤로 전달한다.
    if (ev.type == JKEventType::KeyDown ||
        ev.type == JKEventType::KeyUp ||
        ev.type == JKEventType::Char) {
        if (focusChild_) {
            focusChild_->RespondMessage(ev);
            return;
        }
    }

    // 타이머(캐럿 깜박임 등)은 포커스를 가진 자식 컨트롤로 전달한다.
    // 포커스 컨트롤이 없으면 JKControl 기본 동작처럼 모든 자식에게 전파한다.
    if (ev.type == JKEventType::Timer) {
        if (focusChild_) {
            focusChild_->RespondMessage(ev);
        } else {
            JKControl::RespondMessage(ev);
        }
        return;
    }

    if (ev.type == JKEventType::MouseDown ||
        ev.type == JKEventType::MouseUp ||
        ev.type == JKEventType::MouseMove) {
        if (ev.type == JKEventType::MouseDown) {
            const JKRect closeBtn = GetCloseButtonRect();
            if (!closeBtn.IsEmpty() && closeBtn.Contains(ev.x, ev.y)) {
                RequestClose();
                return;
            }

            WindowRegion region = HitTestRegion(ev.x, ev.y);
            const JKRect sr = GetScreenRect();
#ifdef DEBUG
            std::printf("[MouseDown] target='%s' ev=(%d,%d) screen=(%d,%d %dx%d) region=%d\n",
                        title_.c_str(), ev.x, ev.y, sr.x, sr.y, sr.w, sr.h,
                        static_cast<int>(region));
#endif
            if (region == WindowRegion::TitleBar && HasAttrFlag(WA_TITLEMOVEABLE)) {
                dragging_ = true;
                dragStartMouse_ = JKPoint{ ev.x, ev.y };
                dragStartRect_ = GetRect();
                return;
            } else if (region == WindowRegion::Border && HasAttrFlag(WA_BORDERRESIZABLE)) {
                // 단순화: 우측 하단 모서리에서만 크기 조정 (물리 픽셀 10px 핫스팟).
                const int32_t hotX = 10;
                const int32_t hotY = 10;
                if (ev.x >= sr.x + sr.w - hotX && ev.y >= sr.y + sr.h - hotY) {
                    resizing_ = true;
                    resizeStartMouse_ = JKPoint{ ev.x, ev.y };
                    resizeStartRect_ = GetRect();
                    return;
                }
            }
        }

        // 드래그/리사이즈 중인 동안은 이 윈도우가 직접 처리하고 자식에게 전달하지 않는다.
        if (dragging_ || resizing_) {
            if (ev.type == JKEventType::MouseMove) {
                if (dragging_) {
                    MoveWindow(ev.dx, ev.dy);
                } else if (resizing_) {
                    ResizeWindow(ev.dx, ev.dy);
                }
            } else if (ev.type == JKEventType::MouseUp) {
                dragging_ = false;
                resizing_ = false;
            }
            return;
        }

        // 클라이언트 영역의 자식 컨트롤로 이벤트를 전달한다.
        JKControl* target = HitTest(ev.x, ev.y);
        if (target && target != this) {
            if (ev.type == JKEventType::MouseDown) {
                target->SetFocus();
                if (g_currentJKApp) g_currentJKApp->SetInputWindow(this);
            }
            target->RespondMessage(ev);
        }
    }
}

} // namespace jk
