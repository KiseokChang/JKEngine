#include <JKControl.h>
#include <JKWindow.h>
#include <JKApplication.h>
#include <algorithm>

namespace jk {

static uint32_t s_nextWinId = 1;

JKControl::JKControl() {
    winId_ = s_nextWinId++;
}

JKControl::~JKControl() = default;

void JKControl::Init() {
    for (auto& child : children_) {
        child->Init();
    }
}

void JKControl::Setup() {
    for (auto& child : children_) {
        child->Setup();
    }
}

void JKControl::Open() {
    visible_ = true;
    for (auto& child : children_) {
        child->Open();
    }
}

void JKControl::Close() {
    for (auto& child : children_) {
        child->Close();
    }
    visible_ = false;
}

void JKControl::Show() {
    visible_ = true;
}

void JKControl::Hide() {
    visible_ = false;
}

bool JKControl::IsVisible() const {
    return visible_;
}

void JKControl::PaintWindow(JKDC& dc) {
    (void)dc;
}

void JKControl::PaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    if (!client.IsEmpty()) {
        dc.PushClipRect(client);
        OnPaintClient(dc);
        PaintFocus(dc);
        dc.PopClipRect();
    } else {
        OnPaintClient(dc);
        PaintFocus(dc);
    }
}

void JKControl::OnPaintClient(JKDC& dc) {
    for (auto& child : children_) {
        if (child->IsVisible()) {
            child->PaintClient(dc);
        }
    }
}

void JKControl::RespondMessage(const JKEvent& ev) {
    // Timer 이벤트는 모든 자식 컨트롤로 전파한다.
    if (ev.type == JKEventType::Timer) {
        for (auto& child : children_) {
            if (child->IsVisible()) {
                child->RespondMessage(ev);
            }
        }
    }
}

void JKControl::SetText(const std::string& text) {
    text_ = text;
}

const std::string& JKControl::GetText() const {
    return text_;
}

void JKControl::SetTextColor(uint8_t r, uint8_t g, uint8_t b) {
    textR_ = r;
    textG_ = g;
    textB_ = b;
}

void JKControl::GetTextColor(uint8_t& r, uint8_t& g, uint8_t& b) const {
    r = textR_;
    g = textG_;
    b = textB_;
}

void JKControl::SetBackColor(uint8_t r, uint8_t g, uint8_t b) {
    backR_ = r;
    backG_ = g;
    backB_ = b;
}

void JKControl::GetBackColor(uint8_t& r, uint8_t& g, uint8_t& b) const {
    r = backR_;
    g = backG_;
    b = backB_;
}

void JKControl::SetRect(const JKRect& rect) {
    rect_ = rect;
    clientRect_ = rect;
}

const JKRect& JKControl::GetRect() const {
    return rect_;
}

void JKControl::SetClientRect(const JKRect& rect) {
    clientRect_ = rect;
}

const JKRect& JKControl::GetClientRect() const {
    return clientRect_;
}

void JKControl::SetAttrFlags(uint32_t flags) {
    attrFlags_ = flags;
}

uint32_t JKControl::GetAttrFlags() const {
    return attrFlags_;
}

bool JKControl::HasAttrFlag(uint32_t flag) const {
    return (attrFlags_ & flag) != 0;
}

JKRect JKControl::GetScreenRect() const {
    if (!parent_) {
        return rect_;
    }
    const JKRect parentClient = parent_->GetScreenClientRect();
    return rect_.OffsetBy(parentClient.x, parentClient.y);
}

JKRect JKControl::GetScreenClientRect() const {
    return GetScreenRect();
}

JKControl* JKControl::HitTest(int32_t x, int32_t y) {
    const JKRect screen = GetScreenRect();
    if (!screen.Contains(x, y)) return nullptr;
    const JKRect client = GetScreenClientRect();
    if (client.Contains(x, y)) {
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            if (!(*it)->IsVisible()) continue;
            JKControl* found = (*it)->HitTest(x, y);
            if (found) return found;
        }
    }
    return this;
}

void JKControl::SetParent(JKControl* parent) {
    parent_ = parent;
}

JKControl* JKControl::GetParent() const {
    return parent_;
}

void JKControl::SetFocus() {
    JKControl* p = parent_;
    while (p) {
        JKWindow* win = dynamic_cast<JKWindow*>(p);
        if (win) {
            if (win->GetFocusChild() == this) {
                if (g_currentJKApp) g_currentJKApp->SetInputWindow(win);
                return;
            }
            JKControl* old = win->GetFocusChild();
            if (old) old->OnKillFocus();
            win->SetFocusChild(this);
            OnSetFocus();
            if (g_currentJKApp) g_currentJKApp->SetInputWindow(win);
            return;
        }
        p = p->GetParent();
    }
}

bool JKControl::IsFocused() const {
    const JKControl* p = this;
    while (p) {
        const JKWindow* win = dynamic_cast<const JKWindow*>(p);
        if (win) {
            return win->GetFocusChild() == this;
        }
        p = p->GetParent();
    }
    return false;
}

void JKControl::PaintFocus(JKDC& dc) const {
    if (!focusable_ || !IsFocused()) return;
    const JKRect client = GetScreenClientRect();
    if (client.IsEmpty()) return;
    JKRect r = client;
    r.x += 2;
    r.y += 2;
    r.w -= 4;
    r.h -= 4;
    if (r.IsEmpty()) return;
    dc.SetColor(0, 0, 255, 255);
    dc.DrawRect(r);
}

void JKControl::AddControl(std::unique_ptr<JKControl> child) {
    if (child) {
        child->SetParent(this);
        children_.push_back(std::move(child));
    }
}

JKControl* JKControl::FindControlById(uint32_t winId) {
    if (winId_ == winId) {
        return this;
    }
    for (auto& child : children_) {
        JKControl* found = child->FindControlById(winId);
        if (found) return found;
    }
    return nullptr;
}

JKControl* JKControl::FindControlByControlId(uint16_t controlId) {
    if (controlId_ == controlId) {
        return this;
    }
    for (auto& child : children_) {
        JKControl* found = child->FindControlByControlId(controlId);
        if (found) return found;
    }
    return nullptr;
}

const std::vector<std::unique_ptr<JKControl>>& JKControl::GetChildren() const {
    return children_;
}

void JKControl::RequestClose() {
    closeRequested_ = true;
    Hide();
}

bool JKControl::IsCloseRequested() const {
    return closeRequested_;
}

void JKControl::RemoveClosedChildren() {
    for (auto& child : children_) {
        child->RemoveClosedChildren();
    }
    auto it = std::remove_if(children_.begin(), children_.end(),
        [](const std::unique_ptr<JKControl>& child) {
            return child->IsCloseRequested();
        });
    children_.erase(it, children_.end());
}

} // namespace jk
