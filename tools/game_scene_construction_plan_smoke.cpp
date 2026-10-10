#include <cassert>
#include <cstddef>
#include <string>

#include "game_scene_construction_plan.h"

namespace {

nevergone::game_levels_scene_prefix::ObjectRecord make_object(
        std::int32_t type_code,
        const std::string& value) {
    nevergone::game_levels_scene_prefix::ObjectRecord object;
    object.first_i32 = type_code;
    object.string_value = value;
    object.first_point_x = static_cast<float>(type_code + 10);
    object.first_point_y = static_cast<float>(type_code + 20);
    return object;
}

}  // namespace

int main() {
    namespace plan_ns = nevergone::game_scene_construction_plan;
    using nevergone::game_levels_scene_instance::LayerInstance;
    using nevergone::game_levels_scene_instance::SceneInstance;

    assert(plan_ns::classify_object(0) == plan_ns::ObjectConstructionKind::kType0SpriteBacked);
    assert(plan_ns::classify_object(1) == plan_ns::ObjectConstructionKind::kUnresolved);
    assert(plan_ns::classify_object(6) == plan_ns::ObjectConstructionKind::kUnresolved);
    assert(std::string(plan_ns::construction_kind_name(
            plan_ns::ObjectConstructionKind::kType0SpriteBacked)) == "type0-sprite-backed");

    SceneInstance scene;
    scene.source_scene_index = 7u;
    scene.guid = "scene-guid";
    scene.first_point_x = 12.5f;
    scene.first_point_y = -3.0f;

    for (std::size_t i = 0; i < 12u; ++i) {
        LayerInstance layer;
        layer.source_layer_index = 100u + i;  // Prove z comes from array order, not this metadata.
        layer.first_float = static_cast<float>(i) + 0.25f;
        scene.layers.push_back(layer);
    }

    scene.layers[0].objects.push_back(make_object(0, "background.png"));
    scene.layers[0].objects.push_back(make_object(4, "type-four"));
    scene.layers[3].objects.push_back(make_object(6, "type-six"));
    scene.layers[10].objects.push_back(make_object(1, "type-one"));
    // The original loadingTex loop never requests source array index 11.
    scene.layers[11].objects.push_back(make_object(0, "ignored.png"));
    scene.object_count = 5u;

    const auto plan = plan_ns::build(scene);
    assert(plan.source_scene_index == 7u);
    assert(plan.guid == "scene-guid");
    assert(plan.first_point_x == 12.5f);
    assert(plan.first_point_y == -3.0f);
    assert(plan.layers.size() == plan_ns::kLoadingTexLayerSlotCount);
    assert(plan.ignored_source_layer_count == 1u);
    assert(plan.object_count == 4u);

    for (std::size_t z = 0; z < plan.layers.size(); ++z) {
        assert(plan.layers[z].z_index == z);
        assert(plan.layers[z].source_layer_index == 100u + z);
        assert(plan.layers[z].first_float == static_cast<float>(z) + 0.25f);
    }

    assert(plan.layers[0].objects.size() == 2u);
    assert(plan.layers[0].objects[0].source_object_index == 0u);
    assert(plan.layers[0].objects[0].type_code == 0);
    assert(plan.layers[0].objects[0].construction_kind ==
            plan_ns::ObjectConstructionKind::kType0SpriteBacked);
    assert(plan.layers[0].objects[0].record.string_value == "background.png");
    assert(plan.layers[0].objects[0].record.first_point_x == 10.0f);

    assert(plan.layers[0].objects[1].source_object_index == 1u);
    assert(plan.layers[0].objects[1].type_code == 4);
    assert(plan.layers[0].objects[1].construction_kind ==
            plan_ns::ObjectConstructionKind::kUnresolved);
    assert(plan.layers[3].objects[0].type_code == 6);
    assert(plan.layers[10].objects[0].type_code == 1);

    for (const auto& layer : plan.layers) {
        for (const auto& object : layer.objects) {
            assert(object.record.string_value != "ignored.png");
        }
    }

    return 0;
}
