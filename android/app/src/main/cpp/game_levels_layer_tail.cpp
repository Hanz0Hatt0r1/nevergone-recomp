#include "game_levels_layer_tail.h"

#include <cstdint>
#include <utility>

namespace nevergone::game_levels_layer_tail {
namespace {

constexpr std::size_t kMinimumObjectRecordBytes =
        sizeof(std::int32_t) + sizeof(std::uint32_t) + 1u +
        sizeof(float) * 5u + sizeof(std::int32_t) + 2u;
constexpr std::size_t kMinimumLayerRecordBytes =
        sizeof(float) + sizeof(std::uint32_t) +
        sizeof(std::uint32_t) + sizeof(std::uint32_t);
constexpr std::size_t kMinimumSceneRecordBytes =
        sizeof(std::uint32_t) + 1u + sizeof(float) * 2u + sizeof(std::uint32_t);

bool read_point_list(
        hp_data::Cursor* cursor,
        std::vector<BorderPoint>* points,
        bool require_trailing_count) {
    if (cursor == nullptr || points == nullptr) return false;

    std::uint32_t raw_count = 0;
    if (!cursor->read_u32_le(&raw_count)) return false;

    const std::size_t count = static_cast<std::size_t>(raw_count);
    const std::size_t remaining = cursor->remaining();
    const std::size_t reserved_tail = require_trailing_count ? sizeof(std::uint32_t) : 0u;
    if (remaining < reserved_tail) return false;
    if (count > (remaining - reserved_tail) / (sizeof(float) * 2u)) return false;

    points->reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        BorderPoint point;
        if (!cursor->read_f32_le(&point.x) || !cursor->read_f32_le(&point.y)) return false;
        points->push_back(point);
    }
    return true;
}

bool read_border_lists(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::vector<BorderPoint>* top,
        std::vector<BorderPoint>* bottom,
        std::size_t* end_offset) {
    if (top == nullptr || bottom == nullptr || end_offset == nullptr || start_offset > reader.size()) {
        return false;
    }
    hp_data::Cursor cursor(reader, start_offset);
    if (!read_point_list(&cursor, top, true)) return false;
    if (!read_point_list(&cursor, bottom, false)) return false;
    *end_offset = cursor.offset();
    return true;
}

}  // namespace

bool parse_first_layer_record(const hp_data::Reader& reader, FirstLayerRecord* out) {
    if (out == nullptr) return false;

    game_levels_scene_prefix::FirstLayerObjectSequence object_sequence;
    if (!game_levels_scene_prefix::parse_first_layer_objects(reader, &object_sequence)) {
        return false;
    }

    FirstLayerRecord parsed;
    parsed.object_sequence = std::move(object_sequence);
    if (!read_border_lists(
            reader,
            parsed.object_sequence.bytes_consumed,
            &parsed.top_border_points,
            &parsed.bottom_border_points,
            &parsed.bytes_consumed)) {
        return false;
    }

    *out = std::move(parsed);
    return true;
}

bool parse_layer_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t top_level_gate,
        LayerRecord* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor header_cursor(reader, start_offset);
    LayerRecord parsed;
    parsed.start_offset = start_offset;
    if (!header_cursor.read_f32_le(&parsed.first_float) ||
            !header_cursor.read_u32_le(&parsed.object_count)) {
        return false;
    }

    const std::size_t object_count = static_cast<std::size_t>(parsed.object_count);
    const std::size_t remaining_after_header = header_cursor.remaining();
    constexpr std::size_t kBorderCountBytes = sizeof(std::uint32_t) * 2u;
    if (remaining_after_header < kBorderCountBytes) return false;
    if (object_count >
            (remaining_after_header - kBorderCountBytes) / kMinimumObjectRecordBytes) {
        return false;
    }

    parsed.objects.reserve(object_count);
    std::size_t offset = header_cursor.offset();
    for (std::size_t i = 0; i < object_count; ++i) {
        game_levels_scene_prefix::ObjectRecord object;
        if (!game_levels_scene_prefix::parse_object_record_at(
                reader, offset, top_level_gate, &object) || object.end_offset <= offset) {
            return false;
        }
        offset = object.end_offset;
        parsed.objects.push_back(std::move(object));
    }

    if (!read_border_lists(
            reader,
            offset,
            &parsed.top_border_points,
            &parsed.bottom_border_points,
            &parsed.end_offset)) {
        return false;
    }
    if (parsed.end_offset <= start_offset) return false;

    *out = std::move(parsed);
    return true;
}

