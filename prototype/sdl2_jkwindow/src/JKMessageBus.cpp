#include <JKMessageBus.h>
#include <algorithm>

namespace jk {

size_t JKMessageBus::ChannelIndex(Channel ch) const {
    size_t idx = static_cast<size_t>(ch);
    if (idx >= channels_.size()) idx = 0;
    return idx;
}

void JKMessageBus::Push(Channel ch, const Payload& payload) {
    auto& q = channels_[ChannelIndex(ch)];
    {
        std::lock_guard<std::mutex> lock(q.mutex);
        q.queue.push_back(payload);
    }
    q.cond.notify_one();
}

void JKMessageBus::Push(Channel ch, const JKEvent& ev) {
    Push(ch, Payload(ev));
}

bool JKMessageBus::Pop(Channel ch, Payload& out) {
    auto& q = channels_[ChannelIndex(ch)];
    std::lock_guard<std::mutex> lock(q.mutex);
    if (q.queue.empty()) return false;
    out = std::move(q.queue.front());
    q.queue.pop_front();
    return true;
}

bool JKMessageBus::PopWait(Channel ch, Payload& out, uint32_t timeoutMs) {
    auto& q = channels_[ChannelIndex(ch)];
    std::unique_lock<std::mutex> lock(q.mutex);
    if (timeoutMs == 0) {
        q.cond.wait(lock, [&q]() { return !q.queue.empty(); });
    } else {
        if (!q.cond.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                             [&q]() { return !q.queue.empty(); })) {
            return false;
        }
    }
    if (q.queue.empty()) return false;
    out = std::move(q.queue.front());
    q.queue.pop_front();
    return true;
}

bool JKMessageBus::IsEmpty(Channel ch) const {
    const auto& q = channels_[ChannelIndex(ch)];
    std::lock_guard<std::mutex> lock(q.mutex);
    return q.queue.empty();
}

void JKMessageBus::Clear(Channel ch) {
    auto& q = channels_[ChannelIndex(ch)];
    std::lock_guard<std::mutex> lock(q.mutex);
    q.queue.clear();
}

void JKMessageBus::ClearAll() {
    for (size_t i = 0; i < channels_.size(); ++i) {
        Clear(static_cast<Channel>(i));
    }
}

} // namespace jk
