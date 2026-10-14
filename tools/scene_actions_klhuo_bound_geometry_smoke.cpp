#include <cassert>
#include <cmath>
#include <limits>

#include "scene_actions_klhuo_bound_geometry.h"

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

int main() {
    namespace geo = nevergone::scene_actions_klhuo_bound_geometry;

    nevergone::game_scene_construction_plan::ScenePlan scene;
    nevergone::game_scene_construction_plan::LayerPlan layer;
    layer.z_index = 0u;
    layer.source_layer_index = 3u;
    layer.first_float = 0.25f;

    nevergone::game_scene_construction_plan::ObjectPlan object;
    object.source_object_index = 5u;
    object.construction_kind =
            nevergone::game_scene_construction_plan::ObjectConstructionKind::kType0SceneActionPair;
    object.type0_resource = nevergone::game_scene_type0_resource::select("klhuo-1.png");
    object.record.string_value = "klhuo-1.png";
    object.record.first_point_x = 100.0f;
    object.record.first_point_y = 200.0f;
    object.record.second_point_x = 1.0f;
    object.record.second_point_y = 1.0f;
    layer.objects.push_back(object);
    scene.layers.push_back(layer);

    auto result = geo::build(scene, 40.0f);
    assert(result.status == geo::Status::kReady);
    assert(result.geometry.size() == 1u);
    assert(result.geometry[0].layer_index == 3u);
    assert(result.geometry[0].object_index == 5u);
    assert(near(result.geometry[0].object_position_x, 30.0f));
    assert(result.geometry[0].sprite_bounds.has_value());
    assert(near(result.geometry[0].sprite_bounds->x, 90.0f));
    assert(near(result.geometry[0].sprite_bounds->y, 190.0f));
    assert(near(result.geometry[0].sprite_bounds->width, 20.0f));
    assert(near(result.geometry[0].sprite_bounds->height, 20.0f));

    scene.layers[0].z_index = 7u;
    result = geo::build(scene, 40.0f);
    assert(result.status == geo::Status::kReady);
    assert(result.geometry.size() == 1u);
    assert(near(result.geometry[0].object_position_x, 0.0f));

    result = geo::build(scene, std::numeric_limits<float>::quiet_NaN());
    assert(result.status == geo::Status::kInvalidMoveInput);
    assert(result.geometry.empty());

    return 0;
}
