#include "game_levels_model.h"

#include <utility>

namespace nevergone::game_levels_model {

bool parse(const hp_data::Reader& reader, Model* out) {
    if (out == nullptr) return false;

    Model parsed;
    if (!game_levels_layer_tail::parse_scene_section(reader, &parsed.scenes)) {
        return false;
    }
    if (!game_levels_actions_section::parse_section(
                reader,
                parsed.scenes.bytes_consumed,
                parsed.scenes.prefix.first_i32,
                &parsed.actions)) {
        return false;
    }
    if (!game_levels_global_section::parse_section(
                reader,
                parsed.actions.end_offset,
                &parsed.global)) {
        return false;
    }
    if (!game_levels_port_node_section::parse_section(
                reader,
                parsed.global.end_offset,
                &parsed.port_nodes)) {
        return false;
    }

    parsed.port_graph = game_levels_port_node_graph::link(parsed.port_nodes.port_nodes);
    parsed.start_scene = game_levels_start_scene::resolve(parsed.scenes, parsed.port_nodes);
    parsed.end_offset = parsed.port_nodes.end_offset;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_model
