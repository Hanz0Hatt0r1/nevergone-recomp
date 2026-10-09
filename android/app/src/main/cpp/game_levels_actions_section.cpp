#include "game_levels_actions_section.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace nevergone::game_levels_actions_section {
namespace {

constexpr std::size_t kBaseRecordBytes =
        (sizeof(std::uint32_t) + 1u) * 2u +
        sizeof(std::uint32_t) * 2u +
        sizeof(std::int32_t) +
        sizeof(float) * 2u;

std::size_t minimum_record_bytes(std::int32_t format_gate) {
    std::size_t result = kBaseRecordBytes;
    if (format_gate > 1) result += sizeof(std::int32_t) * 2u;
    if (format_gate > 3) result += sizeof(std::int32_t) * 2u;
    return result;
}

bool read_length_prefixed_string(
        hp_data::Cursor* cursor,
        std::uint32_t* raw_length,
        std::string* value) {
    if (cursor == nullptr || raw_length == nullptr || value == nullptr) return false;
    if (!cursor->read_u32_le(raw_length) || !cursor->skip(1u)) return false;
    const std::size_t length = static_cast<std::size_t>(*raw_length);
    if (length > cursor->remaining()) return false;
    return cursor->read_fixed_string(length, value);
}

}  // namespace

bool parse_action_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t format_gate,
        ActionRecord* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    ActionRecord parsed;
    parsed.start_offset = start_offset;

    if (!read_length_prefixed_string(
                &cursor, &parsed.first_string_length, &parsed.first_string) ||
            !read_length_prefixed_string(
                &cursor, &parsed.second_string_length, &parsed.second_string) ||
            !cursor.read_u32_le(&parsed.first_u32) ||
            !cursor.read_u32_le(&parsed.second_u32) ||
            !cursor.read_i32_le(&parsed.first_i32) ||
            !cursor.read_f32_le(&parsed.point_x) ||
            !cursor.read_f32_le(&parsed.point_y)) {
        return false;
    }

    if (format_gate > 1) {
        parsed.has_gate_gt1_fields = true;
        if (!cursor.read_i32_le(&parsed.gate_gt1_first_i32) ||
                !cursor.read_i32_le(&parsed.gate_gt1_second_i32)) {
            return false;
        }
    }

    if (format_gate > 3) {
        parsed.has_gate_gt3_fields = true;
        if (!cursor.read_i32_le(&parsed.gate_gt3_first_i32) ||
                !cursor.read_i32_le(&parsed.gate_gt3_second_i32)) {
            return false;
        }
    }

    parsed.end_offset = cursor.offset();
    if (parsed.end_offset <= start_offset) return false;
    *out = std::move(parsed);
    return true;
}

bool parse_section(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t format_gate,
        Section* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    Section parsed;
    parsed.start_offset = start_offset;
    if (!cursor.read_u32_le(&parsed.action_count)) return false;

    const std::size_t count = static_cast<std::size_t>(parsed.action_count);
    const std::size_t minimum = minimum_record_bytes(format_gate);
    if (count > cursor.remaining() / minimum) return false;

    parsed.actions.reserve(count);
    std::size_t offset = cursor.offset();
    for (std::size_t i = 0; i < count; ++i) {
        ActionRecord action;
        if (!parse_action_record_at(reader, offset, format_gate, &action) ||
                action.end_offset <= offset) {
            return false;
        }
        offset = action.end_offset;
        parsed.actions.push_back(std::move(action));
    }

    parsed.end_offset = offset;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_actions_section
