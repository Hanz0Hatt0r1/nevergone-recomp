#include <cassert>
#include <cstdint>
#include <limits>

#include "enemy_actions_base_sprite_update.h"

int main() {
    namespace sprite = nevergone::enemy_actions_base_sprite_update;
    nevergone::enemy_actions_wbg_prefix::ActionFrameRecord frame;
    frame.float_values[0] = 10.0f;   // AFD+0x14
    frame.float_values[1] = 20.0f;   // AFD+0x18
    frame.float_values[2] = 0.25f;   // AFD+0x24
    frame.float_values[3] = 0.75f;   // AFD+0x28
    frame.float_values[9] = 450.0f;  // AFD+0x34
    frame.float_values[10] = 1.5f;   // AFD+0x44
    frame.float_values[11] = 2.0f;   // AFD+0x48
    frame.first_bool = true;          // AFD+0x40
    frame.second_i32 = 0x1234ff80;    // AFD+0x50, low byte only
    frame.third_string = "frame.png";

    sprite::SystemInputs system;
    system.field_194 = 3.0f;
    system.field_198 = 4.0f;
    system.field_280 = 5.0f;

    auto plan = sprite::build(frame, true, 6.0f, system);
    assert(plan.should_set_display_frame);
    assert(plan.sprite_frame_resolved);
    assert(plan.frame_name_68 == "frame.png");
    assert(plan.position_x == 13.0f);
    assert(plan.position_y == 25.0f); // 20 + 4 + 6 - 5
    assert(plan.field_1c0_x == plan.position_x);
    assert(plan.field_1c0_y == plan.position_y);
    assert(plan.anchor_x == 0.25f);
    assert(plan.anchor_y == 0.75f);
    assert(plan.scale_x == 1.5f);
    assert(plan.scale_y == 2.0f);
    assert(plan.rotation == 90.0f);   // exactly one subtraction of 360
    assert(plan.visible);
    assert(plan.opacity == 0x80u);
    assert(plan.flip_x);

    // system+0x16a XORs the serialized AFD+0x40 flip byte.
    system.flag_16a = 1u;
    plan = sprite::build(frame, false, 0.0f, system);
    assert(plan.should_set_display_frame); // native passes even a null frame ptr
    assert(!plan.sprite_frame_resolved);
    assert(!plan.flip_x);
    assert(plan.position_y == 19.0f);      // default/no-cut offset is zero

    // Rotation wraps only once, not modulo 360.
    frame.float_values[9] = 810.0f;
    plan = sprite::build(frame, true, 0.0f, system);
    assert(plan.rotation == 450.0f);

    // Values below 360, including negative values, pass through unchanged.
    frame.float_values[9] = -30.0f;
    plan = sprite::build(frame, true, 0.0f, system);
    assert(plan.rotation == -30.0f);

    // VCMPE GE is false for NaN, matching the no-subtraction path.
    frame.float_values[9] = std::numeric_limits<float>::quiet_NaN();
    plan = sprite::build(frame, true, 0.0f, system);
    assert(plan.rotation != plan.rotation);

    return 0;
}
