#include "enemy_actions_wbg_prefix.h"

#include <limits>
#include <utility>

namespace nevergone::enemy_actions_wbg_prefix {
namespace {

bool read_string_field(
        hp_data::Cursor* cursor,
        std::int32_t* out_length,
        std::string* out_string) {
    if (cursor == nullptr || out_length == nullptr || out_string == nullptr) return false;

    std::int32_t length_i32 = 0;
    if (!cursor->read_i32_le(&length_i32) || length_i32 < 0) return false;
    const auto length = static_cast<std::size_t>(length_i32);
    if (length > kMaxStringPayloadBytes) return false;
    if (!cursor->skip(kStringSeparatorBytes)) return false;

    std::string value;
    if (!cursor->read_fixed_string(length, &value)) return false;

    *out_length = length_i32;
    *out_string = std::move(value);
    return true;
}

bool positive_count_fits(
        std::int32_t count,
        std::size_t remaining,
        std::size_t minimum_record_bytes) {
    if (count <= 0) return true;
    if (minimum_record_bytes == 0u) return false;
    return static_cast<std::uint64_t>(count) <=
            static_cast<std::uint64_t>(remaining / minimum_record_bytes);
}

}  // namespace

bool parse_prefix(const hp_data::Reader& reader, Prefix* out) {
    if (out == nullptr) return false;

    hp_data::Cursor cursor(reader);
    Prefix parsed;
    if (!cursor.read_i32_le(&parsed.first_i32) ||
        !cursor.read_f32_le(&parsed.first_float) ||
        !cursor.read_f32_le(&parsed.second_float) ||
        !cursor.read_u32_le(&parsed.action_frame_count)) {
        return false;
    }
    parsed.bytes_consumed = cursor.offset();
    if (parsed.bytes_consumed != kPrefixBytes) return false;

    *out = std::move(parsed);
    return true;
}

bool parse_action_frame_record(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        ActionFrameRecord* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    ActionFrameRecord parsed;
    if (!cursor.read_i32_le(&parsed.first_i32)) return false;
    for (float& value : parsed.float_values) {
        if (!cursor.read_f32_le(&value)) return false;
    }
    if (!cursor.read_bool8(&parsed.first_bool) ||
        !cursor.read_i32_le(&parsed.second_i32) ||
        !cursor.read_f32_le(&parsed.trailing_float) ||
        !read_string_field(&cursor, &parsed.first_string_length_i32, &parsed.first_string) ||
        !read_string_field(&cursor, &parsed.second_string_length_i32, &parsed.second_string) ||
        !read_string_field(&cursor, &parsed.third_string_length_i32, &parsed.third_string)) {
        return false;
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    const std::size_t payload_bytes =
            static_cast<std::size_t>(parsed.first_string_length_i32) +
            static_cast<std::size_t>(parsed.second_string_length_i32) +
            static_cast<std::size_t>(parsed.third_string_length_i32);
    if (payload_bytes > std::numeric_limits<std::size_t>::max() - kActionFrameFixedBytes ||
        parsed.bytes_consumed != kActionFrameFixedBytes + payload_bytes) {
        return false;
    }

    *out = std::move(parsed);
    return true;
}

bool parse_secondary_record(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        SecondaryRecord* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    SecondaryRecord parsed;
    if (!cursor.read_i32_le(&parsed.first_i32) ||
        !cursor.read_f32_le(&parsed.first_float) ||
        !cursor.read_f32_le(&parsed.second_float)) {
        return false;
    }
    parsed.bytes_consumed = cursor.offset() - start_offset;
    if (parsed.bytes_consumed != kSecondaryRecordBytes) return false;

    *out = parsed;
    return true;
}

bool parse_initial_sections(const hp_data::Reader& reader, InitialSections* out) {
    if (out == nullptr) return false;

    InitialSections parsed;
    if (!parse_prefix(reader, &parsed.prefix)) return false;

    std::size_t offset = parsed.prefix.bytes_consumed;
    const std::size_t initial_remaining = reader.size() - offset;
    if (parsed.prefix.action_frame_count > initial_remaining / kActionFrameFixedBytes) {
        return false;
    }

    for (std::uint32_t i = 0; i < parsed.prefix.action_frame_count; ++i) {
        ActionFrameRecord record;
        if (!parse_action_frame_record(reader, offset, &record)) return false;
        if (record.bytes_consumed > reader.size() - offset) return false;
        offset += record.bytes_consumed;
        parsed.action_frames.push_back(std::move(record));
    }

    hp_data::Cursor count_cursor(reader, offset);
    if (!count_cursor.read_i32_le(&parsed.secondary_record_count_i32)) return false;
    offset = count_cursor.offset();

    if (!positive_count_fits(
                parsed.secondary_record_count_i32,
                reader.size() - offset,
                kSecondaryRecordBytes)) {
        return false;
    }

    for (std::int32_t i = 0; i < parsed.secondary_record_count_i32; ++i) {
        SecondaryRecord record;
        if (!parse_secondary_record(reader, offset, &record)) return false;
        offset += record.bytes_consumed;
        parsed.secondary_records.push_back(record);
    }

    parsed.bytes_consumed = offset;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::enemy_actions_wbg_prefix
