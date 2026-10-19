#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>

#include "game_clock.h"
#include "game_scene_camera_contract.h"
#include "game_scene_render_queue.h"
#include "me_actions_initial_sprite_transform.h"
#include "tutorial_attack_live_gate.h"
#include "tutorial_enemy_action_animation.h"
#include "tutorial_enemy_action_document_store.h"
#include "tutorial_enemy_action_selection.h"
#include "enemy_actions_sprite_update.h"
#include "enemy_actions_hitpoint_sprite_position.h"

namespace nevergone::game_scene_enemy_render_queue {

inline game_scene_render_queue::Queue with_live_enemies(
        game_scene_render_queue::Queue queue,
        const tutorial_attack_live_gate::Snapshot& live,
        std::uint64_t game_tick,
        float camera_eye_x) {
    if (!live.attack.wave_started || live.attack.wave_generation == 0u ||
        live.enemy_bindings.wave_generation != live.attack.wave_generation ||
        live.attack.start_game_tick > game_tick) {
        return queue;
    }

    const auto camera_translation =
            game_scene_camera_contract::world_screen_translation_x(camera_eye_x);
    const float camera_x = camera_translation.value_or(0.0f);
    const double wave_elapsed = static_cast<double>(
            game_tick - live.attack.start_game_tick) * game_clock::kFixedStepSeconds;

    for (const auto& binding : live.enemy_bindings.bindings) {
        if (!binding.active || binding.wave_generation != live.attack.wave_generation ||
            binding.slot >= live.attack.roster.enemies.size()) {
            continue;
        }
        if (binding.enemy_id != 3u) continue;
        const auto* selection = tutorial_enemy_action_selection::for_enemy(binding.enemy_id);
        const auto* state_action = tutorial_enemy_action_selection::for_enemy_state(
                binding.enemy_id, binding.current_enemy_state);
        if (selection == nullptr || state_action == nullptr) continue;
        const auto documents = tutorial_enemy_action_document_store::current(
                binding.enemy_id, binding.current_enemy_state);
        if (!documents) continue;

        const auto& slot = live.attack.roster.enemies[binding.slot];
        if (!slot.materialized) continue;
        const bool enemy_dead = enemy_health_state::is_dead(slot.health);
        const double actor_elapsed = wave_elapsed - slot.request.delay_seconds;
        if (actor_elapsed < 0.0) continue;

        tutorial_enemy_action_animation::Sample animation;
        if (enemy_dead) {
            // tickEnemy() pauses the action system before stepping a dead actor.
            animation = tutorial_enemy_action_animation::sample_action(
                    documents->state_action,
                    documents->state_action_id,
                    documents->state_action_loops,
                    0.0);
        } else if (binding.state_changed) {
            if (binding.state_started_game_tick > game_tick) continue;
            const double state_elapsed = static_cast<double>(
                    game_tick - binding.state_started_game_tick) *
                    game_clock::kFixedStepSeconds;
            animation = tutorial_enemy_action_animation::sample_action(
                    documents->state_action,
                    documents->state_action_id,
                    documents->state_action_loops,
                    state_elapsed);
        } else {
            animation = tutorial_enemy_action_animation::sample(
                    documents->appearance,
                    selection->start_action_id,
                    selection->start_action_loops,
                    documents->state_action,
                    documents->state_action_id,
                    documents->state_action_loops,
                    actor_elapsed);
        }
        if (animation.document == nullptr ||
            animation.frame_index >= animation.document->primary_records.size()) {
            continue;
        }

        const auto& frame = animation.document->primary_records[animation.frame_index];
        const auto action_flags = animation.document == &documents->appearance
                ? selection->start_action_flags
                : state_action->flags;
        enemy_actions_base_sprite_update::SystemInputs system;
        system.field_194 = binding.spawn_x + camera_x;
        system.field_198 = binding.spawn_y;
        // No reconstructed movement producer currently changes the action
        // system's swapBufferPos argument from its play() baseline (0, 0).
        system.field_284_x = 0.0f;
        const auto action_sprite = enemy_actions_sprite_update::build(
                frame, true, 0.0f, system);
        if (!action_sprite.visible || action_sprite.frame_name_68.empty() ||
            frame.first_string.empty()) {
            continue;
        }

        me_actions_initial_sprite_transform::Plan body;
        body.position_x = action_sprite.position_x;
        body.position_y = action_sprite.position_y;
        body.anchor_x = action_sprite.anchor_x;
        body.anchor_y = action_sprite.anchor_y;
        body.scale_x = action_sprite.scale_x;
        body.scale_y = action_sprite.scale_y;
        body.rotation = action_sprite.rotation;
        body.flip_x = action_sprite.flip_x;

        game_scene_render_queue::SpriteCommand command;
        command.resource = {
                game_scene_type0_resource::Kind::kSpriteFrameByName,
                action_sprite.frame_name_68,
                true};
        command.action_frame_transform = body;
        // EnemyActionsSystem type 0 resolves ordinary enemy frames from the
        // scene's shared CCSpriteFrameCache. Its updateData() +0x250==0 branch
        // does not add the +0x60 plist here, so keep lookup on the ordered
        // GameLevels preload list instead of forcing the logical frame plist.
        command.action_frame_opacity = action_sprite.opacity;
        command.enemy_action_not_hit = action_flags.not_hit;
        command.enemy_action_go = action_flags.go;
        command.tutorial_enemy_collision_body =
                binding.enemy_id == 3u && binding.current_enemy_state == 0x1c &&
                !enemy_dead;
        command.enemy_wave_generation = binding.wave_generation;
        command.enemy_id = binding.enemy_id;
        command.enemy_state = binding.current_enemy_state;
        command.enemy_spawn_y = binding.spawn_y;
        // play() initializes EnemyActionsSystem+0x284 to (0, 0). The current
        // queue carries Section-E positions with the known spawn/camera inputs;
        // later movement and EnemyObject offsets remain separate combat inputs.
        const auto hitpoints = enemy_actions_hitpoint_sprite_position::for_frame(
                *animation.document,
                static_cast<std::int32_t>(animation.frame_index), system);
        for (const auto& hitpoint : hitpoints) {
            queue.enemy_action_hitpoints.push_back({
                    binding.slot,
                    static_cast<std::int32_t>(animation.frame_index),
                    hitpoint.action_data_index,
                    hitpoint.frame_ordinal,
                    hitpoint.x,
                    hitpoint.y,
                    action_flags.not_hit,
                    action_flags.go});
        }
        command.layer_z_index = 7u;
        command.source_layer_index = 11u;
        command.source_object_index = binding.slot;

        const auto insertion = std::find_if(
                queue.sprites.begin(), queue.sprites.end(),
                [](const auto& sprite) { return sprite.layer_z_index > 7u; });
        queue.sprites.insert(insertion, std::move(command));
        ++queue.sprite_frame_lookup_count;
        ++queue.tutorial_enemy_action_sprite_count;
    }

    return queue;
}

inline game_scene_render_queue::Queue current(
        game_scene_render_queue::Queue queue,
        float camera_eye_x) {
    const auto live = tutorial_attack_live_gate::live_snapshot();
    if (!live.attack.wave_started || live.enemy_bindings.active_count == 0u) return queue;

    return with_live_enemies(
            std::move(queue), live, game_clock::tick_count(), camera_eye_x);
}

}  // namespace nevergone::game_scene_enemy_render_queue
