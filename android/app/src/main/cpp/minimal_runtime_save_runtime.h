#pragma once

#include <cstdint>
#include <string>

#include "game_scene_player_motion_state.h"
#include "game_scene_player_spawn_state.h"
#include "minimal_runtime_save.h"

namespace nevergone::minimal_runtime_save_runtime {

struct Snapshot {
    bool load_attempted = false;
    bool pending_record = false;
    bool restored = false;
    std::uint64_t capture_count = 0;
    std::uint64_t restore_count = 0;
    std::uint64_t load_failure_count = 0;
    std::uint64_t write_failure_count = 0;
    std::uint64_t last_saved_motion_generation = 0;
    std::uint64_t last_save_game_tick = 0;
    minimal_runtime_save::Record record;
};

void reset_session();

bool capture_current(
    const std::string& files_dir,
    const game_scene_player_spawn_state::Snapshot& spawn,
    const game_scene_player_motion_state::Snapshot& motion);

// Render-thread autosave. Snapshots are supplied by the caller so persistence
// observes one coherent scene/player sample rather than re-reading globals.
bool autosave_if_due(
    const std::string& files_dir,
    const game_scene_player_spawn_state::Snapshot& spawn,
    const game_scene_player_motion_state::Snapshot& motion,
    std::uint64_t game_tick);

bool restore_for_spawn(
    const std::string& files_dir,
    const game_scene_player_spawn_state::Snapshot& spawn,
    std::uint32_t player_index,
    std::uint64_t game_tick);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::minimal_runtime_save_runtime
