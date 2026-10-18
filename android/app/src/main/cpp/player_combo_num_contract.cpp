#include "player_combo_num_contract.h"

namespace nevergone::player_combo_num_contract {

void add_combo_num(State& state) noexcept {
    ++state.combo_number_14c;
    state.field_2f0 = 0u;
}

std::uint32_t get_combo_num(const State& state) noexcept {
    return state.combo_number_14c;
}

}  // namespace nevergone::player_combo_num_contract
