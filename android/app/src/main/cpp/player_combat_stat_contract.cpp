#include "player_combat_stat_contract.h"

namespace nevergone::player_combat_stat_contract {

void set_equipment_add_health(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->equipment_add_health_110 = value;
}

void add_equipment_add_health(State* state, float delta) noexcept {
    if (state == nullptr) return;
    state->equipment_add_health_110 += delta;
}

void set_equipment_add_damage_absorb(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->equipment_add_damage_absorb_118 = value;
}

void set_tools_add_health(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->tools_add_health_120 = value;
}

void set_tools_damage_absorb(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->tools_damage_absorb_factor_430 = 1.0f - value;
}

}  // namespace nevergone::player_combat_stat_contract
