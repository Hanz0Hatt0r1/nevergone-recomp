#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "enemy_actions_wbg_document.h"

namespace nevergone::player_action_hitpoint_contract {

struct RecordRef {
    // Index into MeActionsData+0xc8 and the parsed Section-E record vector.
    std::size_t action_data_index = 0;
    // Zero-based ordinal among records for the selected current frame.
    std::size_t frame_ordinal = 0;
};

// MeActionsSystem::curFrameHitPointCount() scans the Section-E records and
// assigns this ordinal to ActionFrameData+0x54 for each match at +0x74.
std::vector<RecordRef> records_for_frame(
    const enemy_actions_wbg_document::Document& document,
    std::int32_t current_frame);

// Mirrors the record-selection prefix of MeActionsSystem::curFrameHitPoint():
// select the source array index matching current_frame and the requested
// ActionFrameData+0x54 ordinal. Sprite lookup/bounding-box work follows this
// selection in the native function and remains outside this contract.
std::optional<RecordRef> select_record(
    const enemy_actions_wbg_document::Document& document,
    std::int32_t current_frame,
    std::int32_t frame_ordinal) noexcept;

}  // namespace nevergone::player_action_hitpoint_contract
