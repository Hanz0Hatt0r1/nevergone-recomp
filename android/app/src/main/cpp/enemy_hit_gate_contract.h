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

}  // namespace nevergone::enemy_hit_gate_contract
