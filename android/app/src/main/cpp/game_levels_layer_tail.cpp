#include "game_levels_layer_tail.h"

#include <cstdint>
#include <utility>

namespace nevergone::game_levels_layer_tail {
namespace {

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

}  // namespace

bool parse_first_layer_record(const hp_data::Reader& reader, FirstLayerRecord* out) {
    if (out == nullptr) return false;

    game_levels_scene_prefix::FirstLayerObjectSequence object_sequence;
    if (!game_levels_scene_prefix::parse_first_layer_objects(reader, &object_sequence)) {
        return false;
    }

    hp_data::Cursor cursor(reader, object_sequence.bytes_consumed);
    FirstLayerRecord parsed;
    parsed.object_sequence = std::move(object_sequence);

    if (!read_point_list(&cursor, &parsed.top_border_points, true)) return false;
    if (!read_point_list(&cursor, &parsed.bottom_border_points, false)) return false;

    parsed.bytes_consumed = cursor.offset();
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_layer_tail
