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

bool checked_three_payload_bytes(
        std::int32_t first,
        std::int32_t second,
        std::int32_t third,
        std::size_t fixed_bytes,
        std::size_t* out_total) {
    if (out_total == nullptr || first < 0 || second < 0 || third < 0) return false;
    const auto first_size = static_cast<std::size_t>(first);
    const auto second_size = static_cast<std::size_t>(second);
    const auto third_size = static_cast<std::size_t>(third);
    if (first_size > std::numeric_limits<std::size_t>::max() - second_size) return false;
    const std::size_t first_two = first_size + second_size;
    if (first_two > std::numeric_limits<std::size_t>::max() - third_size) return false;
    const std::size_t payload_bytes = first_two + third_size;
    if (payload_bytes > std::numeric_limits<std::size_t>::max() - fixed_bytes) return false;
    *out_total = fixed_bytes + payload_bytes;
    return true;
}

bool read_nested_record(hp_data::Cursor* cursor, NestedActionFrameRecord* out) {
    if (cursor == nullptr || out == nullptr) return false;
    const std::size_t start_offset = cursor->offset();

    NestedActionFrameRecord parsed;
    if (!cursor->read_i32_le(&parsed.first_i32)) return false;
    for (float& value : parsed.float_values) {
        if (!cursor->read_f32_le(&value)) return false;
    }
    if (!cursor->read_bool8(&parsed.first_bool) ||
        !cursor->read_i32_le(&parsed.second_i32) ||
        !read_string_field(cursor, &parsed.first_string_length_i32, &parsed.first_string) ||
        !read_string_field(cursor, &parsed.second_string_length_i32, &parsed.second_string) ||
        !read_string_field(cursor, &parsed.third_string_length_i32, &parsed.third_string)) {
        return false;
    }

    parsed.bytes_consumed = cursor->offset() - start_offset;
    std::size_t expected_bytes = 0;
    if (!checked_three_payload_bytes(
                parsed.first_string_length_i32,
                parsed.second_string_length_i32,
                parsed.third_string_length_i32,
                kNestedActionFrameFixedBytes,
                &expected_bytes) ||
        parsed.bytes_consumed != expected_bytes) {
        return false;
    }

    *out = std::move(parsed);
    return true;
}

