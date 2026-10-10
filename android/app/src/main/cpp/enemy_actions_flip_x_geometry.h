#pragma once

#include "enemy_actions_base_sprite_update.h"
#include "enemy_actions_wbg_prefix.h"

namespace nevergone::enemy_actions_flip_x_geometry {

struct Plan {
    bool applied = false;

    float pivot_x = 0.0f;
    float pivot_y = 0.0f;

    float position_x = 0.0f;
    float position_y = 0.0f;
    float field_1c0_x = 0.0f;
    float field_1c0_y = 0.0f;

    float anchor_x = 0.0f;
    float anchor_y = 0.0f;
    float rotation = 0.0f;
    float scale_x = 0.0f;
    float scale_y = 0.0f;
};

// Reconstructs the flip-X-only geometry slice at 0x2ac6f0..0x2ac7d2.
// Native reaches this block only when the already-resolved final flip-X flag is
// nonzero. The point helpers used there are equivalent to point subtraction
// followed by point addition:
//
//   delta = pivot - current_position;
//   delta.y = -delta.y;
//   mirrored = pivot + delta;
//
// which simplifies to x' = 2*pivot.x - x and y' = y.
inline Plan build(
        const enemy_actions_wbg_prefix::ActionFrameRecord& frame,
        const enemy_actions_base_sprite_update::Plan& base) {
    Plan plan;
    if (!base.flip_x) return plan;

    // Section-A construction maps serialized float indices 6 and 7 to
    // ActionFrameData+0x1c/+0x20.
    plan.pivot_x = frame.float_values[6];
    plan.pivot_y = frame.float_values[7];

    plan.position_x = (2.0f * plan.pivot_x) - base.position_x;
    plan.position_y = base.position_y;
    plan.field_1c0_x = plan.position_x;
    plan.field_1c0_y = plan.position_y;

    // Native mirrors the anchor around 0.5 on X only:
    // 0.5 - (AFD+0x24 - 0.5) == 1.0 - AFD+0x24.
    plan.anchor_x = 1.0f - frame.float_values[2];
    plan.anchor_y = frame.float_values[3];

    // s16 already contains the one-step-normalized rotation from the common
    // base-sprite path. The flip block negates that exact value.
    plan.rotation = -base.rotation;
    plan.scale_x = base.scale_x;
    plan.scale_y = base.scale_y;
    plan.applied = true;
    return plan;
}

}  // namespace nevergone::enemy_actions_flip_x_geometry
