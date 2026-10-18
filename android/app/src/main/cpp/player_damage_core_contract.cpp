#include "player_damage_core_contract.h"

#include <cmath>

namespace nevergone::player_damage_core_contract {

float resolve(const Terms& terms) noexcept {
    const float difference = terms.enemy_power_2ec - terms.weapon_curve_origin_8c;
    float enemy_power_curve = terms.weapon_curve_offset_90 +
            difference * terms.weapon_curve_scale_94 * difference;
    if (enemy_power_curve < 0.0f) enemy_power_curve = 0.0f;

    const float base_multiplier = 0.95f +
            (terms.meat_attack / terms.enemy_power_scale_2f4) * 0.05f;
    const float weighted_base = (terms.base_attack + enemy_power_curve) * base_multiplier;
    const float weighted_supr = terms.supr_attack * 0.3f;
    const float attack_type_multiplier =
            terms.player_attack_type_144 == 2 ? 0.022f : 0.03f;
    return (weighted_supr + weighted_base) * attack_type_multiplier;
}

bool resolve_weapon_attack_up_on_attr(
        float bonus_a8, bool weapon_exists,
        std::uint32_t weapon_attribute_a4) noexcept {
    return bonus_a8 > 0.0f && weapon_exists && weapon_attribute_a4 != 0x10u;
}

bool resolve_weapon_attack_up_on_not_attr(
        float bonus_9c, bool weapon_exists,
        std::uint32_t weapon_attribute_a4) noexcept {
    return bonus_9c > 0.0f && weapon_exists && weapon_attribute_a4 == 0x10u;
}

bool resolve_atk5_bonus_damage(std::uint64_t lrand48_value) noexcept {
    return lrand48_value % 100u < 5u;
}

bool resolve_enemy_is_low_health_percent(
        float current_health_164, float max_health_160) noexcept {
    const float percent = std::trunc(
        (current_health_164 / max_health_160) * 100.0f);
    return percent < 30.0f;
}

void apply_property_attack_bonuses(
        AttackValues* values, const PropertyBonusTerms& terms) noexcept {
    if (values == nullptr) return;

    if (terms.weapon_attack_up_on_attr) {
        values->base_attack =
            values->base_attack + terms.weapon_attack_up_on_attr_a8;
    } else if (terms.weapon_attack_up_on_not_attr) {
        values->base_attack =
            values->base_attack + terms.weapon_attack_up_on_not_attr_9c;
    }

    if (terms.atk5_bonus_damage) {
        values->base_attack = values->base_attack + terms.atk5_bonus_damage_1c;
    }
    if (terms.property_94 > 0.0f) {
        float random_bonus = terms.random_value_25c * terms.property_94;
        if (random_bonus > 100.0f) random_bonus = 100.0f;
        values->base_attack = values->base_attack + random_bonus;
    }
    if (terms.property_98 > 0.0f && terms.player_attack_attribute_is_two) {
        values->base_attack = values->base_attack + terms.property_98;
    }
    if (terms.enemy_is_low_health_percent && terms.property_a4 > 0.0f) {
        values->base_attack = values->base_attack + terms.property_a4;
    }
    if (terms.app_parameter_mode_is_nine) {
        const float scaled = terms.random_value_230 * terms.property_88;
        float parameter_bonus = static_cast<float>(static_cast<std::int32_t>(scaled));
        if (parameter_bonus > 200.0f) parameter_bonus = 200.0f;
        values->base_attack = values->base_attack + parameter_bonus;
    }
}

float apply_primary_damage_multipliers(
        float damage, const PrimaryDamageMultiplierTerms& terms) noexcept {
    for (const float multiplier : terms.player_multipliers_420) {
        if (multiplier < 1.0f) damage = damage * multiplier;
    }

    if (terms.weapon_multiplier_300_enabled) {
        float weapon_multiplier = 1.05f;
        if (terms.weapon_field_e4 != 0) {
            weapon_multiplier = 1.15f;
            if (terms.weapon_field_e4 == 2) weapon_multiplier = 1.25f;
        }
        damage = damage * weapon_multiplier;
    }
    return damage;
}

std::optional<float> resolve_action_damage_factor(
        const std::vector<float>& factors, std::int32_t frame_index) noexcept {
    if (frame_index < 0 ||
            static_cast<std::size_t>(frame_index) >= factors.size()) {
        return std::nullopt;
    }
    return factors[static_cast<std::size_t>(frame_index)];
}

float apply_action_damage_factor(float damage, float action_factor) noexcept {
    return damage * action_factor;
}

}  // namespace nevergone::player_damage_core_contract
