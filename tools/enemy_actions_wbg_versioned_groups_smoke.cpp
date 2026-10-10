#include "enemy_actions_wbg_prefix.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
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
        std::uint8_t framing) {
    append_i32(out, static_cast<std::int32_t>(bytes.size()));
    append_u8(out, framing);
    out->insert(out->end(), bytes.begin(), bytes.end());
}

void append_record(
        std::vector<std::uint8_t>* out,
        std::int32_t first_i32,
        float first_float,
        bool first_bool,
        std::int32_t second_i32,
        std::uint32_t first_u32,
        std::uint32_t second_u32,
        const std::vector<std::uint8_t>& first_string,
        const std::vector<std::uint8_t>& second_string,
        const std::vector<std::uint8_t>& third_string) {
    append_i32(out, first_i32);
    for (int i = 0; i < 12; ++i) append_f32(out, first_float + static_cast<float>(i));
    append_u8(out, first_bool ? 1u : 0u);
    append_i32(out, second_i32);
    append_u32(out, first_u32);
    append_u32(out, second_u32);
    append_string_field(out, first_string, 0xe1u);
    append_string_field(out, second_string, 0xe2u);
    append_string_field(out, third_string, 0xe3u);
}

std::vector<std::uint8_t> legacy_fixture() {
    std::vector<std::uint8_t> bytes;
    append_i32(&bytes, 1);
    append_record(
            &bytes, 71, 20.5f, true, -72, 0x12345678u, 0x89abcdefu,
            {'a'}, {'b', 'c'}, {'d', 'e', 'f'});
    append_i32(&bytes, -1);
    append_i32(&bytes, 0);
    append_i32(&bytes, 0);
    append_i32(&bytes, 0);
    append_i32(&bytes, 0);
    return bytes;
}

}  // namespace

int main() {
    namespace wbg = nevergone::enemy_actions_wbg_prefix;

    assert(wbg::versioned_group_count_for_header(0x68) == 6u);
    assert(wbg::versioned_group_count_for_header(0x69) == 20u);
    assert(wbg::versioned_group_count_for_header(-1) == 6u);

    const auto legacy_bytes = legacy_fixture();
    wbg::VersionedActionFrameBlock legacy;
    assert(wbg::parse_versioned_action_frame_groups(
            nevergone::hp_data::Reader(legacy_bytes), 0, 0x68, &legacy));
    assert(legacy.header_word0 == 0x68);
    assert(legacy.expected_group_count == 6u);
    assert(legacy.groups.size() == 6u);
    assert(legacy.groups[0].record_count_i32 == 1);
    assert(legacy.groups[0].records.size() == 1u);
    const auto& record = legacy.groups[0].records[0];
    assert(record.first_i32 == 71);
    for (int i = 0; i < 12; ++i) {
        assert(std::fabs(record.float_values[static_cast<std::size_t>(i)] -
                         (20.5f + static_cast<float>(i))) < 0.0001f);
    }
    assert(record.first_bool);
    assert(record.second_i32 == -72);
    assert(record.first_u32 == 0x12345678u);
    assert(record.second_u32 == 0x89abcdefu);
    assert(record.first_string_framing_u8 == 0xe1u);
    assert(record.first_string == "a");
    assert(record.second_string_framing_u8 == 0xe2u);
    assert(record.second_string == "bc");
    assert(record.third_string_framing_u8 == 0xe3u);
    assert(record.third_string == "def");
    assert(record.bytes_consumed == wbg::kVersionedActionFrameFixedBytes + 6u);
    assert(legacy.groups[0].bytes_consumed ==
           wbg::kVersionedGroupHeaderBytes + record.bytes_consumed);
    assert(legacy.groups[1].record_count_i32 == -1);
    assert(legacy.groups[1].records.empty());
    assert(legacy.bytes_consumed == legacy_bytes.size());

    // Modern headers require exactly 20 serialized group-count fields even
    // when every signed count is zero.
    std::vector<std::uint8_t> modern_bytes;
    for (std::size_t i = 0; i < wbg::kModernVersionedGroupCount; ++i) {
        append_i32(&modern_bytes, 0);
    }
    wbg::VersionedActionFrameBlock modern;
    assert(wbg::parse_versioned_action_frame_groups(
            nevergone::hp_data::Reader(modern_bytes), 0, 0x69, &modern));
    assert(modern.expected_group_count == 20u);
    assert(modern.groups.size() == 20u);
    assert(modern.bytes_consumed == 20u * wbg::kVersionedGroupHeaderBytes);

    // A missing mandatory group header fails transactionally.
    std::vector<std::uint8_t> five_headers;
    for (int i = 0; i < 5; ++i) append_i32(&five_headers, 0);
    wbg::VersionedActionFrameBlock unchanged;
    unchanged.header_word0 = 77;
    unchanged.expected_group_count = 99u;
    assert(!wbg::parse_versioned_action_frame_groups(
            nevergone::hp_data::Reader(five_headers), 0, 0x68, &unchanged));
    assert(unchanged.header_word0 == 77);
    assert(unchanged.expected_group_count == 99u);

    // Reserve the remaining mandatory group headers before accepting a positive
    // record count. Declaring two records with bytes for only one must fail
    // before vector allocation for two records.
    std::vector<std::uint8_t> truncated_records;
    append_i32(&truncated_records, 2);
    append_record(
            &truncated_records, 80, 1.0f, false, 81, 1u, 2u,
            {}, {}, {});
    for (int i = 0; i < 5; ++i) append_i32(&truncated_records, 0);
    assert(!wbg::parse_versioned_action_frame_groups(
            nevergone::hp_data::Reader(truncated_records), 0, 0x68, &unchanged));
    assert(unchanged.header_word0 == 77);

    return 0;
}
