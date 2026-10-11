#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "game_levels_scene_instance.h"

namespace nevergone::game_scene_player_entry_markers {

// One type-6 layer object whose serialized tail value matches the currently
// retained GameLevels event-port type. The point remains in serialized scene
// coordinates; applying the original loadingTex viewport transform is a later
// boundary.
struct Marker {
    std::size_t source_layer_index = 0;
    std::size_t source_object_index = 0;
    std::uint32_t event_port_type = 0;
    float source_x = 0.0f;
    float source_y = 0.0f;
};

// Mirrors only the proven selection boundary in GameScene::loadingTex(): visit
// the first 11 ordered layers, inspect objects in source order, and retain type
// 6 records whose +0x58 tail value equals GameLevels +0x40 (stored event-port
// type). A malformed matching record fails closed. Multiple matches are kept in
// traversal order because the original executes the position path for each one.
std::optional<std::vector<Marker>> collect(
        const game_levels_scene_instance::SceneInstance& scene,
        std::uint32_t stored_event_port_type);

}  // namespace nevergone::game_scene_player_entry_markers
