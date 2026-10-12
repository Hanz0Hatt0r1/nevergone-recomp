#include <cassert>

#include "game_scene_direct_asset_requests.h"
#include "game_scene_player_motion_state.h"
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
    spawn.generation = 42;
    spawn.source_scene_index = 2;
    spawn.scene_guid = scene.guid;
    spawn.player_x = 600.0f;
    spawn.player_y = 468.0f;
    spawn.source_x = 600.0f;
    spawn.source_y = 300.0f;

    enemy_actions_wbg_prefix::ActionFrameRecord frame;
    frame.first_string = "p01_02_default.plist";
    frame.first_string_length_i32 = 20;
    frame.third_string = "player_001.png";
    frame.third_string_length_i32 = 14;
    frame.float_values[2] = 0.5f;
    frame.float_values[3] = 0.5f;
    frame.float_values[10] = 1.0f;
    frame.float_values[11] = 1.0f;

    player_control_runtime_state::reset();
    game_scene_player_motion_state::reset();
    const auto ready = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(ready.sprites.size() == 3u);
    assert(ready.sprite_frame_lookup_count == 1u);
    assert(ready.sprites[1].resource.resource_name == "player_001.png");
    assert(ready.sprites[1].transform.position_x == 600.0f);
    assert(ready.sprites[1].transform.position_y == 468.0f);
    assert(ready.sprites[1].player_body_transform.has_value());
    assert(ready.sprites[1].player_body_atlas_plist.has_value());
    assert(*ready.sprites[1].player_body_atlas_plist == "p01_02_default.plist");
    assert(!ready.sprites[1].player_body_transform->flip_x);
    assert(ready.sprites[1].layer_z_index == 7u);
    const auto requests = game_scene_direct_asset_requests::build(ready);
    assert(requests.requests.size() == 1u);
    assert(requests.requests[0].sprite_command_index == 1u);
    assert(requests.requests[0].frame_name == "player_001.png");
    assert(requests.requests[0].player_body);
    assert(requests.requests[0].player_atlas_plist == "p01_02_default.plist");

    // Live updateData() uses the currently selected ActionFrameData for the body
    // transform as well as the sprite-frame key. A later frame must therefore
    // carry its own position/anchor/scale/rotation/Z instead of inheriting frame 0.
    enemy_actions_wbg_document::Document live_document;
    live_document.primary_records.push_back(frame);
    auto frame1 = frame;
    frame1.first_string = "p01_03_default.plist";
    frame1.first_string_length_i32 = 20;
    frame1.third_string = "player_002.png";
    frame1.third_string_length_i32 = 14;
    frame1.float_values[0] = 12.0f;
    frame1.float_values[1] = -7.0f;
    frame1.float_values[2] = 0.25f;
    frame1.float_values[3] = 0.75f;
    frame1.float_values[8] = 9.0f;
    frame1.float_values[9] = 15.0f;
    frame1.float_values[10] = 1.25f;
    frame1.float_values[11] = 0.8f;
    live_document.primary_records.push_back(frame1);
    me_actions_runtime_state::State second_frame;
    second_frame.current_frame_1a4 = 1;
    second_frame.previous_frame_1a8 = 0;
    const auto live = game_scene_player_render_queue::with_live_player(
            scene, spawn, &live_document, second_frame);
    assert(live.sprites.size() == 3u);
    assert(live.sprites[1].resource.resource_name == "player_002.png");
    assert(live.sprites[1].player_body_atlas_plist.has_value());
    assert(*live.sprites[1].player_body_atlas_plist == "p01_03_default.plist");
    assert(live.sprites[1].player_body_transform.has_value());
    const auto& live_transform = *live.sprites[1].player_body_transform;
    assert(live_transform.position_x == 12.0f);
    assert(live_transform.position_y == -7.0f);
    assert(live_transform.anchor_x == 0.25f);
    assert(live_transform.anchor_y == 0.75f);
    assert(live_transform.scale_x == 1.25f);
    assert(live_transform.scale_y == 0.8f);
    assert(live_transform.rotation == 15.0f);
    assert(live_transform.child_z_order == 9);

    // BattleManager/player is a GameScene child, not a class-layer object. The
    // original camera therefore contributes the full -eyeX world translation.
    const auto camera_shifted =
            game_scene_player_render_queue::with_initial_player(scene, spawn, &frame, 100.0f);
    assert(camera_shifted.sprites[1].transform.position_x == 500.0f);
    assert(camera_shifted.sprites[1].transform.position_y == 468.0f);

    assert(game_scene_player_motion_state::sync_spawn(spawn.generation, 1u, 100u));
    assert(game_scene_player_motion_state::advance_normal_sample(101u, true, 1.0f, 0.0f));
    const auto moved = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(moved.sprites[1].transform.position_x == 608.0f);
    assert(moved.sprites[1].transform.position_y == 468.0f);
    assert(game_scene_player_motion_state::snapshot().applied_steps == 1u);

    // A second logical tick uses the exact recovered Y scale of 4 units.
    assert(game_scene_player_motion_state::advance_normal_sample(102u, true, 0.0f, 1.0f));
    const auto moved_y = game_scene_player_render_queue::with_initial_player(scene, spawn, &frame);
    assert(moved_y.sprites[1].transform.position_x == 608.0f);
    assert(moved_y.sprites[1].transform.position_y == 472.0f);

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
