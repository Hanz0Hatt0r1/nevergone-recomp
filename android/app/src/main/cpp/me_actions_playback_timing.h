#pragma once

#include <cstddef>
#include <cstdint>

#include "enemy_actions_wbg_document.h"
#include "me_actions_combo_boundaries.h"

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

// Frame/boundary fields initialized by MeActionsSystem::play().
struct Cursor {
    State timing;
    std::int32_t current_frame_1a4 = 0;
    std::int32_t previous_frame_1a8 = -1;
    std::uint32_t boundary_index_1f8 = 0;
    std::uint32_t secondary_boundary_index_1fc = 0;
    std::int32_t last_frame_1f4 = 1;
    bool finished_cycle_200 = false;
    bool boundary_gate_201 = true;
    bool update_marker_218 = false;
    bool update_marker_219 = false;
    bool update_data_gate_1bf = true;
};

struct PlaybackResult {
    Cursor cursor;
    bool valid_projection = false;
    bool valid_frame = false;
    std::size_t frames_advanced = 0;
    std::size_t update_data_count = 0;
    bool became_done = false;
};

State initial_play_state();
Cursor initial_cursor();

// Exact pre-frame-advance prefix of MeActionsSystem::waUpdate(float).
TimingResult apply_wa_update_timing(
    const enemy_actions_wbg_document::Document& document,
    std::int32_t current_frame_1a4,
    float delta_seconds,
    State state);

// Extends the timing prefix through the recovered +0xb0/+0xb4 frame-boundary
// loop. One call may advance multiple frames when the retained accumulator is
// large enough; delta_seconds is accumulated only once, matching native control
// flow when waUpdate loops back after updateData().
PlaybackResult apply_wa_update_playback(
    const enemy_actions_wbg_document::Document& document,
    const me_actions_combo_boundaries::Boundaries& boundaries,
    float delta_seconds,
    Cursor cursor);

inline bool is_done(const State& state) {
    // MeActionsSystem::isDone() is exactly LDRB [this,#0x220].
    return state.done_220;
}

}  // namespace nevergone::me_actions_playback_timing
