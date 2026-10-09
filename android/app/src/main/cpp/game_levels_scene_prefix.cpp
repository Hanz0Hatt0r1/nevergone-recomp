#include "game_levels_scene_prefix.h"

#include <utility>

namespace nevergone::game_levels_scene_prefix {

bool parse(const hp_data::Reader& reader, Prefix* out) {
    if (out == nullptr) return false;

    hp_data::Cursor cursor(reader);
    Prefix parsed;
    if (!cursor.read_i32_le(&parsed.first_i32) ||
        !cursor.read_u32_le(&parsed.scene_count)) {
        return false;
    }
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

    if (!cursor.read_u32_le(&parsed.first_string_length) || !cursor.skip(1)) {
        return false;
    }

    const std::size_t string_length = parsed.first_string_length;
    constexpr std::size_t kTrailingFixedBytes = sizeof(float) * 2u + sizeof(std::uint32_t);
    if (string_length > cursor.remaining() ||
        kTrailingFixedBytes > cursor.remaining() - string_length) {
        return false;
    }

    if (!cursor.read_fixed_string(string_length, &parsed.first_string) ||
        !cursor.read_f32_le(&parsed.first_point_x) ||
        !cursor.read_f32_le(&parsed.first_point_y) ||
        !cursor.read_u32_le(&parsed.layer_count)) {
        return false;
    }

    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

bool parse_first_layer_header(const hp_data::Reader& reader, FirstLayerHeader* out) {
    if (out == nullptr) return false;

    FirstSceneHeader scene_header;
    if (!parse_first_scene_header(reader, &scene_header) || scene_header.layer_count == 0) {
        return false;
    }

    hp_data::Cursor cursor(reader, scene_header.bytes_consumed);
    FirstLayerHeader parsed;
    parsed.scene_header = std::move(scene_header);
    if (!cursor.read_f32_le(&parsed.first_float) ||
        !cursor.read_u32_le(&parsed.object_count)) {
        return false;
    }

    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

bool parse_first_object_prefix(const hp_data::Reader& reader, FirstObjectPrefix* out) {
    if (out == nullptr) return false;

    FirstLayerHeader layer_header;
    if (!parse_first_layer_header(reader, &layer_header) || layer_header.object_count == 0) {
        return false;
    }

    hp_data::Cursor cursor(reader, layer_header.bytes_consumed);
    FirstObjectPrefix parsed;
    parsed.layer_header = std::move(layer_header);
    if (!cursor.read_i32_le(&parsed.first_i32) ||
        !cursor.read_u32_le(&parsed.string_length)) {
        return false;
    }

    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

bool parse_first_object_header(const hp_data::Reader& reader, FirstObjectHeader* out) {
    if (out == nullptr) return false;

    FirstObjectPrefix prefix;
    if (!parse_first_object_prefix(reader, &prefix)) return false;

    hp_data::Cursor cursor(reader, prefix.bytes_consumed);
    FirstObjectHeader parsed;
    parsed.prefix = std::move(prefix);

    // ARMv7 LoadGL_Scene adds five to the stream offset from the start of the
    // uint32 length field. parse_first_object_prefix() already consumed the
    // four length bytes, so exactly one additional byte remains to skip here.
    if (!cursor.skip(1)) return false;

    const std::size_t string_length = parsed.prefix.string_length;
    constexpr std::size_t kTrailingFixedBytes =
            sizeof(float) * 5u + sizeof(std::int32_t) + 2u * sizeof(std::uint8_t);
    if (string_length > cursor.remaining() ||
        kTrailingFixedBytes > cursor.remaining() - string_length) {
        return false;
    }

    if (!cursor.read_fixed_string(string_length, &parsed.first_string) ||
        !cursor.read_f32_le(&parsed.first_point_x) ||
        !cursor.read_f32_le(&parsed.first_point_y) ||
        !cursor.read_f32_le(&parsed.first_float) ||
        !cursor.read_f32_le(&parsed.second_point_x) ||
        !cursor.read_f32_le(&parsed.second_point_y) ||
        !cursor.read_i32_le(&parsed.second_i32) ||
        !cursor.read_bool8(&parsed.first_bool) ||
        !cursor.read_bool8(&parsed.second_bool)) {
        return false;
    }

    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_scene_prefix
