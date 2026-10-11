#include "game_scene_player_motion_state.h"

#include <mutex>
#include <sstream>

#include "player_motion_contract.h"

namespace nevergone::game_scene_player_motion_state {
namespace {

std::mutex g_mutex;
std::uint64_t g_generation = 0;
Snapshot g_snapshot;

void clear_locked() {
    ++g_generation;
    g_snapshot = {};
    g_snapshot.generation = g_generation;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear_locked();
}

bool sync_spawn(std::uint64_t spawn_generation, std::uint32_t player_index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (player_index < 1u || player_index > 3u) {
        if (g_snapshot.ready) clear_locked();
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
    return true;
}

bool apply_normal_direction(float direction_x, float direction_y) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready) return false;

    const auto movement = player_motion_contract::movement_value(
            g_snapshot.player_index,
            player_motion_contract::kNormalMoveActionState,
            {direction_x, direction_y});
    if (!movement.has_value()) return false;

    g_snapshot.offset_x += movement->x;
    g_snapshot.offset_y += movement->y;
    ++g_snapshot.applied_steps;
    ++g_generation;
    g_snapshot.generation = g_generation;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_snapshot;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameScene player motion\n";
    out << "ready: " << (state.ready ? "true" : "false") << "\n";
    out << "generation: " << state.generation << "\n";
    out << "spawn generation: " << state.spawn_generation << "\n";
    out << "player index: " << state.player_index << "\n";
    out << "applied steps: " << state.applied_steps << "\n";
    out << "offset: " << state.offset_x << "," << state.offset_y << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_player_motion_state
