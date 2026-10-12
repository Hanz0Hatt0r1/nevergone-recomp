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
    frame.float_values[2] = 120.0f;
    frame.float_values[3] = 45.0f;
    frame.float_values[9] = 30.0f;
    frame.float_values[10] = 1.5f;
    frame.float_values[11] = 0.75f;

    const auto plan = update::build_non_flipped(frame, 2.0f, false);
    assert(plan.has_value());
    assert(plan->atlas_plist == "scene_effect.plist");
    assert(plan->frame_name == "scene_effect_03.png");
    assert(near(plan->position_x, 120.0f));
    assert(near(plan->position_y, 45.0f));
    assert(near(plan->rotation, 30.0f));
    assert(near(plan->scale_x, 3.0f));
    assert(near(plan->scale_y, 1.5f));
    assert(!plan->flip_x);

    // Reflected geometry is a distinct native updateData branch and must not be
    // approximated by reusing the non-flipped position formula.
    assert(!update::build_non_flipped(frame, 2.0f, true).has_value());

    auto missing = frame;
    missing.first_string.clear();
    assert(!update::build_non_flipped(missing, 1.0f, false).has_value());

    auto bad = frame;
    bad.float_values[3] = INFINITY;
    assert(!update::build_non_flipped(bad, 1.0f, false).has_value());
    assert(!update::build_non_flipped(frame, NAN, false).has_value());

    return 0;
}
