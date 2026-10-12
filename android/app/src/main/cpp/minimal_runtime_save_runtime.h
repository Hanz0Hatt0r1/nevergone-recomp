#pragma once

#include <cstdint>
#include <string>

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
    minimal_runtime_save::Record record;
};

void reset_session();

// Captures only an already-ready first-scene player/motion cursor. This does not
// serialize the entire original game state and intentionally writes the
// project-owned sidecar format.
bool capture_current(const std::string& files_dir);

// Loads the sidecar once per process and applies it only after the same scene
// identity and player index have been reconstructed normally. A mismatch stays
// pending instead of changing route/scene/player selection.
bool restore_for_spawn(
    const std::string& files_dir,
    const game_scene_player_spawn_state::Snapshot& spawn,
    std::uint32_t player_index,
    std::uint64_t game_tick);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::minimal_runtime_save_runtime
