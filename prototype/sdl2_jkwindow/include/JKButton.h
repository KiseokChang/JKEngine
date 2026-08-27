#ifndef JKBUTTON_H
#define JKBUTTON_H

#include <JKStatic.h>
#include <functional>

namespace jk {

class JKButton : public JKStatic {
public:
    JKButton();
    explicit JKButton(const JKRect& rect, uint16_t controlId = 0);

    void SetEnable(bool enable) { enabled_ = enable; }
    bool IsEnabled() const { return enabled_; }

    void SetOnClick(std::function<void()> callback) { onClick_ = std::move(callback); }

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

    virtual void OnClick() {
        if (onClick_) {
            onClick_();
        }
    }

protected:
    bool status_ = false;
    bool selecting_ = false;
    bool enabled_ = true;
    int32_t depth_ = 2;
    std::function<void()> onClick_;
};

} // namespace jk

#endif // JKBUTTON_H
