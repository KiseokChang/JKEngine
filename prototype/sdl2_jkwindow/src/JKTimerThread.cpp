#include <JKTimerThread.h>
#include <SDL.h>
#include <cstdio>

namespace jk {

JKTimerThread::JKTimerThread() = default;

JKTimerThread::~JKTimerThread() {
    Stop();
}

void JKTimerThread::Start(JKMessageBus* bus) {
    if (running_ || thread_.joinable()) return;
    bus_ = bus;
    running_ = true;
    thread_ = std::thread(&JKTimerThread::Run, this);
}

void JKTimerThread::Stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    bus_ = nullptr;
}

void JKTimerThread::Run() {
    uint32_t lastTick = SDL_GetTicks();
    while (running_) {
        const uint32_t now = SDL_GetTicks();
        uint32_t nextDeadline = 0;

        {
            std::lock_guard<std::mutex> lock(timersMutex_);

            // Legacy timer.
            if (legacyIntervalMs_ > 0) {
                if (now - lastLegacyTick_ >= legacyIntervalMs_) {
                    lastLegacyTick_ = now;
                    if (bus_) {
                        JKEvent ev;
                        ev.type = JKEventType::Timer;
                        ev.targetId = legacyWinId_;
                        ev.winId = legacyWinId_;
                        bus_->Push(JKMessageBus::Channel::Timer, ev);
                    }
                }
                nextDeadline = lastLegacyTick_ + legacyIntervalMs_;
            }

            // Deadline-based timers.
            while (!timers_.empty() && timers_.begin()->deadline <= now) {
                DeadlineTimer t = *timers_.begin();
                timers_.erase(timers_.begin());
                if (bus_) {
                    JKEvent ev;
                    ev.type = JKEventType::Timer;
                    ev.targetId = t.winId;
                    ev.winId = t.winId;
                    bus_->Push(JKMessageBus::Channel::Timer, ev);
                }
                if (t.repeat) {
                    t.deadline = now + t.intervalMs;
                    timers_.insert(t);
                }
            }

            if (!timers_.empty()) {
                uint32_t tDeadline = timers_.begin()->deadline;
                if (nextDeadline == 0 || tDeadline < nextDeadline) {
                    nextDeadline = tDeadline;
                }
            }
        }

        // Sleep until the next deadline or a short polling interval.
        if (nextDeadline > now) {
            uint32_t waitMs = nextDeadline - now;
            if (waitMs > 16) waitMs = 16;
            SDL_Delay(waitMs);
        } else {
            SDL_Delay(1);
        }

        (void)lastTick; // unused fallback
    }
}

uint64_t JKTimerThread::AddTimer(uint32_t winId, uint32_t intervalMs, bool repeat) {
    if (intervalMs == 0) return 0;
    std::lock_guard<std::mutex> lock(timersMutex_);
    DeadlineTimer t;
    t.handle = nextHandle_++;
    t.winId = winId;
    t.intervalMs = intervalMs;
    t.deadline = SDL_GetTicks() + intervalMs;
    t.repeat = repeat;
    timers_.insert(t);
    return t.handle;
}

void JKTimerThread::RemoveTimer(uint64_t handle) {
    std::lock_guard<std::mutex> lock(timersMutex_);
    for (auto it = timers_.begin(); it != timers_.end(); ++it) {
        if (it->handle == handle) {
            timers_.erase(it);
            return;
        }
    }
}

void JKTimerThread::RemoveTimersForWindow(uint32_t winId) {
    std::lock_guard<std::mutex> lock(timersMutex_);
    for (auto it = timers_.begin(); it != timers_.end(); ) {
        if (it->winId == winId) {
            it = timers_.erase(it);
        } else {
            ++it;
        }
    }
}

void JKTimerThread::SetLegacyTimer(uint32_t winId, uint32_t intervalMs) {
    std::lock_guard<std::mutex> lock(timersMutex_);
    legacyWinId_ = winId;
    legacyIntervalMs_ = intervalMs;
    lastLegacyTick_ = SDL_GetTicks();
}

void JKTimerThread::ClearLegacyTimer() {
    std::lock_guard<std::mutex> lock(timersMutex_);
    legacyIntervalMs_ = 0;
}

} // namespace jk
