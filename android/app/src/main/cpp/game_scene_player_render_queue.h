#pragma once

#include <algorithm>
#include <cstdint>

#include "controls_function_state_runtime.h"
#include "game_clock.h"
#include "game_levels_runtime_state.h"
#include "game_scene_camera_contract.h"
#include "game_scene_camera_render_queue.h"
#include "game_scene_enemy_render_queue.h"
#include "game_scene_player_frame_request.h"
#include "game_scene_player_joystick_render_queue.h"
#include "game_scene_player_motion_state.h"
#include "game_scene_player_spawn_state.h"
#include "game_scene_render_queue.h"
#include "me_actions_initial_sprite_transform.h"
#include "me_actions_live_playback.h"
#include "meplayer_live_action_state.h"
#include "player_action_document_store.h"
#include "player_control_runtime_state.h"
#include "player_offline_career.h"

namespace nevergone::game_scene_player_render_queue {

inline game_scene_render_queue::Queue with_live_player(
        game_scene_render_queue::Queue queue,
        const game_scene_player_spawn_state::Snapshot& spawn,
        const enemy_actions_wbg_document::Document* document,
        me_actions_runtime_state::State runtime,
        float camera_eye_x = 0.0f,
        int player_career = 0,
        int player_clothes_id = -1) {
    if (spawn.status != game_scene_player_spawn_state::Status::kReady ||
        spawn.source_scene_index != queue.source_scene_index ||
        spawn.scene_guid != queue.guid || document == nullptr ||
        document->primary_records.empty()) return queue;

    const auto request = game_scene_player_frame_request::build(
            *document, runtime, {spawn.player_x, spawn.player_y});
    if (request.status != game_scene_player_frame_request::Status::kReady ||
        !request.resource.has_value() || !request.lookup.current_frame_in_range) return queue;

    const std::size_t frame_index = request.lookup.selected_primary_index;
    if (frame_index >= document->primary_records.size()) return queue;
    const auto& selected_frame = document->primary_records[frame_index];

    // The recovered MeActionsSystem::updateData() body path reads position,
    // anchor, scale, rotation, flip and child-Z from the *current* primary
    // ActionFrameData, not permanently from frame zero. The project-supported
    // play(int,float) path resets MeActionsSystem +0x204/+0x208 to zero before
    // updateData(), so the existing validated direct-frame transform applies to
    // each selected live frame without inventing extra mutable offsets.
    const auto body_transform =
        me_actions_initial_sprite_transform::build(selected_frame);
    if (!body_transform.has_value()) return queue;

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
    player.resource = *request.resource;
    player.transform.position_x = player_x;
    player.transform.position_y = player_y;
    auto transform = *body_transform;
    const auto control = player_control_runtime_state::snapshot();
    if (control.primary_state.has_value()) {
        transform.flip_x = control.flip_x;
    }
    player.player_body_transform = transform;
    player.player_career = player_career;
    player.player_clothes_id = player_clothes_id;

    if (!selected_frame.first_string.empty()) {
        player.player_body_atlas_plist = selected_frame.first_string;
    }
    player.layer_z_index = 7u;
    player.source_layer_index = 11u;
    player.source_object_index = 0u;
    const auto insertion = std::find_if(queue.sprites.begin(), queue.sprites.end(),
            [](const auto& sprite) { return sprite.layer_z_index > 7u; });
    queue.sprites.insert(insertion, std::move(player));
    ++queue.sprite_frame_lookup_count;
    return queue;
}

inline game_scene_render_queue::Queue with_initial_player(
        game_scene_render_queue::Queue queue,
        const game_scene_player_spawn_state::Snapshot& spawn,
        const enemy_actions_wbg_prefix::ActionFrameRecord* initial_frame,
        float camera_eye_x = 0.0f) {
    if (initial_frame == nullptr) return queue;
    enemy_actions_wbg_document::Document document;
    document.primary_records.push_back(*initial_frame);
    return with_live_player(
        std::move(queue),
        spawn,
        &document,
        me_actions_runtime_state::initial_play_state(),
        camera_eye_x);
}

inline float current_camera_eye_x(
        const game_scene_player_spawn_state::Snapshot& spawn) {
    if (spawn.status != game_scene_player_spawn_state::Status::kReady) return 0.0f;
    const auto port = game_levels_runtime_state::current_port_node_record();
    if (!port.has_value()) return 0.0f;

    const float scene_extent_x = static_cast<float>(
            static_cast<std::int32_t>(port->fifth_u32));
    const auto motion = game_scene_player_motion_state::snapshot();
    const float player_world_x = spawn.player_x +
            (motion.ready && motion.spawn_generation == spawn.generation
                    ? motion.offset_x : 0.0f);
    const auto eye = game_scene_camera_contract::follow_eye_x(
            player_world_x, scene_extent_x);
    return eye.value_or(0.0f);
}

inline game_scene_render_queue::Queue current(game_scene_render_queue::Queue queue) {
    const auto document = player_action_document_store::current_document();
    const auto spawn = game_scene_player_spawn_state::snapshot();
    const float camera_eye_x = current_camera_eye_x(spawn);

    queue = game_scene_camera_render_queue::apply_static_world_camera(
            std::move(queue), camera_eye_x);
    queue = game_scene_enemy_render_queue::current(std::move(queue), camera_eye_x);

    me_actions_runtime_state::State runtime = me_actions_runtime_state::initial_play_state();
    if (document.has_value()) {
        const auto live_action = meplayer_live_action_state::snapshot();
        if (live_action.ready && live_action.loaded_revision != 0u &&
            live_action.current_action_state == live_action.desired_action_state) {
            (void)me_actions_live_playback::sync_and_tick(
                live_action.generation,
                live_action.loaded_revision,
                live_action.current_action_state,
                *document,
                game_clock::tick_count());

            const auto playback = me_actions_live_playback::snapshot();
            if (playback.ready && playback.cursor.timing.done_220) {
                const auto function = controls_function_state_runtime::snapshot();
                (void)meplayer_live_action_state::observe_action_completion(
                    live_action.generation,
                    live_action.loaded_revision,
                    live_action.current_action_state,
                    true,
                    function.function_state);
            }
            runtime = me_actions_live_playback::runtime_state();
        }
    }

    const auto appearance = player_offline_career::current();
    queue = with_live_player(
            std::move(queue),
            spawn,
            document ? &*document : nullptr,
            runtime,
            camera_eye_x,
            static_cast<int>(appearance.career),
            appearance.clothes_id);

    return game_scene_player_joystick_render_queue::current(std::move(queue));
}

}  // namespace nevergone::game_scene_player_render_queue
