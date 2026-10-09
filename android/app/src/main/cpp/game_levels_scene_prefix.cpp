#include "game_levels_scene_prefix.h"

#include <utility>

namespace nevergone::game_levels_scene_prefix {
namespace {
constexpr std::size_t kMinimumObjectRecordBytes =
        sizeof(std::int32_t) + sizeof(std::uint32_t) + 1u +
        sizeof(float) * 5u + sizeof(std::int32_t) + 2u;
}

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
            kTrailingFixedBytes > cursor.remaining() - string_length) return false;
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
    if (!parse_first_scene_header(reader, &scene_header) || scene_header.layer_count == 0) return false;
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
    if (!parse_first_layer_header(reader, &layer_header) || layer_header.object_count == 0) return false;
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
    if (!cursor.skip(1)) return false;
    const std::size_t string_length = static_cast<std::size_t>(parsed.prefix.second_u32);
    constexpr std::size_t kTrailingFixedBytes = sizeof(float) * 5u + sizeof(std::int32_t) + 2u;
    if (string_length > cursor.remaining() ||
            kTrailingFixedBytes > cursor.remaining() - string_length) return false;
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

bool parse_first_object_conditional_header(
        const hp_data::Reader& reader,
        FirstObjectConditionalHeader* out) {
    if (out == nullptr) return false;
    FirstObjectVersionExtension extension;
    if (!parse_first_object_version_extension(reader, &extension)) return false;
    hp_data::Cursor cursor(reader, extension.bytes_consumed);
    FirstObjectConditionalHeader parsed;
    parsed.extension = std::move(extension);
    if (parsed.extension.core.prefix.first_i32 == 0) {
        parsed.bytes_consumed = cursor.offset();
        *out = std::move(parsed);
        return true;
    }
    parsed.present = true;
    const std::int32_t top_level_gate =
        parsed.extension.core.prefix.layer_header.scene_header.prefix.first_i32;
    if (!cursor.read_u32_le(&parsed.first_u32)) return false;
    if (top_level_gate > 1 && !cursor.read_u32_le(&parsed.second_u32)) return false;
    if (!cursor.read_u32_le(&parsed.string_length) || !cursor.skip(1)) return false;
    const std::size_t string_length = static_cast<std::size_t>(parsed.string_length);
    if (string_length > cursor.remaining() ||
            sizeof(std::int32_t) > cursor.remaining() - string_length) return false;
    if (!cursor.read_fixed_string(string_length, &parsed.string_value)) return false;
    if (!cursor.read_i32_le(&parsed.primary_i32)) return false;
    if (parsed.primary_i32 == 1 && !cursor.read_i32_le(&parsed.secondary_i32)) return false;
    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

bool parse_first_object_record(const hp_data::Reader& reader, FirstObjectRecord* out) {
    if (out == nullptr) return false;
    FirstObjectConditionalHeader header;
    if (!parse_first_object_conditional_header(reader, &header)) return false;
    hp_data::Cursor cursor(reader, header.bytes_consumed);
    FirstObjectRecord parsed;
    parsed.header = std::move(header);
    const std::int32_t object_type = parsed.header.extension.core.prefix.first_i32;
    if (object_type == 0) {
        parsed.bytes_consumed = cursor.offset();
        *out = std::move(parsed);
        return true;
    }
    const std::uint32_t type_bits = static_cast<std::uint32_t>(object_type);
    const bool one_int_type = ((type_bits & ~0x2u) == 0x4u) || object_type == 9;
    if (one_int_type) {
        parsed.has_tail_i32 = true;
        if (!cursor.read_i32_le(&parsed.tail_i32)) return false;
    } else if (object_type == 10) {
        parsed.has_tail_i32 = true;
        parsed.has_tail_points = true;
        if (!cursor.read_i32_le(&parsed.tail_i32)) return false;
        if (!cursor.read_f32_le(&parsed.first_tail_point_x)) return false;
        if (!cursor.read_f32_le(&parsed.first_tail_point_y)) return false;
        if (!cursor.read_f32_le(&parsed.second_tail_point_x)) return false;
        if (!cursor.read_f32_le(&parsed.second_tail_point_y)) return false;
    }
    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

bool parse_object_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t top_level_gate,
        ObjectRecord* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    ObjectRecord parsed;
    parsed.start_offset = start_offset;

    if (!cursor.read_i32_le(&parsed.first_i32) ||
            !cursor.read_u32_le(&parsed.string_length) ||
            !cursor.skip(1)) return false;

    const std::size_t string_length = static_cast<std::size_t>(parsed.string_length);
    constexpr std::size_t kCoreTrailingBytes = sizeof(float) * 5u + sizeof(std::int32_t) + 2u;
    if (string_length > cursor.remaining() ||
            kCoreTrailingBytes > cursor.remaining() - string_length) return false;
    if (!cursor.read_fixed_string(string_length, &parsed.string_value) ||
            !cursor.read_f32_le(&parsed.first_point_x) ||
            !cursor.read_f32_le(&parsed.first_point_y) ||
            !cursor.read_f32_le(&parsed.middle_float) ||
            !cursor.read_f32_le(&parsed.second_point_x) ||
            !cursor.read_f32_le(&parsed.second_point_y) ||
            !cursor.read_i32_le(&parsed.trailing_i32) ||
            !cursor.read_bool8(&parsed.first_bool) ||
            !cursor.read_bool8(&parsed.second_bool)) return false;

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

    if (parsed.first_i32 != 0) {
        parsed.conditional_present = true;
        if (!cursor.read_u32_le(&parsed.conditional_first_u32)) return false;
        if (top_level_gate > 1 && !cursor.read_u32_le(&parsed.conditional_second_u32)) return false;
        if (!cursor.read_u32_le(&parsed.conditional_string_length) || !cursor.skip(1)) return false;
        const std::size_t conditional_length =
                static_cast<std::size_t>(parsed.conditional_string_length);
        if (conditional_length > cursor.remaining() ||
                sizeof(std::int32_t) > cursor.remaining() - conditional_length) return false;
        if (!cursor.read_fixed_string(conditional_length, &parsed.conditional_string_value) ||
                !cursor.read_i32_le(&parsed.primary_i32)) return false;
        if (parsed.primary_i32 == 1 && !cursor.read_i32_le(&parsed.secondary_i32)) return false;

        const std::uint32_t type_bits = static_cast<std::uint32_t>(parsed.first_i32);
        const bool one_int_type = ((type_bits & ~0x2u) == 0x4u) || parsed.first_i32 == 9;
        if (one_int_type) {
            parsed.has_tail_i32 = true;
            if (!cursor.read_i32_le(&parsed.tail_i32)) return false;
        } else if (parsed.first_i32 == 10) {
            parsed.has_tail_i32 = true;
            parsed.has_tail_points = true;
            if (!cursor.read_i32_le(&parsed.tail_i32) ||
                    !cursor.read_f32_le(&parsed.first_tail_point_x) ||
                    !cursor.read_f32_le(&parsed.first_tail_point_y) ||
                    !cursor.read_f32_le(&parsed.second_tail_point_x) ||
                    !cursor.read_f32_le(&parsed.second_tail_point_y)) return false;
        }
    }

    parsed.end_offset = cursor.offset();
    *out = std::move(parsed);
    return true;
}

bool parse_first_layer_objects(
        const hp_data::Reader& reader,
        FirstLayerObjectSequence* out) {
    if (out == nullptr) return false;

    FirstLayerHeader layer_header;
    if (!parse_first_layer_header(reader, &layer_header)) return false;

    const std::size_t object_count = static_cast<std::size_t>(layer_header.object_count);
    hp_data::Cursor cursor(reader, layer_header.bytes_consumed);
    if (object_count > cursor.remaining() / kMinimumObjectRecordBytes) return false;

    FirstLayerObjectSequence parsed;
    parsed.layer_header = std::move(layer_header);
    parsed.objects.reserve(object_count);
    std::size_t offset = parsed.layer_header.bytes_consumed;
    const std::int32_t top_level_gate = parsed.layer_header.scene_header.prefix.first_i32;
    for (std::size_t i = 0; i < object_count; ++i) {
        ObjectRecord record;
        if (!parse_object_record_at(reader, offset, top_level_gate, &record) ||
                record.end_offset <= offset) return false;
        offset = record.end_offset;
        parsed.objects.push_back(std::move(record));
    }

    parsed.bytes_consumed = offset;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_scene_prefix
