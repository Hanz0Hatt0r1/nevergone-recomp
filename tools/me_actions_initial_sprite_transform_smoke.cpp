#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

#include "me_actions_initial_sprite_transform.h"

int main() {
    namespace transform = nevergone::me_actions_initial_sprite_transform;
    using nevergone::enemy_actions_wbg_prefix::ActionFrameRecord;

    ActionFrameRecord frame;
    frame.float_values[0] = 10.0f;   // AFD+0x14 local X
    frame.float_values[1] = 20.0f;   // AFD+0x18 local Y
    frame.float_values[2] = 0.25f;   // AFD+0x24 anchor X
    frame.float_values[3] = 0.75f;   // AFD+0x28 anchor Y
    frame.float_values[6] = 30.0f;   // AFD+0x1c flip pivot X
    frame.float_values[7] = 40.0f;   // AFD+0x20 flip pivot Y
    frame.float_values[8] = -2.75f;  // converted to AFD+0x38 child Z
    frame.float_values[9] = 33.0f;   // AFD+0x34 rotation
    frame.float_values[10] = 1.5f;   // AFD+0x44 scale X
    frame.float_values[11] = 0.5f;   // AFD+0x48 scale Y

    const auto normal = transform::build(frame);
    assert(normal.has_value());
    assert(normal->position_x == 10.0f);
    assert(normal->position_y == 20.0f);
    assert(normal->anchor_x == 0.25f);
    assert(normal->anchor_y == 0.75f);
    assert(normal->scale_x == 1.5f);
    assert(normal->scale_y == 0.5f);
    assert(normal->rotation == 33.0f);
    assert(normal->rotation_during_move_update == 33.0f);
    assert(!normal->flip_x);
    assert(normal->child_z_order == -2);

    frame.first_bool = true;
    const auto flipped = transform::build(frame);
    assert(flipped.has_value());
    assert(flipped->flip_x);
    assert(flipped->position_x == 50.0f);  // 2*30 - 10
    assert(flipped->position_y == 20.0f);
    assert(flipped->anchor_x == 0.75f);
    assert(flipped->anchor_y == 0.75f);
    assert(flipped->scale_x == 1.5f);
    assert(flipped->scale_y == 0.5f);
    // MeActions temporarily negates rotation while computing frame movement,
    // then restores the original rotation on the visible body sprite.
    assert(flipped->rotation_during_move_update == -33.0f);
    assert(flipped->rotation == 33.0f);
    assert(flipped->child_z_order == -2);

    frame.float_values[8] = 3.9f;
    const auto positive_z = transform::build(frame);
    assert(positive_z.has_value());
    assert(positive_z->child_z_order == 3);

    frame.float_values[0] = std::numeric_limits<float>::quiet_NaN();
    assert(!transform::build(frame).has_value());
    frame.float_values[0] = 10.0f;

    frame.float_values[8] = std::numeric_limits<float>::infinity();
    assert(!transform::build(frame).has_value());
    frame.float_values[8] = 0.0f;

    frame.first_bool = true;
    frame.float_values[6] = std::numeric_limits<float>::quiet_NaN();
    assert(!transform::build(frame).has_value());
    frame.float_values[6] = 30.0f;

    frame.float_values[7] = std::numeric_limits<float>::infinity();
    assert(!transform::build(frame).has_value());

    return 0;
}
