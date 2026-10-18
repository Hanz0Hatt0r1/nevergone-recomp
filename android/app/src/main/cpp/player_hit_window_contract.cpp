#include "player_hit_window_contract.h"

#include "hit_window_counter_contract.h"

namespace nevergone::player_hit_window_contract {

namespace {

hit_window_counter_contract::State counters(const State& state) noexcept {
    return {
        state.has_action_system,
        state.hit_counter_2f4,
        state.action_hit_counter_1fc,
        state.combo_counter_2f8,
        state.action_combo_counter_1f8,
    };
}

State apply_counters(const State& state,
        hit_window_counter_contract::State updated) noexcept {
    State result = state;
    result.hit_counter_2f4 = updated.recorded_hit;
    result.combo_counter_2f8 = updated.recorded_combo;
    return result;
}

}  // namespace

bool is_hit_flag(const State& state) noexcept {
    return hit_window_counter_contract::is_hit_flag(counters(state));
}

bool is_combo_hit(const State& state) noexcept {
    return hit_window_counter_contract::is_combo_hit(counters(state));
}

State record_hit_flag(State state) noexcept {
    return apply_counters(state,
        hit_window_counter_contract::record_hit_flag(counters(state)));
}

State record_combo_hit(State state) noexcept {
    return apply_counters(state,
        hit_window_counter_contract::record_combo_hit(counters(state)));
}

std::uint32_t temp_hit_flag_count(const State& state) noexcept {
    return hit_window_counter_contract::temp_hit_flag_count(counters(state));
}

std::uint32_t temp_combo_hit_count(const State& state) noexcept {
    return hit_window_counter_contract::temp_combo_hit_count(counters(state));
}

}  // namespace nevergone::player_hit_window_contract
