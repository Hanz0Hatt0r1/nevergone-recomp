#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "enemy_actions_wbg_document.h"

namespace nevergone::enemy_actions_hitpoint_sprite_position {

struct Candidate {
    std::size_t action_data_index = 0;
    std::size_t frame_ordinal = 0;
    float x = 0.0f;
    float y = 0.0f;
};

// Mirrors the Section-E part of EnemyActionsSystem::swapBufferPos(). Native
// selects records by ActionFrameData+0x74, uses the same array index in the
// parallel +0x150 sprite array, and positions each sprite at
// (ActionFrameData+0x80 - system+0x284.x, ActionFrameData+0x84).
inline std::vector<Candidate> for_frame(
        const enemy_actions_wbg_document::Document& document,
        std::int32_t current_frame,
        float system_offset_x) {
    std::vector<Candidate> result;
    std::size_t ordinal = 0;
    for (std::size_t index = 0;
         index < document.fixed_tail_block.records.size();
         ++index) {
        const auto& record = document.fixed_tail_block.records[index];
        if (record.first_i32 != current_frame) continue;
        result.push_back(Candidate{
                index,
                ordinal++,
                record.float_values[2] - system_offset_x,
                record.float_values[3]});
    }
    return result;
}

}  // namespace nevergone::enemy_actions_hitpoint_sprite_position
