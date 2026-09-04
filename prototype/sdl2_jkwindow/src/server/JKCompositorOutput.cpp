#include <server/JKCompositorOutput.h>

namespace jk {
namespace server {

JKCompositorOutput::JKCompositorOutput(int index, const JKRect& bounds, float scale)
    : index_(index), bounds_(bounds), scale_(scale) {
}

} // namespace server
} // namespace jk
