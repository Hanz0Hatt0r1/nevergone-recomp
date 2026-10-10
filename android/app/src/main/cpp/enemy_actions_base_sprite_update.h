#pragma once

#include <cstdint>
#include <string>

#include "enemy_actions_wbg_prefix.h"

namespace nevergone::enemy_actions_base_sprite_update {

struct SystemInputs {
    float field_194 = 0.0f;
    float field_198 = 0.0f;
    float field_280 = 0.0f;
    std::uint8_t flag_16a = 0;
};

struct Plan {
    bool should_set_display_frame = true;
    bool sprite_frame_resolved = false;
    std::string frame_name_68;

    float position_x = 0.0f;
    float position_y = 0.0f;
    float field_1c0_x = 0.0f;
    float field_1c0_y = 0.0f;

    float anchor_x = 0.0f;
    float anchor_y = 0.0f;
    float scale_x = 0.0f;
    float scale_y = 0.0f;
    float rotation = 0.0f;
    bool visible = true;
    std::uint8_t opacity = 0;
    bool flip_x = false;
};

// Reconstructs the common 0x2ac600..0x2ac6e8 sprite-property slice after all
// cut-processing paths rejoin. `cut_field_18` is the EnemyObject-owned cut
// vertical offset; default/no-cut paths supply zero, exactly as native does.
inline Plan build(
        const enemy_actions_wbg_prefix::ActionFrameRecord& frame,
        bool sprite_frame_resolved,
        float cut_field_18,
        const SystemInputs& system) {
    Plan plan;
    plan.sprite_frame_resolved = sprite_frame_resolved;
    plan.frame_name_68 = frame.third_string;

    // Section-A construction maps serialized float indices to AFD offsets:
    // f0->+0x14, f1->+0x18, f2->+0x24, f3->+0x28,
    // f9->+0x34, f10->+0x44, f11->+0x48.
    const float field_14 = frame.float_values[0];
    const float field_18 = frame.float_values[1];
    const float field_24 = frame.float_values[2];
    const float field_28 = frame.float_values[3];
    const float field_34 = frame.float_values[9];
    const float field_44 = frame.float_values[10];
    const float field_48 = frame.float_values[11];

    plan.position_x = field_14 + system.field_194;
    plan.position_y =
            field_18 + system.field_198 + cut_field_18 - system.field_280;
    plan.field_1c0_x = plan.position_x;
    plan.field_1c0_y = plan.position_y;

    plan.anchor_x = field_24;
    plan.anchor_y = field_28;
    plan.scale_x = field_44;
    plan.scale_y = field_48;

    // Native performs exactly one subtraction when AFD+0x34 >= 360.0f.
    plan.rotation = field_34;
    if (field_34 >= 360.0f) plan.rotation = field_34 - 360.0f;

    plan.visible = true;
    // AFD+0x50 stores Section-A second_i32. Native reads only its low byte
    // before calling CCSprite::setOpacity().
    plan.opacity = static_cast<std::uint8_t>(
            static_cast<std::uint32_t>(frame.second_i32) & 0xffu);

    plan.flip_x = frame.first_bool;
    if (system.flag_16a != 0u) plan.flip_x = !plan.flip_x;
    return plan;
}

}  // namespace nevergone::enemy_actions_base_sprite_update
