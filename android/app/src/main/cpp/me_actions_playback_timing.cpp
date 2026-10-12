#include "me_actions_playback_timing.h"

#include <cmath>

namespace nevergone::me_actions_playback_timing {
namespace {

constexpr float kMillisecondsPerSecond = 1000.0f;

}  // namespace

State initial_play_state() {
    State state;
    // MeActionsSystem::play(): +0x220=0, +0x1bc=1, +0x1b4=0,
    // +0x1b0=0. The initial +0x1ac copy is recomputed by waUpdate from the
    // selected AFD before each timing comparison, so it does not need retained
    // storage in this bounded helper.
    state.attach_duration_1b0 = 0.0f;
    state.accumulator_1b4 = 0.0f;
    state.reset_accumulator_1bc = true;
    state.done_220 = false;
    return state;
}

TimingResult apply_wa_update_timing(
        const enemy_actions_wbg_document::Document& document,
        std::int32_t current_frame_1a4,
        float delta_seconds,
        State state) {
    TimingResult result;
    result.state = state;

    // waUpdate returns immediately when isDone/+0x220 is nonzero.
    if (state.done_220) return result;

    if (state.reset_accumulator_1bc) {
        result.state.reset_accumulator_1bc = false;
        result.state.accumulator_1b4 = 0.0f;
    } else {
        // Native uses a literal 0x447a0000 (1000.0f) and VMLA.f32.
        result.state.accumulator_1b4 += delta_seconds * kMillisecondsPerSecond;
    }

    if (current_frame_1a4 < 0) return result;
    const std::size_t frame_index = static_cast<std::size_t>(current_frame_1a4);
    if (frame_index >= document.primary_records.size() ||
        frame_index >= document.final_table.entries.size()) {
        return result;
    }

    result.valid_frame = true;
    result.frame_index = frame_index;
    result.action_frame_duration_5c =
        document.final_table.entries[frame_index].reciprocal_value;
    result.threshold_1ac =
        result.action_frame_duration_5c + result.state.attach_duration_1b0;

    // VCMPE followed by BLT takes the early-return branch for ordinary
    // accumulator<threshold and for unordered/NaN compares. Preserve that
    // behavior explicitly; infinities otherwise follow native float arithmetic.
    if (std::isnan(result.state.accumulator_1b4) ||
        std::isnan(result.threshold_1ac) ||
        result.state.accumulator_1b4 < result.threshold_1ac) {
        return result;
    }

    result.state.accumulator_1b4 -= result.threshold_1ac;
    result.frame_processing_ready = true;
    return result;
}

}  // namespace nevergone::me_actions_playback_timing
