#pragma once

#include <cstdint>

namespace nevergone::hit_window_counter_contract {

struct State {
    bool has_action_system = false;
    std::uint32_t recorded_hit = 0;
    std::uint32_t current_hit = 0;
    std::uint32_t recorded_combo = 0;
    std::uint32_t current_combo = 0;
};

inline bool is_hit_flag(const State& state) noexcept {
    return !state.has_action_system || state.recorded_hit != state.current_hit;
}

inline bool is_combo_hit(const State& state) noexcept {
    return !state.has_action_system || state.recorded_combo != state.current_combo;
}

inline State record_hit_flag(State state) noexcept {
    if (state.has_action_system) state.recorded_hit = state.current_hit;
    return state;
}

inline State record_combo_hit(State state) noexcept {
    if (state.has_action_system) state.recorded_combo = state.current_combo;
    return state;
}

inline std::uint32_t temp_hit_flag_count(const State& state) noexcept {
    return state.has_action_system ? state.current_hit : 0u;
}

inline std::uint32_t temp_combo_hit_count(const State& state) noexcept {
    return state.has_action_system ? state.current_combo : 0u;
}

}  // namespace nevergone::hit_window_counter_contract
