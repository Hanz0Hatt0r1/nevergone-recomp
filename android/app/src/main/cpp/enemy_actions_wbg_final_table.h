#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::enemy_actions_wbg_final_table {

// Section G has no serialized count field of its own. loadWBGFile iterates once
// per primary record (the unsigned count already stored at EnemyActionsData+0xc4).
constexpr std::size_t kEntryBytes = sizeof(std::int32_t);
constexpr std::size_t kActionFrameReciprocalTargetOffset = 0x5cu;

struct Entry {
    std::int32_t serialized_value = 0;
    float reciprocal_value = 0.0f;

    // Proven destination byte offset inside the selected ActionFrameData.
    std::size_t action_frame_target_offset = kActionFrameReciprocalTargetOffset;
};

struct Table {
    std::uint32_t primary_record_count = 0;
    std::vector<Entry> entries;
    std::size_t bytes_consumed = 0;
};

// Parse the evidence-backed final per-primary int table from start_offset.
// primary_record_count comes from the already-parsed WBG header; Section G has
// no count header. For every entry the original stores 1.0f / float(value) at
// ActionFrameData+0x5c. Output is transactional on truncation.
bool parse(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::uint32_t primary_record_count,
        Table* out);

}  // namespace nevergone::enemy_actions_wbg_final_table
