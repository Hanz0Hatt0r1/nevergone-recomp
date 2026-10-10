#include <cassert>
#include <cmath>

#include "enemy_actions_sprite_update.h"

using nevergone::enemy_actions_base_sprite_update::SystemInputs;
using nevergone::enemy_actions_sprite_update::build;
using nevergone::enemy_actions_wbg_prefix::ActionFrameRecord;

namespace {

bool close_enough(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    ActionFrameRecord frame;
    frame.float_values[0] = 3.0f;    // +0x14
    frame.float_values[1] = 5.0f;    // +0x18
    frame.float_values[2] = 0.25f;   // +0x24 anchor X
    frame.float_values[3] = 0.75f;   // +0x28 anchor Y
    frame.float_values[6] = 10.0f;   // +0x1c flip pivot X
    frame.float_values[7] = 4.0f;    // +0x20 flip pivot Y
    frame.float_values[9] = 450.0f;  // +0x34 rotation
    frame.float_values[10] = 1.5f;   // +0x44 scale X
    frame.float_values[11] = 0.5f;   // +0x48 scale Y
    frame.first_bool = true;
    frame.second_i32 = 0x1234;
    frame.third_string = "frame.png";

    SystemInputs system;
    system.field_194 = 2.0f;
    system.field_198 = 7.0f;
    system.field_280 = 1.0f;

    // Base point is (5, 13); flip geometry mirrors X around pivot X=10.
    const auto flipped = build(frame, true, 2.0f, system);
    assert(flipped.should_set_display_frame);
    assert(flipped.sprite_frame_resolved);
    assert(flipped.frame_name_68 == "frame.png");
    assert(flipped.flip_x);
    assert(flipped.flip_geometry_applied);
    assert(close_enough(flipped.base.position_x, 5.0f));
    assert(close_enough(flipped.base.position_y, 13.0f));
    assert(close_enough(flipped.position_x, 15.0f));
    assert(close_enough(flipped.position_y, 13.0f));
    assert(close_enough(flipped.field_1c0_x, 15.0f));
    assert(close_enough(flipped.field_1c0_y, 13.0f));
    assert(close_enough(flipped.anchor_x, 0.75f));
    assert(close_enough(flipped.anchor_y, 0.75f));
    assert(close_enough(flipped.rotation, -90.0f));
    assert(close_enough(flipped.scale_x, 1.5f));
    assert(close_enough(flipped.scale_y, 0.5f));
    assert(flipped.visible);
    assert(flipped.opacity == 0x34u);

    // system+0x16a inversion changes the final flip flag and therefore skips
    // the conditional geometry while retaining the common base plan.
    system.flag_16a = 1u;
    const auto unflipped = build(frame, false, 2.0f, system);
    assert(!unflipped.sprite_frame_resolved);
    assert(!unflipped.flip_x);
    assert(!unflipped.flip_geometry_applied);
    assert(close_enough(unflipped.position_x, 5.0f));
    assert(close_enough(unflipped.position_y, 13.0f));
    assert(close_enough(unflipped.anchor_x, 0.25f));
    assert(close_enough(unflipped.rotation, 90.0f));

    return 0;
}
