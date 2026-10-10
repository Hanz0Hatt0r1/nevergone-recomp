#include <cassert>
#include <string>

#include "game_scene_render_queue.h"

namespace {

nevergone::game_scene_construction_plan::ObjectPlan sprite_object(
        std::size_t source_index,
        nevergone::game_scene_type0_resource::Kind resource_kind,
        const std::string& name,
        float x) {
    namespace plan_ns = nevergone::game_scene_construction_plan;
    plan_ns::ObjectPlan object;
    object.source_object_index = source_index;
    object.type_code = 0;
    object.construction_kind = plan_ns::ObjectConstructionKind::kType0SpriteBacked;
    object.type0_resource = nevergone::game_scene_type0_resource::Selection{resource_kind, name};
    plan_ns::Type0SpriteTransform transform;
    transform.position_x = x;
    transform.position_y = x + 1.0f;
    transform.rotation = x + 2.0f;
    transform.scale_x = x + 3.0f;
    transform.scale_y = x + 4.0f;
    transform.flip_x = true;
    transform.child_z_order = static_cast<std::int32_t>(x + 5.0f);
    object.type0_sprite_transform = transform;
    return object;
}

}  // namespace

int main() {
    namespace plan_ns = nevergone::game_scene_construction_plan;
    namespace queue_ns = nevergone::game_scene_render_queue;
    namespace resource_ns = nevergone::game_scene_type0_resource;

    plan_ns::ScenePlan plan;
    plan.source_scene_index = 4u;
    plan.guid = "render-scene";
    plan.object_count = 5u;

    plan_ns::LayerPlan first;
    first.z_index = 0u;
    first.source_layer_index = 10u;
    first.objects.push_back(sprite_object(0u, resource_ns::Kind::kSpriteFrameByName, "atlas.png", 1.0f));

    plan_ns::ObjectPlan action;
    action.source_object_index = 1u;
    action.type_code = 0;
    action.construction_kind = plan_ns::ObjectConstructionKind::kType0SceneActionPair;
    action.type0_resource = resource_ns::Selection{resource_ns::Kind::kSceneActionPair, "klhuo-1.png"};
    first.objects.push_back(action);

    plan_ns::LayerPlan second;
    second.z_index = 1u;
    second.source_layer_index = 20u;
    second.objects.push_back(sprite_object(0u, resource_ns::Kind::kDirectFile, "gktianchong.png", 10.0f));

    plan_ns::ObjectPlan unresolved;
    unresolved.source_object_index = 1u;
    unresolved.type_code = 6;
    unresolved.construction_kind = plan_ns::ObjectConstructionKind::kUnresolved;
    second.objects.push_back(unresolved);

    auto incomplete = sprite_object(2u, resource_ns::Kind::kSpriteFrameByName, "missing-transform.png", 20.0f);
    incomplete.type0_sprite_transform.reset();
    second.objects.push_back(incomplete);

    plan.layers.push_back(first);
    plan.layers.push_back(second);

    const auto queue = queue_ns::build(plan);
    assert(queue.source_scene_index == 4u);
    assert(queue.guid == "render-scene");
    assert(queue.sprites.size() == 2u);
    assert(queue.sprite_frame_lookup_count == 1u);
    assert(queue.direct_file_count == 1u);
    assert(queue.scene_action_pair_count == 1u);
    assert(queue.unresolved_object_count == 2u);

    assert(queue.sprites[0].layer_z_index == 0u);
    assert(queue.sprites[0].source_layer_index == 10u);
    assert(queue.sprites[0].source_object_index == 0u);
    assert(queue.sprites[0].resource.kind == resource_ns::Kind::kSpriteFrameByName);
    assert(queue.sprites[0].resource.resource_name == "atlas.png");
    assert(!queue.sprites[0].direct_asset_relative_path.has_value());
    assert(queue.sprites[0].transform.position_x == 1.0f);

    assert(queue.sprites[1].layer_z_index == 1u);
    assert(queue.sprites[1].source_layer_index == 20u);
    assert(queue.sprites[1].source_object_index == 0u);
    assert(queue.sprites[1].resource.kind == resource_ns::Kind::kDirectFile);
    assert(queue.sprites[1].resource.resource_name == "gktianchong.png");
    assert(queue.sprites[1].direct_asset_relative_path.has_value());
    assert(*queue.sprites[1].direct_asset_relative_path ==
           "gamescene/gs_res_image_file/gktianchong.png");
    assert(queue.sprites[1].transform.position_x == 10.0f);

    return 0;
}
