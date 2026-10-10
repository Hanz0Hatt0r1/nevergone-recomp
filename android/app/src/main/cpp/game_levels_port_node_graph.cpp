#include "game_levels_port_node_graph.h"

#include <cstddef>
#include <vector>

namespace nevergone::game_levels_port_node_graph {

Graph link(const std::vector<game_levels_port_node_section::PortNodeRecord>& port_nodes) {
    Graph result;
    result.nodes.resize(port_nodes.size());

    for (std::size_t source = 0; source < port_nodes.size(); ++source) {
        for (std::size_t candidate = 0; candidate < port_nodes.size(); ++candidate) {
            if (candidate == source) continue;
            if (port_nodes[source].third_string != port_nodes[candidate].first_string) continue;

            result.nodes[source].forward_linked = true;
            result.nodes[source].forward_index = candidate;
            result.nodes[candidate].reverse_linked = true;
            result.nodes[candidate].reverse_index = source;
            ++result.forward_link_count;
            break;
        }
    }

    return result;
}

}  // namespace nevergone::game_levels_port_node_graph
