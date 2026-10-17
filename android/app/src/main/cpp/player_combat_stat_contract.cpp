#include "player_combat_stat_contract.h"

#include <cstring>

namespace nevergone::player_combat_stat_contract {

std::int32_t resolve_weapon_star_level(std::uint32_t hole_array_count) noexcept {
    return hole_array_count == 0
        ? 0
        : static_cast<std::int32_t>(hole_array_count - 1U);
}

float resolve_weapon_add_hole_base_attack(
        std::int32_t star_level, float weapon_field_7c) noexcept {
    if (star_level <= 1) return 0.0f;
    float value = static_cast<float>(star_level + 2) * weapon_field_7c;
    value *= static_cast<float>(star_level - 1);
    return 0.0f + value * 0.5f;
}

float resolve_weapon_add_temper_base_attack(
        std::int32_t weapon_field_ac, float weapon_field_7c) noexcept {
    const std::uint32_t shifted_bits =
        static_cast<std::uint32_t>(weapon_field_ac) << 1U;
    std::int32_t shifted_value = 0;
    std::memcpy(&shifted_value, &shifted_bits, sizeof(shifted_value));
    return static_cast<float>(shifted_value) * weapon_field_7c;
}

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

float resolve_damage_absorb(
        const State& player, const DamageAbsorbTerms& terms) noexcept {
    float equipment_absorb = 0.0f;
    for (const auto& effect : terms.equipment_effects) {
        if (effect.type == 0x1cu) equipment_absorb = effect.value;
    }

    float multiplier_bonus = terms.parameter_multiplier_bonus_308;
    if (terms.use_management_level_effects) {
        multiplier_bonus = 0.0f;
        for (const auto& effect : terms.management_effects) {
            if (effect.type != 10u) continue;
            multiplier_bonus = multiplier_bonus + effect.base_bonus;
            multiplier_bonus = multiplier_bonus +
                static_cast<float>(effect.level - 1u) * effect.growth_bonus;
        }
    }

    float attribute_absorb = 0.0f;
    for (const auto& effect : terms.attribute_effects) {
        if ((effect.type == 2u || effect.type == 4u) && effect.enabled) {
            attribute_absorb +=
                (static_cast<float>(effect.level) * effect.per_level_bonus) / 100.0f;
        }
    }

    const float armband_multiplier = terms.armband_equipped
        ? terms.armband_multiplier_40 : 0.0f;
    const float armband_absorb = terms.armband_equipped
        ? terms.armband_absorb_addition / 100.0f : 0.0f;
    const float absorb_sum = terms.player_level_absorb + equipment_absorb +
        armband_absorb + terms.player_status_absorb_140 +
        player.equipment_add_damage_absorb_118 + attribute_absorb;
    const float multiplier = multiplier_bonus + 1.0f + armband_multiplier;
    return absorb_sum * multiplier;
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
