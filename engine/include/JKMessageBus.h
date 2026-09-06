#ifndef JKMESSAGEBUS_H
#define JKMESSAGEBUS_H

#include <JKEvent.h>
#include <SDL.h>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <functional>
#include <cstdint>

namespace jk {

// Thread-safe multi-channel message bus used by Phase 1 thread separation.
// Channels:
//   Input  - SDL event thread -> application thread (JKEvent messages)
//   Timer  - timer thread -> application thread (JKEvent messages)
//   Render - application thread -> render thread (SDL event + scene description)
//   Audio  - application thread -> audio thread (audio commands)
class JKMessageBus {
public:
    enum class Channel {
        Input,
        Timer,
        Render,
        Audio,
        Count
    };

    // Generic payload that can carry either a JKEvent or binary data (used for
    // render scene descriptions and audio commands).
    struct Payload {
        uint32_t type = 0;
        JKEvent event{};
        std::vector<uint8_t> data;

        Payload() = default;
        explicit Payload(const JKEvent& ev) : type(0), event(ev) {}
        Payload(uint32_t t, std::vector<uint8_t> d) : type(t), data(std::move(d)) {}
    };

    void Push(Channel ch, const Payload& payload);
    void Push(Channel ch, const JKEvent& ev);

    // Non-blocking pop. Returns true if a payload was retrieved.
    bool Pop(Channel ch, Payload& out);

    // Blocking pop with timeout. Returns true if a payload was retrieved before
    // the timeout expired. A timeout of 0 means wait indefinitely.
    bool PopWait(Channel ch, Payload& out, uint32_t timeoutMs);

    bool IsEmpty(Channel ch) const;
    void Clear(Channel ch);
    void ClearAll();

private:
    struct ChannelQueue {
        mutable std::mutex mutex;
        std::condition_variable cond;
        std::deque<Payload> queue;
    };

    std::array<ChannelQueue, static_cast<size_t>(Channel::Count)> channels_;

    size_t ChannelIndex(Channel ch) const;
};

} // namespace jk

#endif // JKMESSAGEBUS_H
