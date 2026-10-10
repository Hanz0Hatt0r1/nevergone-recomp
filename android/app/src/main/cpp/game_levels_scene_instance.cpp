#include "game_levels_scene_instance.h"

#include <cstddef>
#include <optional>
#include <utility>

#include "game_levels_scene_navigation.h"

namespace nevergone::game_levels_scene_instance {

std::optional<SceneInstance> build(
        const game_levels_model::Model& model,
        std::size_t scene_index) {
    if (scene_index >= model.scenes.scenes.size()) return std::nullopt;

    const auto& source = model.scenes.scenes[scene_index];
    SceneInstance instance;
    instance.source_scene_index = scene_index;
    instance.guid = source.string_value;
    instance.first_point_x = source.first_point_x;
    instance.first_point_y = source.first_point_y;
    instance.layers.reserve(source.layers.size());

    for (std::size_t layer_index = 0; layer_index < source.layers.size(); ++layer_index) {
        const auto& source_layer = source.layers[layer_index];
        LayerInstance layer;
        layer.source_layer_index = layer_index;
        layer.first_float = source_layer.first_float;
        layer.objects = source_layer.objects;
        layer.top_border_points = source_layer.top_border_points;
        layer.bottom_border_points = source_layer.bottom_border_points;
        instance.object_count += layer.objects.size();
        instance.layers.push_back(std::move(layer));
    }

    return instance;
}

std::optional<SceneInstance> build_current(const game_levels_model::Model& model) {
    const auto selection = game_levels_scene_navigation::resolve_current(
            model.scenes,
            model.port_nodes,
            model.navigation);
    if (!selection.scene_index.has_value()) return std::nullopt;
    return build(model, *selection.scene_index);
}

}  // namespace nevergone::game_levels_scene_instance
