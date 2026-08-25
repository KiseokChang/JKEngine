#ifndef JKMESSAGEQUE_H
#define JKMESSAGEQUE_H

#include <JKEvent.h>
#include <deque>

namespace jk {

class JKMessageQue {
public:
    void Push(const JKEvent& ev);
    bool Pop(JKEvent& out);
    bool IsEmpty() const;
    void Clear();

private:
    std::deque<JKEvent> queue_;
};

} // namespace jk

#endif // JKMESSAGEQUE_H
