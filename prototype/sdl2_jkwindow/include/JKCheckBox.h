#ifndef JKCHECKBOX_H
#define JKCHECKBOX_H

#include <JKControl.h>

namespace jk {

class JKCheckBox : public JKControl {
public:
    JKCheckBox();
    explicit JKCheckBox(const JKRect& rect, uint16_t controlId = 0);

    void SetStatus(bool status) { status_ = status; }
    bool GetStatus() const { return status_; }
    void SetAutoControl(bool autoControl) { autoControl_ = autoControl; }

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

protected:
    bool status_ = false;
    bool autoControl_ = true;
};

} // namespace jk

#endif // JKCHECKBOX_H
