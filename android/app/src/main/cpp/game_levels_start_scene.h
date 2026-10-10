#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "game_levels_layer_tail.h"
#include "game_levels_port_node_section.h"

namespace nevergone::game_levels_start_scene {

struct Selection {
    std::optional<std::size_t> port_node_index;
    std::optional<std::size_t> scene_index;
    std::string scene_guid;
    bool used_first_port_fallback = false;
};

// Reproduces the recovered startup lookup boundary:
// 1) choose the first port node whose first_bool is true;
// 2) if none is marked and nodes exist, fall back to index 0;
// 3) resolve the selected port's first_string against the first matching
//    SceneRecord::string_value.
Selection resolve(
        const game_levels_layer_tail::SceneSection& scenes,
        const game_levels_port_node_section::Section& port_nodes);

}  // namespace nevergone::game_levels_start_scene
