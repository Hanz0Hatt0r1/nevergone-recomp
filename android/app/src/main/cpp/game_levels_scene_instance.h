#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "game_levels_layer_tail.h"
#include "game_levels_model.h"
#include "game_levels_scene_prefix.h"

namespace nevergone::game_levels_scene_instance {

struct LayerInstance {
    std::size_t source_layer_index = 0;
    float first_float = 0.0f;
    std::vector<game_levels_scene_prefix::ObjectRecord> objects;
    std::vector<game_levels_layer_tail::BorderPoint> top_border_points;
    std::vector<game_levels_layer_tail::BorderPoint> bottom_border_points;
};

struct SceneInstance {
    std::size_t source_scene_index = 0;
    std::string guid;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    std::vector<LayerInstance> layers;
    std::size_t object_count = 0;
};

// Builds a project-owned structural scene instance from one parsed SceneRecord.
// This intentionally copies only already recovered serialized data; it does not
// reproduce unresolved GameScene/createGSObject constructors or original pointers.
std::optional<SceneInstance> build(
        const game_levels_model::Model& model,
        std::size_t scene_index);

// Resolves the model's retained current port to its current SceneRecord and
// builds the corresponding structural instance.
std::optional<SceneInstance> build_current(const game_levels_model::Model& model);

}  // namespace nevergone::game_levels_scene_instance
