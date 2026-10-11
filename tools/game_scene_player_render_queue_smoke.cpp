#include <cassert>

#include "game_scene_direct_asset_requests.h"
#include "game_scene_player_render_queue.h"

int main() {
    using namespace nevergone;
    game_scene_render_queue::Queue scene;
    scene.source_scene_index = 2;
    scene.guid = "synthetic-scene";

    game_scene_player_spawn_state::Snapshot spawn;
    spawn.status = game_scene_player_spawn_state::Status::kReady;
    spawn.source_scene_index = 2;
    spawn.scene_guid = scene.guid;
    spawn.player_x = 32.0f;
    spawn.player_y = 20.0f;
    spawn.source_x = 600.0f;
    spawn.source_y = 300.0f;

    enemy_actions_wbg_prefix::ActionFrameRecord frame;
    frame.third_string = "player_001.png";
    frame.third_string_length_i32 = 14;

    const auto ready = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(ready.sprites.size() == 1u);
    assert(ready.sprite_frame_lookup_count == 1u);
    assert(ready.sprites[0].resource.resource_name == "player_001.png");
    assert(ready.sprites[0].transform.position_x == 600.0f);
    assert(ready.sprites[0].transform.position_y == 340.0f);
    const auto requests = game_scene_direct_asset_requests::build(ready);
    assert(requests.requests.size() == 1u);
    assert(requests.requests[0].sprite_command_index == 0u);
    assert(requests.requests[0].frame_name == "player_001.png");

    auto absent = game_scene_player_render_queue::with_initial_player(scene, spawn, nullptr);
    assert(absent.sprites.empty());
    spawn.scene_guid = "other";
    absent = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(absent.sprites.empty());
    spawn.scene_guid = scene.guid;
    frame.third_string = "../unsafe.png";
    frame.third_string_length_i32 = 13;
    absent = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(absent.sprites.empty());
}
