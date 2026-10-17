#pragma once

#include <cstdint>

namespace nevergone::enemy_hit_gate_contract {

struct State {
    bool has_action_system = false;
    std::uint8_t allow_hit_player_244 = 0;
    std::uint32_t last_hit_counter_16c = 0;
    std::uint32_t action_hit_counter_2a4 = 0;
};

// Mirrors the original EnemyObject accessors and hit-frame guard.
inline bool allow_hit_player(const State& state) noexcept {
    return state.allow_hit_player_244 != 0;
}

inline bool is_hit_flag(const State& state) noexcept {
    return !state.has_action_system ||
        state.last_hit_counter_16c != state.action_hit_counter_2a4;
}

inline std::uint32_t temp_hit_flag_count(const State& state) noexcept {
    return state.has_action_system ? state.action_hit_counter_2a4 : 0u;
}

inline State record_hit_flag(State state) noexcept {
    if (state.has_action_system) {
        state.last_hit_counter_16c = state.action_hit_counter_2a4;
    }
    return state;
}

// EnemyObject::isAttackMiss() compares action-system frame +0x294 with a
// state-specific cutoff selected from EnemyObject +0x274.
inline bool is_attack_miss(
        std::int32_t enemy_state_274, std::int32_t current_frame_294) noexcept {
    std::int32_t frame_cutoff = 0;
    switch (enemy_state_274) {
        case 0x07:
        case 0x08:
            frame_cutoff = 4;
            break;
        case 0x09:
        case 0x0a:
            frame_cutoff = 10;
            break;
        case 0x0b:
            frame_cutoff = 15;
            break;
        case 0x0c:
        case 0x11:
        case 0x2d:
            frame_cutoff = 21;
            break;
        default:
            return false;
    }
    return current_frame_294 > frame_cutoff;
}

}  // namespace nevergone::enemy_hit_gate_contract
