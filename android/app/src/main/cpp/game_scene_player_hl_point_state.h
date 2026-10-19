#pragma once

#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>

#include "player_hl_point_contract.h"

namespace nevergone::game_scene_player_hl_point_state {

struct Snapshot {
    bool ready = false;
    bool spawn_bound = false;
    std::uint64_t generation = 0;
    std::uint64_t spawn_generation = 0;
    std::uint64_t game_tick = 0;
    std::uint32_t player_index = 0;
    std::uint32_t action_state = 0;
    player_hl_point_contract::Rect action_sprite_bounds;
    player_hl_point_contract::Rect player_bounds;
    player_hl_point_contract::Point player_hl_point;
    player_hl_point_contract::MovementBoundsCache movement_bounds_cache;
};

inline std::mutex g_mutex;
inline Snapshot g_snapshot;
inline std::uint64_t g_generation = 0;

inline void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_generation;
    g_snapshot = {};
    g_snapshot.generation = g_generation;
}

inline void invalidate() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready) return;
    g_snapshot.ready = false;
    ++g_generation;
    g_snapshot.generation = g_generation;
}

// Rebuilds the native MEPlayer::playerBoundingBox/updateHLValue chain for a
// successfully rendered live player frame. Player index is MEPlayer+0x144,
// read directly by playerIdx(); the point inputs are the recovered native
// fields +0x240, +0x248 and +0x268.
inline bool update(
        std::uint64_t spawn_generation,
        std::uint32_t player_index,
        std::uint32_t action_state,
        std::uint64_t game_tick,
        player_hl_point_contract::Rect action_sprite_bounds,
        player_hl_point_contract::Point position_240,
        player_hl_point_contract::Point offset_248,
        player_hl_point_contract::Point position_268) {
    if (spawn_generation == 0u || player_index < 1u || player_index > 5u) {
        invalidate();
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.spawn_bound ||
        g_snapshot.spawn_generation != spawn_generation ||
        g_snapshot.player_index != player_index) {
        ++g_generation;
        g_snapshot = {};
        g_snapshot.spawn_bound = true;
        g_snapshot.spawn_generation = spawn_generation;
        g_snapshot.player_index = player_index;
        g_snapshot.generation = g_generation;
    }

    const auto bounds = player_hl_point_contract::player_bounds(
            action_sprite_bounds,
            action_state,
            position_240,
            offset_248,
            position_268,
            g_snapshot.movement_bounds_cache);
    if (!bounds.has_value()) {
        g_snapshot.ready = false;
        ++g_generation;
        g_snapshot.generation = g_generation;
        return false;
    }

    const auto point = player_hl_point_contract::update_hl_point(
            bounds->bounds,
            action_state,
            player_index,
            g_snapshot.player_hl_point);
    if (!point.has_value()) {
        g_snapshot.ready = false;
        ++g_generation;
        g_snapshot.generation = g_generation;
        return false;
    }

    g_snapshot.ready = true;
    g_snapshot.game_tick = game_tick;
    g_snapshot.action_state = action_state;
    g_snapshot.action_sprite_bounds = action_sprite_bounds;
    g_snapshot.player_bounds = bounds->bounds;
    g_snapshot.player_hl_point = *point;
    g_snapshot.movement_bounds_cache = bounds->movement_cache;
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
    out << "Player hit-point anchor\n";
    out << "ready: " << (state.ready ? "true" : "false") << "\n";
    out << "generation: " << state.generation << "\n";
    out << "spawn generation: " << state.spawn_generation << "\n";
    out << "player index: " << state.player_index << "\n";
    out << "action state: 0x" << std::hex << state.action_state << std::dec << "\n";
    out << "game tick: " << state.game_tick << "\n";
    out << "action sprite bounds: " << state.action_sprite_bounds.x << ","
        << state.action_sprite_bounds.y << "," << state.action_sprite_bounds.width
        << "," << state.action_sprite_bounds.height << "\n";
    out << "player bounds: " << state.player_bounds.x << ","
        << state.player_bounds.y << "," << state.player_bounds.width
        << "," << state.player_bounds.height << "\n";
    out << "player HL point: " << state.player_hl_point.x << ","
        << state.player_hl_point.y << "\n";
    out << "movement bounds cached: "
        << (state.movement_bounds_cache.valid ? "true" : "false") << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_player_hl_point_state
