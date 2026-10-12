#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "trap_actions_document.h"

namespace nevergone::scene_actions_play_state {

struct State {
    std::uint8_t mode_24e = 0;
    bool active_15d = false;
    bool first_update_15c = false;
    bool done_1a8 = true;
    bool end_184 = false;
    std::int32_t last_frame_14c = -1;
    std::int32_t secondary_last_170 = -1;
    std::size_t frame_index_250 = 0;
    std::size_t boundary_cursor_94_25c = 0;
    std::size_t boundary_cursor_98_260 = 0;
    float accumulator_154 = 0.0f;
    float current_frame_value_150 = 0.0f;
};

// Reconstruct the fields established by SceneActionsSystem::play() before its
// updateData() call. The selected ActionFrameData is index 0 and +0x5c has
// already been materialized by the recovered TrapActions final reciprocal table.
// Keep the +0x5c value structurally named here: waUpdate timing semantics are
// reconstructed separately.
inline std::optional<State> build(
        const trap_actions_document::Document& document,
        std::uint8_t mode_24e) {
    if (mode_24e > 1u) return std::nullopt;
    if (document.core.primary.records.empty()) return std::nullopt;
    if (document.final_table.entries.size() != document.core.primary.records.size()) {
        return std::nullopt;
    }
    if (document.final_table.entries.empty()) return std::nullopt;

    const float frame_value = document.final_table.entries[0].reciprocal_value;
    if (!std::isfinite(frame_value)) return std::nullopt;

    State state;
    state.mode_24e = mode_24e;
    state.active_15d = true;
    state.first_update_15c = true;
    state.done_1a8 = false;
    state.end_184 = false;
    state.last_frame_14c = -1;
    state.secondary_last_170 = -1;
    state.frame_index_250 = 0u;
    state.boundary_cursor_94_25c = 0u;
    state.boundary_cursor_98_260 = 0u;
    state.accumulator_154 = 0.0f;
    state.current_frame_value_150 = frame_value;
    return state;
}

}  // namespace nevergone::scene_actions_play_state