bool read_versioned_record(hp_data::Cursor* cursor, VersionedActionFrameRecord* out) {
    if (cursor == nullptr || out == nullptr) return false;
    const std::size_t start_offset = cursor->offset();

    VersionedActionFrameRecord parsed;
    if (!cursor->read_i32_le(&parsed.first_i32)) return false;
    for (float& value : parsed.float_values) {
        if (!cursor->read_f32_le(&value)) return false;
    }
    if (!cursor->read_bool8(&parsed.first_bool) ||
        !cursor->read_i32_le(&parsed.second_i32) ||
        !cursor->read_u32_le(&parsed.first_u32) ||
        !cursor->read_u32_le(&parsed.second_u32) ||
        !read_string_field(cursor, &parsed.first_string_length_i32, &parsed.first_string) ||
        !read_string_field(cursor, &parsed.second_string_length_i32, &parsed.second_string) ||
        !read_string_field(cursor, &parsed.third_string_length_i32, &parsed.third_string)) {
        return false;
    }

    parsed.bytes_consumed = cursor->offset() - start_offset;
    std::size_t expected_bytes = 0;
    if (!checked_three_payload_bytes(
                parsed.first_string_length_i32,
                parsed.second_string_length_i32,
                parsed.third_string_length_i32,
                kVersionedActionFrameFixedBytes,
                &expected_bytes) ||
        parsed.bytes_consumed != expected_bytes) {
        return false;
    }

    *out = std::move(parsed);
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
    std::size_t expected_bytes = 0;
    if (!checked_three_payload_bytes(
                parsed.first_string_length_i32,
                parsed.second_string_length_i32,
                parsed.third_string_length_i32,
                kActionFrameFixedBytes,
                &expected_bytes) ||
        parsed.bytes_consumed != expected_bytes) {
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

bool parse_nested_action_frame_block(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        NestedActionFrameBlock* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    NestedActionFrameBlock parsed;
    if (!cursor.read_i32_le(&parsed.group_count_i32)) return false;

    if (parsed.group_count_i32 > 0) {
        const auto group_count = static_cast<std::size_t>(parsed.group_count_i32);
        if (group_count > cursor.remaining() / kNestedGroupHeaderBytes) return false;
        parsed.groups.reserve(group_count);

        for (std::size_t group_index = 0; group_index < group_count; ++group_index) {
            const std::size_t group_start = cursor.offset();
            NestedActionFrameGroup group;
            if (!cursor.read_u32_le(&group.record_count)) return false;

            const auto record_count = static_cast<std::size_t>(group.record_count);
            if (record_count > cursor.remaining() / kNestedActionFrameFixedBytes) return false;
            group.records.reserve(record_count);
            for (std::size_t record_index = 0; record_index < record_count; ++record_index) {
                NestedActionFrameRecord record;
                if (!read_nested_record(&cursor, &record)) return false;
                group.records.push_back(std::move(record));
            }

            group.bytes_consumed = cursor.offset() - group_start;
            parsed.groups.push_back(std::move(group));
        }
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    *out = std::move(parsed);
    return true;
}

bool parse_versioned_action_frame_groups(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t header_word0,
        VersionedActionFrameBlock* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    VersionedActionFrameBlock parsed;
    parsed.header_word0 = header_word0;
    parsed.expected_group_count = versioned_group_count_for_header(header_word0);

    if (parsed.expected_group_count > cursor.remaining() / kVersionedGroupHeaderBytes) {
        return false;
    }
    parsed.groups.reserve(parsed.expected_group_count);

    for (std::size_t group_index = 0;
         group_index < parsed.expected_group_count;
         ++group_index) {
        const std::size_t group_start = cursor.offset();
        VersionedActionFrameGroup group;
        if (!cursor.read_i32_le(&group.record_count_i32)) return false;

        if (group.record_count_i32 > 0) {
            const std::size_t record_count = static_cast<std::size_t>(group.record_count_i32);
            const std::size_t groups_after = parsed.expected_group_count - group_index - 1u;
            const std::size_t reserved_header_bytes =
                    groups_after * kVersionedGroupHeaderBytes;
            if (cursor.remaining() < reserved_header_bytes) return false;
            const std::size_t record_budget = cursor.remaining() - reserved_header_bytes;
            if (record_count > record_budget / kVersionedActionFrameFixedBytes) return false;

            group.records.reserve(record_count);
            for (std::size_t record_index = 0; record_index < record_count; ++record_index) {
                VersionedActionFrameRecord record;
                if (!read_versioned_record(&cursor, &record)) return false;
                group.records.push_back(std::move(record));
            }
        }

        group.bytes_consumed = cursor.offset() - group_start;
        parsed.groups.push_back(std::move(group));
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    *out = std::move(parsed);
    return true;
}

bool parse_fixed_tail_action_frame_block(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        FixedTailActionFrameBlock* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    FixedTailActionFrameBlock parsed;
    if (!cursor.read_i32_le(&parsed.count_i32)) return false;

    if (parsed.count_i32 > 0) {
        const std::size_t count = static_cast<std::size_t>(parsed.count_i32);
        if (count > cursor.remaining() / kFixedTailActionFrameBytes) return false;
        parsed.records.reserve(count);

        for (std::size_t i = 0; i < count; ++i) {
            FixedTailActionFrameRecord record;
            if (!cursor.read_i32_le(&record.first_i32)) return false;
            for (float& value : record.float_values) {
                if (!cursor.read_f32_le(&value)) return false;
            }
            if (!cursor.read_bool8(&record.first_bool)) return false;
            parsed.records.push_back(record);
        }
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    const std::size_t expected_bytes = kFixedTailBlockHeaderBytes +
            parsed.records.size() * kFixedTailActionFrameBytes;
    if (parsed.bytes_consumed != expected_bytes) return false;

    *out = std::move(parsed);
    return true;
}

bool parse_primary_indexed_tuple_block(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::uint32_t action_frame_count,
        PrimaryIndexedTupleBlock* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    PrimaryIndexedTupleBlock parsed;
    parsed.expected_record_count = action_frame_count;

    const std::size_t count = static_cast<std::size_t>(action_frame_count);
    if (count > cursor.remaining() / kPrimaryIndexedTupleBytes) return false;
    parsed.records.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        PrimaryIndexedTupleRecord record;
        for (std::int32_t& value : record.i32_values) {
            if (!cursor.read_i32_le(&value)) return false;
        }
        for (float& value : record.float_values) {
            if (!cursor.read_f32_le(&value)) return false;
        }
        parsed.records.push_back(record);
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    if (parsed.bytes_consumed != count * kPrimaryIndexedTupleBytes) return false;

    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::enemy_actions_wbg_prefix
