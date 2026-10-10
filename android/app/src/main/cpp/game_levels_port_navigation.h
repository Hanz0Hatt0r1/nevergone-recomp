#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "game_levels_port_node_graph.h"
#include "game_levels_start_scene.h"

namespace nevergone::game_levels_port_navigation {

struct State {
    std::optional<std::size_t> current_port_node_index;
    std::uint32_t stored_event_port_type = 0;
};

enum class StepStatus {
    kNoCurrentPort = 0,
    kUnsupportedEventType,
    kTraversed,
};

struct StepResult {
    StepStatus status = StepStatus::kNoCurrentPort;
    std::optional<std::size_t> previous_port_node_index;
    std::optional<std::size_t> current_port_node_index;
    std::uint32_t stored_event_port_type = 0;
};

State make_initial_state(const game_levels_start_scene::Selection& start_scene);

// Reproduces GameLevels::GetPortNodeLinkPortNode(EVENT_PORT_TYPE):
// requested type 0 follows the recovered reverse link and stores event type 1;
// requested type 1 follows the recovered forward link and stores event type 0.
// Other types leave state untouched. A supported transition with a missing link
// clears the current port, matching the original null-pointer assignment.
StepResult step(
        const game_levels_port_node_graph::Graph& graph,
        std::uint32_t requested_event_port_type,
        State* state);

}  // namespace nevergone::game_levels_port_navigation
