#include "enemy_actions_wbg_prefix.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

void append_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    out->push_back(static_cast<std::uint8_t>(value));
    out->push_back(static_cast<std::uint8_t>(value >> 8u));
    out->push_back(static_cast<std::uint8_t>(value >> 16u));
    out->push_back(static_cast<std::uint8_t>(value >> 24u));
}

void append_i32(std::vector<std::uint8_t>* out, std::int32_t value) {
    append_u32(out, static_cast<std::uint32_t>(value));
}

void append_f32(std::vector<std::uint8_t>* out, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(out, bits);
}

void append_tuple(
        std::vector<std::uint8_t>* out,
        const std::array<std::int32_t, 4>& ints,
        const std::array<float, 2>& floats) {
    for (const auto value : ints) append_i32(out, value);
    for (const auto value : floats) append_f32(out, value);
}

}  // namespace

int main() {
    namespace wbg = nevergone::enemy_actions_wbg_prefix;

    std::vector<std::uint8_t> bytes = {0xaa, 0xbb, 0xcc};
    append_tuple(&bytes, {1, 2, 3, 4}, {1.25f, -2.5f});
    append_tuple(&bytes, {-5, 6, -7, 8}, {9.5f, 10.75f});
    bytes.push_back(0xee);  // must remain outside the exact two-record parse.

    wbg::PrimaryIndexedTupleBlock block;
    assert(wbg::parse_primary_indexed_tuple_block(
            nevergone::hp_data::Reader(bytes), 3u, 2u, &block));
    assert(block.expected_record_count == 2u);
    assert(block.records.size() == 2u);
    assert(block.bytes_consumed == 2u * wbg::kPrimaryIndexedTupleBytes);
    assert(block.records[0].i32_values == (std::array<std::int32_t, 4>{1, 2, 3, 4}));
    assert(block.records[1].i32_values == (std::array<std::int32_t, 4>{-5, 6, -7, 8}));
    assert(std::fabs(block.records[0].float_values[0] - 1.25f) < 0.0001f);
    assert(std::fabs(block.records[0].float_values[1] + 2.5f) < 0.0001f);
    assert(std::fabs(block.records[1].float_values[0] - 9.5f) < 0.0001f);
    assert(std::fabs(block.records[1].float_values[1] - 10.75f) < 0.0001f);

    wbg::PrimaryIndexedTupleBlock zero;
    assert(wbg::parse_primary_indexed_tuple_block(
            nevergone::hp_data::Reader(bytes), bytes.size(), 0u, &zero));
    assert(zero.expected_record_count == 0u);
    assert(zero.records.empty());
    assert(zero.bytes_consumed == 0u);

    std::vector<std::uint8_t> truncated;
    append_tuple(&truncated, {11, 12, 13, 14}, {15.0f, 16.0f});
    wbg::PrimaryIndexedTupleBlock unchanged;
    unchanged.expected_record_count = 77u;
    unchanged.records.push_back({{88, 0, 0, 0}, {0.0f, 0.0f}});
    unchanged.bytes_consumed = 99u;
    assert(!wbg::parse_primary_indexed_tuple_block(
            nevergone::hp_data::Reader(truncated), 0u, 2u, &unchanged));
    assert(unchanged.expected_record_count == 77u);
    assert(unchanged.records.size() == 1u);
    assert(unchanged.records[0].i32_values[0] == 88);
    assert(unchanged.bytes_consumed == 99u);

    return 0;
}
