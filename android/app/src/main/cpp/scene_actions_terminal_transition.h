#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include "scene_actions_play_state.h"

namespace nevergone::scene_actions_terminal_transition {

struct Result {
    bool accepted = false;
    bool update_data_requested = false;
    bool completed = false;
    bool looped_to_frame_zero = false;
};

inline Result apply_common(
        std::int32_t terminal_last_frame,
        scene_actions_play_state::State* state) {
    Result result;
    if (state == nullptr || !state->active_15d || state->mode_24e > 1u) return result;

    state->terminal_last_frame_180 = terminal_last_frame;
    state->end_184 = true;
    state->frame_index_250 = 0u;
    state->done_1a8 = true;

    result.accepted = true;
    if (state->mode_24e == 0u) {
        // Born mode exits the branch with done still asserted and no updateData.
        result.completed = true;
        return result;
    }

    // Idle mode clears done in the same waUpdate branch and falls through to
    // updateData() for frame zero, closing the loop without waiting for step().
    state->done_1a8 = false;
    result.update_data_requested = true;
    result.looped_to_frame_zero = true;
    return result;
}

// Native +0x94 combo-boundary terminal path:
//   r8 = current ActionComboValue+0x18
//   if (frame > r8) +0x180 = r8 - 1
// The subtraction is a 32-bit write; boundary zero therefore produces -1.
inline Result apply_combo_boundary(
        std::size_t boundary_frame_index,
        scene_actions_play_state::State* state) {
    if (boundary_frame_index > static_cast<std::size_t>(
            std::numeric_limits<std::int32_t>::max())) {
        return {};
    }
    const std::uint32_t bits = static_cast<std::uint32_t>(boundary_frame_index) - 1u;
    return apply_common(static_cast<std::int32_t>(bits), state);
}

// Native fallback path used when TrapActionsData+0x94 is empty:
//   count = primary CCArray::count()
//   if (frame > count) +0x180 = count - 1
// Valid parsed non-empty TrapActions documents always have a final +0x94 run,
// so the live klhuo path does not use this fallback. Keep it explicit rather
// than conflating its count semantics with the combo boundary above.
inline Result apply_primary_count(
        std::size_t primary_count,
        scene_actions_play_state::State* state) {
    if (primary_count == 0u || primary_count > static_cast<std::size_t>(
            std::numeric_limits<std::int32_t>::max())) {
        return {};
    }
    return apply_common(static_cast<std::int32_t>(primary_count - 1u), state);
}

}  // namespace nevergone::scene_actions_terminal_transition
