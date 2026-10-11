#include <cassert>

#include "game_scene_direct_asset_requests.h"
#include "game_scene_player_render_queue.h"

int main() {
    using namespace nevergone;
    game_scene_render_queue::Queue scene;
    scene.source_scene_index = 2;
    scene.guid = "synthetic-scene";
    game_scene_render_queue::SpriteCommand before;
    before.layer_z_index = 6;
    scene.sprites.push_back(before);
    game_scene_render_queue::SpriteCommand after;
    after.layer_z_index = 8;
    scene.sprites.push_back(after);

    game_scene_player_spawn_state::Snapshot spawn;
    spawn.status = game_scene_player_spawn_state::Status::kReady;
    spawn.source_scene_index = 2;
    spawn.scene_guid = scene.guid;
    spawn.player_x = 600.0f;
    spawn.player_y = 468.0f;
    spawn.source_x = 600.0f;
    spawn.source_y = 300.0f;

    enemy_actions_wbg_prefix::ActionFrameRecord frame;
    frame.third_string = "player_001.png";
    frame.third_string_length_i32 = 14;
    frame.float_values[2] = 0.5f;
    frame.float_values[3] = 0.5f;
    frame.float_values[10] = 1.0f;
    frame.float_values[11] = 1.0f;

    const auto ready = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(ready.sprites.size() == 3u);
    assert(ready.sprite_frame_lookup_count == 1u);
    assert(ready.sprites[1].resource.resource_name == "player_001.png");
    assert(ready.sprites[1].transform.position_x == 600.0f);
    assert(ready.sprites[1].transform.position_y == 468.0f);
    assert(ready.sprites[1].player_body_transform.has_value());
    assert(ready.sprites[1].layer_z_index == 7u);
    const auto requests = game_scene_direct_asset_requests::build(ready);
    assert(requests.requests.size() == 1u);
    assert(requests.requests[0].sprite_command_index == 1u);
    assert(requests.requests[0].frame_name == "player_001.png");
    assert(requests.requests[0].player_body);

    auto absent = game_scene_player_render_queue::with_initial_player(scene, spawn, nullptr);
    assert(absent.sprites.size() == 2u);
    spawn.scene_guid = "other";
    absent = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(absent.sprites.size() == 2u);
    spawn.scene_guid = scene.guid;
    frame.third_string = "../unsafe.png";
    frame.third_string_length_i32 = 13;
    absent = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(absent.sprites.size() == 2u);
}
