#include "minimal_runtime_save_runtime.h"

#include <limits>
#include <mutex>
#include <sstream>

#include "game_scene_player_motion_restore.h"

namespace nevergone::minimal_runtime_save_runtime {
namespace {

constexpr std::uint64_t kAutosaveIntervalTicks = 35u;
std::mutex g_mutex;
Snapshot g_snapshot{};

bool same_scene(
        const minimal_runtime_save::Record& record,
        const game_scene_player_spawn_state::Snapshot& spawn,
        std::uint32_t player_index) {
    return spawn.status == game_scene_player_spawn_state::Status::kReady &&
        spawn.source_scene_index <= std::numeric_limits<std::uint32_t>::max() &&
        record.scene_index == static_cast<std::uint32_t>(spawn.source_scene_index) &&
        record.scene_guid == spawn.scene_guid &&
        record.player_index == player_index;
}

}  // namespace

void reset_session() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_snapshot = {};
}

bool capture_current(
        const std::string& files_dir,
        const game_scene_player_spawn_state::Snapshot& spawn,
        const game_scene_player_motion_state::Snapshot& motion) {
    if (files_dir.empty() ||
        spawn.status != game_scene_player_spawn_state::Status::kReady ||
        spawn.source_scene_index > std::numeric_limits<std::uint32_t>::max() ||
        !motion.ready || motion.spawn_generation != spawn.generation ||
        motion.player_index < 1u || motion.player_index > 3u) {
        return false;
    }

    minimal_runtime_save::Record record;
    record.scene_index = static_cast<std::uint32_t>(spawn.source_scene_index);
    record.scene_guid = spawn.scene_guid;
    record.player_index = motion.player_index;
    record.motion_offset_x = motion.offset_x;
    record.motion_offset_y = motion.offset_y;
    if (!minimal_runtime_save::valid(record)) return false;

    const bool written = minimal_runtime_save::write_atomic(files_dir, record);
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!written) {
        ++g_snapshot.write_failure_count;
        return false;
    }
    ++g_snapshot.capture_count;
    g_snapshot.record = record;
    return true;
}

bool autosave_if_due(
        const std::string& files_dir,
        const game_scene_player_spawn_state::Snapshot& spawn,
        const game_scene_player_motion_state::Snapshot& motion,
        std::uint64_t game_tick) {
    if (!motion.ready || motion.applied_steps == 0u ||
        motion.spawn_generation != spawn.generation) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (motion.generation == g_snapshot.last_saved_motion_generation) return true;
        if (g_snapshot.capture_count != 0u && game_tick >= g_snapshot.last_save_game_tick &&
            game_tick - g_snapshot.last_save_game_tick < kAutosaveIntervalTicks) {
            return true;
        }
    }

    if (!capture_current(files_dir, spawn, motion)) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_snapshot.last_saved_motion_generation = motion.generation;
    g_snapshot.last_save_game_tick = game_tick;
    return true;
}

bool restore_for_spawn(
        const std::string& files_dir,
        const game_scene_player_spawn_state::Snapshot& spawn,
        std::uint32_t player_index,
        std::uint64_t game_tick) {
    minimal_runtime_save::Record pending;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_snapshot.restored) return true;
        if (!g_snapshot.load_attempted) {
            g_snapshot.load_attempted = true;
            minimal_runtime_save::Record loaded;
            if (minimal_runtime_save::read(files_dir, &loaded)) {
                g_snapshot.record = loaded;
                g_snapshot.pending_record = true;
            } else {
                ++g_snapshot.load_failure_count;
            }
        }
        if (!g_snapshot.pending_record) return false;
        pending = g_snapshot.record;
    }

    if (!same_scene(pending, spawn, player_index)) return false;

    if (!game_scene_player_motion_state::restore_offset(
            spawn.generation,
            player_index,
            game_tick,
            pending.motion_offset_x,
            pending.motion_offset_y)) {
        return false;
    }

    const auto restored_motion = game_scene_player_motion_state::snapshot();
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_snapshot.restored) return true;
    g_snapshot.restored = true;
    g_snapshot.pending_record = false;
    g_snapshot.last_saved_motion_generation = restored_motion.generation;
    g_snapshot.last_save_game_tick = game_tick;
    ++g_snapshot.restore_count;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_snapshot;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "Minimal runtime save\n";
    out << "load attempted: " << (state.load_attempted ? "yes" : "no") << "\n";
    out << "pending: " << (state.pending_record ? "yes" : "no") << "\n";
    out << "restored: " << (state.restored ? "yes" : "no") << "\n";
    out << "captures: " << state.capture_count << "\n";
    out << "restores: " << state.restore_count << "\n";
    out << "load failures: " << state.load_failure_count << "\n";
    out << "write failures: " << state.write_failure_count << "\n";
    out << "last saved motion generation: " << state.last_saved_motion_generation << "\n";
    out << "last save tick: " << state.last_save_game_tick << "\n";
    if (!state.record.scene_guid.empty()) {
        out << "scene: " << state.record.scene_index << " " << state.record.scene_guid << "\n";
        out << "player index: " << state.record.player_index << "\n";
        out << "motion offset: " << state.record.motion_offset_x
            << "," << state.record.motion_offset_y << "\n";
    }
    return out.str();
}

}  // namespace nevergone::minimal_runtime_save_runtime
