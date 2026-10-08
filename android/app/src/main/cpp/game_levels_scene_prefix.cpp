#include "game_levels_scene_prefix.h"

#include <utility>

namespace nevergone::game_levels_scene_prefix {

bool parse(const hp_data::Reader& reader, Prefix* out) {
    if (out == nullptr) return false;

    hp_data::Cursor cursor(reader);
    Prefix parsed;
    if (!cursor.read_i32_le(&parsed.first_i32)) return false;
    if (!cursor.read_u32_le(&parsed.scene_count)) return false;
    parsed.bytes_consumed = cursor.offset();

    *out = parsed;
    return true;
}

bool parse_first_scene_header(const hp_data::Reader& reader, FirstSceneHeader* out) {
    if (out == nullptr) return false;

    Prefix prefix;
    if (!parse(reader, &prefix) || prefix.scene_count == 0) return false;

    hp_data::Cursor cursor(reader, prefix.bytes_consumed);
    FirstSceneHeader parsed;
    parsed.prefix = prefix;

    if (!cursor.read_u32_le(&parsed.first_string_length)) return false;
    if (!cursor.skip(1)) return false;

    const std::size_t string_length = static_cast<std::size_t>(parsed.first_string_length);
    constexpr std::size_t kTrailingFixedBytes = sizeof(float) * 2u + sizeof(std::uint32_t);
    if (string_length > cursor.remaining() ||
            kTrailingFixedBytes > cursor.remaining() - string_length) {
        return false;
    }

    if (!cursor.read_fixed_string(string_length, &parsed.first_string)) return false;
    if (!cursor.read_f32_le(&parsed.first_point_x)) return false;
    if (!cursor.read_f32_le(&parsed.first_point_y)) return false;
    if (!cursor.read_u32_le(&parsed.layer_count)) return false;
    parsed.bytes_consumed = cursor.offset();

    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_scene_prefix
