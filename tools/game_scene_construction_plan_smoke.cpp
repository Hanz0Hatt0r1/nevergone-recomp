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
    object.middle_float = static_cast<float>(type_code + 30);
    object.second_point_x = static_cast<float>(type_code + 40);
    object.second_point_y = static_cast<float>(type_code + 50);
    object.trailing_i32 = type_code + 60;
    object.first_bool = type_code == 0;
    return object;
}

}  // namespace

int main() {
    namespace plan_ns = nevergone::game_scene_construction_plan;
    namespace resource_ns = nevergone::game_scene_type0_resource;
    using nevergone::game_levels_scene_instance::LayerInstance;
    using nevergone::game_levels_scene_instance::SceneInstance;

    assert(plan_ns::classify_object(0) == plan_ns::ObjectConstructionKind::kType0SpriteBacked);
    assert(plan_ns::classify_object(1) == plan_ns::ObjectConstructionKind::kUnresolved);
    assert(plan_ns::classify_object(6) == plan_ns::ObjectConstructionKind::kUnresolved);
    assert(std::string(plan_ns::construction_kind_name(
            plan_ns::ObjectConstructionKind::kType0SceneActionPair)) == "type0-scene-action-pair");

    assert(resource_ns::select("background.png").kind == resource_ns::Kind::kSpriteFrameByName);
    assert(resource_ns::select("czyanwu1.png").kind == resource_ns::Kind::kDirectFile);
    assert(resource_ns::select("czyanwu3.png").kind == resource_ns::Kind::kDirectFile);
    assert(resource_ns::select("gktianchong.png").kind == resource_ns::Kind::kDirectFile);
    assert(resource_ns::select("gkyuanjing.png").kind == resource_ns::Kind::kDirectFile);
    assert(resource_ns::select("klhuo-1.png").kind == resource_ns::Kind::kSceneActionPair);

    SceneInstance scene;
    scene.source_scene_index = 7u;
    scene.guid = "scene-guid";
    scene.first_point_x = 12.5f;
    scene.first_point_y = -3.0f;

    for (std::size_t i = 0; i < 12u; ++i) {
        LayerInstance layer;
        layer.source_layer_index = 100u + i;
        layer.first_float = static_cast<float>(i) + 0.25f;
        scene.layers.push_back(layer);
    }

    scene.layers[0].objects.push_back(make_object(0, "background.png"));
    scene.layers[0].objects.push_back(make_object(0, "gktianchong.png"));
    scene.layers[0].objects.push_back(make_object(0, "klhuo-1.png"));
    scene.layers[0].objects.push_back(make_object(4, "type-four"));
    scene.layers[3].objects.push_back(make_object(6, "type-six"));
    scene.layers[10].objects.push_back(make_object(1, "type-one"));
    scene.layers[11].objects.push_back(make_object(0, "ignored.png"));
    scene.object_count = 7u;

    const auto plan = plan_ns::build(scene);
    assert(plan.source_scene_index == 7u);
    assert(plan.guid == "scene-guid");
    assert(plan.layers.size() == plan_ns::kLoadingTexLayerSlotCount);
    assert(plan.ignored_source_layer_count == 1u);
    assert(plan.object_count == 6u);

    for (std::size_t z = 0; z < plan.layers.size(); ++z) {
        assert(plan.layers[z].z_index == z);
        assert(plan.layers[z].source_layer_index == 100u + z);
        assert(plan.layers[z].first_float == static_cast<float>(z) + 0.25f);
    }

    assert(plan.layers[0].objects.size() == 4u);
    const auto& cached = plan.layers[0].objects[0];
    assert(cached.construction_kind == plan_ns::ObjectConstructionKind::kType0SpriteBacked);
    assert(cached.type0_resource.has_value());
    assert(cached.type0_resource->kind == resource_ns::Kind::kSpriteFrameByName);
    assert(cached.type0_resource->resource_name == "background.png");
    assert(cached.type0_sprite_transform.has_value());
    assert(cached.type0_sprite_transform->position_x == 10.0f);
    assert(cached.type0_sprite_transform->position_y == 20.0f);
    assert(cached.type0_sprite_transform->rotation == 30.0f);
    assert(cached.type0_sprite_transform->scale_x == 40.0f);
    assert(cached.type0_sprite_transform->scale_y == 50.0f);
    assert(cached.type0_sprite_transform->flip_x);
    assert(cached.type0_sprite_transform->child_z_order == 60);

    const auto& direct = plan.layers[0].objects[1];
    assert(direct.construction_kind == plan_ns::ObjectConstructionKind::kType0SpriteBacked);
    assert(direct.type0_resource->kind == resource_ns::Kind::kDirectFile);
    assert(direct.type0_resource->resource_name == "gktianchong.png");
    assert(direct.type0_sprite_transform.has_value());

    const auto& action_pair = plan.layers[0].objects[2];
    assert(action_pair.construction_kind == plan_ns::ObjectConstructionKind::kType0SceneActionPair);
    assert(action_pair.type0_resource->kind == resource_ns::Kind::kSceneActionPair);
    assert(!action_pair.type0_sprite_transform.has_value());

    const auto& type4 = plan.layers[0].objects[3];
    assert(type4.construction_kind == plan_ns::ObjectConstructionKind::kUnresolved);
    assert(!type4.type0_resource.has_value());
    assert(!type4.type0_sprite_transform.has_value());

    for (const auto& layer : plan.layers) {
        for (const auto& object : layer.objects) {
            assert(object.record.string_value != "ignored.png");
        }
    }

    return 0;
}
