#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "enemy_actions_wbg_document.h"

namespace nevergone::me_actions_playback_timing {

// Player-specific offset state recovered from MeActionsSystem. Do not reuse the
// EnemyActionsSystem timing/runtime offsets: both consume compatible
// ActionFrameData records but their playback objects have different layouts.
struct State {
    float attach_duration_1b0 = 0.0f;
    float accumulator_1b4 = 0.0f;
    bool reset_accumulator_1bc = true;
    bool done_220 = false;
};

struct TimingResult {
    State state;
    bool valid_frame = false;
    std::size_t frame_index = 0;
    float action_frame_duration_5c = 0.0f;
    float threshold_1ac = 0.0f;
    bool frame_processing_ready = false;
};

State initial_play_state();

// Exact pre-frame-advance prefix of MeActionsSystem::waUpdate(float):
// - +0x220 done exits immediately;
// - first update after play clears +0x1bc and +0x1b4 without accumulating dt;
// - otherwise +0x1b4 += dt * 1000.0f;
// - threshold +0x1ac = ActionFrameData+0x5c + +0x1b0;
// - when accumulator >= threshold, threshold is subtracted and native proceeds
//   into the player-specific frame/boundary branch.
//
// Section G of the parsed action document is the already-proven source of
// ActionFrameData+0x5c. This function intentionally stops before frame advance;
// action-data +0xb0/+0xb4 boundary semantics are a separate contract.
TimingResult apply_wa_update_timing(
    const enemy_actions_wbg_document::Document& document,
    std::int32_t current_frame_1a4,
    float delta_seconds,
    State state);

inline bool is_done(const State& state) {
    // MeActionsSystem::isDone() is exactly LDRB [this,#0x220].
    return state.done_220;
}

}  // namespace nevergone::me_actions_playback_timing
