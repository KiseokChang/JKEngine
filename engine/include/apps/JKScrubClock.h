// Position-tracking scrub clock (docs/50 §11, spec 2026-09-24 §3.2).
// The drag/wheel/reverse inputs feed a TARGET T; the DISPLAY position D
// chases it at up to kFlowMax frames per UI frame — continuous flow, not
// a snap. Pure logic, SDL-free (JKTermSelection.h precedent).
#pragma once
#include <algorithm>

namespace jk {

class JKScrubClock {
public:
    // Session start: D seeds at the live playback position; T starts there
    // too so a session opened without input is stationary.
    void Reset(double pos) { d_ = t_ = pos; }

    // Inputs converge: dial dθ, wheel ticks, reverse cadence all SetTarget.
    void SetTarget(double t) { t_ = t; }

    double Target() const { return t_; }
    double Pos() const { return d_; }

    // One UI frame: move D toward T by at most flowMax frames. fps<=0
    // quantizes at 30 fps (spec §4: the dial is time-accumulated either way).
    double Chase(double fps, double flowMax, double dur) {
        const double f = fps > 0.0 ? fps : 30.0;
        const double step = flowMax / f;
        if (t_ > d_)      d_ = std::min(d_ + step, t_);
        else if (t_ < d_) d_ = std::max(d_ - step, t_);
        if (dur > 0.0) d_ = std::clamp(d_, 0.0, dur);
        return d_;
    }

private:
    double d_ = 0.0; // display position (seconds)
    double t_ = 0.0; // input target (seconds)
};

} // namespace jk