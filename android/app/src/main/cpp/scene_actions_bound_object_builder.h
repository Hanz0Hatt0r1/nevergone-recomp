#pragma once

#include <cstddef>
#include <optional>

#include "game_scene_layer_object_parent_x.h"
#include "game_scene_object_event_id.h"
#include "game_scene_render_queue.h"
#include "scene_actions_klhuo_fallback_bounds.h"
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

// Fully reconstruct the current klhuo GameSceneObject spatial view from the
// retained render-queue command plus the current GameSceneClassLayer::move()
// X input. The fallback sprite bounds come from the recovered invisible 20x20
// empty.png path, while the owning GameSceneObject parent X comes from the
// recovered layer ID/camera-factor movement formula.
inline std::optional<scene_actions_spatial_pair_binding::BoundObject> build_klhuo(
        const game_scene_render_queue::SceneActionPairCommand& command,
        float move_input_x) {
    const auto bounds = scene_actions_klhuo_fallback_bounds::build(command.record);
    if (!bounds.has_value()) return std::nullopt;

    const auto parent_x = game_scene_layer_object_parent_x::from_move_input(
            command.layer_z_index,
            command.layer_camera_factor,
            move_input_x);
    if (!parent_x.has_value()) return std::nullopt;

    GeometryInput geometry;
    geometry.object_position_x = *parent_x;
    geometry.sprite_bounds = *bounds;
    return build(
            command.source_layer_index,
            command.source_object_index,
            command.record,
            geometry);
}

}  // namespace nevergone::scene_actions_bound_object_builder
