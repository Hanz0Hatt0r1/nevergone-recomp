#pragma once

#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>

#include "player_motion_contract.h"

namespace nevergone::game_scene_player_motion_state {

struct Snapshot {
    bool ready = false;
    std::uint64_t generation = 0;
    std::uint64_t spawn_generation = 0;
    std::uint32_t player_index = 0;
    std::uint64_t last_game_tick = 0;
    std::uint64_t applied_steps = 0;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

inline std::mutex g_mutex;
inline std::uint64_t g_generation = 0;
inline Snapshot g_snapshot;

inline void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_generation;
    g_snapshot = {};
    g_snapshot.generation = g_generation;
}

// Binds the live movement cursor to one recovered player spawn. A new spawn
// generation clears accumulated motion. Player indices outside the statically
// proven playerMoveValue normal-movement domain (1..3) fail closed.
inline bool sync_spawn(
        std::uint64_t spawn_generation,
        std::uint32_t player_index,
        std::uint64_t game_tick) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (player_index < 1u || player_index > 3u) {
        if (g_snapshot.ready) {
            ++g_generation;
            g_snapshot = {};
            g_snapshot.generation = g_generation;
        }
        return false;
    }

    if (g_snapshot.ready &&
        g_snapshot.spawn_generation == spawn_generation &&
        g_snapshot.player_index == player_index) {
        return true;
    }

    ++g_generation;
    g_snapshot = {};
    g_snapshot.ready = true;
    g_snapshot.generation = g_generation;
    g_snapshot.spawn_generation = spawn_generation;
    g_snapshot.player_index = player_index;
    g_snapshot.last_game_tick = game_tick;
    return true;
}

// Advances the movement cursor to the supplied fixed game tick. Inactive input
// consumes elapsed ticks without moving so a later joystick activation cannot
// accumulate an artificial backlog. Active input applies the recovered normal
// action-state movement vector once per elapsed logical tick.
inline bool advance_normal_sample(
        std::uint64_t game_tick,
        bool active,
        float direction_x,
        float direction_y) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready) return false;
    if (game_tick <= g_snapshot.last_game_tick) return true;

    const std::uint64_t elapsed = game_tick - g_snapshot.last_game_tick;
    g_snapshot.last_game_tick = game_tick;
    if (!active) return true;

    const auto movement = player_motion_contract::movement_value(
            g_snapshot.player_index,
            player_motion_contract::kNormalMoveActionState,
            {direction_x, direction_y});
    if (!movement.has_value()) return false;

    g_snapshot.offset_x += movement->x * static_cast<float>(elapsed);
    g_snapshot.offset_y += movement->y * static_cast<float>(elapsed);
    g_snapshot.applied_steps += elapsed;
    ++g_generation;
    g_snapshot.generation = g_generation;
    return true;
}

inline Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_snapshot;
}

inline std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameScene player motion\n";
    out << "ready: " << (state.ready ? "true" : "false") << "\n";
    out << "generation: " << state.generation << "\n";
    out << "spawn generation: " << state.spawn_generation << "\n";
    out << "player index: " << state.player_index << "\n";
    out << "last game tick: " << state.last_game_tick << "\n";
    out << "applied steps: " << state.applied_steps << "\n";
    out << "offset: " << state.offset_x << "," << state.offset_y << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_player_motion_state
