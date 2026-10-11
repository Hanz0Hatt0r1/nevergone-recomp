#include <cassert>

#include "game_scene_direct_asset_requests.h"
#include "game_scene_player_render_queue.h"
#include "game_scene_player_facing_input.h"

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

    player_control_runtime_state::reset();
    const auto ready = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(ready.sprites.size() == 3u);
    assert(ready.sprite_frame_lookup_count == 1u);
    assert(ready.sprites[1].resource.resource_name == "player_001.png");
    assert(ready.sprites[1].transform.position_x == 600.0f);
    assert(ready.sprites[1].transform.position_y == 468.0f);
    assert(ready.sprites[1].player_body_transform.has_value());
    assert(!ready.sprites[1].player_body_transform->flip_x);
    assert(ready.sprites[1].layer_z_index == 7u);
    const auto requests = game_scene_direct_asset_requests::build(ready);
    assert(requests.requests.size() == 1u);
    assert(requests.requests[0].sprite_command_index == 1u);
    assert(requests.requests[0].frame_name == "player_001.png");
    assert(requests.requests[0].player_body);

    assert(player_control_runtime_state::apply_primary_state(
            player_control_contract::PrimaryControlState::kState12));
    const auto left_facing =
            game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(left_facing.sprites[1].player_body_transform.has_value());
    assert(left_facing.sprites[1].player_body_transform->flip_x);

    assert(player_control_runtime_state::apply_horizontal_sample(1.0f, 0.0f));
    const auto right_facing =
            game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(right_facing.sprites[1].player_body_transform.has_value());
    assert(!right_facing.sprites[1].player_body_transform->flip_x);

    const auto generation = player_control_runtime_state::snapshot().generation;
    assert(!player_control_runtime_state::apply_horizontal_sample(0.01f, 180.0f));
    assert(player_control_runtime_state::snapshot().generation == generation);

    // The Android gesture bridge feeds the same recovered state model.
    game_scene_player_facing_input::reset();
    assert(!game_scene_player_facing_input::on_touch(
            0, 7, 700, 500, 1000, 600, 2, scene.guid));
    assert(game_scene_player_facing_input::on_touch(
            0, 7, 200, 500, 1000, 600, 2, scene.guid));
    assert(game_scene_player_facing_input::on_touch(
            2, 7, 120, 500, 1000, 600, 2, scene.guid));
    assert(player_control_runtime_state::snapshot().primary_state ==
           player_control_contract::PrimaryControlState::kState12);
    assert(!game_scene_player_facing_input::on_touch(
            2, 8, 280, 500, 1000, 600, 2, scene.guid));
    const auto turned = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(turned.sprites[1].player_body_transform->flip_x);
    assert(turned.sprites[1].transform.position_x == ready.sprites[1].transform.position_x);
    assert(game_scene_direct_asset_requests::build(turned).revision == requests.revision);
    assert(game_scene_player_facing_input::on_touch(
            1, 7, 120, 500, 1000, 600, 2, scene.guid));
    assert(game_scene_player_facing_input::on_touch(
            0, 8, 200, 500, 1000, 600, 2, scene.guid));
    assert(game_scene_player_facing_input::on_touch(
            2, 8, 280, 500, 1000, 600, 2, scene.guid));
    assert(player_control_runtime_state::snapshot().primary_state ==
           player_control_contract::PrimaryControlState::kState13);
    assert(game_scene_player_facing_input::on_touch(
            3, 8, 280, 500, 1000, 600, 2, scene.guid));
    assert(game_scene_player_facing_input::on_touch(
            0, 9, 200, 500, 1000, 600, 3, "next-scene"));
    assert(!player_control_runtime_state::snapshot().primary_state.has_value());

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
