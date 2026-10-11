#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "enemy_actions_wbg_prefix.h"

namespace nevergone::me_actions_initial_sprite_transform {

// Final visible properties of the primary MeActionsSystem sprite (+0x118)
// after the first play()->updateData() call. These values are local to the
// MeActionsSystem node; the node itself is positioned at MEPlayer::getPlayerPos().
struct Plan {
    float position_x = 0.0f;
    float position_y = 0.0f;
    float anchor_x = 0.0f;
    float anchor_y = 0.0f;
    float scale_x = 0.0f;
    float scale_y = 0.0f;
    float rotation = 0.0f;
    bool flip_x = false;
    std::int32_t child_z_order = 0;

    // The flip branch temporarily negates rotation before
    // updateActionFrameMoveValue(), then native restores the frame's original
    // rotation. This records the temporary value separately so callers do not
    // mistake it for the final visible transform.
    float rotation_during_move_update = 0.0f;
};

inline bool finite(float value) {
    return std::isfinite(value);
}

inline std::optional<std::int32_t> child_z_order(float serialized_value) {
    if (!finite(serialized_value)) return std::nullopt;
    const double value = static_cast<double>(serialized_value);
    if (value < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
            value > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        return std::nullopt;
    }
    // Native Section-C uses VCVT.S32.F32 for ActionFrameData+0x38. For valid
    // shipped values this is the same truncation-toward-zero contract as this
    // cast; malformed/out-of-range values are rejected above.
    return static_cast<std::int32_t>(serialized_value);
}

// Reconstructs only the initial primary-player path proven for
// MEPlayer::createActions()->MeActionsSystem::play()->updateData(). On this path:
//   system+0x204/+0x208 = 0
//   system+0x2f8 = 1.0
//   system+0x644/+0x648 = 0
//   system+0x1be = 0
// so the native formulas reduce directly to the selected ActionFrameData.
inline std::optional<Plan> build(
        const enemy_actions_wbg_prefix::ActionFrameRecord& frame) {
    // Section-A construction mapping already recovered for the shared AFD
    // layout: f0/f1 -> +0x14/+0x18, f2/f3 -> +0x24/+0x28,
    // f6/f7 -> +0x1c/+0x20, f8 -> +0x38 (int conversion),
    // f9 -> +0x34, f10/f11 -> +0x44/+0x48.
    const float position_x = frame.float_values[0];
    const float position_y = frame.float_values[1];
    const float anchor_x = frame.float_values[2];
    const float anchor_y = frame.float_values[3];
    const float rotation = frame.float_values[9];
    const float scale_x = frame.float_values[10];
    const float scale_y = frame.float_values[11];

    if (!finite(position_x) || !finite(position_y) ||
            !finite(anchor_x) || !finite(anchor_y) ||
            !finite(rotation) || !finite(scale_x) || !finite(scale_y)) {
        return std::nullopt;
    }

    const auto z_order = child_z_order(frame.float_values[8]);
    if (!z_order.has_value()) return std::nullopt;

    Plan plan;
    plan.position_x = position_x;
    plan.position_y = position_y;
    plan.anchor_x = anchor_x;
    plan.anchor_y = anchor_y;
    plan.scale_x = scale_x;
    plan.scale_y = scale_y;
    plan.rotation = rotation;
    plan.flip_x = frame.first_bool;
    plan.child_z_order = *z_order;
    plan.rotation_during_move_update = rotation;

    if (plan.flip_x) {
        const float pivot_x = frame.float_values[6];
        const float pivot_y = frame.float_values[7];
        if (!finite(pivot_x) || !finite(pivot_y)) return std::nullopt;

        // Native computes:
        //   delta = pivot - position
        //   delta.y = -delta.y
        //   position = pivot + delta
        // which mirrors X about pivot.x while preserving Y for finite inputs.
        plan.position_x = 2.0f * pivot_x - position_x;
        plan.position_y = pivot_y + (position_y - pivot_y);
        plan.anchor_x = 1.0f - anchor_x;
        plan.rotation_during_move_update = -rotation;
        if (!finite(plan.position_x) || !finite(plan.position_y) ||
                !finite(plan.anchor_x) ||
                !finite(plan.rotation_during_move_update)) {
            return std::nullopt;
        }
    }

    return plan;
}

}  // namespace nevergone::me_actions_initial_sprite_transform
