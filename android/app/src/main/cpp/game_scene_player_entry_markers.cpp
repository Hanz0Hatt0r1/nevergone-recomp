#include "game_scene_player_entry_markers.h"

#include <algorithm>
#include <cmath>

namespace nevergone::game_scene_player_entry_markers {

std::optional<std::vector<Marker>> collect(
        const game_levels_scene_instance::SceneInstance& scene,
        std::uint32_t stored_event_port_type) {
    std::vector<Marker> markers;
    const std::size_t layer_count = std::min<std::size_t>(scene.layers.size(), 11u);

    for (std::size_t layer_index = 0; layer_index < layer_count; ++layer_index) {
        const auto& layer = scene.layers[layer_index];
        for (std::size_t object_index = 0; object_index < layer.objects.size(); ++object_index) {
            const auto& object = layer.objects[object_index];
            if (object.first_i32 != 6) continue;
            if (!object.has_tail_i32) return std::nullopt;
            if (static_cast<std::uint32_t>(object.tail_i32) != stored_event_port_type) continue;
            if (!std::isfinite(object.first_point_x) || !std::isfinite(object.first_point_y)) {
                return std::nullopt;
            }

            Marker marker;
            marker.source_layer_index = layer.source_layer_index;
            marker.source_object_index = object_index;
            marker.event_port_type = stored_event_port_type;
            marker.source_x = object.first_point_x;
            marker.source_y = object.first_point_y;
            markers.push_back(marker);
        }
    }
    return markers;
}

}  // namespace nevergone::game_scene_player_entry_markers
