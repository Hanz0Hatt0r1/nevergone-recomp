#pragma once

#include <cstdint>
#include <string>

#include "enemy_actions_secondary_entry.h"
#include "enemy_actions_wbg_prefix.h"

namespace nevergone::enemy_actions_secondary_sprite_update {

struct SystemInputs {
    float field_16c = 0.0f;
    float field_170 = 0.0f;
    float field_194 = 0.0f;
    float field_198 = 0.0f;
    float field_278 = 0.0f;
    float field_2a8 = 0.0f;
    std::uint8_t flag_16a = 0;
};

struct Plan {
    bool active = false;

    bool should_lookup_sprite_frame = false;
    std::string frame_name_68;
    bool sprite_frame_resolved = false;
    bool should_set_display_frame = false;

    std::uint8_t color_r = 0;
    std::uint8_t color_g = 0;
    std::uint8_t color_b = 0;
    std::uint8_t opacity = 0;

    float scale_x = 0.0f;
    float scale_y = 0.0f;
    float position_x = 0.0f;
    float position_y = 0.0f;
    float field_1c8_x = 0.0f;
    float field_1c8_y = 0.0f;
    float rotation = 0.0f;
    float anchor_x = 0.0f;
    float anchor_y = 0.0f;
    bool flip_y = false;
    bool should_enter_flag_16a_geometry = false;
};

// Reconstructs the secondary sprite property slice at 0x2ac86e..0x2ac984.
// It is entered only after a secondary EAD+0x8c record has been selected.
inline Plan build(
        const enemy_actions_wbg_prefix::ActionFrameRecord& primary_frame,
        const enemy_actions_secondary_entry::Result& secondary_entry,
        bool sprite_frame_resolved,
        const SystemInputs& system) {
    Plan plan;
    if (!secondary_entry.selected_record_available) return plan;

    plan.active = true;
    plan.should_lookup_sprite_frame = true;
    plan.frame_name_68 = primary_frame.third_string;
    plan.sprite_frame_resolved = sprite_frame_resolved;
    // Native forwards the sprite-frame pointer to setDisplayFrame() without a
    // null check in this local slice.
    plan.should_set_display_frame = true;

    // ARM VCMPE + BLE uses the non-positive/unordered path unless +0x278 is
    // strictly greater than zero. C++ `>` likewise returns false for NaN.
    if (system.field_278 > 0.0f) {
        plan.color_r = 0xffu;
        plan.color_g = 0xffu;
        plan.color_b = 0xffu;
        plan.opacity = 0x1eu;
    } else {
        plan.color_r = 0u;
        plan.color_g = 0u;
        plan.color_b = 0u;
        plan.opacity = 0x96u;
    }

    // Section-A mapping: f0->+0x14, f2->+0x24, f3->+0x28,
    // f9->+0x34, f10->+0x44, f11->+0x48.
    const float field_14 = primary_frame.float_values[0];
    const float field_24 = primary_frame.float_values[2];
    const float field_28 = primary_frame.float_values[3];
    const float field_34 = primary_frame.float_values[9];
    const float field_44 = primary_frame.float_values[10];
    const float field_48 = primary_frame.float_values[11];

    constexpr float kSecondaryScaleYFactor = 0.2f;
    constexpr float kField170Factor = 0.8f;

    plan.scale_x = field_44;
    plan.scale_y = field_48 * kSecondaryScaleYFactor;

    plan.position_x = system.field_194 + field_14;
    plan.position_y =
            system.field_16c + system.field_170 * kField170Factor -
            system.field_198 - system.field_194 * kSecondaryScaleYFactor +
            system.field_2a8 + secondary_entry.selected_field_18;
    plan.field_1c8_x = plan.position_x;
    plan.field_1c8_y = plan.position_y;

    // Unlike the primary/base slice, this local secondary path forwards +0x34
    // directly; there is no one-step 360 subtraction here.
    plan.rotation = field_34;
    plan.anchor_x = field_24;
    plan.anchor_y = field_28;

    // Native calls CCSprite::setFlipY(true) unconditionally before the +0x16a
    // follow-up geometry gate.
    plan.flip_y = true;
    plan.should_enter_flag_16a_geometry = system.flag_16a != 0u;
    return plan;
}

}  // namespace nevergone::enemy_actions_secondary_sprite_update
