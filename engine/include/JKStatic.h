#ifndef JKSTATIC_H
#define JKSTATIC_H

#include <JKControl.h>

namespace jk {

class JKStatic : public JKControl {
public:
    JKStatic();
    explicit JKStatic(const JKRect& rect, uint16_t controlId = 0);

    void SetAdjustFlag(uint8_t flag) { adjustFlag_ = flag; }
    uint8_t GetAdjustFlag() const { return adjustFlag_; }

    void OnPaintClient(JKDC& dc) override;
    JKPoint MeasureContent() const override;

protected:
    uint8_t adjustFlag_ = 0;
};

} // namespace jk

#endif // JKSTATIC_H
