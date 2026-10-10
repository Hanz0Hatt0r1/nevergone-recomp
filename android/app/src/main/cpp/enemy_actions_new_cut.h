#pragma once

#include <string>

#include "enemy_actions_cut_rect.h"

namespace nevergone::enemy_actions_new_cut {

struct Result {
    bool unmatched_enemy_cut_path = false;
    bool sprite_frame_resolved = false;
    bool should_patch_sprite_frame_rect = false;
    bool should_create_actions_cut = false;
    bool should_append_to_enemy_cut_array = false;
    std::string frame_name_14;
    float field_18 = 0.0f;
    float field_1c = 0.0f;
    float patched_rect_height = 0.0f;
    float delta = 0.0f;
};

// Bounded side-effect plan for the unmatched EnemyObject-owned cut branch.
// Native only materializes a new ActionsCut when that branch is active and the
// earlier spriteFrameByName() returned a frame. Allocation/retain/CCArray IO is
// surfaced as signals rather than reproduced here.
inline Result materialize(
        bool unmatched_enemy_cut_path,
        bool sprite_frame_resolved,
        const std::string& current_frame_name_68,
        float original_rect_height,
        float field_280,
        float action_hpoint_offset) {
    Result result;
    result.unmatched_enemy_cut_path = unmatched_enemy_cut_path;
    result.sprite_frame_resolved = sprite_frame_resolved;

    if (!unmatched_enemy_cut_path || !sprite_frame_resolved) return result;

    const auto adjustment = enemy_actions_cut_rect::adjust_enemy_cut(
            original_rect_height,
            field_280,
            action_hpoint_offset);

    result.should_patch_sprite_frame_rect = adjustment.should_set_rect;
    result.should_create_actions_cut = true;
    result.should_append_to_enemy_cut_array = true;
    result.frame_name_14 = current_frame_name_68;
    result.field_18 = adjustment.cut_field_18;
    result.field_1c = adjustment.cut_field_1c;
    result.patched_rect_height = adjustment.patched_rect_height;
    result.delta = adjustment.delta;
    return result;
}

}  // namespace nevergone::enemy_actions_new_cut
