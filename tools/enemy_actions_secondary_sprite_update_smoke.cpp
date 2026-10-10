#include <cassert>
#include <cmath>
#include <limits>

#include "enemy_actions_secondary_sprite_update.h"

namespace {

bool close_enough(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    using nevergone::enemy_actions_secondary_entry::Result;
    using nevergone::enemy_actions_secondary_sprite_update::SystemInputs;
    using nevergone::enemy_actions_secondary_sprite_update::build;
    using nevergone::enemy_actions_wbg_prefix::ActionFrameRecord;

    ActionFrameRecord primary;
    primary.float_values[0] = 3.0f;
    primary.float_values[2] = 0.25f;
    primary.float_values[3] = 0.75f;
    primary.float_values[9] = 450.0f;
    primary.float_values[10] = 1.5f;
    primary.float_values[11] = 0.5f;
    primary.third_string = "frame.png";

    Result secondary;
    secondary.selected_record_available = true;
    secondary.selected_field_18 = 2.0f;

    SystemInputs system;
    system.field_16c = 10.0f;
    system.field_170 = 4.0f;
    system.field_194 = 5.0f;
    system.field_198 = 6.0f;
    system.field_278 = 1.0f;
    system.field_2a8 = 7.0f;
    system.flag_16a = 1u;

    const auto plan = build(primary, secondary, true, system);
    assert(plan.active);
    assert(plan.should_lookup_sprite_frame);
    assert(plan.frame_name_68 == "frame.png");
    assert(plan.sprite_frame_resolved);
    assert(plan.should_set_display_frame);
    assert(plan.color_r == 0xffu && plan.color_g == 0xffu && plan.color_b == 0xffu);
    assert(plan.opacity == 0x1eu);
    assert(close_enough(plan.scale_x, 1.5f));
    assert(close_enough(plan.scale_y, 0.1f));
    assert(close_enough(plan.position_x, 8.0f));
    // 10 + 0.8*4 - 6 - 0.2*5 + 7 + 2 = 15.2
    assert(close_enough(plan.position_y, 15.2f));
    assert(close_enough(plan.field_1c8_x, 8.0f));
    assert(close_enough(plan.field_1c8_y, 15.2f));
    assert(close_enough(plan.rotation, 450.0f));
    assert(close_enough(plan.anchor_x, 0.25f));
    assert(close_enough(plan.anchor_y, 0.75f));
    assert(plan.flip_y);
    assert(plan.should_enter_flag_16a_geometry);

    system.field_278 = 0.0f;
    system.flag_16a = 0u;
    const auto zero_gate = build(primary, secondary, false, system);
    assert(zero_gate.active);
    assert(!zero_gate.sprite_frame_resolved);
    assert(zero_gate.should_set_display_frame);
    assert(zero_gate.color_r == 0u && zero_gate.color_g == 0u && zero_gate.color_b == 0u);
    assert(zero_gate.opacity == 0x96u);
    assert(!zero_gate.should_enter_flag_16a_geometry);

    system.field_278 = std::numeric_limits<float>::quiet_NaN();
    const auto nan_gate = build(primary, secondary, true, system);
    assert(nan_gate.color_r == 0u);
    assert(nan_gate.opacity == 0x96u);

    Result missing;
    const auto inactive = build(primary, missing, true, system);
    assert(!inactive.active);
    assert(!inactive.should_lookup_sprite_frame);
    assert(!inactive.should_set_display_frame);

    return 0;
}
