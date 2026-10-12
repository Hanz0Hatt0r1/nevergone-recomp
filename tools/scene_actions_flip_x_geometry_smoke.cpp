#include <cassert>
#include <cmath>

#include "scene_actions_flip_x_geometry.h"

namespace {
bool near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
}

int main() {
    namespace base = nevergone::scene_actions_base_sprite_update;
    namespace flip = nevergone::scene_actions_flip_x_geometry;

    nevergone::enemy_actions_wbg_prefix::ActionFrameRecord frame;
    frame.float_values[0] = 20.0f;   // AFD +0x14
    frame.float_values[1] = 40.0f;   // AFD +0x18
    frame.float_values[2] = 0.25f;   // anchor X
    frame.float_values[3] = 0.75f;   // anchor Y
    frame.float_values[6] = 80.0f;   // AFD +0x1c
    frame.float_values[7] = 100.0f;  // AFD +0x20
    frame.float_values[9] = 30.0f;   // rotation
    frame.float_values[10] = 1.5f;
    frame.float_values[11] = 0.75f;

    base::SystemInputs system;
    system.scale_240 = 2.0f;
    system.position_offset_x_244 = 100.0f;
    system.position_offset_y_248 = 50.0f;
    system.flip_x_15e = true;

    const auto plan = flip::build(frame, system);
    assert(plan.has_value());
    // base=((20+100)/2,(40+50)/2)=(60,45); q=(40,50)
    // native q+(q-base with Y negated) => (20,45)
    assert(near(plan->base_x, 60.0f));
    assert(near(plan->base_y, 45.0f));
    assert(near(plan->q_x, 40.0f));
    assert(near(plan->q_y, 50.0f));
    assert(near(plan->position_x, 20.0f));
    assert(near(plan->position_y, 45.0f));
    assert(near(plan->anchor_x, 0.75f));
    assert(near(plan->anchor_y, 0.75f));
    assert(near(plan->rotation, -30.0f));
    assert(near(plan->scale_x, 3.0f));
    assert(near(plan->scale_y, 1.5f));
    assert(plan->flip_x);

    auto non_flip = system;
    non_flip.flip_x_15e = false;
    assert(!flip::build(frame, non_flip).has_value());
    auto zero = system;
    zero.scale_240 = 0.0f;
    assert(!flip::build(frame, zero).has_value());
    auto bad = frame;
    bad.float_values[6] = INFINITY;
    assert(!flip::build(bad, system).has_value());
    return 0;
}
