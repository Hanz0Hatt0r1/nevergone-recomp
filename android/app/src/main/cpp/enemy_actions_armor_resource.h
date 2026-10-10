#pragma once

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <string>

#include "enemy_actions_wbg_prefix.h"

namespace nevergone::enemy_actions_armor_resource {

constexpr const char* kDirectPlistName = "T_E01_default.plist";

struct Result {
    std::size_t armor_id = 0;
    std::string field_60;

    bool special_direct_plist = false;

    bool formatted_path_available = false;
    std::string formatted_path;

    // Native control-flow reaches the ordinary add/record block under these
    // conditions. A deterministic request is only emitted when the path value
    // is recovered as well.
    bool native_would_reach_formatted_load_block = false;
    bool unresolved_formatted_path_register = false;

    bool should_add_sprite_frames = false;
    std::string sprite_frames_file;
    bool should_record_enemy_object_res = false;
};

inline std::string format_two_arg_path(
        const char* format,
        std::int32_t value,
        const std::string& name) {
    char buffer[512]{};
    std::snprintf(buffer, sizeof(buffer), format, value, name.c_str());
    return std::string(buffer);
}

inline std::string format_mode3_slot0_path(
        std::int32_t field_268,
        std::int32_t field_188) {
    char buffer[512]{};
    std::snprintf(
            buffer,
            sizeof(buffer),
            "l%02d/w_res/L%02d_W%02d_default.plist",
            field_268,
            field_268,
            field_188);
    return std::string(buffer);
}

inline Result apply(
        const enemy_actions_wbg_prefix::NestedActionFrameRecord& record,
        std::size_t armor_id,
        std::int32_t field_250,
        std::int32_t field_260,
        std::int32_t field_268,
        std::int32_t field_188) {
    Result result;
    result.armor_id = armor_id;
    result.field_60 = record.first_string;

    switch (field_250) {
        case 0:
            result.formatted_path_available = true;
            result.formatted_path = format_two_arg_path(
                    "enemy%02d/res/%s", field_268, record.first_string);
            break;
        case 1:
            result.formatted_path_available = true;
            result.formatted_path = format_two_arg_path(
                    "npc%02d/res/%s", field_268, record.first_string);
            break;
        case 2:
            result.formatted_path_available = true;
            result.formatted_path = format_two_arg_path(
                    "pet%02d/res/%s", field_268, record.first_string);
            break;
        case 3:
            if (armor_id == 0u) {
                result.formatted_path_available = true;
                result.formatted_path = format_mode3_slot0_path(field_268, field_188);
            } else if (armor_id == 1u) {
                result.formatted_path_available = true;
                result.formatted_path = format_two_arg_path(
                        "l%02d/res/%s", field_268, record.first_string);
            } else {
                result.unresolved_formatted_path_register = true;
            }
            break;
        default:
            break;
    }

    if (record.first_string == kDirectPlistName) {
        result.special_direct_plist = true;
        result.should_add_sprite_frames = true;
        result.sprite_frames_file = record.first_string;
        return result;
    }

    if (field_250 < 0 || field_250 > 3) return result;

    bool reaches_load = false;
    if (field_250 == 0) {
        reaches_load = field_260 != 0x16 && field_260 != 0x17 &&
                field_260 != 0x1a && field_260 != 0x22;
    } else if (field_250 == 1) {
        reaches_load = field_260 != 0x16 && field_260 != 0x17;
    } else {
        reaches_load = true;
    }

    result.native_would_reach_formatted_load_block = reaches_load;
    if (!reaches_load || !result.formatted_path_available) return result;

    result.should_add_sprite_frames = true;
    result.sprite_frames_file = result.formatted_path;
    result.should_record_enemy_object_res = true;
    return result;
}

}  // namespace nevergone::enemy_actions_armor_resource
