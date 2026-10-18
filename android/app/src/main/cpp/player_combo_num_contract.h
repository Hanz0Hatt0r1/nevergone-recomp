#pragma once

#include <cstdint>

namespace nevergone::player_combo_num_contract {

struct State {
    std::uint32_t combo_number_14c = 0;
    std::uint32_t field_2f0 = 0;
};

// Mirrors MEPlayer::addComboNum() and MEPlayer::getPlayerComboNum().
void add_combo_num(State& state) noexcept;
std::uint32_t get_combo_num(const State& state) noexcept;

}  // namespace nevergone::player_combo_num_contract
