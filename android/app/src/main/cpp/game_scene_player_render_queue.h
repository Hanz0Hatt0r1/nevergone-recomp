#pragma once

#include "game_scene_player_frame_request.h"
#include "game_scene_player_spawn_state.h"
#include "game_scene_render_queue.h"
#include "player_action_document_store.h"

namespace nevergone::game_scene_player_render_queue {

inline game_scene_render_queue::Queue with_initial_player(
        game_scene_render_queue::Queue queue,
        const game_scene_player_spawn_state::Snapshot& spawn,
        const enemy_actions_wbg_prefix::ActionFrameRecord* initial_frame) {
    if (spawn.status != game_scene_player_spawn_state::Status::kReady ||
        spawn.source_scene_index != queue.source_scene_index ||
        spawn.scene_guid != queue.guid || initial_frame == nullptr) return queue;

    enemy_actions_wbg_document::Document document;
    document.primary_records.push_back(*initial_frame);
    game_scene_player_frame_request::State state;
    state.action_document_ready = true;
    state.current_frame_1a4 = 0;  // MeActionsSystem::initWithFile and play().
    const auto request = game_scene_player_frame_request::build(
            document, state, {spawn.player_x, spawn.player_y});
    if (request.status != game_scene_player_frame_request::Status::kReady ||
        !request.resource.has_value()) return queue;

    game_scene_render_queue::SpriteCommand player;
    player.resource = *request.resource;
    // The source marker and the proven GameScene Y correction locate the
    // sprite in the same design space as the static GameScene objects.
    player.transform.position_x = spawn.source_x;
    player.transform.position_y = spawn.source_y + 40.0f;
    player.layer_z_index = 11u;
    player.source_layer_index = 11u;
    player.source_object_index = 0u;
    queue.sprites.push_back(std::move(player));
    ++queue.sprite_frame_lookup_count;
    return queue;
}

inline game_scene_render_queue::Queue current(game_scene_render_queue::Queue queue) {
    const auto frame = player_action_document_store::initial_frame_record();
    const auto spawn = game_scene_player_spawn_state::snapshot();
    return with_initial_player(std::move(queue), spawn, frame ? &*frame : nullptr);
}

}  // namespace nevergone::game_scene_player_render_queue
