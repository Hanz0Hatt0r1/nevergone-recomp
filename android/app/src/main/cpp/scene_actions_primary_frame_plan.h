#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "scene_actions_base_sprite_update.h"
#include "scene_actions_flip_x_geometry.h"
#include "trap_actions_document.h"

namespace nevergone::scene_actions_primary_frame_plan {

struct Plan {
    std::size_t frame_index = 0;
    std::string atlas_plist;
    std::string frame_name;
    float position_x = 0.0f;
    float position_y = 0.0f;
    float anchor_x = 0.0f;
    float anchor_y = 0.0f;
    float rotation = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    bool flip_x = false;
};

// Compose the two independently recovered primary-sprite updateData geometry
// branches for the selected TrapActions primary frame. No rendering side effect
// happens here: this is the deterministic resource/geometry descriptor consumed
// by later pixel staging and GL code.
inline std::optional<Plan> build(
        const trap_actions_document::Document& document,
        std::size_t frame_index,
        const scene_actions_base_sprite_update::SystemInputs& system) {
    if (frame_index >= document.core.primary.records.size()) return std::nullopt;
    const auto& frame = document.core.primary.records[frame_index];
    if (frame.first_string.empty() || frame.third_string.empty()) return std::nullopt;

    Plan plan;
    plan.frame_index = frame_index;
    plan.atlas_plist = frame.first_string;
    plan.frame_name = frame.third_string;

    if (!system.flip_x_15e) {
        const auto normal = scene_actions_base_sprite_update::build_non_flipped(frame, system);
        if (!normal.has_value()) return std::nullopt;
        plan.position_x = normal->position_x;
        plan.position_y = normal->position_y;
        plan.anchor_x = normal->anchor_x;
        plan.anchor_y = normal->anchor_y;
        plan.rotation = normal->rotation;
        plan.scale_x = normal->scale_x;
        plan.scale_y = normal->scale_y;
        plan.flip_x = false;
        return plan;
    }

    const auto reflected = scene_actions_flip_x_geometry::build(frame, system);
    if (!reflected.has_value()) return std::nullopt;
    plan.position_x = reflected->position_x;
    plan.position_y = reflected->position_y;
    plan.anchor_x = reflected->anchor_x;
    plan.anchor_y = reflected->anchor_y;
    plan.rotation = reflected->rotation;
    plan.scale_x = reflected->scale_x;
    plan.scale_y = reflected->scale_y;
    plan.flip_x = true;
    return plan;
}

}  // namespace nevergone::scene_actions_primary_frame_plan
