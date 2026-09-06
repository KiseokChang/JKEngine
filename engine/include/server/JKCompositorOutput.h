#ifndef JKCOMPOSITOROUTPUT_H
#define JKCOMPOSITOROUTPUT_H

#include <JKTypes.h>
#include <SDL.h>
#include <cstdint>

namespace jk {
namespace server {

// One display/output in the server compositor.
// For Phase 2 there is a single output; Phase 3 will support multiple displays.
class JKCompositorOutput {
public:
    JKCompositorOutput(int index, const JKRect& bounds, float scale);

    int Index() const { return index_; }
    JKRect Bounds() const { return bounds_; }
    float Scale() const { return scale_; }

private:
    int index_ = 0;
    JKRect bounds_;
    float scale_ = 1.0f;
};

} // namespace server
} // namespace jk

#endif // JKCOMPOSITOROUTPUT_H
