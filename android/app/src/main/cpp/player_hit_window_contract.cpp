#include "player_hit_window_contract.h"

namespace nevergone::player_hit_window_contract {

bool is_hit_flag(const State& state) noexcept {
    return !state.has_action_system ||
        state.hit_counter_2f4 != state.action_hit_counter_1fc;
}

bool is_combo_hit(const State& state) noexcept {
    return !state.has_action_system ||
        state.combo_counter_2f8 != state.action_combo_counter_1f8;
}

State record_hit_flag(State state) noexcept {
    if (state.has_action_system) {
        state.hit_counter_2f4 = state.action_hit_counter_1fc;
    }
    return state;
}

State record_combo_hit(State state) noexcept {
    if (state.has_action_system) {
        state.combo_counter_2f8 = state.action_combo_counter_1f8;
    }
    return state;
}

std::uint32_t temp_hit_flag_count(const State& state) noexcept {
    return state.has_action_system ? state.action_hit_counter_1fc : 0u;
}

std::uint32_t temp_combo_hit_count(const State& state) noexcept {
    return state.has_action_system ? state.action_combo_counter_1f8 : 0u;
}

}  // namespace nevergone::player_hit_window_contract
