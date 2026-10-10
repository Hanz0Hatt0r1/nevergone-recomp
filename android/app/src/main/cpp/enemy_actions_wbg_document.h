#pragma once

#include <cstddef>
#include <vector>

#include "enemy_actions_wbg_combo_section.h"
#include "enemy_actions_wbg_final_table.h"
#include "enemy_actions_wbg_prefix.h"
#include "hp_data_reader.h"

namespace nevergone::enemy_actions_wbg_document {

struct Document {
    enemy_actions_wbg_prefix::Prefix prefix;
    std::vector<enemy_actions_wbg_prefix::ActionFrameRecord> primary_records;
    enemy_actions_wbg_prefix::CompactActionFrameBlock compact_block;
    enemy_actions_wbg_prefix::NestedActionFrameBlock nested_block;
    enemy_actions_wbg_prefix::VersionedActionFrameBlock versioned_block;
    enemy_actions_wbg_prefix::FixedTailActionFrameBlock fixed_tail_block;
    enemy_actions_wbg_combo_section::Block combo_block;
    enemy_actions_wbg_final_table::Table final_table;

    // Number of serialized bytes consumed by the recovered Sections A-G.
    std::size_t bytes_consumed = 0;
    // Bytes after the recovered parser boundary. They are reported rather than
    // rejected because the original malformed/trailing-file policy is unresolved.
    std::size_t trailing_bytes = 0;
};

// Compose the already evidence-backed Sections A-G into one bounded parse.
// This function adds no semantic field names and does not require EOF after
// Section G. Output is changed only if every recovered section parses.
bool parse(const hp_data::Reader& reader, Document* out);

}  // namespace nevergone::enemy_actions_wbg_document
