#include "game_levels_port_navigation.h"

#include <cstddef>
#include <cstdint>

namespace nevergone::game_levels_port_navigation {

State make_initial_state(const game_levels_start_scene::Selection& start_scene) {
    State state;
    state.current_port_node_index = start_scene.port_node_index;
    state.stored_event_port_type = 0u;
    return state;
}

StepResult step(
        const game_levels_port_node_graph::Graph& graph,
        std::uint32_t requested_event_port_type,
        State* state) {
    StepResult result;
    if (state == nullptr) return result;

    result.previous_port_node_index = state->current_port_node_index;
    result.current_port_node_index = state->current_port_node_index;
    result.stored_event_port_type = state->stored_event_port_type;

    if (!state->current_port_node_index.has_value() ||
            *state->current_port_node_index >= graph.nodes.size()) {
        result.status = StepStatus::kNoCurrentPort;
        return result;
    }

    if (requested_event_port_type != 0u && requested_event_port_type != 1u) {
        result.status = StepStatus::kUnsupportedEventType;
        return result;
    }

    const auto& links = graph.nodes[*state->current_port_node_index];
    if (requested_event_port_type == 0u) {
        state->stored_event_port_type = 1u;
        state->current_port_node_index = links.reverse_index;
    } else {
        state->stored_event_port_type = 0u;
        state->current_port_node_index = links.forward_index;
    }

    result.status = StepStatus::kTraversed;
    result.current_port_node_index = state->current_port_node_index;
    result.stored_event_port_type = state->stored_event_port_type;
    return result;
}

}  // namespace nevergone::game_levels_port_navigation
