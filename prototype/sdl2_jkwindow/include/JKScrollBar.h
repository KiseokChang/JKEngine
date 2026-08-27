#ifndef JKSCROLLBAR_H
#define JKSCROLLBAR_H

#include <JKControl.h>
#include <functional>

namespace jk {

enum class ScrollBarDir { Horizontal, Vertical };

class JKScrollBar : public JKControl {
public:
    JKScrollBar();
    explicit JKScrollBar(const JKRect& rect, uint16_t controlId = 0,
                         ScrollBarDir dir = ScrollBarDir::Vertical);

    void SetRange(int32_t min, int32_t max, int32_t pageSize);
    void SetPos(int32_t pos);
    int32_t GetPos() const { return pos_; }

    void SetOnScroll(std::function<void(int32_t)> cb) { onScroll_ = std::move(cb); }

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

private:
    ScrollBarDir dir_ = ScrollBarDir::Vertical;
    int32_t min_ = 0;
    int32_t max_ = 100;
    int32_t pageSize_ = 10;
    int32_t pos_ = 0;

    bool dragging_ = false;

    std::function<void(int32_t)> onScroll_;

    int32_t GetTrackSize() const;
    int32_t GetThumbSize() const;
    int32_t GetThumbPos() const;
    void SetPosFromMouse(int32_t mouseCoord);
};

} // namespace jk

#endif // JKSCROLLBAR_H
