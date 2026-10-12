#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::trap_actions_combo_section {

constexpr std::size_t kTupleBytes = 0x18u;
constexpr std::size_t kInt32CountPerTuple = 4u;
constexpr std::size_t kFloatCountPerTuple = 2u;
constexpr std::size_t kArray90Offset = 0x90u;
constexpr std::size_t kArray94Offset = 0x94u;
constexpr std::size_t kArray98Offset = 0x98u;
constexpr std::size_t kFirstFloatBaseOffset = 0xc4u;
constexpr std::size_t kSecondFloatBaseOffset = 0x254u;
constexpr std::size_t kFloatStrideBytes = sizeof(float);

struct DerivedActionComboValue {
    std::size_t target_array_offset = 0;
    std::size_t source_tuple_index = 0;
    std::int32_t field_14 = 0;
    std::int32_t field_18 = 1;
    std::int32_t field_1c = 1;
    std::int32_t field_20 = 0;
};

struct Tuple {
    std::array<std::int32_t, kInt32CountPerTuple> i32_values{};
    std::array<float, kFloatCountPerTuple> float_values{};
    std::size_t first_float_target_offset = 0;
    std::size_t second_float_target_offset = 0;
};

struct Block {
    std::uint32_t primary_record_count = 0;
    std::vector<Tuple> tuples;
    std::vector<DerivedActionComboValue> array_90_values;
    std::vector<DerivedActionComboValue> array_94_values;
    std::vector<DerivedActionComboValue> array_98_values;
    std::size_t bytes_consumed = 0;
};

// TrapActions Section F-equivalent has no serialized count of its own; native
// iterates exactly the already-parsed primary record count. Its 24-byte tuple
// wire format matches EnemyActions, but derived ActionComboValue destinations
// and one grouping rule differ, so the semantics are reconstructed here.
bool parse(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::uint32_t primary_record_count,
        Block* out);

}  // namespace nevergone::trap_actions_combo_section
