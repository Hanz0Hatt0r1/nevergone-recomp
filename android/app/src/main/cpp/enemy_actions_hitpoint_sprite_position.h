#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "enemy_actions_base_sprite_update.h"
#include "enemy_actions_wbg_document.h"
#include "game_scene_direct_geometry.h"
#include "me_actions_initial_sprite_transform.h"

namespace nevergone::enemy_actions_hitpoint_sprite_position {

constexpr int kHitPointSpriteSize = 8;

struct Candidate {
    std::size_t action_data_index = 0;
    std::size_t frame_ordinal = 0;
    // Center of the transformed 8x8 hitPoint sprite in EnemyActionsSystem
    // coordinates, after the current +0x284.x swapBufferPos adjustment.
    float x = 0.0f;
    float y = 0.0f;
};

// Reconstructs the Section-E sprite transform written by
// EnemyActionsSystem::updateActionFrameMoveValue(), then the position
// adjustment made by swapBufferPos(). The fixed-tail row's +0x74 frame index
// selects it; the same array index addresses the parallel +0x150 sprite array.
inline std::vector<Candidate> for_frame(
        const enemy_actions_wbg_document::Document& document,
        std::int32_t current_frame,
        const enemy_actions_base_sprite_update::SystemInputs& system) {
    std::vector<Candidate> result;
    std::size_t ordinal = 0;
    for (std::size_t index = 0;
         index < document.fixed_tail_block.records.size();
        ++index) {
        const auto& record = document.fixed_tail_block.records[index];
        if (record.first_i32 != current_frame) continue;
        const std::size_t frame_ordinal = ordinal++;

        me_actions_initial_sprite_transform::Plan transform;
        transform.position_x = record.float_values[0] + system.field_194;
        transform.position_y = record.float_values[1] + system.field_198 -
                system.field_280;
        transform.anchor_x = record.float_values[2];
        transform.anchor_y = record.float_values[3];
        transform.scale_x = record.float_values[9];
        transform.scale_y = 1.0f;
        transform.rotation = 0.0f;

        if (system.flag_16a != 0u) {
            const float pivot_x = record.float_values[6];
            const float pivot_y = record.float_values[7];
            float delta_x = pivot_x - transform.position_x;
            float delta_y = pivot_y - transform.position_y;
            delta_y = -delta_y;
            transform.position_x = pivot_x + delta_x;
            transform.position_y = pivot_y + delta_y;
            transform.anchor_x = 0.5f - (transform.anchor_x - 0.5f);
            transform.scale_x = -transform.scale_x;
        }

        // swapBufferPos applies the action-system point after updateData has
        // stored the transformed position at ActionFrameData+0x80/+0x84.
        transform.position_x -= system.field_284_x;
        const auto bounds = game_scene_direct_geometry::player_action_sprite_bounds(
                transform, kHitPointSpriteSize, kHitPointSpriteSize);
        if (!bounds.has_value()) continue;

        result.push_back(Candidate{
                index,
                frame_ordinal,
                bounds->x + bounds->width * 0.5f,
                bounds->y + bounds->height * 0.5f});
    }
    return result;
}

}  // namespace nevergone::enemy_actions_hitpoint_sprite_position
