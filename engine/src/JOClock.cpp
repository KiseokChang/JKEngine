#include <JOClock.h>
#include <JKEvent.h>
#include <cstdio>
#include <ctime>

namespace jk {

JOClock::JOClock() = default;

JOClock::JOClock(const JKRect& rect, uint16_t controlId) {
    SetRect(rect);
    SetControlId(controlId);
    SetAdjustFlag(ADJ_XYCENTER);
    SetBackColor(192, 192, 192);
    SetTextColor(0, 0, 0);
    UpdateTimeText();
}

void JOClock::UpdateTimeText() {
    time_t now = std::time(nullptr);
    tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
    SetText(buf);
}

void JOClock::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer) {
        UpdateTimeText();
    }
    JKStatic::RespondMessage(ev);
}

} // namespace jk
