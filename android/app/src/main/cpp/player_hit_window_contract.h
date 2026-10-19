#pragma once

#include <cstdint>

namespace nevergone::player_hit_window_contract {

struct State {
    bool has_action_system = false;
    std::uint32_t hit_counter_2f4 = 0;
    std::uint32_t combo_counter_2f8 = 0;
    std::uint32_t action_hit_counter_1fc = 0;
    std::uint32_t action_combo_counter_1f8 = 0;
};

// Mirrors MEPlayer::isHitFlag()/isComboHit(): without an action system the
// native methods allow the hit; otherwise they compare the saved local
// counter against the corresponding action-system counter.
bool is_hit_flag(const State& state) noexcept;
bool is_combo_hit(const State& state) noexcept;

// Mirrors addHitFlagCount()/addComboHitCount(); null action systems leave the
// local snapshot unchanged.
State record_hit_flag(State state) noexcept;
State record_combo_hit(State state) noexcept;

// Mirrors tempHitFlagCount()/tempComboHitCount(), returning zero when there
// is no action system.
std::uint32_t temp_hit_flag_count(const State& state) noexcept;
std::uint32_t temp_combo_hit_count(const State& state) noexcept;

}  // namespace nevergone::player_hit_window_contract
