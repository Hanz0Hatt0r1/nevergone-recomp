#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>

#include "game_scene_render_queue.h"

namespace {

nevergone::game_scene_construction_plan::ObjectPlan sprite_object(
        std::size_t source_index,
        nevergone::game_scene_type0_resource::Kind resource_kind,
        const std::string& name,
        float x,
        nevergone::game_scene_type0_post_init::Behavior post_init =
                nevergone::game_scene_type0_post_init::Behavior::kNone) {
    namespace plan_ns = nevergone::game_scene_construction_plan;
    plan_ns::ObjectPlan object;
    object.source_object_index = source_index;
    object.type_code = 0;
    object.construction_kind = plan_ns::ObjectConstructionKind::kType0SpriteBacked;
    object.type0_resource = nevergone::game_scene_type0_resource::Selection{resource_kind, name};
    object.type0_post_init = post_init;
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

bool near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    namespace plan_ns = nevergone::game_scene_construction_plan;
    namespace post_ns = nevergone::game_scene_type0_post_init;
    namespace queue_ns = nevergone::game_scene_render_queue;
    namespace resource_ns = nevergone::game_scene_type0_resource;

    // lrand48 normalization and the two recovered initWithData() formula families.
    assert(near(post_ns::normalize_lrand48(0u), 0.0f));
    assert(near(post_ns::normalize_lrand48(0x40000000u), 0.5f));
    assert(post_ns::normalize_lrand48(0x7fffffffu) < 1.0f);

    const auto skew_zero = post_ns::make_random_skew_loop_params(0u, 0u);
    assert(near(skew_zero.segment_duration_seconds, 1.0f));
    assert(near(skew_zero.skew_parameter, 2.0f));
    const auto skew_half =
            post_ns::make_random_skew_loop_params(0x40000000u, 0x40000000u);
    assert(near(skew_half.segment_duration_seconds, 1.75f));
    assert(near(skew_half.skew_parameter, 2.75f));

    const auto fade_zero = post_ns::make_random_fade_loop_params(0u, 0u);
    assert(near(fade_zero.delay_seconds, 6.0f));
    assert(near(fade_zero.fade_duration_seconds, 2.0f));
    assert(fade_zero.low_opacity == 0u);
    assert(fade_zero.high_opacity == 255u);
    const auto fade_half =
            post_ns::make_random_fade_loop_params(0x40000000u, 0x40000000u);
    assert(near(fade_half.delay_seconds, 7.5f));
    assert(fade_half.low_opacity == 12u);
    const auto fade_max =
            post_ns::make_random_fade_loop_params(0x7fffffffu, 0x7fffffffu);
    assert(fade_max.delay_seconds < 9.0f);
    assert(fade_max.delay_seconds > 8.99f);
    assert(fade_max.low_opacity == 25u);

    plan_ns::ScenePlan plan;
    plan.source_scene_index = 4u;
    plan.guid = "render-scene";
    plan.object_count = 8u;

    plan_ns::LayerPlan first;
    first.z_index = 0u;
    first.source_layer_index = 10u;
    first.objects.push_back(sprite_object(0u, resource_ns::Kind::kSpriteFrameByName, "atlas.png", 1.0f));
    first.objects.push_back(sprite_object(
            1u,
            resource_ns::Kind::kSpriteFrameByName,
            "gkshu03.png",
            2.0f,
            post_ns::Behavior::kRandomSkewLoop));
    first.objects.push_back(sprite_object(
            2u,
            resource_ns::Kind::kSpriteFrameByName,
            "gkdeng05.png",
            3.0f,
            post_ns::Behavior::kRandomFadeLoop));

    plan_ns::ObjectPlan action;
    action.source_object_index = 3u;
    action.type_code = 0;
    action.construction_kind = plan_ns::ObjectConstructionKind::kType0SceneActionPair;
    action.type0_resource = resource_ns::Selection{resource_ns::Kind::kSceneActionPair, "klhuo-1.png"};
    first.objects.push_back(action);

    plan_ns::LayerPlan second;
    second.z_index = 1u;
    second.source_layer_index = 20u;
    second.objects.push_back(sprite_object(0u, resource_ns::Kind::kDirectFile, "gktianchong.png", 10.0f));

    auto hidden = sprite_object(1u, resource_ns::Kind::kDirectFile, "empty.png", 15.0f);
    hidden.type0_resource->initially_visible = false;
    second.objects.push_back(hidden);

    plan_ns::ObjectPlan unresolved;
    unresolved.source_object_index = 2u;
    unresolved.type_code = 6;
    unresolved.construction_kind = plan_ns::ObjectConstructionKind::kUnresolved;
    second.objects.push_back(unresolved);

    auto incomplete = sprite_object(3u, resource_ns::Kind::kSpriteFrameByName, "missing-transform.png", 20.0f);
    incomplete.type0_sprite_transform.reset();
    second.objects.push_back(incomplete);

    plan.layers.push_back(first);
    plan.layers.push_back(second);

    const auto queue = queue_ns::build(plan);
    assert(queue.source_scene_index == 4u);
    assert(queue.guid == "render-scene");
    assert(queue.sprites.size() == 4u);
    assert(queue.sprite_frame_lookup_count == 3u);
    assert(queue.direct_file_count == 1u);
    assert(queue.scene_action_pair_count == 1u);
    assert(queue.unresolved_object_count == 2u);

    assert(queue.sprites[0].layer_z_index == 0u);
    assert(queue.sprites[0].source_layer_index == 10u);
    assert(queue.sprites[0].source_object_index == 0u);
    assert(queue.sprites[0].resource.kind == resource_ns::Kind::kSpriteFrameByName);
    assert(queue.sprites[0].resource.resource_name == "atlas.png");
    assert(queue.sprites[0].resource.initially_visible);
    assert(queue.sprites[0].post_init == post_ns::Behavior::kNone);
    assert(!queue.sprites[0].direct_asset_relative_path.has_value());
    assert(queue.sprites[0].transform.position_x == 1.0f);

    assert(queue.sprites[1].resource.resource_name == "gkshu03.png");
    assert(queue.sprites[1].post_init == post_ns::Behavior::kRandomSkewLoop);
    assert(queue.sprites[2].resource.resource_name == "gkdeng05.png");
    assert(queue.sprites[2].post_init == post_ns::Behavior::kRandomFadeLoop);

    assert(queue.sprites[3].layer_z_index == 1u);
    assert(queue.sprites[3].source_layer_index == 20u);
    assert(queue.sprites[3].source_object_index == 0u);
    assert(queue.sprites[3].resource.kind == resource_ns::Kind::kDirectFile);
    assert(queue.sprites[3].resource.resource_name == "gktianchong.png");
    assert(queue.sprites[3].resource.initially_visible);
    assert(queue.sprites[3].post_init == post_ns::Behavior::kNone);
    assert(queue.sprites[3].direct_asset_relative_path.has_value());
    assert(*queue.sprites[3].direct_asset_relative_path == "gktianchong.png");
    assert(queue.sprites[3].transform.position_x == 10.0f);

    return 0;
}
