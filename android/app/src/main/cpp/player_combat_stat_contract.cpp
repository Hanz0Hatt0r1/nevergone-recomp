#include "player_combat_stat_contract.h"

namespace nevergone::player_combat_stat_contract {

WeaponFinalAddBaseResult resolve_weapon_final_add_base_attack(
        const WeaponFinalAddBaseTerms& terms) noexcept {
    float value = 0.0f;
    if (terms.management_hole_gate_nonzero) {
        value = terms.add_hole_base_attack + value;
    }
    if (terms.weapon_field_18_is_zero) value += terms.add_temper_base_attack;
    value = value + terms.extra_base_attack + terms.weapon_field_60;
    const bool extra_flag_64 = terms.extra_base_attack > 0.0f;
    if (value < 0.0f) value = 0.0f;
    return WeaponFinalAddBaseResult{value, extra_flag_64};
}

float resolve_attack(const State& player, const AttackTerms& terms) noexcept {
    return terms.weapon_base_attack_38 + terms.weapon_final_add_base_attack +
        terms.player_level_attack_110 + player.equipment_add_attack_114 +
        terms.player_status_value_144;
}

void set_equipment_add_attack(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->equipment_add_attack_114 = value;
}

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
