#include "game_levels_start_scene.h"

#include <cstddef>

namespace nevergone::game_levels_start_scene {

Selection resolve(
        const game_levels_layer_tail::SceneSection& scenes,
        const game_levels_port_node_section::Section& port_nodes) {
    Selection result;

    for (std::size_t i = 0; i < port_nodes.port_nodes.size(); ++i) {
        if (!port_nodes.port_nodes[i].first_bool) continue;
        result.port_node_index = i;
        break;
    }

    if (!result.port_node_index.has_value() && !port_nodes.port_nodes.empty()) {
        result.port_node_index = 0u;
        result.used_first_port_fallback = true;
    }

    if (!result.port_node_index.has_value()) return result;

    const auto& selected_port = port_nodes.port_nodes[*result.port_node_index];
    result.scene_guid = selected_port.first_string;
    for (std::size_t i = 0; i < scenes.scenes.size(); ++i) {
        if (scenes.scenes[i].string_value != result.scene_guid) continue;
        result.scene_index = i;
        break;
    }

    return result;
}

}  // namespace nevergone::game_levels_start_scene
