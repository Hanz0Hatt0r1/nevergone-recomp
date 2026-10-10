#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "enemy_actions_layout_evidence.h"
#include "enemy_actions_wbg_combo_section.h"

namespace {

void append_u32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
    assert(bytes != nullptr);
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
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}

void append_tuple(
        std::vector<std::uint8_t>* bytes,
        std::int32_t a,
        std::int32_t b,
        std::int32_t c,
        std::int32_t d,
        float e,
        float f) {
    append_i32(bytes, a);
    append_i32(bytes, b);
    append_i32(bytes, c);
    append_i32(bytes, d);
    append_f32(bytes, e);
    append_f32(bytes, f);
}

}  // namespace

int main() {
    using nevergone::enemy_actions_layout_evidence::kFirstObservedRegionOffset;
    using nevergone::enemy_actions_layout_evidence::kObservedElementBytes;
    using nevergone::enemy_actions_layout_evidence::kObservedSampleCount;
    using nevergone::enemy_actions_layout_evidence::kSecondObservedRegionOffset;
    using nevergone::enemy_actions_wbg_combo_section::Block;
    using nevergone::enemy_actions_wbg_combo_section::kArray94Offset;
    using nevergone::enemy_actions_wbg_combo_section::kArray98Offset;
    using nevergone::enemy_actions_wbg_combo_section::kArray9cOffset;
    using nevergone::enemy_actions_wbg_combo_section::kTupleBytes;
    using nevergone::enemy_actions_wbg_combo_section::parse;
    using nevergone::hp_data::Reader;

    std::vector<std::uint8_t> bytes{0xaa, 0xbb, 0xcc};
    // Integer 1 drives +0x98 runs after +1, integer 2 drives the always-emitted
    // +0x9c field +0x1c after +1, and integer 3 drives +0x94 runs after +1.
    append_tuple(&bytes, 100, 0, 4, 0, 1.25f, -2.5f);  // states 1 / 5 / 1
    append_tuple(&bytes, 101, 2, -1, 5, 3.5f, 7.75f);  // states 3 / 0 / 6
    append_tuple(&bytes, 102, 2, std::numeric_limits<std::int32_t>::max(), 5, 8.5f, 9.5f);
    append_tuple(&bytes, 103, 2, 9, 5, 10.5f, 11.5f);
    const Reader reader(bytes);

    Block parsed;
    assert(parse(reader, 3u, 4u, &parsed));
    assert(parsed.primary_record_count == 4u);
    assert(parsed.bytes_consumed == 4u * kTupleBytes);
    assert(parsed.tuples.size() == 4u);

    assert(parsed.tuples[0].i32_values[0] == 100);
    assert(parsed.tuples[0].i32_values[1] == 0);
    assert(parsed.tuples[0].i32_values[2] == 4);
    assert(parsed.tuples[0].i32_values[3] == 0);
    assert(std::fabs(parsed.tuples[0].float_values[0] - 1.25f) < 0.0001f);
    assert(std::fabs(parsed.tuples[0].float_values[1] + 2.5f) < 0.0001f);
    assert(parsed.tuples[0].first_float_target_offset == kFirstObservedRegionOffset);
    assert(parsed.tuples[0].second_float_target_offset == kSecondObservedRegionOffset);
    assert(parsed.tuples[1].first_float_target_offset ==
           kFirstObservedRegionOffset + kObservedElementBytes);
    assert(parsed.tuples[1].second_float_target_offset ==
           kSecondObservedRegionOffset + kObservedElementBytes);

    // +0x9c always receives one initialized ActionComboValue per tuple. Only
    // field +0x1c is replaced with tuple integer 2 plus one.
    assert(parsed.array_9c_values.size() == 4u);
    for (std::size_t i = 0; i < parsed.array_9c_values.size(); ++i) {
        const auto& value = parsed.array_9c_values[i];
        assert(value.target_array_offset == kArray9cOffset);
        assert(value.source_tuple_index == i);
        assert(value.field_14 == 0);
        assert(value.field_18 == 1);
        assert(value.field_20 == 0);
    }
    assert(parsed.array_9c_values[0].field_1c == 5);
    assert(parsed.array_9c_values[1].field_1c == 0);
    assert(parsed.array_9c_values[2].field_1c == std::numeric_limits<std::int32_t>::min());
    assert(parsed.array_9c_values[3].field_1c == 10);

    // +0x94 run state is driven by tuple integer 3 plus one. Tuple 1 changes
    // the state from 1 to 6; tuple 3 closes the final run even though unchanged.
    assert(parsed.array_94_values.size() == 2u);
    assert(parsed.array_94_values[0].target_array_offset == kArray94Offset);
    assert(parsed.array_94_values[0].source_tuple_index == 1u);
    assert(parsed.array_94_values[0].field_14 == 0);
    assert(parsed.array_94_values[0].field_18 == 1);
    assert(parsed.array_94_values[0].field_1c == 1);
    assert(parsed.array_94_values[0].field_20 == 0);
    assert(parsed.array_94_values[1].source_tuple_index == 3u);
    assert(parsed.array_94_values[1].field_14 == 2);
    assert(parsed.array_94_values[1].field_18 == 3);
    assert(parsed.array_94_values[1].field_1c == 6);

    // +0x98 is the equivalent run boundary for tuple integer 1 plus one.
    assert(parsed.array_98_values.size() == 2u);
    assert(parsed.array_98_values[0].target_array_offset == kArray98Offset);
    assert(parsed.array_98_values[0].source_tuple_index == 1u);
    assert(parsed.array_98_values[0].field_14 == 0);
    assert(parsed.array_98_values[0].field_18 == 1);
    assert(parsed.array_98_values[0].field_1c == 1);
    assert(parsed.array_98_values[1].source_tuple_index == 3u);
    assert(parsed.array_98_values[1].field_14 == 2);
    assert(parsed.array_98_values[1].field_18 == 3);
    assert(parsed.array_98_values[1].field_1c == 3);

    // Zero primary records consume no Section F bytes and emit no objects.
    Block empty;
    assert(parse(reader, 3u, 0u, &empty));
    assert(empty.tuples.empty());
    assert(empty.array_94_values.empty());
    assert(empty.array_98_values.empty());
    assert(empty.array_9c_values.empty());
    assert(empty.bytes_consumed == 0u);

    // Truncation is transactional.
    std::vector<std::uint8_t> truncated(bytes.begin(), bytes.end() - 1);
    Block sentinel;
    sentinel.primary_record_count = 77u;
    sentinel.bytes_consumed = 99u;
    assert(!parse(Reader(truncated), 3u, 4u, &sentinel));
    assert(sentinel.primary_record_count == 77u);
    assert(sentinel.bytes_consumed == 99u);
    assert(sentinel.tuples.empty());
    assert(sentinel.array_94_values.empty());
    assert(sentinel.array_98_values.empty());
    assert(sentinel.array_9c_values.empty());

    // The two proven destination regions contain exactly 100 elements.
    assert(!parse(reader, 3u, static_cast<std::uint32_t>(kObservedSampleCount + 1u), &sentinel));
    assert(sentinel.primary_record_count == 77u);
    assert(sentinel.bytes_consumed == 99u);

    return 0;
}
