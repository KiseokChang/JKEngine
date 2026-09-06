#ifndef JKTIMERTHREAD_H
#define JKTIMERTHREAD_H

#include <JKMessageBus.h>
#include <JKEvent.h>
#include <atomic>
#include <thread>
#include <set>
#include <cstdint>

namespace jk {

// Timer thread owns the deadline heap and posts JKEventType::Timer messages to
// the message bus Timer channel. This replaces the deadline-based scheduler
// that previously ran in the application thread.
class JKTimerThread {
public:
    JKTimerThread();
    ~JKTimerThread();

    void Start(JKMessageBus* bus);
    void Stop();

    // Register a repeating or one-shot timer. Returns a handle that can be
    // passed to RemoveTimer. The resulting Timer event has winId set to the
    // given winId.
    uint64_t AddTimer(uint32_t winId, uint32_t intervalMs, bool repeat);
    void RemoveTimer(uint64_t handle);
    void RemoveTimersForWindow(uint32_t winId);

    void SetLegacyTimer(uint32_t winId, uint32_t intervalMs);
    void ClearLegacyTimer();

private:
    struct DeadlineTimer {
        uint64_t handle = 0;
        uint32_t winId = 0;
        uint32_t intervalMs = 0;
        uint32_t deadline = 0;
        bool repeat = false;

        bool operator<(const DeadlineTimer& other) const {
            return deadline < other.deadline;
        }
    };

    JKMessageBus* bus_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread thread_;

    mutable std::mutex timersMutex_;
    std::multiset<DeadlineTimer> timers_;
    uint64_t nextHandle_ = 1;

    uint32_t legacyWinId_ = 0;
    uint32_t legacyIntervalMs_ = 0;
    uint32_t lastLegacyTick_ = 0;

    void Run();
};

} // namespace jk

#endif // JKTIMERTHREAD_H
