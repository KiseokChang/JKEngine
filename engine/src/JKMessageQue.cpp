#include <JKMessageQue.h>

namespace jk {

void JKMessageQue::Push(const JKEvent& ev) {
    queue_.push_back(ev);
}

bool JKMessageQue::Pop(JKEvent& out) {
    if (queue_.empty()) {
        return false;
    }
    out = queue_.front();
    queue_.pop_front();
    return true;
}

bool JKMessageQue::IsEmpty() const {
    return queue_.empty();
}

void JKMessageQue::Clear() {
    queue_.clear();
}

} // namespace jk
