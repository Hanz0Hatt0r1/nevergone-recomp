#pragma once

#include <cstddef>
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

// Parse the two counted CCPoint lists that immediately follow the first
// layer's object loop. ARMv7 call targets prove the first list is sent to
// addTopBorderPoint() and the second to addBottomBorderPoint(). The returned
// byte count is the exact stream position at the subsequent AddLayer() join.
bool parse_first_layer_record(const hp_data::Reader& reader, FirstLayerRecord* out);

}  // namespace nevergone::game_levels_layer_tail
