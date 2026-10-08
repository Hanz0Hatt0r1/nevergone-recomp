#include "game_levels_scene_prefix.h"

#include <limits>

namespace nevergone::game_levels_scene_prefix {

bool parse(const hp_data::Reader& reader, Prefix* out) {
    if (out == nullptr) return false;

    hp_data::Cursor cursor(reader);
    Prefix parsed;
    if (!cursor.read_i32_le(&parsed.first_i32)) return false;
    if (!cursor.read_u32_le(&parsed.second_u32)) return false;
    if (!cursor.read_u32_le(&parsed.third_u32)) return false;
    parsed.bytes_consumed = cursor.offset();

    *out = parsed;
    return true;
}

bool parse_first_record_header(const hp_data::Reader& reader, FirstRecordHeader* out) {
    if (out == nullptr) return false;

    Prefix prefix;
    if (!parse(reader, &prefix)) return false;

    hp_data::Cursor cursor(reader, prefix.bytes_consumed);
    if (!cursor.skip(1)) return false;

    const std::size_t string_length = static_cast<std::size_t>(prefix.third_u32);
    if (string_length > cursor.remaining()) return false;

    FirstRecordHeader parsed;
    parsed.prefix = prefix;
    if (!cursor.read_fixed_string(string_length, &parsed.first_string)) return false;
    if (!cursor.read_f32_le(&parsed.first_point_x)) return false;
    if (!cursor.read_f32_le(&parsed.first_point_y)) return false;
    parsed.bytes_consumed = cursor.offset();

    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_scene_prefix
