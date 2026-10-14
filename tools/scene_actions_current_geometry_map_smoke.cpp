#include <cassert>
#include <cstdint>

#include "game_scene_direct_asset_requests.h"
#include "game_scene_direct_asset_store.h"
#include "scene_actions_current_geometry_map.h"

int main() {
    namespace current = nevergone::scene_actions_current_geometry_map;
    namespace store = nevergone::game_scene_direct_asset_store;

    nevergone::game_scene_construction_plan::ScenePlan scene;
    scene.source_scene_index = 3u;
    scene.guid = "scene-guid";
    scene.object_count = 2u;

    nevergone::game_scene_construction_plan::LayerPlan layer;
    layer.z_index = 0u;
    layer.source_layer_index = 2u;
    layer.first_float = 0.25f;

    nevergone::game_scene_construction_plan::ObjectPlan source;
    source.source_object_index = 0u;
    source.type_code = 0;
    source.construction_kind =
            nevergone::game_scene_construction_plan::ObjectConstructionKind::kType0SpriteBacked;
    source.type0_resource = nevergone::game_scene_type0_resource::select("gktianchong.png");
    source.type0_sprite_transform = nevergone::game_scene_construction_plan::Type0SpriteTransform{};
    source.record.string_value = "gktianchong.png";
    source.record.conditional_present = true;
    source.record.conditional_first_u32 = 2001u;
    source.record.first_point_x = 100.0f;
    source.record.first_point_y = 50.0f;
    source.record.second_point_x = 1.0f;
    source.record.second_point_y = 1.0f;
    layer.objects.push_back(source);

    nevergone::game_scene_construction_plan::ObjectPlan pair_object;
    pair_object.source_object_index = 1u;
    pair_object.type_code = 0;
    pair_object.construction_kind =
            nevergone::game_scene_construction_plan::ObjectConstructionKind::kType0SceneActionPair;
    pair_object.type0_resource = nevergone::game_scene_type0_resource::select("klhuo-1.png");
    pair_object.record.string_value = "klhuo-1.png";
    pair_object.record.first_point_x = 100.0f;
    pair_object.record.first_point_y = 50.0f;
    pair_object.record.second_point_x = 1.0f;
    pair_object.record.second_point_y = 1.0f;
    layer.objects.push_back(pair_object);
    scene.layers.push_back(layer);

    nevergone::game_scene_render_queue::Queue queue;
    queue.source_scene_index = 3u;
    queue.guid = "scene-guid";
    nevergone::game_scene_render_queue::SpriteCommand sprite;
    sprite.layer_z_index = 0u;
    sprite.source_layer_index = 2u;
    sprite.source_object_index = 0u;
    sprite.resource = *source.type0_resource;
    sprite.direct_asset_relative_path = "gktianchong.png";
    sprite.layer_camera_factor = 0.25f;
    sprite.transform.position_x = 100.0f;
    sprite.transform.position_y = 50.0f;
    sprite.transform.scale_x = 1.0f;
    sprite.transform.scale_y = 1.0f;
    queue.sprites.push_back(sprite);
    queue.direct_file_count = 1u;

    nevergone::game_scene_render_queue::SceneActionPairCommand pair;
    pair.source_layer_index = 2u;
    pair.source_object_index = 1u;
    pair.resource = *pair_object.type0_resource;
    pair.record = pair_object.record;
    queue.scene_action_pairs.push_back(pair);

    const auto requests = nevergone::game_scene_direct_asset_requests::build(queue);
    const std::uint32_t pixels[4] = {
        0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
    };
    store::clear();
    assert(store::begin(requests.revision, 1u));
    assert(store::upload(requests.revision, 0u, 0u, 2, 2, pixels, 4u));
    assert(store::finish(requests.revision));

    auto result = current::build(scene, queue, 40.0f);
    assert(result.status == current::Status::kReady);
    assert(result.objects.size() == 2u);
    assert(result.objects[0].view.event_id_108 == 2001);
    assert(result.objects[0].view.sprite_bounds.has_value());
    assert(result.objects[1].view.resource_name == "klhuo-1.png");
    assert(result.objects[1].view.sprite_bounds.has_value());
    assert(result.objects[0].view.object_position_x == 30.0f);
    assert(result.objects[1].view.object_position_x == 30.0f);

    auto mismatched = scene;
    mismatched.guid = "other-scene";
    result = current::build(mismatched, queue, 40.0f);
    assert(result.status == current::Status::kSceneMismatch);

    store::clear();
    return 0;
}
