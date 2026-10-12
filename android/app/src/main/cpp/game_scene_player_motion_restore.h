#pragma once

#include <cmath>
#include <cstdint>

#include "game_scene_player_motion_state.h"
#include "minimal_runtime_save.h"

namespace nevergone::game_scene_player_motion_state {

// Project-owned persistence hook. It can only restore the cursor that has
// already been bound to the same concrete spawn/player by sync_spawn(); it
// cannot create a player, select a scene, or change the movement domain.
inline bool restore_offset(
        std::uint64_t spawn_generation,
        std::uint32_t player_index,
        std::uint64_t game_tick,
        float offset_x,
        float offset_y) {
    if (!std::isfinite(offset_x) || !std::isfinite(offset_y) ||
        std::fabs(offset_x) > minimal_runtime_save::kMaxAbsMotionOffset ||
        std::fabs(offset_y) > minimal_runtime_save::kMaxAbsMotionOffset) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready ||
        g_snapshot.spawn_generation != spawn_generation ||
        g_snapshot.player_index != player_index) {
        return false;
    }

    g_snapshot.offset_x = offset_x;
    g_snapshot.offset_y = offset_y;
    g_snapshot.last_game_tick = game_tick;
    g_snapshot.applied_steps = 0u;
    g_snapshot.normal_steps = 0u;
    g_snapshot.fast_steps = 0u;
    g_snapshot.last_action_state = player_motion_contract::kNormalMoveActionState;
    ++g_generation;
    g_snapshot.generation = g_generation;
    return true;
}

}  // namespace nevergone::game_scene_player_motion_state
