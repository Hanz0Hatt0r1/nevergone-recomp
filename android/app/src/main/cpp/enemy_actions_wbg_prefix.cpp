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

bool parse_compact_action_frame_block(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        CompactActionFrameBlock* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    CompactActionFrameBlock parsed;
    if (!cursor.read_i32_le(&parsed.count_i32)) return false;

    if (parsed.count_i32 > 0) {
        const auto count = static_cast<std::size_t>(parsed.count_i32);
        if (count > cursor.remaining() / kCompactActionFrameBytes) return false;
        parsed.records.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            CompactActionFrameRecord record;
            if (!cursor.read_i32_le(&record.first_i32) ||
                !cursor.read_f32_le(&record.first_float) ||
                !cursor.read_f32_le(&record.second_float)) {
                return false;
            }
            parsed.records.push_back(record);
        }
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    const std::size_t expected_bytes = kCompactBlockHeaderBytes +
            parsed.records.size() * kCompactActionFrameBytes;
    if (parsed.bytes_consumed != expected_bytes) return false;

    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::enemy_actions_wbg_prefix
