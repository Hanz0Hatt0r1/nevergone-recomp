#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "trap_actions_combo_section.h"

namespace combo = nevergone::trap_actions_combo_section;

namespace {

void append_u32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
    bytes->push_back(static_cast<std::uint8_t>(value & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
}

void append_i32(std::vector<std::uint8_t>* bytes, std::int32_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
}

void append_f32(std::vector<std::uint8_t>* bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}

void append_tuple(
        std::vector<std::uint8_t>* bytes,
        std::int32_t a,
        std::int32_t b,
        std::int32_t c,
        std::int32_t d,
        float first,
        float second) {
    append_i32(bytes, a);
    append_i32(bytes, b);
    append_i32(bytes, c);
    append_i32(bytes, d);
    append_f32(bytes, first);
    append_f32(bytes, second);
}

}  // namespace

int main() {
    std::vector<std::uint8_t> bytes(5u, 0x5au);
    const std::size_t start = bytes.size();
    append_tuple(&bytes, 100, 0, 4, 1, 1.5f, 10.0f);
    append_tuple(&bytes, 101, 2, 5, 7, 2.5f, 20.0f);
    append_tuple(&bytes, 102, 2, 6, 7, 3.5f, 30.0f);

    nevergone::hp_data::Reader reader(bytes);
    combo::Block block;
    assert(combo::parse(reader, start, 3u, &block));
    assert(block.primary_record_count == 3u);
    assert(block.tuples.size() == 3u);
    assert(block.bytes_consumed == 3u * combo::kTupleBytes);

    assert(block.tuples[0].first_float_target_offset == combo::kFirstFloatBaseOffset);
    assert(block.tuples[1].first_float_target_offset == combo::kFirstFloatBaseOffset + 4u);
    assert(block.tuples[2].second_float_target_offset == combo::kSecondFloatBaseOffset + 8u);
    assert(block.tuples[1].float_values[0] == 2.5f);
    assert(block.tuples[1].float_values[1] == 20.0f);

    // +0x90 receives one object per tuple with field +0x1c = third int + 1.
    assert(block.array_90_values.size() == 3u);
    assert(block.array_90_values[0].target_array_offset == combo::kArray90Offset);
    assert(block.array_90_values[0].field_1c == 5);
    assert(block.array_90_values[1].field_1c == 6);
    assert(block.array_90_values[2].field_1c == 7);

    // +0x94 groups the raw fourth int. Tuple 0 keeps initial state 1; tuple 1
    // changes it to 7, and the last tuple closes the final run.
    assert(block.array_94_values.size() == 2u);
    assert(block.array_94_values[0].target_array_offset == combo::kArray94Offset);
    assert(block.array_94_values[0].source_tuple_index == 1u);
    assert(block.array_94_values[0].field_14 == 0);
    assert(block.array_94_values[0].field_18 == 1);
    assert(block.array_94_values[0].field_1c == 1);
    assert(block.array_94_values[1].source_tuple_index == 2u);
    assert(block.array_94_values[1].field_14 == 2);
    assert(block.array_94_values[1].field_18 == 2);
    assert(block.array_94_values[1].field_1c == 7);

    // +0x98 groups second int + 1. It changes 1 -> 3 at tuple 1 and closes at 2.
    assert(block.array_98_values.size() == 2u);
    assert(block.array_98_values[0].target_array_offset == combo::kArray98Offset);
    assert(block.array_98_values[0].source_tuple_index == 1u);
    assert(block.array_98_values[0].field_14 == 0);
    assert(block.array_98_values[0].field_1c == 1);
    assert(block.array_98_values[1].source_tuple_index == 2u);
    assert(block.array_98_values[1].field_14 == 2);
    assert(block.array_98_values[1].field_1c == 3);

    // No serialized section count: zero primary records consumes zero bytes.
    combo::Block empty;
    assert(combo::parse(reader, start, 0u, &empty));
    assert(empty.tuples.empty());
    assert(empty.bytes_consumed == 0u);

    // Declared tuple count is bounded before allocation and output is transactional.
    combo::Block unchanged;
    unchanged.bytes_consumed = 77u;
    assert(!combo::parse(reader, start, 4u, &unchanged));
    assert(unchanged.bytes_consumed == 77u);
    assert(unchanged.tuples.empty());

    return 0;
}
