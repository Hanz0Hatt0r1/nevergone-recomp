#pragma once

#include <cstddef>
#include <optional>

#include "game_scene_object_event_id.h"
#include "scene_actions_spatial_pair_binding.h"

namespace nevergone::scene_actions_bound_object_builder {

struct GeometryInput {
    float object_position_x = 0.0f;
    std::optional<scene_actions_spatial_trigger::Rect> sprite_bounds;
};

inline scene_actions_spatial_pair_binding::BoundObject build(
        std::size_t layer_index,
        std::size_t object_index,
        const game_levels_scene_prefix::ObjectRecord& record,
        const GeometryInput& geometry) {
    scene_actions_spatial_pair_binding::BoundObject out;
    out.ref = {layer_index, object_index};
    out.view.event_id_108 = game_scene_object_event_id::from_record(record);
    out.view.resource_name = record.string_value;
    out.view.object_position_x = geometry.object_position_x;
    out.view.sprite_bounds = geometry.sprite_bounds;
    return out;
}

}  // namespace nevergone::scene_actions_bound_object_builder
