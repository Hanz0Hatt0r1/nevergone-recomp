#include <cassert>
#include <cstddef>

#include "game_levels_port_navigation.h"
#include "game_levels_port_node_graph.h"
#include "game_levels_start_scene.h"

int main() {
    namespace nav = nevergone::game_levels_port_navigation;
    namespace graph_ns = nevergone::game_levels_port_node_graph;

    graph_ns::Graph graph;
    graph.nodes.resize(3u);
    graph.nodes[0].forward_index = 1u;
    graph.nodes[0].forward_linked = true;
    graph.nodes[1].reverse_index = 0u;
    graph.nodes[1].reverse_linked = true;
    graph.nodes[1].forward_index = 2u;
    graph.nodes[1].forward_linked = true;
    graph.nodes[2].reverse_index = 1u;
    graph.nodes[2].reverse_linked = true;

    nevergone::game_levels_start_scene::Selection start;
    start.port_node_index = 1u;
    nav::State state = nav::make_initial_state(start);
    assert(state.current_port_node_index == 1u);
    assert(state.stored_event_port_type == 0u);

    // Recovered event type 0 follows reverse and stores type 1.
    auto result = nav::step(graph, 0u, &state);
    assert(result.status == nav::StepStatus::kTraversed);
    assert(result.previous_port_node_index == 1u);
    assert(result.current_port_node_index == 0u);
    assert(state.current_port_node_index == 0u);
    assert(state.stored_event_port_type == 1u);

    // Recovered event type 1 follows forward and stores type 0.
    result = nav::step(graph, 1u, &state);
    assert(result.status == nav::StepStatus::kTraversed);
    assert(result.previous_port_node_index == 0u);
    assert(result.current_port_node_index == 1u);
    assert(state.stored_event_port_type == 0u);

    // Unsupported values return null-equivalent behavior without mutating state.
    result = nav::step(graph, 7u, &state);
    assert(result.status == nav::StepStatus::kUnsupportedEventType);
    assert(state.current_port_node_index == 1u);
    assert(state.stored_event_port_type == 0u);

    // A supported transition stores the opposite event type even when the
    // recovered link is null, and the original current pointer becomes null.
    state.current_port_node_index = 2u;
    state.stored_event_port_type = 0u;
    result = nav::step(graph, 1u, &state);
    assert(result.status == nav::StepStatus::kTraversed);
    assert(!state.current_port_node_index.has_value());
    assert(state.stored_event_port_type == 0u);

    // Null/invalid current port leaves state unchanged.
    result = nav::step(graph, 0u, &state);
    assert(result.status == nav::StepStatus::kNoCurrentPort);
    assert(!state.current_port_node_index.has_value());
    state.current_port_node_index = 99u;
    state.stored_event_port_type = 5u;
    result = nav::step(graph, 0u, &state);
    assert(result.status == nav::StepStatus::kNoCurrentPort);
    assert(state.current_port_node_index == 99u);
    assert(state.stored_event_port_type == 5u);

    assert(nav::step(graph, 0u, nullptr).status == nav::StepStatus::kNoCurrentPort);
    return 0;
}
