#pragma once

#include <cstddef>

#include "game_levels_actions_section.h"
#include "game_levels_global_section.h"
#include "game_levels_layer_tail.h"
#include "game_levels_port_navigation.h"
#include "game_levels_port_node_graph.h"
#include "game_levels_port_node_section.h"
#include "game_levels_start_scene.h"
#include "hp_data_reader.h"

namespace nevergone::game_levels_model {

struct Model {
    game_levels_layer_tail::SceneSection scenes;
    game_levels_actions_section::Section actions;
    game_levels_global_section::Section global;
    game_levels_port_node_section::Section port_nodes;
    game_levels_port_node_graph::Graph port_graph;
    game_levels_start_scene::Selection start_scene;
    game_levels_port_navigation::State navigation;
    std::size_t end_offset = 0;
};

// Parses the complete evidence-backed LoadGameLevels stream into one retained
// project-owned model, then performs the recovered port linking, start-scene
// lookup and initial current-port state. The physical file may contain bytes
// after end_offset; the original loader does not require EOF equality. Output
// is transactional on failure.
bool parse(const hp_data::Reader& reader, Model* out);

}  // namespace nevergone::game_levels_model
