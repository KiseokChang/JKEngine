#ifndef JOCLOCK_H
#define JOCLOCK_H

#include <JKStatic.h>

namespace jk {

class JOClock : public JKStatic {
public:
    JOClock();
    explicit JOClock(const JKRect& rect, uint16_t controlId = 0);

    void RespondMessage(const JKEvent& ev) override;

private:
    void UpdateTimeText();
};

} // namespace jk

#endif // JOCLOCK_H
