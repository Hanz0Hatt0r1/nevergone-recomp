#include "game_levels_port_node_section.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace nevergone::game_levels_port_node_section {
namespace {

constexpr std::size_t kMinimumStringBytes = sizeof(std::uint32_t) + 1u;
constexpr std::size_t kMinimumRecordBytes =
        kMinimumStringBytes * 3u + sizeof(std::uint32_t) * 5u + 3u;

bool read_string(
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

bool parse_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        PortNodeRecord* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    PortNodeRecord parsed;
    parsed.start_offset = start_offset;
    if (!read_string(&cursor, &parsed.first_string_length, &parsed.first_string) ||
            !read_string(&cursor, &parsed.second_string_length, &parsed.second_string) ||
            !cursor.read_u32_le(&parsed.first_u32) ||
            !cursor.read_u32_le(&parsed.second_u32) ||
            !cursor.read_bool8(&parsed.first_bool) ||
            !cursor.read_bool8(&parsed.second_bool) ||
            !cursor.read_bool8(&parsed.third_bool) ||
            !cursor.read_u32_le(&parsed.third_u32) ||
            !cursor.read_u32_le(&parsed.fourth_u32) ||
            !cursor.read_u32_le(&parsed.fifth_u32) ||
            !read_string(&cursor, &parsed.third_string_length, &parsed.third_string)) {
        return false;
    }
    parsed.end_offset = cursor.offset();
    *out = std::move(parsed);
    return true;
}

bool parse_section(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        Section* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    Section parsed;
    parsed.start_offset = start_offset;
    if (!cursor.read_u32_le(&parsed.port_node_count)) return false;

    const std::size_t count = static_cast<std::size_t>(parsed.port_node_count);
    if (count > cursor.remaining() / kMinimumRecordBytes) return false;
    parsed.port_nodes.reserve(count);

    std::size_t offset = cursor.offset();
    for (std::size_t i = 0; i < count; ++i) {
        PortNodeRecord record;
        if (!parse_record_at(reader, offset, &record) || record.end_offset <= offset) {
            return false;
        }
        offset = record.end_offset;
        parsed.port_nodes.push_back(std::move(record));
    }

    parsed.end_offset = offset;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_port_node_section
