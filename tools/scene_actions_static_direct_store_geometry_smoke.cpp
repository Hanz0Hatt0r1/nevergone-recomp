#include <cassert>
#include <cstdint>

#include "game_scene_direct_asset_requests.h"
#include "game_scene_direct_asset_store.h"
#include "scene_actions_static_direct_store_geometry.h"

int main() {
    namespace runtime = nevergone::scene_actions_static_direct_store_geometry;
    namespace store = nevergone::game_scene_direct_asset_store;

    nevergone::game_scene_render_queue::Queue queue;
    queue.source_scene_index = 3u;
    queue.guid = "scene-guid";

    nevergone::game_scene_render_queue::SpriteCommand command;
    command.layer_z_index = 0u;
    command.source_layer_index = 2u;
    command.source_object_index = 4u;
    command.resource = nevergone::game_scene_type0_resource::select("gktianchong.png");
    command.direct_asset_relative_path = "gktianchong.png";
    command.layer_camera_factor = 0.25f;
    command.transform.position_x = 100.0f;
    command.transform.position_y = 50.0f;
    command.transform.scale_x = 1.0f;
    command.transform.scale_y = 1.0f;
    queue.sprites.push_back(command);
    queue.direct_file_count = 1u;

    const auto requests = nevergone::game_scene_direct_asset_requests::build(queue);
    assert(requests.requests.size() == 1u);

    store::clear();
    auto result = runtime::capture(queue, 40.0f);
    assert(result.status == runtime::Status::kStoreUnavailable);

    const std::uint32_t pixels[4] = {
        0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
    };
    assert(store::begin(requests.revision, 1u));
    assert(store::upload(requests.revision, 0u, 0u, 2, 2, pixels, 4u));
    assert(store::finish(requests.revision));

    result = runtime::capture(queue, 40.0f);
    assert(result.status == runtime::Status::kReady);
    assert(result.collector.geometry.size() == 1u);
    assert(result.collector.geometry[0].layer_index == 2u);
    assert(result.collector.geometry[0].object_index == 4u);
    assert(result.collector.geometry[0].object_position_x == 30.0f);

    auto other_queue = queue;
    other_queue.guid = "other-scene";
    result = runtime::capture(other_queue, 40.0f);
    assert(result.status == runtime::Status::kRevisionMismatch);

    store::clear();
    assert(store::begin(requests.revision, 1u));
    assert(store::upload(requests.revision, 0u, 7u, 2, 2, pixels, 4u));
    assert(store::finish(requests.revision));
    result = runtime::capture(queue, 40.0f);
    assert(result.status == runtime::Status::kAssetIdentityMismatch);

    store::clear();
    return 0;
}
