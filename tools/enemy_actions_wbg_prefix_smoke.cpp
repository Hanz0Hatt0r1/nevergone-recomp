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

void append_action_frame(
        std::vector<std::uint8_t>* out,
        std::int32_t first_i32,
        float float_base,
        std::int32_t second_i32,
        float trailing_float,
        const std::vector<std::uint8_t>& first,
        const std::vector<std::uint8_t>& second,
        const std::vector<std::uint8_t>& third) {
    append_i32(out, first_i32);
    for (int i = 0; i < 12; ++i) append_f32(out, float_base + static_cast<float>(i));
    append_u8(out, first_i32 >= 0 ? 1u : 0u);
    append_i32(out, second_i32);
    append_f32(out, trailing_float);
    append_string_field(out, first, 0xaau);
    append_string_field(out, second, 0xbbu);
    append_string_field(out, third, 0xccu);
}

void append_secondary(
        std::vector<std::uint8_t>* out,
        std::int32_t first_i32,
        float first_float,
        float second_float) {
    append_i32(out, first_i32);
    append_f32(out, first_float);
    append_f32(out, second_float);
}

struct Fixture {
    std::vector<std::uint8_t> bytes;
    std::size_t first_record_offset = 0;
    std::size_t second_record_offset = 0;
    std::size_t secondary_count_offset = 0;
};

Fixture fixture() {
    Fixture result;
    auto& bytes = result.bytes;
    append_i32(&bytes, 105);
    append_f32(&bytes, 1.25f);
    append_f32(&bytes, -2.5f);
    append_u32(&bytes, 2u);

    result.first_record_offset = bytes.size();
    append_action_frame(
            &bytes, 7, 0.5f, -3, 9.25f,
            {'a', 'b', 'c'}, {}, {'d', 0, 'f', 'g'});
    result.second_record_offset = bytes.size();
    append_action_frame(
            &bytes, -8, -4.0f, 17, -1.5f,
            {'x'}, {'y', 'z'}, {});

    result.secondary_count_offset = bytes.size();
    append_i32(&bytes, 2);
    append_secondary(&bytes, 11, 2.5f, -3.5f);
    append_secondary(&bytes, -22, 4.25f, 8.5f);
    return result;
}

void write_i32(std::vector<std::uint8_t>* bytes, std::size_t offset, std::int32_t value) {
    const auto u = static_cast<std::uint32_t>(value);
    (*bytes)[offset + 0] = static_cast<std::uint8_t>(u);
    (*bytes)[offset + 1] = static_cast<std::uint8_t>(u >> 8u);
    (*bytes)[offset + 2] = static_cast<std::uint8_t>(u >> 16u);
    (*bytes)[offset + 3] = static_cast<std::uint8_t>(u >> 24u);
}

}  // namespace

