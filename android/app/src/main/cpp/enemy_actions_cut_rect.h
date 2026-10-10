#pragma once

#include <cmath>

namespace nevergone::enemy_actions_cut_rect {

struct EnemyAdjustment {
    float field_280 = 0.0f;
    float action_hpoint_offset = 0.0f;
    float delta = 0.0f;
    float cut_field_18 = 0.0f;
    float cut_field_1c = 0.0f;
    float patched_rect_height = 0.0f;
    bool delta_applied = false;
    bool should_set_rect = false;
};

// Reconstructs the finite/unordered comparison and arithmetic shared by the
// existing-EnemyObject-cut and new-EnemyObject-cut paths. Native compares the
// action H-point offset with system+0x280 and applies the positive difference
// only when offset < +0x280. VCMPE unordered/NaN does not take the MI branch,
// so the recovered delta remains zero for unordered input.
inline EnemyAdjustment adjust_enemy_cut(
        float original_rect_height,
        float field_280,
        float action_hpoint_offset) {
    EnemyAdjustment result;
    result.field_280 = field_280;
    result.action_hpoint_offset = action_hpoint_offset;
    result.cut_field_1c = original_rect_height;
    result.patched_rect_height = original_rect_height;
    result.should_set_rect = true;

    if (!std::isnan(field_280) && !std::isnan(action_hpoint_offset) &&
        action_hpoint_offset < field_280) {
        result.delta = field_280 - action_hpoint_offset;
        result.delta_applied = true;
    }

    result.patched_rect_height = original_rect_height - result.delta;
    result.cut_field_18 = result.delta * 0.5f;
    return result;
}

struct DefaultPatch {
    float cut_field_1c = 0.0f;
    float patched_rect_height = 0.0f;
    bool should_set_rect = true;
};

// The system-default +0x27c match does not run the H-point subtraction. Native
// copies the current CCSpriteFrame rect and replaces only its fourth float with
// matched ActionsCut+0x1c before calling setRect().
inline DefaultPatch patch_default_cut(float cut_field_1c) {
    return {cut_field_1c, cut_field_1c, true};
}

}  // namespace nevergone::enemy_actions_cut_rect
