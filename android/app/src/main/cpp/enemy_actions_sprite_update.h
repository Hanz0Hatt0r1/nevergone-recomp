#pragma once

#include <cstdint>
#include <string>

#include "enemy_actions_base_sprite_update.h"
#include "enemy_actions_flip_x_geometry.h"
#include "enemy_actions_wbg_prefix.h"

namespace nevergone::enemy_actions_sprite_update {

struct Plan {
    enemy_actions_base_sprite_update::Plan base;
    enemy_actions_flip_x_geometry::Plan flip_geometry;

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
    bool flip_geometry_applied = false;
};

// Composes the already recovered common base-sprite slice and the conditional
// flip-X geometry slice. This is intentionally a pure side-effect plan: callers
// remain responsible for resolving the frame pointer and issuing cocos2d calls.
inline Plan build(
        const enemy_actions_wbg_prefix::ActionFrameRecord& frame,
        bool sprite_frame_resolved,
        float cut_field_18,
        const enemy_actions_base_sprite_update::SystemInputs& system) {
    Plan plan;
    plan.base = enemy_actions_base_sprite_update::build(
            frame, sprite_frame_resolved, cut_field_18, system);
    plan.flip_geometry = enemy_actions_flip_x_geometry::build(frame, plan.base);

    plan.should_set_display_frame = plan.base.should_set_display_frame;
    plan.sprite_frame_resolved = plan.base.sprite_frame_resolved;
    plan.frame_name_68 = plan.base.frame_name_68;
    plan.position_x = plan.base.position_x;
    plan.position_y = plan.base.position_y;
    plan.field_1c0_x = plan.base.field_1c0_x;
    plan.field_1c0_y = plan.base.field_1c0_y;
    plan.anchor_x = plan.base.anchor_x;
    plan.anchor_y = plan.base.anchor_y;
    plan.scale_x = plan.base.scale_x;
    plan.scale_y = plan.base.scale_y;
    plan.rotation = plan.base.rotation;
    plan.visible = plan.base.visible;
    plan.opacity = plan.base.opacity;
    plan.flip_x = plan.base.flip_x;

    if (plan.flip_geometry.applied) {
        plan.position_x = plan.flip_geometry.position_x;
        plan.position_y = plan.flip_geometry.position_y;
        plan.field_1c0_x = plan.flip_geometry.field_1c0_x;
        plan.field_1c0_y = plan.flip_geometry.field_1c0_y;
        plan.anchor_x = plan.flip_geometry.anchor_x;
        plan.anchor_y = plan.flip_geometry.anchor_y;
        plan.scale_x = plan.flip_geometry.scale_x;
        plan.scale_y = plan.flip_geometry.scale_y;
        plan.rotation = plan.flip_geometry.rotation;
        plan.flip_geometry_applied = true;
    }
    return plan;
}

}  // namespace nevergone::enemy_actions_sprite_update
