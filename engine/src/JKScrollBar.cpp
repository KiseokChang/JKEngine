#include <JKScrollBar.h>
#include <JKEvent.h>
#include <algorithm>

namespace jk {

JKScrollBar::JKScrollBar() = default;

JKScrollBar::JKScrollBar(const JKRect& rect, uint16_t controlId, ScrollBarDir dir)
    : dir_(dir) {
    SetRect(rect);
    SetControlId(controlId);
    SetBackColor(192, 192, 192);
    SetTextColor(0, 0, 0);
}

void JKScrollBar::SetRange(int32_t min, int32_t max, int32_t pageSize) {
    min_ = min;
    max_ = std::max(min, max);
    pageSize_ = std::max(1, pageSize);
    pos_ = std::max(min_, std::min(pos_, max_));
}

void JKScrollBar::SetPos(int32_t pos) {
    pos = std::max(min_, std::min(pos, max_));
    if (pos != pos_) {
        pos_ = pos;
        if (onScroll_) onScroll_(pos_);
    }
}

int32_t JKScrollBar::GetTrackSize() const {
    const JKRect client = GetScreenClientRect();
    return dir_ == ScrollBarDir::Vertical ? std::max(1, client.h - 32)
                                        : std::max(1, client.w - 32);
}

int32_t JKScrollBar::GetThumbSize() const {
    int32_t track = GetTrackSize();
    int32_t range = max_ - min_ + pageSize_;
    if (range <= 0) return track;
    int32_t size = track * pageSize_ / range;
    return std::max(8, size);
}

int32_t JKScrollBar::GetThumbPos() const {
    int32_t track = GetTrackSize();
    int32_t thumb = GetThumbSize();
    int32_t avail = track - thumb;
    if (max_ <= min_ || avail <= 0) return 0;
    int32_t p = (pos_ - min_) * avail / (max_ - min_);
    return std::max(0, std::min(p, avail));
}

void JKScrollBar::SetPosFromMouse(int32_t mouseCoord) {
    const JKRect client = GetScreenClientRect();
    int32_t trackStart = (dir_ == ScrollBarDir::Vertical ? client.y + 16 : client.x + 16);
    int32_t track = GetTrackSize();
    int32_t thumb = GetThumbSize();
    int32_t avail = track - thumb;
    if (avail <= 0) return;
    int32_t rel = mouseCoord - trackStart - thumb / 2;
    int32_t newPos = min_ + rel * (max_ - min_) / avail;
    SetPos(newPos);
}

void JKScrollBar::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.Box3D(client, 1, 192, 192, 192, 255, 255, 255, 0, 0, 0);

    JKRect track = client;
    if (dir_ == ScrollBarDir::Vertical) {
        track.x += 2; track.w -= 4;
        track.y += 16; track.h -= 32;
    } else {
        track.y += 2; track.h -= 4;
        track.x += 16; track.w -= 32;
    }
    dc.SetColor(220, 220, 220, 255);
    dc.FillRect(track);

    int32_t thumbSize = GetThumbSize();
    int32_t thumbPos = GetThumbPos();
    JKRect thumb;
    if (dir_ == ScrollBarDir::Vertical) {
        thumb = JKRect{ track.x, track.y + thumbPos, track.w, thumbSize };
    } else {
        thumb = JKRect{ track.x + thumbPos, track.y, thumbSize, track.h };
    }
    dc.Box3D(thumb, 1, 255, 255, 255, 255, 255, 255, 128, 128, 128);

    JKControl::OnPaintClient(dc);
}

void JKScrollBar::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();
        dragging_ = true;
        SetPosFromMouse(dir_ == ScrollBarDir::Vertical ? ev.y : ev.x);
    } else if (ev.type == JKEventType::MouseMove) {
        if (dragging_) {
            SetPosFromMouse(dir_ == ScrollBarDir::Vertical ? ev.y : ev.x);
        }
    } else if (ev.type == JKEventType::MouseUp) {
        dragging_ = false;
    }
    JKControl::RespondMessage(ev);
}

} // namespace jk
