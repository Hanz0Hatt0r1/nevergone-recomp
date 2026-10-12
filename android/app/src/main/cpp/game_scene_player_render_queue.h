#pragma once

#include <algorithm>
#include <cstdint>

#include "game_levels_runtime_state.h"
#include "game_scene_camera_contract.h"
#include "game_scene_camera_render_queue.h"
#include "game_scene_initial_player_sprite_request.h"
#include "game_scene_player_joystick_render_queue.h"
#include "game_scene_player_motion_state.h"
#include "game_scene_player_spawn_state.h"
#include "game_scene_render_queue.h"
#include "player_action_document_store.h"
#include "player_control_runtime_state.h"

namespace nevergone::game_scene_player_render_queue {

inline game_scene_render_queue::Queue with_initial_player(
        game_scene_render_queue::Queue queue,
        const game_scene_player_spawn_state::Snapshot& spawn,
        const enemy_actions_wbg_prefix::ActionFrameRecord* initial_frame,
        float camera_eye_x = 0.0f) {
    if (spawn.status != game_scene_player_spawn_state::Status::kReady ||
        spawn.source_scene_index != queue.source_scene_index ||
        spawn.scene_guid != queue.guid || initial_frame == nullptr) return queue;

    enemy_actions_wbg_document::Document document;
    document.primary_records.push_back(*initial_frame);
    const auto request = game_scene_initial_player_sprite_request::build(
            document, {spawn.player_x, spawn.player_y});
    if (request.status != game_scene_initial_player_sprite_request::Status::kReady ||
        !request.frame_request.resource.has_value() ||
        !request.sprite_transform.has_value()) return queue;

    float player_x = spawn.player_x;
    float player_y = spawn.player_y;
    const auto motion = game_scene_player_motion_state::snapshot();
    if (motion.ready && motion.spawn_generation == spawn.generation) {
        player_x += motion.offset_x;
        player_y += motion.offset_y;
    }
    const auto camera_translation =
            game_scene_camera_contract::world_screen_translation_x(camera_eye_x);
    if (camera_translation.has_value()) player_x += *camera_translation;

    game_scene_render_queue::SpriteCommand player;
    player.resource = *request.frame_request.resource;
    player.transform.position_x = player_x;
    player.transform.position_y = player_y;
    auto body_transform = *request.sprite_transform;
    const auto control = player_control_runtime_state::snapshot();
    if (control.primary_state.has_value()) {
        // MEPlayer::filpPlayer(bool, ...) forwards the recovered facing flag to
        // MeActionsSystem::filpAction(bool), which calls CCSprite::setFlipX on
        // the body sprite without changing its local frame geometry.
        body_transform.flip_x = control.flip_x;
    }
    player.player_body_transform = body_transform;
    // BattleManager is a GameScene child at z=7. Same-z scene children were
    // added earlier, so place this branch after layer 7 and before layer 8.
    player.layer_z_index = 7u;
    player.source_layer_index = 11u;
    player.source_object_index = 0u;
    const auto insertion = std::find_if(queue.sprites.begin(), queue.sprites.end(),
            [](const auto& sprite) { return sprite.layer_z_index > 7u; });
    queue.sprites.insert(insertion, std::move(player));
    ++queue.sprite_frame_lookup_count;
    return queue;
}

inline float current_initial_camera_eye_x(
        const game_scene_player_spawn_state::Snapshot& spawn) {
    if (spawn.status != game_scene_player_spawn_state::Status::kReady) return 0.0f;
    const auto port = game_levels_runtime_state::current_port_node_record();
    if (!port.has_value()) return 0.0f;

    // loadingTex() converts GameScenePortNodeData +0x4b4 through signed-int to
    // float before subtracting half the win width.
    const float scene_extent_x = static_cast<float>(
            static_cast<std::int32_t>(port->fifth_u32));
    const auto eye = game_scene_camera_contract::initial_eye_x(
            spawn.source_x, scene_extent_x);
    return eye.value_or(0.0f);
}

inline game_scene_render_queue::Queue current(game_scene_render_queue::Queue queue) {
    const auto frame = player_action_document_store::initial_frame_record();
    const auto spawn = game_scene_player_spawn_state::snapshot();
    const float camera_eye_x = current_initial_camera_eye_x(spawn);

    // The shipped scene does not draw serialized object coordinates directly.
    // updateCameraTrack() first moves every GameSceneObject through its class
    // layer and the GameScene camera then translates the whole world. Flatten
    // those two transforms before inserting BattleManager/player content.
    queue = game_scene_camera_render_queue::apply_static_world_camera(
            std::move(queue), camera_eye_x);
    queue = with_initial_player(
            std::move(queue), spawn, frame ? &*frame : nullptr, camera_eye_x);

    // Controls owns the fixed joystick outside the serialized GameScene world
    // layers. Append it after camera composition so it remains screen-space.
    return game_scene_player_joystick_render_queue::current(std::move(queue));
}

}  // namespace nevergone::game_scene_player_render_queue
