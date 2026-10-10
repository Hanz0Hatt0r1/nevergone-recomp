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

void append_compact_record(
        std::vector<std::uint8_t>* out,
        std::int32_t first_i32,
        float first_float,
        float second_float) {
    append_i32(out, first_i32);
    append_f32(out, first_float);
    append_f32(out, second_float);
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

    append_i32(&bytes, 2);
    append_compact_record(&bytes, 21, 1.5f, 2.5f);
    append_compact_record(&bytes, 22, 3.5f, 4.5f);
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

    const std::size_t compact_offset = prefix.bytes_consumed + record.bytes_consumed;
    wbg::CompactActionFrameBlock compact;
    assert(wbg::parse_compact_action_frame_block(reader, compact_offset, &compact));
    assert(compact.count_i32 == 2);
    assert(compact.records.size() == 2u);
    assert(compact.records[0].first_i32 == 21);
    assert(std::fabs(compact.records[0].first_float - 1.5f) < 0.0001f);
    assert(std::fabs(compact.records[0].second_float - 2.5f) < 0.0001f);
    assert(compact.records[1].first_i32 == 22);
    assert(std::fabs(compact.records[1].first_float - 3.5f) < 0.0001f);
    assert(std::fabs(compact.records[1].second_float - 4.5f) < 0.0001f);
    assert(compact.bytes_consumed ==
           wbg::kCompactBlockHeaderBytes + 2u * wbg::kCompactActionFrameBytes);
    assert(compact_offset + compact.bytes_consumed == bytes.size());

    // Prefix parsing is transactional on truncation.
    wbg::Prefix unchanged_prefix;
    unchanged_prefix.first_i32 = 0x12345678;
    const nevergone::hp_data::Reader short_prefix(
            std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 15));
    assert(!wbg::parse_prefix(short_prefix, &unchanged_prefix));
    assert(unchanged_prefix.first_i32 == 0x12345678);

    // Record parsing is transactional on truncation.
    const nevergone::hp_data::Reader truncated_record(
            std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + compact_offset - 1));
    wbg::ActionFrameRecord unchanged_record;
    unchanged_record.first_i32 = 99;
    unchanged_record.first_string = "keep";
    assert(!wbg::parse_action_frame_record(
            truncated_record, wbg::kPrefixBytes, &unchanged_record));
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

    // The original compact-loop test is signed: a negative count consumes only
    // the count field and produces no ActionFrameData objects.
    std::vector<std::uint8_t> negative_count_bytes;
    append_i32(&negative_count_bytes, -2);
    wbg::CompactActionFrameBlock negative_count;
    assert(wbg::parse_compact_action_frame_block(
            nevergone::hp_data::Reader(negative_count_bytes), 0, &negative_count));
    assert(negative_count.count_i32 == -2);
    assert(negative_count.records.empty());
    assert(negative_count.bytes_consumed == wbg::kCompactBlockHeaderBytes);

    // A positive count is pre-bounded against the remaining 12-byte records,
    // and failure must not mutate the caller's prior output.
    std::vector<std::uint8_t> truncated_compact;
    append_i32(&truncated_compact, 2);
    append_compact_record(&truncated_compact, 30, 6.0f, 7.0f);
    wbg::CompactActionFrameBlock unchanged_compact;
    unchanged_compact.count_i32 = 77;
    unchanged_compact.records.push_back({88, 0.0f, 0.0f});
    assert(!wbg::parse_compact_action_frame_block(
            nevergone::hp_data::Reader(truncated_compact), 0, &unchanged_compact));
    assert(unchanged_compact.count_i32 == 77);
    assert(unchanged_compact.records.size() == 1u);
    assert(unchanged_compact.records[0].first_i32 == 88);

    return 0;
}
