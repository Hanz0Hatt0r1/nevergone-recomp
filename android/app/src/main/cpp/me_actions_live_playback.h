#pragma once

#include <cstdint>
#include <string>

#include "enemy_actions_wbg_document.h"
#include "me_actions_playback_timing.h"
#include "me_actions_runtime_state.h"

namespace nevergone::me_actions_live_playback {

struct Snapshot {
    bool ready = false;
    std::uint64_t action_generation = 0;
    std::uint64_t loaded_revision = 0;
    std::uint32_t action_state = 0;
    bool looping = false;
    std::uint64_t last_game_tick = 0;
    std::uint64_t frame_revision = 0;
    std::uint64_t update_count = 0;
    std::uint64_t completed_action_count = 0;
    me_actions_playback_timing::Cursor cursor;
};

void reset();

// Synchronize one already-loaded player action document and advance it at most
// once for each logical game_clock tick. A new action generation/revision/state
// starts with the exact MeActionsSystem::play() cursor. Repeated render-queue
// queries in the same tick are idempotent and cannot accelerate animation.
bool sync_and_tick(
    std::uint64_t action_generation,
    std::uint64_t loaded_revision,
    std::uint32_t action_state,
    bool looping,
    const enemy_actions_wbg_document::Document& document,
    std::uint64_t game_tick);

Snapshot snapshot();
me_actions_runtime_state::State runtime_state();
std::string status_report();

}  // namespace nevergone::me_actions_live_playback
