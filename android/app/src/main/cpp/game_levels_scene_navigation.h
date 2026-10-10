#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "game_levels_layer_tail.h"
#include "game_levels_port_navigation.h"
#include "game_levels_port_node_graph.h"
#include "game_levels_port_node_section.h"

namespace nevergone::game_levels_scene_navigation {

struct Selection {
    std::optional<std::size_t> port_node_index;
    std::optional<std::size_t> scene_index;
    std::string scene_guid;
};

struct Transition {
    game_levels_port_navigation::StepResult port_step;
    Selection scene;
};

// Mirrors GameScene::getGLGameSceneData(): use the current port node's first
// serialized string as the GUID and return the first matching scene record.
Selection resolve_current(
        const game_levels_layer_tail::SceneSection& scenes,
        const game_levels_port_node_section::Section& port_nodes,
        const game_levels_port_navigation::State& navigation);

// Project-owned convenience boundary for the recovered checkpoint transition:
// apply GetPortNodeLinkPortNode semantics, then resolve the scene associated
// with the resulting current port. Unsupported event types keep the old scene.
Transition step_and_resolve(
        const game_levels_layer_tail::SceneSection& scenes,
        const game_levels_port_node_section::Section& port_nodes,
        const game_levels_port_node_graph::Graph& graph,
        std::uint32_t requested_event_port_type,
        game_levels_port_navigation::State* navigation);

}  // namespace nevergone::game_levels_scene_navigation
