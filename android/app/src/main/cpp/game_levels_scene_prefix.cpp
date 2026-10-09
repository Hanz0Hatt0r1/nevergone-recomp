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

bool parse_first_layer_header(const hp_data::Reader& reader, FirstLayerHeader* out) {
    if (out == nullptr) return false;
    FirstSceneHeader scene_header;
    if (!parse_first_scene_header(reader, &scene_header) || scene_header.layer_count == 0) {
        return false;
    }

    hp_data::Cursor cursor(reader, scene_header.bytes_consumed);
    FirstLayerHeader parsed;
    parsed.scene_header = std::move(scene_header);
    if (!cursor.read_f32_le(&parsed.first_float)) return false;
    if (!cursor.read_u32_le(&parsed.object_count)) return false;
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
    if (!cursor.read_i32_le(&parsed.first_i32)) return false;
    if (!cursor.read_u32_le(&parsed.second_u32)) return false;
    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

bool parse_first_object_core(const hp_data::Reader& reader, FirstObjectCore* out) {
    if (out == nullptr) return false;

    FirstObjectPrefix prefix;
    if (!parse_first_object_prefix(reader, &prefix)) return false;

    hp_data::Cursor cursor(reader, prefix.bytes_consumed);
    FirstObjectCore parsed;
    parsed.prefix = std::move(prefix);

    // The ARMv7 stream offset is advanced by 5 immediately after the length
    // read: four bytes are the already-consumed uint32 and one additional byte
    // remains opaque. The cursor is already after the uint32, so skip one.
    if (!cursor.skip(1)) return false;

    const std::size_t string_length = static_cast<std::size_t>(parsed.prefix.second_u32);
    constexpr std::size_t kTrailingFixedBytes =
        sizeof(float) * 5u + sizeof(std::int32_t) + 2u;
    if (string_length > cursor.remaining() ||
            kTrailingFixedBytes > cursor.remaining() - string_length) {
        return false;
    }

    if (!cursor.read_fixed_string(string_length, &parsed.string_value)) return false;
    if (!cursor.read_f32_le(&parsed.first_point_x)) return false;
    if (!cursor.read_f32_le(&parsed.first_point_y)) return false;
    if (!cursor.read_f32_le(&parsed.middle_float)) return false;
    if (!cursor.read_f32_le(&parsed.second_point_x)) return false;
    if (!cursor.read_f32_le(&parsed.second_point_y)) return false;
    if (!cursor.read_i32_le(&parsed.trailing_i32)) return false;
    if (!cursor.read_bool8(&parsed.first_bool)) return false;
    if (!cursor.read_bool8(&parsed.second_bool)) return false;

    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

bool parse_first_object_version_extension(
        const hp_data::Reader& reader,
        FirstObjectVersionExtension* out) {
    if (out == nullptr) return false;

    FirstObjectCore core;
    if (!parse_first_object_core(reader, &core)) return false;

    hp_data::Cursor cursor(reader, core.bytes_consumed);
    FirstObjectVersionExtension parsed;
    parsed.core = std::move(core);

    const std::int32_t top_level_gate =
        parsed.core.prefix.layer_header.scene_header.prefix.first_i32;
    if (top_level_gate > 2) {
        std::uint32_t count = 0;
        if (!cursor.read_u32_le(&count)) return false;

        const std::size_t value_count = static_cast<std::size_t>(count);
        if (value_count > cursor.remaining() / sizeof(std::uint32_t)) return false;

        parsed.extra_u32_values.reserve(value_count);
        for (std::size_t i = 0; i < value_count; ++i) {
            std::uint32_t value = 0;
            if (!cursor.read_u32_le(&value)) return false;
            parsed.extra_u32_values.push_back(value);
        }
    }

    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_scene_prefix
