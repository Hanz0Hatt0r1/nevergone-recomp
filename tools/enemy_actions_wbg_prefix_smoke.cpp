#include "enemy_actions_wbg_prefix.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

void append_u8(std::vector<std::uint8_t>* out, std::uint8_t value) {
    out->push_back(value);
}

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
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(out, bits);
}

void append_string_field(
        std::vector<std::uint8_t>* out,
        const std::vector<std::uint8_t>& bytes,
        std::uint8_t separator) {
    append_i32(out, static_cast<std::int32_t>(bytes.size()));
    append_u8(out, separator);
    out->insert(out->end(), bytes.begin(), bytes.end());
}

std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> bytes;
    append_i32(&bytes, 105);
    append_f32(&bytes, 1.25f);
    append_f32(&bytes, -2.5f);
    append_u32(&bytes, 1u);

    append_i32(&bytes, 7);
    for (int i = 0; i < 12; ++i) append_f32(&bytes, 0.5f + static_cast<float>(i));
    append_u8(&bytes, 1u);
    append_i32(&bytes, -3);
    append_f32(&bytes, 9.25f);
    append_string_field(&bytes, {'a', 'b', 'c'}, 0xaau);
    append_string_field(&bytes, {}, 0xbbu);
    append_string_field(&bytes, {'d', 0, 'f', 'g'}, 0xccu);
    return bytes;
}

}  // namespace

int main() {
    namespace wbg = nevergone::enemy_actions_wbg_prefix;
    const auto bytes = fixture();
    const nevergone::hp_data::Reader reader(bytes);

    wbg::Prefix prefix;
    assert(wbg::parse_prefix(reader, &prefix));
    assert(prefix.first_i32 == 105);
    assert(std::fabs(prefix.first_float - 1.25f) < 0.0001f);
    assert(std::fabs(prefix.second_float + 2.5f) < 0.0001f);
    assert(prefix.action_frame_count == 1u);
    assert(prefix.bytes_consumed == wbg::kPrefixBytes);

    wbg::ActionFrameRecord record;
    assert(wbg::parse_action_frame_record(reader, prefix.bytes_consumed, &record));
    assert(record.first_i32 == 7);
    for (int i = 0; i < 12; ++i) {
        assert(std::fabs(record.float_values[static_cast<std::size_t>(i)] -
                         (0.5f + static_cast<float>(i))) < 0.0001f);
    }
    assert(record.first_bool);
    assert(record.second_i32 == -3);
    assert(std::fabs(record.trailing_float - 9.25f) < 0.0001f);
    assert(record.first_string_length_i32 == 3);
    assert(record.first_string == "abc");
    assert(record.second_string_length_i32 == 0);
    assert(record.second_string.empty());
    assert(record.third_string_length_i32 == 4);
    assert(record.third_string == "d");
    assert(record.bytes_consumed == wbg::kActionFrameFixedBytes + 7u);
    assert(prefix.bytes_consumed + record.bytes_consumed == bytes.size());

    // Prefix parsing is transactional on truncation.
    wbg::Prefix unchanged_prefix;
    unchanged_prefix.first_i32 = 0x12345678;
    const nevergone::hp_data::Reader short_prefix(
            std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 15));
    assert(!wbg::parse_prefix(short_prefix, &unchanged_prefix));
    assert(unchanged_prefix.first_i32 == 0x12345678);

    // Record parsing is transactional on truncation.
    auto truncated = bytes;
    truncated.pop_back();
    wbg::ActionFrameRecord unchanged_record;
    unchanged_record.first_i32 = 99;
    unchanged_record.first_string = "keep";
    assert(!wbg::parse_action_frame_record(
            nevergone::hp_data::Reader(truncated), wbg::kPrefixBytes, &unchanged_record));
    assert(unchanged_record.first_i32 == 99);
    assert(unchanged_record.first_string == "keep");

    // Negative signed lengths and payloads that would cross the original
    // 0x100-byte temporary buffers are rejected before a char copy.
    auto negative_length = bytes;
    const std::size_t first_length_offset = wbg::kPrefixBytes + 61u;
    negative_length[first_length_offset + 0] = 0xffu;
    negative_length[first_length_offset + 1] = 0xffu;
    negative_length[first_length_offset + 2] = 0xffu;
    negative_length[first_length_offset + 3] = 0xffu;
    assert(!wbg::parse_action_frame_record(
            nevergone::hp_data::Reader(negative_length), wbg::kPrefixBytes, &record));

    auto oversized_length = bytes;
    oversized_length[first_length_offset + 0] = 0x00u;
    oversized_length[first_length_offset + 1] = 0x01u;
    oversized_length[first_length_offset + 2] = 0x00u;
    oversized_length[first_length_offset + 3] = 0x00u;
    assert(!wbg::parse_action_frame_record(
            nevergone::hp_data::Reader(oversized_length), wbg::kPrefixBytes, &record));

    return 0;
}
