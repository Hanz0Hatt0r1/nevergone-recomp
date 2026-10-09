#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::game_levels_actions_section {

struct ActionRecord {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    std::uint32_t first_string_length = 0;
    std::string first_string;
    std::uint32_t second_string_length = 0;
    std::string second_string;
    std::uint32_t first_u32 = 0;
    std::uint32_t second_u32 = 0;
    std::int32_t first_i32 = 0;
    float point_x = 0.0f;
    float point_y = 0.0f;
    bool has_gate_gt1_fields = false;
    std::int32_t gate_gt1_first_i32 = 0;
    std::int32_t gate_gt1_second_i32 = 0;
    bool has_gate_gt3_fields = false;
    std::int32_t gate_gt3_first_i32 = 0;
    std::int32_t gate_gt3_second_i32 = 0;
};

struct Section {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    std::uint32_t action_count = 0;
    std::vector<ActionRecord> actions;
};

// Parses one complete LoadGL_Actions record at an explicit stream offset.
// The format gate is the signed GameLevels value already recovered at the
// beginning of LoadGL_Scene(). Output is transactional on failure.
bool parse_action_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t format_gate,
        ActionRecord* out);

// Parses the complete LoadGL_Actions section beginning at its uint32 count.
// end_offset is the exact stream position handed to the following section.
bool parse_section(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t format_gate,
        Section* out);

}  // namespace nevergone::game_levels_actions_section
