#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "game_levels_port_node_graph.h"
#include "game_levels_port_node_section.h"

namespace {
using Record = nevergone::game_levels_port_node_section::PortNodeRecord;

Record record(const std::string& first, const std::string& third) {
    Record value;
    value.first_string = first;
    value.third_string = third;
    return value;
}
}  // namespace

int main() {
    namespace graph = nevergone::game_levels_port_node_graph;

    {
        const std::vector<Record> records;
        const auto linked = graph::link(records);
        assert(linked.nodes.empty());
        assert(linked.forward_link_count == 0u);
    }

    {
        const std::vector<Record> records{
            record("start", "target"),
            record("target", "missing"),
        };
        const auto linked = graph::link(records);
        assert(linked.forward_link_count == 1u);
        assert(linked.nodes[0].forward_linked);
        assert(linked.nodes[0].forward_index == 1u);
        assert(linked.nodes[1].reverse_linked);
        assert(linked.nodes[1].reverse_index == 0u);
        assert(!linked.nodes[1].forward_linked);
    }

    {
        // The original explicitly skips source == candidate, so an otherwise
        // matching self-only node remains unlinked.
        const std::vector<Record> records{record("same", "same")};
        const auto linked = graph::link(records);
        assert(linked.forward_link_count == 0u);
        assert(!linked.nodes[0].forward_linked);
        assert(!linked.nodes[0].reverse_linked);
    }

    {
        // First matching candidate wins in inner-loop array order.
        const std::vector<Record> records{
            record("source", "dup"),
            record("dup", "none"),
            record("dup", "none"),
        };
        const auto linked = graph::link(records);
        assert(linked.forward_link_count == 1u);
        assert(linked.nodes[0].forward_index == 1u);
        assert(linked.nodes[1].reverse_index == 0u);
        assert(!linked.nodes[2].reverse_linked);
    }

    {
        // A later source targeting the same candidate overwrites only the
        // candidate reverse pointer; both source forward links remain.
        const std::vector<Record> records{
            record("source-a", "target"),
            record("source-b", "target"),
            record("target", "none"),
        };
        const auto linked = graph::link(records);
        assert(linked.forward_link_count == 2u);
        assert(linked.nodes[0].forward_index == 2u);
        assert(linked.nodes[1].forward_index == 2u);
        assert(linked.nodes[2].reverse_linked);
        assert(linked.nodes[2].reverse_index == 1u);
    }

    {
        const std::vector<Record> records{
            record("a", "missing-a"),
            record("b", "missing-b"),
        };
        const auto linked = graph::link(records);
        assert(linked.forward_link_count == 0u);
        assert(!linked.nodes[0].forward_index.has_value());
        assert(!linked.nodes[1].reverse_index.has_value());
    }

    return 0;
}
