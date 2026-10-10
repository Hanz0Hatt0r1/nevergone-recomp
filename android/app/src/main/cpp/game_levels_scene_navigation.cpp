#include "game_levels_scene_navigation.h"

#include <cstddef>

namespace nevergone::game_levels_scene_navigation {

Selection resolve_current(
        const game_levels_layer_tail::SceneSection& scenes,
        const game_levels_port_node_section::Section& port_nodes,
        const game_levels_port_navigation::State& navigation) {
    Selection result;
    if (!navigation.current_port_node_index.has_value()) return result;

    const std::size_t port_index = *navigation.current_port_node_index;
    if (port_index >= port_nodes.port_nodes.size()) return result;

    result.port_node_index = port_index;
    result.scene_guid = port_nodes.port_nodes[port_index].first_string;
    for (std::size_t i = 0; i < scenes.scenes.size(); ++i) {
        if (scenes.scenes[i].string_value != result.scene_guid) continue;
        result.scene_index = i;
        break;
    }
    return result;
}

Transition step_and_resolve(
        const game_levels_layer_tail::SceneSection& scenes,
        const game_levels_port_node_section::Section& port_nodes,
        const game_levels_port_node_graph::Graph& graph,
        std::uint32_t requested_event_port_type,
        game_levels_port_navigation::State* navigation) {
    Transition result;
    result.port_step = game_levels_port_navigation::step(
            graph,
            requested_event_port_type,
            navigation);
    if (navigation != nullptr) {
        result.scene = resolve_current(scenes, port_nodes, *navigation);
    }
    return result;
}

}  // namespace nevergone::game_levels_scene_navigation
