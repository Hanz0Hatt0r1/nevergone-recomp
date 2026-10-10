#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::game_levels_global_section {

struct StringRecord {
    std::uint32_t byte_length = 0;
    std::string value;
};

struct IntIntFloatRecord {
    std::int32_t first_i32 = 0;
    std::int32_t second_i32 = 0;
    float first_float = 0.0f;
};

struct UintPairRecord {
    std::uint32_t first_u32 = 0;
    std::uint32_t second_u32 = 0;
};

struct EnemyRecord {
    std::int32_t first_i32 = 0;
    std::int32_t second_i32 = 0;
    float first_float = 0.0f;
    std::int32_t third_i32 = 0;
    std::int32_t fourth_i32 = 0;
    std::int32_t fifth_i32 = 0;
    std::int32_t sixth_i32 = 0;
    std::int32_t seventh_i32 = 0;
};

struct Section {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    std::uint32_t string_count = 0;
    std::vector<StringRecord> strings;
    std::uint32_t first_u32 = 0;
    std::uint32_t second_u32 = 0;
    std::uint32_t int_int_float_count = 0;
    std::vector<IntIntFloatRecord> int_int_float_records;
    std::uint32_t uint_pair_count = 0;
    std::vector<UintPairRecord> uint_pair_records;
    std::uint32_t enemy_count = 0;
    std::vector<EnemyRecord> enemies;
};

// Parse the complete evidence-backed LoadGL_Global stream section beginning at
// its first uint32 count. Output is transactional on failure and end_offset is
// the exact position handed to LoadGL_PortNode().
bool parse_section(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        Section* out);

}  // namespace nevergone::game_levels_global_section
