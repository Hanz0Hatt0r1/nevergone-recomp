#pragma once

#include <cmath>
#include <cstddef>
#include <optional>

#include "game_scene_layer_object_parent_x.h"
#include "game_scene_render_queue.h"
#include "scene_actions_spatial_trigger.h"
#include "scene_actions_sprite_bounds.h"

namespace nevergone::game_scene_direct_spatial_geometry {

struct Entry {
    std::size_t source_layer_index = 0;
    std::size_t source_object_index = 0;
    float object_position_x = 0.0f;
    scene_actions_spatial_trigger::Rect sprite_bounds{};
};

// Build the exact geometry inputs consumed by GameSceneObject::getObjectRect()
// for an untrimmed direct-file type-0 sprite. `skew_x_degrees` must be the same
// live animation sample used by the draw pass. Sprite-frame-by-name commands are
// intentionally rejected because trimmed-frame local offsets are not recovered.
inline std::optional<Entry> build(
        const game_scene_render_queue::SpriteCommand& command,
        float skew_x_degrees,
        int texture_width,
        int texture_height,
        float camera_eye_x) {
    if (command.resource.kind != game_scene_type0_resource::Kind::kDirectFile ||
            !command.direct_asset_relative_path.has_value() ||
            command.player_body_transform.has_value() ||
            !std::isfinite(camera_eye_x)) {
        return std::nullopt;
    }

    const auto bounds = scene_actions_sprite_bounds::build_untrimmed(
            command.transform,
            skew_x_degrees,
            texture_width,
            texture_height);
    if (!bounds.has_value()) return std::nullopt;

    const auto parent_x = game_scene_layer_object_parent_x::from_move_input(
            command.layer_z_index,
            command.layer_camera_factor,
            camera_eye_x);
    if (!parent_x.has_value()) return std::nullopt;

    Entry entry;
    entry.source_layer_index = command.source_layer_index;
    entry.source_object_index = command.source_object_index;
    entry.object_position_x = *parent_x;
    entry.sprite_bounds = *bounds;
    return entry;
}

}  // namespace nevergone::game_scene_direct_spatial_geometry
