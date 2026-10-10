#pragma once

#include <cstdint>

namespace nevergone::enemy_actions_frame_move_value {

struct BoundsObservation {
    float origin_x = 0.0f;
    float origin_y = 0.0f;
    float width = 0.0f;
};

struct State {
    std::int32_t field_17c = 0;
    float field_180 = 0.0f;
    float field_184 = 0.0f;
    float field_298 = 0.0f;
    float field_29c = 0.0f;
};

struct Result {
    State state;
    bool applied = false;
};

// Reconstructs EnemyActionsSystem::updateActionFrameMoveValue() at
// 0x2ac28e..0x2ac364. `primary_count` is the count returned by EAD+0x88.
inline Result apply(
        std::int32_t current_frame_294,
        bool enemy_actions_data_present,
        bool primary_array_present,
        std::uint32_t primary_count,
        const BoundsObservation& sprite_bounds,
        State state) {
    Result out{state, false};

    if (state.field_17c == current_frame_294) return out;
    if (!enemy_actions_data_present) return out;
    if (!primary_array_present) return out;

    // Native compares the raw 32-bit current-frame value to CCArray::count()
    // for equality only. It does not perform a >= range check here.
    if (static_cast<std::uint32_t>(current_frame_294) == primary_count) {
        return out;
    }

    const float point_x = sprite_bounds.origin_x + sprite_bounds.width * 0.5f;
    const float point_y = sprite_bounds.origin_y;

    if (current_frame_294 == 0) {
        state.field_298 = point_x;
        state.field_29c = point_y;
    } else {
        const float delta_x = point_x - state.field_298;
        const float delta_y = point_y - state.field_29c;

        state.field_180 = state.field_180 + delta_x;
        state.field_184 = state.field_184 + delta_y;

        // This is intentionally not the absolute current point. Native assigns
        // the computed delta point to +0x298/+0x29c on nonzero frames.
        state.field_298 = delta_x;
        state.field_29c = delta_y;
    }

    state.field_17c = current_frame_294;
    out.state = state;
    out.applied = true;
    return out;
}

}  // namespace nevergone::enemy_actions_frame_move_value
