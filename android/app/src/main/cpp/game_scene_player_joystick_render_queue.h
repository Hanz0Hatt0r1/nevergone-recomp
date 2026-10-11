#pragma once

#include <limits>
#include <utility>

#include "game_scene_player_joystick_input.h"
#include "game_scene_render_queue.h"

namespace nevergone::game_scene_player_joystick_render_queue {

constexpr const char* kDockResourceName = "Dock.png";
constexpr const char* kDockImportedRelativePath = "controls_ui/Dock.png";
constexpr const char* kActiveDockResourceName = "Dock_s.png";
constexpr const char* kActiveDockImportedRelativePath = "controls_ui/Dock_s.png";

// Controls::init() creates the fixed CCJoyStick and installs Dock.png. The
// recovered activation callback replaces it with Dock_s.png; deactivation
// restores Dock.png. Both are centered children of the dock container.
inline game_scene_render_queue::Queue with_dock(
        game_scene_render_queue::Queue queue,
        bool active) {
    game_scene_render_queue::SpriteCommand dock;
    // The controls layer is a screen-space GameScene UI overlay, not one of the
    // serialized world layers. Appending it makes the same ordering explicit in
    // the flat direct renderer while retaining a diagnostic z sentinel.
    dock.layer_z_index = std::numeric_limits<std::size_t>::max();
    dock.source_layer_index = std::numeric_limits<std::size_t>::max();
    dock.source_object_index = 0u;
    dock.resource.kind = game_scene_type0_resource::Kind::kDirectFile;
    dock.resource.resource_name = active ? kActiveDockResourceName : kDockResourceName;
    dock.direct_asset_relative_path = active
        ? kActiveDockImportedRelativePath
        : kDockImportedRelativePath;
    dock.transform.position_x = game_scene_player_joystick_input::kCenterX;
    dock.transform.position_y = game_scene_player_joystick_input::kCenterY;
    dock.transform.rotation = 0.0f;
    dock.transform.scale_x = 1.0f;
    dock.transform.scale_y = 1.0f;
    queue.sprites.push_back(std::move(dock));
    ++queue.direct_file_count;
    return queue;
}

inline game_scene_render_queue::Queue current(game_scene_render_queue::Queue queue) {
    return with_dock(std::move(queue),
                     game_scene_player_joystick_input::snapshot().captured);
}

}  // namespace nevergone::game_scene_player_joystick_render_queue
