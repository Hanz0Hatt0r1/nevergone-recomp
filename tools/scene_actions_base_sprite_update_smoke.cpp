#include <cassert>
#include <cmath>

#include "scene_actions_base_sprite_update.h"

namespace {

bool near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    namespace update = nevergone::scene_actions_base_sprite_update;

    nevergone::enemy_actions_wbg_prefix::ActionFrameRecord frame;
    frame.first_string = "scene_effect.plist";
    frame.third_string = "scene_effect_03.png";
    frame.float_values[0] = 20.0f;
    frame.float_values[1] = 40.0f;
    frame.float_values[2] = 0.25f;
    frame.float_values[3] = 0.75f;
    frame.float_values[9] = 30.0f;
    frame.float_values[10] = 1.5f;
    frame.float_values[11] = 0.75f;

    update::SystemInputs system;
    system.scale_240 = 2.0f;
    system.position_offset_x_244 = 100.0f;
    system.position_offset_y_248 = 50.0f;

    const auto plan = update::build_non_flipped(frame, system);
    assert(plan.has_value());
    assert(plan->atlas_plist == "scene_effect.plist");
    assert(plan->frame_name == "scene_effect_03.png");
    assert(near(plan->position_x, 60.0f));
    assert(near(plan->position_y, 45.0f));
    assert(near(plan->anchor_x, 0.25f));
    assert(near(plan->anchor_y, 0.75f));
    assert(near(plan->rotation, 30.0f));
    assert(near(plan->scale_x, 3.0f));
    assert(near(plan->scale_y, 1.5f));
    assert(!plan->flip_x);

    // Reflected geometry is a distinct native updateData branch and must not be
    // approximated by reusing the non-flipped position formula.
    auto flipped = system;
    flipped.flip_x_15e = true;
    assert(!update::build_non_flipped(frame, flipped).has_value());

    auto missing = frame;
    missing.first_string.clear();
    assert(!update::build_non_flipped(missing, system).has_value());

    auto bad = frame;
    bad.float_values[3] = INFINITY;
    assert(!update::build_non_flipped(bad, system).has_value());
    auto zero_scale = system;
    zero_scale.scale_240 = 0.0f;
    assert(!update::build_non_flipped(frame, zero_scale).has_value());

    return 0;
}
