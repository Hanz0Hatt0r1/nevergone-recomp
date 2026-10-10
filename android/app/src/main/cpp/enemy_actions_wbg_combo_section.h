#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::enemy_actions_wbg_combo_section {

// Section F has no serialized count of its own. loadWBGFile iterates exactly
// the primary-record count previously stored at EnemyActionsData+0xc4.
constexpr std::size_t kTupleBytes = 0x18u;
constexpr std::size_t kInt32CountPerTuple = 4u;
constexpr std::size_t kFloatCountPerTuple = 2u;

struct Tuple {
    std::array<std::int32_t, kInt32CountPerTuple> i32_values{};
    std::array<float, kFloatCountPerTuple> float_values{};

    // Proven write targets for this tuple index. These are byte offsets inside
    // EnemyActionsData, not semantic field names.
    std::size_t first_float_target_offset = 0;
    std::size_t second_float_target_offset = 0;
};

struct Block {
    std::uint32_t primary_record_count = 0;
    std::vector<Tuple> tuples;
    std::size_t bytes_consumed = 0;
};

// Parse the evidence-backed Section F boundary from start_offset. The count is
// supplied by the already-parsed prefix because the stream contains no Section
// F count field. Output is transactional on truncation or an out-of-range count.
bool parse(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::uint32_t primary_record_count,
        Block* out);

}  // namespace nevergone::enemy_actions_wbg_combo_section
