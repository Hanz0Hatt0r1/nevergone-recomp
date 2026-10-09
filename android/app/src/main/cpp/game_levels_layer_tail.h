#pragma once

#include <cstddef>
#include <cstdint>
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

// Generic complete layer record. start_offset/end_offset are absolute reader
// positions. The recovered first_float remains intentionally structural.
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

// Parse the two counted CCPoint lists that immediately follow the first
// layer's object loop. ARMv7 call targets prove the first list is sent to
// addTopBorderPoint() and the second to addBottomBorderPoint(). The returned
// byte count is the exact stream position at the subsequent AddLayer() join.
bool parse_first_layer_record(const hp_data::Reader& reader, FirstLayerRecord* out);

// Parse one complete layer from an explicit stream offset using the recovered
// top-level format gate for every nested object record. Success ends exactly at
// the original AddLayer() join for that layer.
bool parse_layer_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t top_level_gate,
        LayerRecord* out);

// Parse exactly the first scene's recovered layer_count layers by chaining each
// complete layer end offset. A zero layer_count is a valid empty layer loop.
bool parse_first_scene_layers(const hp_data::Reader& reader, FirstSceneLayerSequence* out);

}  // namespace nevergone::game_levels_layer_tail
