#include <JKControl.h>

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
        dc.PopClipRect();
    } else {
        OnPaintClient(dc);
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
    (void)ev;
}

void JKControl::SetText(const std::string& text) {
    text_ = text;
}

const std::string& JKControl::GetText() const {
    return text_;
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

void JKControl::SetParent(JKControl* parent) {
    parent_ = parent;
}

JKControl* JKControl::GetParent() const {
    return parent_;
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

} // namespace jk