bool parse_first_scene_layers(const hp_data::Reader& reader, FirstSceneLayerSequence* out) {
    if (out == nullptr) return false;

    game_levels_scene_prefix::FirstSceneHeader scene_header;
    if (!game_levels_scene_prefix::parse_first_scene_header(reader, &scene_header)) return false;

    const std::size_t layer_count = static_cast<std::size_t>(scene_header.layer_count);
    hp_data::Cursor cursor(reader, scene_header.bytes_consumed);
    if (layer_count > cursor.remaining() / kMinimumLayerRecordBytes) return false;

    FirstSceneLayerSequence parsed;
    parsed.scene_header = std::move(scene_header);
    parsed.layers.reserve(layer_count);
    std::size_t offset = parsed.scene_header.bytes_consumed;
    const std::int32_t top_level_gate = parsed.scene_header.prefix.first_i32;
    for (std::size_t i = 0; i < layer_count; ++i) {
        LayerRecord layer;
        if (!parse_layer_record_at(reader, offset, top_level_gate, &layer) ||
                layer.end_offset <= offset) {
            return false;
        }
        offset = layer.end_offset;
        parsed.layers.push_back(std::move(layer));
    }

    parsed.bytes_consumed = offset;
    *out = std::move(parsed);
    return true;
}

bool parse_scene_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t top_level_gate,
        SceneRecord* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    SceneRecord parsed;
    parsed.start_offset = start_offset;
    if (!cursor.read_u32_le(&parsed.string_length) || !cursor.skip(1)) return false;

    const std::size_t string_length = static_cast<std::size_t>(parsed.string_length);
    constexpr std::size_t kTrailingHeaderBytes = sizeof(float) * 2u + sizeof(std::uint32_t);
    if (string_length > cursor.remaining() ||
            kTrailingHeaderBytes > cursor.remaining() - string_length) {
        return false;
    }
    if (!cursor.read_fixed_string(string_length, &parsed.string_value) ||
            !cursor.read_f32_le(&parsed.first_point_x) ||
            !cursor.read_f32_le(&parsed.first_point_y) ||
            !cursor.read_u32_le(&parsed.layer_count)) {
        return false;
    }

    const std::size_t layer_count = static_cast<std::size_t>(parsed.layer_count);
    if (layer_count > cursor.remaining() / kMinimumLayerRecordBytes) return false;

    parsed.layers.reserve(layer_count);
    std::size_t offset = cursor.offset();
    for (std::size_t i = 0; i < layer_count; ++i) {
        LayerRecord layer;
        if (!parse_layer_record_at(reader, offset, top_level_gate, &layer) ||
                layer.end_offset <= offset) {
            return false;
        }
        offset = layer.end_offset;
        parsed.layers.push_back(std::move(layer));
    }

    parsed.end_offset = offset;
    if (parsed.end_offset <= start_offset) return false;
    *out = std::move(parsed);
    return true;
}

bool parse_scene_section(const hp_data::Reader& reader, SceneSection* out) {
    if (out == nullptr) return false;

    game_levels_scene_prefix::Prefix prefix;
    if (!game_levels_scene_prefix::parse(reader, &prefix)) return false;

    hp_data::Cursor cursor(reader, prefix.bytes_consumed);
    const std::size_t scene_count = static_cast<std::size_t>(prefix.scene_count);
    if (scene_count > cursor.remaining() / kMinimumSceneRecordBytes) return false;

    SceneSection parsed;
    parsed.prefix = prefix;
    parsed.scenes.reserve(scene_count);
    std::size_t offset = prefix.bytes_consumed;
    for (std::size_t i = 0; i < scene_count; ++i) {
        SceneRecord scene;
        if (!parse_scene_record_at(reader, offset, prefix.first_i32, &scene) ||
                scene.end_offset <= offset) {
            return false;
        }
        offset = scene.end_offset;
        parsed.scenes.push_back(std::move(scene));
    }

    parsed.bytes_consumed = offset;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_layer_tail
