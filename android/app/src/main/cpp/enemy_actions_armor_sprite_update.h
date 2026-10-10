#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "enemy_actions_wbg_prefix.h"

namespace nevergone::enemy_actions_armor_sprite_update {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Result {
    std::size_t armor_id = 0;
    std::string frame_name_78;
    bool should_lookup_sprite_frame = true;
    bool should_set_display_frame = true;

    Point base_position;
    Point final_position;
    Point anchor;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float rotation = 0.0f;

    bool visibility_from_field_258 = false;
    bool flip_x = false;
    bool applied_flip_geometry = false;
    std::uint8_t opacity_50 = 0;
    bool mode3_forces_visible = false;
    bool final_visible = false;
};

inline float toggle_float_sign_bit(float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    bits ^= 0x80000000u;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline Result apply(
        const enemy_actions_wbg_prefix::NestedActionFrameRecord& record,
        std::size_t armor_id,
        float field_194,
        float field_198,
        float field_280,
        std::uint8_t field_258_for_armor_id,
        std::uint8_t flag_16a,
        std::int32_t field_250) {
    Result result;
    result.armor_id = armor_id;

    // Section-C constructor mapping recovered from loadWBGFile:
    // +0x14/+0x18 <- float[0]/float[1]
    // +0x1c/+0x20 <- float[6]/float[7]
    // +0x24/+0x28 <- float[2]/float[3]
    // +0x34       <- float[9]
    // +0x44/+0x48 <- float[10]/float[11]
    // +0x40       <- first_bool
    // +0x50       <- second_i32
    // +0x78       <- third_string
    result.frame_name_78 = record.third_string;

    result.base_position.x = field_194 + record.float_values[0];
    result.base_position.y = field_198 + record.float_values[1] - field_280;
    result.final_position = result.base_position;

    result.anchor.x = record.float_values[2];
    result.anchor.y = record.float_values[3];
    result.scale_x = record.float_values[10];
    result.scale_y = record.float_values[11];
    result.rotation = record.float_values[9];

    result.visibility_from_field_258 = (field_258_for_armor_id ^ 1u) != 0u;
    result.flip_x = record.first_bool;
    if (flag_16a != 0u) result.flip_x = !result.flip_x;

    if (result.flip_x) {
        result.applied_flip_geometry = true;
        const Point pivot{record.float_values[6], record.float_values[7]};
        result.final_position.x = 2.0f * pivot.x - result.base_position.x;
        result.final_position.y = result.base_position.y;
        result.anchor.x = 1.0f - result.anchor.x;
        result.rotation = toggle_float_sign_bit(result.rotation);
    }

    result.opacity_50 = static_cast<std::uint8_t>(
            static_cast<std::uint32_t>(record.second_i32) & 0xffu);

    result.mode3_forces_visible = field_250 == 3;
    result.final_visible = result.mode3_forces_visible
            ? true
            : result.visibility_from_field_258;

    return result;
}

}  // namespace nevergone::enemy_actions_armor_sprite_update
