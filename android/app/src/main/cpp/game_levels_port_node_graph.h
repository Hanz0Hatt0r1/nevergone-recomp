#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "game_levels_port_node_section.h"

namespace nevergone::game_levels_port_node_graph {

struct NodeLinks {
    bool forward_linked = false;
    bool reverse_linked = false;
    std::optional<std::size_t> forward_index;
    std::optional<std::size_t> reverse_index;
};

struct Graph {
    std::vector<NodeLinks> nodes;
    std::size_t forward_link_count = 0;
};

// Reproduces the recovered LinkScenePortNode nested-loop semantics without
// using original object pointers. For each source node, the first different
// candidate whose first_string equals source.third_string becomes its forward
// link. The candidate's reverse link is assigned to that source and may be
// overwritten by a later source, matching the original outer-loop order.
Graph link(const std::vector<game_levels_port_node_section::PortNodeRecord>& port_nodes);

}  // namespace nevergone::game_levels_port_node_graph