int main() {
    namespace wbg = nevergone::enemy_actions_wbg_prefix;
    const Fixture data = fixture();
    const nevergone::hp_data::Reader reader(data.bytes);

    wbg::Prefix prefix;
    assert(wbg::parse_prefix(reader, &prefix));
    assert(prefix.first_i32 == 105);
    assert(std::fabs(prefix.first_float - 1.25f) < 0.0001f);
    assert(std::fabs(prefix.second_float + 2.5f) < 0.0001f);
    assert(prefix.action_frame_count == 2u);
    assert(prefix.bytes_consumed == wbg::kPrefixBytes);

    wbg::ActionFrameRecord first;
    assert(wbg::parse_action_frame_record(reader, data.first_record_offset, &first));
    assert(first.first_i32 == 7);
    for (int i = 0; i < 12; ++i) {
        assert(std::fabs(first.float_values[static_cast<std::size_t>(i)] -
                         (0.5f + static_cast<float>(i))) < 0.0001f);
    }
    assert(first.first_bool);
    assert(first.second_i32 == -3);
    assert(std::fabs(first.trailing_float - 9.25f) < 0.0001f);
    assert(first.first_string_length_i32 == 3);
    assert(first.first_string == "abc");
    assert(first.second_string_length_i32 == 0);
    assert(first.second_string.empty());
    assert(first.third_string_length_i32 == 4);
    assert(first.third_string == "d");
    assert(first.bytes_consumed == wbg::kActionFrameFixedBytes + 7u);

    wbg::SecondaryRecord secondary;
    const std::size_t first_secondary_offset = data.secondary_count_offset + 4u;
    assert(wbg::parse_secondary_record(reader, first_secondary_offset, &secondary));
    assert(secondary.first_i32 == 11);
    assert(std::fabs(secondary.first_float - 2.5f) < 0.0001f);
    assert(std::fabs(secondary.second_float + 3.5f) < 0.0001f);
    assert(secondary.bytes_consumed == wbg::kSecondaryRecordBytes);

    wbg::InitialSections parsed;
    assert(wbg::parse_initial_sections(reader, &parsed));
    assert(parsed.prefix.action_frame_count == 2u);
    assert(parsed.action_frames.size() == 2u);
    assert(parsed.action_frames[0].first_i32 == 7);
    assert(parsed.action_frames[1].first_i32 == -8);
    assert(!parsed.action_frames[1].first_bool);
    assert(parsed.action_frames[1].first_string == "x");
    assert(parsed.action_frames[1].second_string == "yz");
    assert(parsed.secondary_record_count_i32 == 2);
    assert(parsed.secondary_records.size() == 2u);
    assert(parsed.secondary_records[0].first_i32 == 11);
    assert(parsed.secondary_records[1].first_i32 == -22);
    assert(std::fabs(parsed.secondary_records[1].second_float - 8.5f) < 0.0001f);
    assert(parsed.bytes_consumed == data.bytes.size());

    // Prefix parsing is transactional on truncation.
    wbg::Prefix unchanged_prefix;
    unchanged_prefix.first_i32 = 0x12345678;
    const nevergone::hp_data::Reader short_prefix(
            std::vector<std::uint8_t>(data.bytes.begin(), data.bytes.begin() + 15));
    assert(!wbg::parse_prefix(short_prefix, &unchanged_prefix));
    assert(unchanged_prefix.first_i32 == 0x12345678);

    // Full initial-section parsing is transactional if the final secondary
    // record is truncated.
    auto truncated = data.bytes;
    truncated.pop_back();
    wbg::InitialSections unchanged_sections;
    unchanged_sections.prefix.first_i32 = 99;
    unchanged_sections.secondary_record_count_i32 = 77;
    assert(!wbg::parse_initial_sections(
            nevergone::hp_data::Reader(truncated), &unchanged_sections));
    assert(unchanged_sections.prefix.first_i32 == 99);
    assert(unchanged_sections.secondary_record_count_i32 == 77);
    assert(unchanged_sections.action_frames.empty());

    // Negative Section B count follows the original signed comparison: the
    // block is present but performs zero record iterations.
    auto negative_secondary_count = data.bytes;
    write_i32(&negative_secondary_count, data.secondary_count_offset, -4);
    negative_secondary_count.resize(data.secondary_count_offset + 4u);
    wbg::InitialSections negative_sections;
    assert(wbg::parse_initial_sections(
            nevergone::hp_data::Reader(negative_secondary_count), &negative_sections));
    assert(negative_sections.secondary_record_count_i32 == -4);
    assert(negative_sections.secondary_records.empty());
    assert(negative_sections.bytes_consumed == negative_secondary_count.size());

    // Positive counts are bounded by the bytes that could minimally contain
    // their records before any vector growth or per-record parse begins.
    auto impossible_secondary_count = data.bytes;
    write_i32(&impossible_secondary_count, data.secondary_count_offset, 1000);
    assert(!wbg::parse_initial_sections(
            nevergone::hp_data::Reader(impossible_secondary_count), &parsed));

    auto impossible_primary_count = data.bytes;
    write_i32(&impossible_primary_count, 12u, 1000000);
    assert(!wbg::parse_initial_sections(
            nevergone::hp_data::Reader(impossible_primary_count), &parsed));

    // Negative signed string lengths and payloads that would cross the original
    // 0x100-byte temporary buffers are rejected before a char copy.
    auto negative_length = data.bytes;
    const std::size_t first_length_offset = data.first_record_offset + 61u;
    write_i32(&negative_length, first_length_offset, -1);
    assert(!wbg::parse_action_frame_record(
            nevergone::hp_data::Reader(negative_length), data.first_record_offset, &first));

    auto oversized_length = data.bytes;
    write_i32(&oversized_length, first_length_offset, 0x100);
    assert(!wbg::parse_action_frame_record(
            nevergone::hp_data::Reader(oversized_length), data.first_record_offset, &first));

    return 0;
}
