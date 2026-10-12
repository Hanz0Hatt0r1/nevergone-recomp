#include <cassert>

#include "scene_actions_klhuo_system_inputs.h"

int main() {
    const auto inputs = nevergone::scene_actions_klhuo_system_inputs::initial();
    assert(inputs.scale_240 == 1.0f);
    assert(inputs.position_offset_x_244 == 0.0f);
    assert(inputs.position_offset_y_248 == 0.0f);
    assert(!inputs.flip_x_15e);
    return 0;
}
