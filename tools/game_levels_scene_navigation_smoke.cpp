#include <cassert>
#include <string>

#include "game_levels_scene_navigation.h"

namespace {
using SceneRecord = nevergone::game_levels_layer_tail::SceneRecord;
using PortRecord = nevergone::game_levels_port_node_section::PortNodeRecord;

SceneRecord scene(const std::string& guid) {
    SceneRecord value;
    value.string_value = guid;
    return value;
}

PortRecord port(const std::string& guid) {
    PortRecord value;
    value.first_string = guid;
    return value;
}
}  // namespace

int main() {
    namespace nav = nevergone::game_levels_port_navigation;
    namespace scene_nav = nevergone::game_levels_scene_navigation;
    namespace graph_ns = nevergone::game_levels_port_node_graph;

    nevergone::game_levels_layer_tail::SceneSection scenes;
    scenes.scenes = {scene("a"), scene("b"), scene("dup"), scene("dup")};

    nevergone::game_levels_port_node_section::Section ports;
    ports.port_nodes = {port("a"), port("b"), port("missing")};

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

    nav::State state;
    state.current_port_node_index = 0u;

    auto current = scene_nav::resolve_current(scenes, ports, state);
    assert(current.port_node_index == 0u);
    assert(current.scene_index == 0u);
    assert(current.scene_guid == "a");

    auto transition = scene_nav::step_and_resolve(scenes, ports, graph, 1u, &state);
    assert(transition.port_step.status == nav::StepStatus::kTraversed);
    assert(state.current_port_node_index == 1u);
    assert(transition.scene.port_node_index == 1u);
    assert(transition.scene.scene_index == 1u);
    assert(transition.scene.scene_guid == "b");

    transition = scene_nav::step_and_resolve(scenes, ports, graph, 0u, &state);
    assert(transition.port_step.status == nav::StepStatus::kTraversed);
    assert(state.current_port_node_index == 0u);
    assert(transition.scene.scene_index == 0u);

    // Unsupported event types preserve both port state and current scene.
    transition = scene_nav::step_and_resolve(scenes, ports, graph, 9u, &state);
    assert(transition.port_step.status == nav::StepStatus::kUnsupportedEventType);
    assert(state.current_port_node_index == 0u);
    assert(transition.scene.scene_index == 0u);

    // A valid transition may resolve a port whose scene GUID is absent.
    state.current_port_node_index = 1u;
    transition = scene_nav::step_and_resolve(scenes, ports, graph, 1u, &state);
    assert(state.current_port_node_index == 2u);
    assert(transition.scene.port_node_index == 2u);
    assert(transition.scene.scene_guid == "missing");
    assert(!transition.scene.scene_index.has_value());

    // Missing forward link clears current port and therefore current scene.
    transition = scene_nav::step_and_resolve(scenes, ports, graph, 1u, &state);
    assert(!state.current_port_node_index.has_value());
    assert(!transition.scene.port_node_index.has_value());
    assert(!transition.scene.scene_index.has_value());
    assert(transition.scene.scene_guid.empty());

    // Invalid retained index is treated as no current scene.
    state.current_port_node_index = 99u;
    current = scene_nav::resolve_current(scenes, ports, state);
    assert(!current.port_node_index.has_value());
    assert(!current.scene_index.has_value());

    // GUID lookup preserves original first-match behavior.
    ports.port_nodes[0].first_string = "dup";
    state.current_port_node_index = 0u;
    current = scene_nav::resolve_current(scenes, ports, state);
    assert(current.scene_index == 2u);

    return 0;
}
