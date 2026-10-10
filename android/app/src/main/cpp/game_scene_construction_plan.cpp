#include "game_scene_construction_plan.h"

#include <algorithm>

namespace nevergone::game_scene_construction_plan {

ObjectConstructionKind classify_object(std::int32_t type_code) {
    if (type_code == 0) return ObjectConstructionKind::kType0SpriteBacked;
    return ObjectConstructionKind::kUnresolved;
}

ScenePlan build(const game_levels_scene_instance::SceneInstance& scene) {
    ScenePlan plan;
    plan.source_scene_index = scene.source_scene_index;
    plan.guid = scene.guid;
    plan.first_point_x = scene.first_point_x;
    plan.first_point_y = scene.first_point_y;

    const std::size_t layer_count = std::min(scene.layers.size(), kLoadingTexLayerSlotCount);
    plan.layers.reserve(layer_count);
    plan.ignored_source_layer_count = scene.layers.size() - layer_count;

    for (std::size_t z = 0; z < layer_count; ++z) {
        const auto& source_layer = scene.layers[z];
        LayerPlan layer;
        layer.z_index = z;
        layer.source_layer_index = source_layer.source_layer_index;
        layer.first_float = source_layer.first_float;
        layer.top_border_points = source_layer.top_border_points;
        layer.bottom_border_points = source_layer.bottom_border_points;
        layer.objects.reserve(source_layer.objects.size());

        for (std::size_t object_index = 0; object_index < source_layer.objects.size(); ++object_index) {
            const auto& source_object = source_layer.objects[object_index];
            ObjectPlan object;
            object.source_object_index = object_index;
            object.type_code = source_object.first_i32;
            object.construction_kind = classify_object(object.type_code);
            object.record = source_object;
            layer.objects.push_back(std::move(object));
            ++plan.object_count;
        }

        plan.layers.push_back(std::move(layer));
    }

    return plan;
}

const char* construction_kind_name(ObjectConstructionKind kind) {
    switch (kind) {
        case ObjectConstructionKind::kUnresolved: return "unresolved";
        case ObjectConstructionKind::kType0SpriteBacked: return "type0-sprite-backed";
    }
    return "unknown";
}

}  // namespace nevergone::game_scene_construction_plan
