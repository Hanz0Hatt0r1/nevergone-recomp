#pragma once

#include <algorithm>
#include <cstdint>

#include "controls_function_state_runtime.h"
#include "game_clock.h"
#include "game_scene_direct_geometry.h"
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
#include "player_attack_state_contract.h"
#include "player_control_runtime_state.h"
#include "player_offline_career.h"

namespace nevergone::game_scene_player_render_queue {

constexpr int kHitPointSpriteSize = 8;

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

    const auto live_action = meplayer_live_action_state::snapshot();
    const bool attack_state = live_action.ready &&
            live_action.spawn_generation == spawn.generation &&
            live_action.loaded_revision != 0u &&
            live_action.current_action_state == live_action.desired_action_state &&
            player_attack_state_contract::is_attack_state(
                    live_action.current_action_state);
    const auto control = player_control_runtime_state::snapshot();
    const bool system_flipped = control.primary_state.has_value() && control.flip_x;

    // Section-E AFDs supply hitPoint sprite transforms. Recover the actual
    // bounds center rather than treating the anchor floats as a position.
    for (const auto& hitpoint : request.lookup.current_frame_hitpoints) {
        if (hitpoint.action_data_index >= document->fixed_tail_block.records.size()) {
            continue;
        }
        const auto& record = document->fixed_tail_block.records[hitpoint.action_data_index];
        const float scale = runtime.action_scale_2f8;
        me_actions_initial_sprite_transform::Plan transform;
        transform.position_x =
                (record.float_values[0] + runtime.action_position_x_204) / scale;
        transform.position_y =
                (record.float_values[1] + runtime.action_position_y_208) / scale -
                runtime.hitpoint_y_offset_648;
        transform.anchor_x = record.float_values[2];
        transform.anchor_y = record.float_values[3];
        transform.scale_x = record.float_values[9];
        transform.scale_y = 1.0f;
        transform.flip_x = system_flipped;
        if (system_flipped) {
            const float pivot_x = record.float_values[6] / scale;
            const float pivot_y = record.float_values[7] / scale;
            const float delta_x = pivot_x - transform.position_x;
            const float delta_y = pivot_y - transform.position_y;
            transform.position_x = pivot_x + delta_x;
            transform.position_y = pivot_y - delta_y;
            transform.anchor_x = 0.5f - (transform.anchor_x - 0.5f);
            transform.scale_x = -transform.scale_x;
        }
        // swapBufferPos applies its independent system+0x2ec translation
        // after updateData has written the action-space position.
        transform.position_x -= runtime.system_offset_x_2ec;
        transform.position_y -= runtime.system_offset_y_2f0;
        const auto bounds = game_scene_direct_geometry::player_action_sprite_bounds(
                transform, kHitPointSpriteSize, kHitPointSpriteSize);
        if (!bounds.has_value()) continue;
        queue.player_action_hitpoints.push_back({
                runtime.current_frame_1a4,
                live_action.generation,
                live_action.loaded_revision,
                hitpoint.action_data_index,
                hitpoint.frame_ordinal,
                bounds->x + bounds->width * 0.5f,
                bounds->y + bounds->height * 0.5f,
                attack_state});
    }

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
    if (control.primary_state.has_value()) {
        transform.flip_x = control.flip_x;
    }
    player.player_body_transform = transform;
    player.player_career = player_career;
    player.player_clothes_id = player_clothes_id;
    player.player_name = spawn.player_name;

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
                live_action.current_action_looping,
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
