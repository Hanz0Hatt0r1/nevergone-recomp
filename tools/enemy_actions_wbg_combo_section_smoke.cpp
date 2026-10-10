#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
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
    using nevergone::enemy_actions_wbg_combo_section::kTupleBytes;
    using nevergone::enemy_actions_wbg_combo_section::parse;
    using nevergone::hp_data::Reader;

    std::vector<std::uint8_t> bytes{0xaa, 0xbb, 0xcc};
    append_tuple(&bytes, 1, -2, 3, -4, 1.25f, -2.5f);
    append_tuple(&bytes, 10, 20, 30, 40, 3.5f, 7.75f);
    const Reader reader(bytes);

    Block parsed;
    assert(parse(reader, 3u, 2u, &parsed));
    assert(parsed.primary_record_count == 2u);
    assert(parsed.bytes_consumed == 2u * kTupleBytes);
    assert(parsed.tuples.size() == 2u);

    assert(parsed.tuples[0].i32_values[0] == 1);
    assert(parsed.tuples[0].i32_values[1] == -2);
    assert(parsed.tuples[0].i32_values[2] == 3);
    assert(parsed.tuples[0].i32_values[3] == -4);
    assert(std::fabs(parsed.tuples[0].float_values[0] - 1.25f) < 0.0001f);
    assert(std::fabs(parsed.tuples[0].float_values[1] + 2.5f) < 0.0001f);
    assert(parsed.tuples[0].first_float_target_offset == kFirstObservedRegionOffset);
    assert(parsed.tuples[0].second_float_target_offset == kSecondObservedRegionOffset);

    assert(parsed.tuples[1].i32_values[0] == 10);
    assert(parsed.tuples[1].i32_values[3] == 40);
    assert(std::fabs(parsed.tuples[1].float_values[0] - 3.5f) < 0.0001f);
    assert(std::fabs(parsed.tuples[1].float_values[1] - 7.75f) < 0.0001f);
    assert(parsed.tuples[1].first_float_target_offset ==
           kFirstObservedRegionOffset + kObservedElementBytes);
    assert(parsed.tuples[1].second_float_target_offset ==
           kSecondObservedRegionOffset + kObservedElementBytes);

    // Zero primary records consume no Section F bytes.
    Block empty;
    assert(parse(reader, 3u, 0u, &empty));
    assert(empty.tuples.empty());
    assert(empty.bytes_consumed == 0u);

    // Truncation is transactional.
    std::vector<std::uint8_t> truncated(bytes.begin(), bytes.end() - 1);
    Block sentinel;
    sentinel.primary_record_count = 77u;
    sentinel.bytes_consumed = 99u;
    assert(!parse(Reader(truncated), 3u, 2u, &sentinel));
    assert(sentinel.primary_record_count == 77u);
    assert(sentinel.bytes_consumed == 99u);
    assert(sentinel.tuples.empty());

    // The two proven destination regions contain exactly 100 elements.
    assert(!parse(reader, 3u, static_cast<std::uint32_t>(kObservedSampleCount + 1u), &sentinel));
    assert(sentinel.primary_record_count == 77u);
    assert(sentinel.bytes_consumed == 99u);

    return 0;
}
