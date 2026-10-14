#include <cassert>

#include "scene_actions_static_direct_geometry.h"

int main() {
    namespace collector = nevergone::scene_actions_static_direct_geometry;

    nevergone::game_scene_render_queue::Queue queue;

    nevergone::game_scene_render_queue::SpriteCommand direct;
    direct.layer_z_index = 0u;
    direct.source_layer_index = 2u;
    direct.source_object_index = 4u;
    direct.resource = nevergone::game_scene_type0_resource::select("gktianchong.png");
    direct.direct_asset_relative_path = "gktianchong.png";
    direct.layer_camera_factor = 0.25f;
    direct.transform.position_x = 100.0f;
    direct.transform.position_y = 50.0f;
    direct.transform.scale_x = 1.0f;
    direct.transform.scale_y = 1.0f;
    queue.sprites.push_back(direct);

    nevergone::game_scene_render_queue::SpriteCommand animated = direct;
    animated.source_object_index = 5u;
    animated.resource = nevergone::game_scene_type0_resource::select("czyanwu1.png");
    animated.direct_asset_relative_path = "czyanwu1.png";
    animated.post_init = nevergone::game_scene_type0_post_init::Behavior::kRandomFadeLoop;
    queue.sprites.push_back(animated);

    nevergone::game_scene_render_queue::SpriteCommand trimmed;
    trimmed.source_layer_index = 2u;
    trimmed.source_object_index = 6u;
    trimmed.resource = nevergone::game_scene_type0_resource::select("gkshu01.png");
    queue.sprites.push_back(trimmed);

    collector::AssetSize direct_asset{0u, 20, 40};
    collector::AssetSize animated_asset{1u, 32, 32};
    auto result = collector::build(queue, 40.0f, {direct_asset, animated_asset});
    assert(result.status == collector::Status::kReady);
    assert(result.geometry.size() == 1u);
    assert(result.geometry[0].layer_index == 2u);
    assert(result.geometry[0].object_index == 4u);
    assert(result.geometry[0].object_position_x == 30.0f);
    assert(result.animated_direct_count == 1u);
    assert(result.trimmed_sprite_count == 1u);

    result = collector::build(queue, 40.0f, {animated_asset});
    assert(result.status == collector::Status::kMissingStaticDirectAsset);
    assert(result.geometry.empty());

    auto duplicate = direct_asset;
    result = collector::build(queue, 40.0f, {direct_asset, duplicate});
    assert(result.status == collector::Status::kDuplicateAsset);

    auto orphan = direct_asset;
    orphan.sprite_command_index = 99u;
    result = collector::build(queue, 40.0f, {orphan});
    assert(result.status == collector::Status::kAssetOutsideQueue);

    return 0;
}
