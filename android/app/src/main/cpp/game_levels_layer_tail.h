#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "game_levels_scene_prefix.h"
#include "hp_data_reader.h"

namespace nevergone::game_levels_layer_tail {

struct BorderPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct FirstLayerRecord {
    game_levels_scene_prefix::FirstLayerObjectSequence object_sequence;
    std::vector<BorderPoint> top_border_points;
    std::vector<BorderPoint> bottom_border_points;
    std::size_t bytes_consumed = 0;
};

struct LayerRecord {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    float first_float = 0.0f;
    std::uint32_t object_count = 0;
    std::vector<game_levels_scene_prefix::ObjectRecord> objects;
    std::vector<BorderPoint> top_border_points;
    std::vector<BorderPoint> bottom_border_points;
};

struct FirstSceneLayerSequence {
    game_levels_scene_prefix::FirstSceneHeader scene_header;
    std::vector<LayerRecord> layers;
    std::size_t bytes_consumed = 0;
};

// Generic complete scene record beginning at its string-length field. The
// original scene-loop stream offset advances directly from the final AddLayer
// join to the next scene header, so end_offset is the exact next scene start.
struct SceneRecord {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    std::uint32_t string_length = 0;
    std::string string_value;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    std::uint32_t layer_count = 0;
    std::vector<LayerRecord> layers;
};

struct SceneSection {
    game_levels_scene_prefix::Prefix prefix;
    std::vector<SceneRecord> scenes;
    std::size_t bytes_consumed = 0;
};

bool parse_first_layer_record(const hp_data::Reader& reader, FirstLayerRecord* out);

bool parse_layer_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t top_level_gate,
        LayerRecord* out);

bool parse_first_scene_layers(const hp_data::Reader& reader, FirstSceneLayerSequence* out);

// Parse one complete variable-width scene record at an explicit stream offset.
// The top-level signed gate is forwarded unchanged to every nested object.
bool parse_scene_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t top_level_gate,
        SceneRecord* out);

// Parse the entire LoadGL_Scene scene loop after the two-field top-level prefix.
// scene_count == 0 is valid and completes at byte 8. Output is transactional.
bool parse_scene_section(const hp_data::Reader& reader, SceneSection* out);

}  // namespace nevergone::game_levels_layer_tail
