#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

#include "enemy_actions_wbg_prefix.h"

namespace nevergone::scene_actions_base_sprite_update {

struct Plan {
    std::string atlas_plist;
    std::string frame_name;
    float position_x = 0.0f;
    float position_y = 0.0f;
    float rotation = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    bool flip_x = false;
};

// Non-flipped SceneActionsSystem::updateData() primary-sprite branch recovered
// from the original ARMv7 routine. ActionFrameData serialization is shared with
// EnemyActionsData and the existing parser already maps these runtime offsets:
//   +0x24 -> float_values[2], +0x28 -> float_values[3]
//   +0x34 -> float_values[9], +0x44 -> float_values[10]
//   +0x48 -> float_values[11], +0x60 -> first_string, +0x68 -> third_string.
// SceneActionsSystem +0x240 is a uniform system scale multiplied into both
// per-frame scale components. +0x15e is passed directly to CCSprite::setFlipX.
//
// The reflected-position geometry is a different native branch and is kept out
// of this helper until its additional offsets are reconstructed independently.
inline std::optional<Plan> build_non_flipped(
        const enemy_actions_wbg_prefix::ActionFrameRecord& frame,
        float system_scale_240,
        bool flip_x_15e) {
    if (flip_x_15e) return std::nullopt;
    if (frame.first_string.empty() || frame.third_string.empty()) return std::nullopt;
    if (!std::isfinite(system_scale_240)) return std::nullopt;

    const float position_x = frame.float_values[2];
    const float position_y = frame.float_values[3];
    const float rotation = frame.float_values[9];
    const float scale_x = frame.float_values[10] * system_scale_240;
    const float scale_y = frame.float_values[11] * system_scale_240;
    if (!std::isfinite(position_x) || !std::isfinite(position_y) ||
        !std::isfinite(rotation) || !std::isfinite(scale_x) || !std::isfinite(scale_y)) {
        return std::nullopt;
    }

    Plan plan;
    plan.atlas_plist = frame.first_string;
    plan.frame_name = frame.third_string;
    plan.position_x = position_x;
    plan.position_y = position_y;
    plan.rotation = rotation;
    plan.scale_x = scale_x;
    plan.scale_y = scale_y;
    plan.flip_x = false;
    return plan;
}

}  // namespace nevergone::scene_actions_base_sprite_update
