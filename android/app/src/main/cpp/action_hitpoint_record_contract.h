#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "enemy_actions_wbg_document.h"

namespace nevergone::action_hitpoint_record_contract {

struct RecordRef {
    // Source index in the parsed Section-E vector. Native stores the array at
    // MeActionsData+0xc8 for the player and EnemyActionsData+0x84 for enemies.
    std::size_t action_data_index = 0;
    // Zero-based ordinal among records for the selected current frame.
    std::size_t frame_ordinal = 0;
};

// The player and enemy hitpoint-count accessors scan Section E and assign this
// ordinal to ActionFrameData+0x54 for each match at +0x74.
std::vector<RecordRef> records_for_frame(
    const enemy_actions_wbg_document::Document& document,
    std::int32_t current_frame);

std::size_t count_for_frame(
    const enemy_actions_wbg_document::Document& document,
    std::int32_t current_frame) noexcept;

// Mirrors the record-selection prefix of MeActionsSystem::curFrameHitPoint()
// and EnemyActionsSystem::GetCurFrameHitPoint(): select the source array index
// matching current_frame and the requested ActionFrameData+0x54 ordinal.
std::optional<RecordRef> select_record(
    const enemy_actions_wbg_document::Document& document,
    std::int32_t current_frame,
    std::int32_t frame_ordinal) noexcept;

}  // namespace nevergone::action_hitpoint_record_contract
