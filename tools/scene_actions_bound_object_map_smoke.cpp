#include <cassert>
#include <vector>

#include "scene_actions_bound_object_map.h"

int main() {
    namespace map = nevergone::scene_actions_bound_object_map;
    namespace spatial = nevergone::scene_actions_spatial_trigger;

    nevergone::game_scene_construction_plan::ScenePlan scene;
    scene.object_count = 2u;
    nevergone::game_scene_construction_plan::LayerPlan layer;
    layer.source_layer_index = 4u;

    nevergone::game_scene_construction_plan::ObjectPlan first;
    first.source_object_index = 0u;
    first.record.string_value = "source.png";
    first.record.conditional_present = true;
    first.record.conditional_first_u32 = 2004u;
    layer.objects.push_back(first);

    nevergone::game_scene_construction_plan::ObjectPlan second;
    second.source_object_index = 1u;
    second.record.string_value = "klhuo-1.png";
    layer.objects.push_back(second);
    scene.layers.push_back(layer);

    map::GeometryEntry geometry;
    geometry.layer_index = 4u;
    geometry.object_index = 0u;
    geometry.object_position_x = 3.0f;
    geometry.sprite_bounds = spatial::Rect{10.0f, 20.0f, 30.0f, 40.0f};

    auto result = map::build(scene, {geometry});
    assert(result.status == map::Status::kReady);
    assert(result.objects.size() == 2u);
    assert(result.objects[0].ref.layer_index == 4u);
    assert(result.objects[0].ref.object_index == 0u);
    assert(result.objects[0].view.event_id_108 == 2004);
    assert(result.objects[0].view.object_position_x == 3.0f);
    assert(result.objects[0].view.sprite_bounds.has_value());
    assert(result.objects[1].ref.object_index == 1u);
    assert(!result.objects[1].view.sprite_bounds.has_value());

    auto duplicate = map::build(scene, {geometry, geometry});
    assert(duplicate.status == map::Status::kDuplicateGeometry);
    assert(duplicate.objects.empty());

    auto orphan = geometry;
    orphan.layer_index = 99u;
    result = map::build(scene, {orphan});
    assert(result.status == map::Status::kGeometryOutsideScene);
    assert(result.objects.empty());

    geometry.sprite_bounds = spatial::Rect{0.0f, 0.0f, -1.0f, 2.0f};
    // finite negative extents are structurally representable here; the recovered
    // spatial predicate owns intersection semantics rather than this mapper.
    result = map::build(scene, {geometry});
    assert(result.status == map::Status::kReady);

    geometry.object_position_x = 0.0f / 0.0f;
    result = map::build(scene, {geometry});
    assert(result.status == map::Status::kInvalidGeometry);
    assert(result.objects.empty());

    return 0;
}
