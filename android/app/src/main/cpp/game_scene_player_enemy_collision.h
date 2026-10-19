#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <vector>

#include "battle_manager_player_intersection_contract.h"
#include "enemy_health_state.h"
#include "enemy_hl_position_contract.h"
#include "game_scene_player_hl_point_state.h"
#include "game_scene_player_motion_state.h"
#include "player_control_runtime_state.h"
#include "player_collision_state_contract.h"
#include "player_motion_contract.h"
#include "tutorial_attack_live_gate.h"
#include "tutorial_player_face_dir_provider.h"

namespace nevergone::game_scene_player_enemy_collision {

struct Resolution {
    bool ready = false;
    bool intersects = false;
    bool corrected = false;
    std::size_t enemy_slot = 0;
    player_motion_contract::Vector movement;
};

// Connects only the recovered Lesson Attack enemy-3 idle/back state. Its
// EnemyObject +0x248 remains zero for action 28 (goValue 3), so enemyHLPos()
// returns the spawn point stored at +0x280. Other enemy states remain closed
// until their actor offsets and movement producers are reconstructed.
inline Resolution resolve_attack_motion(
        const game_scene_player_hl_point_state::Snapshot& player,
        const game_scene_player_motion_state::Snapshot& motion,
        const player_control_runtime_state::Snapshot& control,
        std::uint32_t player_action_state,
        player_motion_contract::Vector proposed_movement,
        const tutorial_attack_live_gate::Snapshot& live) noexcept {
    Resolution result;
    result.movement = proposed_movement;

    if (!player.ready || !live.attack.wave_started || live.attack.wave_generation == 0u ||
            live.attack.roster.generation != live.attack.wave_generation ||
            live.enemy_bindings.wave_generation != live.attack.wave_generation ||
            player.spawn_generation != motion.spawn_generation ||
            player.player_index != motion.player_index ||
            player.game_tick != motion.last_game_tick ||
            player.action_state != player_action_state ||
            player.player_index < 1u || player.player_index > 3u ||
            !std::isfinite(proposed_movement.x) || !std::isfinite(proposed_movement.y)) {
        return result;
    }

    const auto face = tutorial_player_face_dir_provider::resolve(control);
    if (!face.ready() || face.face_dir.control_generation != control.generation) return result;

    std::vector<battle_manager_player_intersection_contract::Enemy> enemies;
    std::vector<std::size_t> enemy_slots;
    for (const auto& binding : live.enemy_bindings.bindings) {
        if (!binding.active || binding.wave_generation != live.attack.wave_generation ||
                binding.slot >= live.attack.roster.planned_count) {
            continue;
        }

        const auto& slot = live.attack.roster.enemies[binding.slot];
        if (!slot.materialized) continue;
        if (binding.enemy_id != 3u || binding.initial_enemy_state != 0x1c ||
                binding.current_enemy_state != 0x1c ||
                slot.health.enemy_state != 0x1c ||
                binding.spawn_x != slot.request.x || binding.spawn_y != slot.request.y) {
            return result;
        }
        if (enemy_health_state::is_dead(slot.health)) continue;

        // initWithID stores enemy ID 3 at +0x2a0 and initializes +0x31f to
        // zero. The reviewed action28 goValue=3 leaves +0x248 at its initialized
        // zero value, so isFly() is false and enemyHLPos() selects +0x280.
        const auto origin = enemy_hl_position_contract::origin_hit_point(
                0x1cu,
                0,
                {},
                {binding.spawn_x, binding.spawn_y},
                {});
        if (!origin.has_value()) return result;

        battle_manager_player_intersection_contract::Enemy enemy;
        enemy.dead = false;
        enemy.state = 0x1c;
        enemy.byte_31f_nonzero = false;
        enemy.flying = false;
        enemy.origin_hit_point = {origin->x, origin->y};
        enemies.push_back(enemy);
        enemy_slots.push_back(binding.slot);
    }

    result.ready = true;
    const auto intersection = battle_manager_player_intersection_contract::resolve(
            {player.player_hl_point.x, player.player_hl_point.y},
            proposed_movement.x,
            face.face_dir.value,
            enemies);
    if (!intersection.intersects) return result;

    result.intersects = true;
    result.enemy_slot = enemy_slots[intersection.enemy_index];
    // The reconstructed tutorial has no incoming-hit writer for MEPlayer+0x3e8.
    if (!player_collision_state_contract::is_collision_state(
                player.player_index, player_action_state, false)) {
        return result;
    }
    if (!intersection.side_flag_1e0 && !intersection.side_flag_1e1) return result;

    const float candidate_x = player.player_hl_point.x + proposed_movement.x;
    if (!(candidate_x < intersection.target_point.x)) return result;

    result.movement.x = intersection.target_point.x - player.player_hl_point.x;
    result.corrected = result.movement.x != proposed_movement.x;
    return result;
}

}  // namespace nevergone::game_scene_player_enemy_collision
