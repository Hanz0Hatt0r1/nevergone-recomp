#pragma once

#include <limits>
#include <utility>

#include "game_scene_player_joystick_input.h"
#include "game_scene_render_queue.h"

namespace nevergone::game_scene_player_joystick_render_queue {

constexpr const char* kDockResourceName = "Dock.png";
constexpr const char* kDockImportedRelativePath = "controls_ui/Dock.png";

// Controls::init() creates a fixed CCJoyStick node at (204, 173), creates the
// dock container first, then setDockTexture("Dock.png") installs a centered
// CCSprite child. Stick/ball containers remain textureless in that recovered
// initializer, so this queue slice intentionally adds only the proven dock.
inline game_scene_render_queue::Queue with_dock(game_scene_render_queue::Queue queue) {
    game_scene_render_queue::SpriteCommand dock;
    // The controls layer is a screen-space GameScene UI overlay, not one of the
    // serialized world layers. Appending it makes the same ordering explicit in
    // the flat direct renderer while retaining a diagnostic z sentinel.
    dock.layer_z_index = std::numeric_limits<std::size_t>::max();
    dock.source_layer_index = std::numeric_limits<std::size_t>::max();
    dock.source_object_index = 0u;
    dock.resource.kind = game_scene_type0_resource::Kind::kDirectFile;
    dock.resource.resource_name = kDockResourceName;
    dock.direct_asset_relative_path = kDockImportedRelativePath;
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
    return with_dock(std::move(queue));
}

}  // namespace nevergone::game_scene_player_joystick_render_queue
