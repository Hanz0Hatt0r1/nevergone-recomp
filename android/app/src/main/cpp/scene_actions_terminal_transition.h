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

// Apply the exact terminal writes performed by SceneActionsSystem::waUpdate()
// once its native one-past comparison has selected the completion branch.
// `exclusive_frame_limit` is the native boundary/count value whose final
// renderable frame is limit-1. Selecting *when* that branch fires remains a
// separate cursor/parser boundary.
inline Result apply(
        std::size_t exclusive_frame_limit,
        scene_actions_play_state::State* state) {
    Result result;
    if (state == nullptr || !state->active_15d || exclusive_frame_limit == 0u ||
            exclusive_frame_limit > static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max()) ||
            state->mode_24e > 1u) {
        return result;
    }

    state->terminal_last_frame_180 =
            static_cast<std::int32_t>(exclusive_frame_limit - 1u);
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

}  // namespace nevergone::scene_actions_terminal_transition
