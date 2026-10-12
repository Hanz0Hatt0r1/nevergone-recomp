#pragma once

#include <cmath>
#include <optional>

#include "scene_actions_base_sprite_update.h"

namespace nevergone::scene_actions_flip_x_geometry {

struct Plan {
    float base_x = 0.0f;
    float base_y = 0.0f;
    float q_x = 0.0f;
    float q_y = 0.0f;
    float position_x = 0.0f;
    float position_y = 0.0f;
    float anchor_x = 0.0f;
    float anchor_y = 0.0f;
    float rotation = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    bool flip_x = true;
};

// Exact reflected branch of SceneActionsSystem::updateData(). The local point
// helpers at 0x351ae8 and 0x351abc are respectively lhs-rhs and lhs+rhs.
// Native computes:
//   base = ((AFD+0x14 + sys+0x244) / scale,
//           (AFD+0x18 + sys+0x248) / scale)
//   q    = (AFD+0x1c / scale, AFD+0x20 / scale)
//   delta = q - base
//   delta.y = -delta.y
//   position = q + delta
// Therefore position simplifies to (2*q.x - base.x, base.y).
// The same branch mirrors anchor X about 0.5 and negates rotation.
inline std::optional<Plan> build(
        const enemy_actions_wbg_prefix::ActionFrameRecord& frame,
        const scene_actions_base_sprite_update::SystemInputs& system) {
    if (!system.flip_x_15e) return std::nullopt;
    if (!std::isfinite(system.scale_240) || system.scale_240 == 0.0f ||
        !std::isfinite(system.position_offset_x_244) ||
        !std::isfinite(system.position_offset_y_248)) {
        return std::nullopt;
    }

    Plan plan;
    plan.base_x =
            (frame.float_values[0] + system.position_offset_x_244) / system.scale_240;
    plan.base_y =
            (frame.float_values[1] + system.position_offset_y_248) / system.scale_240;
    // Existing ActionFrameData mapping: float_values[6]/[7] -> +0x1c/+0x20.
    plan.q_x = frame.float_values[6] / system.scale_240;
    plan.q_y = frame.float_values[7] / system.scale_240;
    plan.position_x = (2.0f * plan.q_x) - plan.base_x;
    plan.position_y = plan.base_y;
    plan.anchor_x = 1.0f - frame.float_values[2];
    plan.anchor_y = frame.float_values[3];
    plan.rotation = -frame.float_values[9];
    plan.scale_x = frame.float_values[10] * system.scale_240;
    plan.scale_y = frame.float_values[11] * system.scale_240;
    plan.flip_x = true;

    if (!std::isfinite(plan.base_x) || !std::isfinite(plan.base_y) ||
        !std::isfinite(plan.q_x) || !std::isfinite(plan.q_y) ||
        !std::isfinite(plan.position_x) || !std::isfinite(plan.position_y) ||
        !std::isfinite(plan.anchor_x) || !std::isfinite(plan.anchor_y) ||
        !std::isfinite(plan.rotation) || !std::isfinite(plan.scale_x) ||
        !std::isfinite(plan.scale_y)) {
        return std::nullopt;
    }
    return plan;
}

}  // namespace nevergone::scene_actions_flip_x_geometry
