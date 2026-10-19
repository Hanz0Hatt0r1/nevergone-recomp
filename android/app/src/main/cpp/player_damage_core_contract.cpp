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

HitOrdinalResult apply_hit_ordinal_damage(
        float damage, std::int32_t hit_ordinal,
        float supr_attack, float meat_attack) noexcept {
    HitOrdinalResult result;
    result.damage = damage;
    if (hit_ordinal >= 1) {
        result.weapon_power_gain_requested = true;
        result.weapon_power_gain = meat_attack;
    }

    float ordinal_factor = 1.0f;
    if (hit_ordinal == 2) {
        ordinal_factor = 0.2f + supr_attack * 0.0025f;
    } else if (hit_ordinal == 3) {
        ordinal_factor = 0.15f + supr_attack * 0.002f;
    } else if (hit_ordinal == 4) {
        ordinal_factor = 0.1f + supr_attack * 0.0015f;
    } else if (hit_ordinal > 4) {
        ordinal_factor = 0.05f + supr_attack * 0.001f;
    }
    if (hit_ordinal >= 2) result.damage = result.damage * ordinal_factor;
    return result;
}

WeaponPowerSetResult set_weapon_power(
        float current_meter, bool current_active,
        const WeaponPowerSetTerms& terms) noexcept {
    WeaponPowerSetResult result;
    result.meter = current_meter;
    result.active = current_active;
    if (!terms.action_system_available) return result;

    result.meter = terms.requested_meter;
    result.active = !(result.meter < 100.0f);
    result.action_color_updated = true;
    result.action_color_index = result.active ? terms.weapon_field_e4 + 1 : 0;
    result.progress_bar_updated = true;
    return result;
}

WeaponPowerAddResult add_weapon_power(
        const WeaponPowerAddTerms& terms) noexcept {
    WeaponPowerAddResult result;
    result.meter = terms.meter;
    result.active = terms.active;
    if (terms.active) return result;

    result.accumulated = true;
    result.accepted_attack_gain = terms.meat_attack;
    if (terms.meat_attack > 0.0f) {
        result.accepted_attack_gain +=
            terms.meat_attack * (terms.property_bonus_8c / 100.0f);
    }

    result.meter += result.accepted_attack_gain;
    if (result.meter < 0.0f) result.meter = 0.0f;
    if (result.meter < 100.0f) {
        result.progress_bar_updated = true;
        return result;
    }

    result.meter = 100.0f;
    result.set_weapon_power_called = true;
    result.spawn_activation_shoot = true;
    result.play_activation_sound = true;
    const WeaponPowerSetTerms set_terms{
        result.meter,
        terms.action_system_available,
        terms.weapon_field_e4,
    };
    const WeaponPowerSetResult set_result =
        set_weapon_power(result.meter, result.active, set_terms);
    result.meter = set_result.meter;
    result.active = set_result.active;
    result.progress_bar_updated = set_result.progress_bar_updated;
    result.action_color_updated = set_result.action_color_updated;
    result.action_color_index = set_result.action_color_index;
    return result;
}

float apply_action_specific_damage_multipliers(
        float damage, const ActionSpecificDamageTerms& terms) noexcept {
    const auto type_one = [&terms](ActionTypeOneDamageValue index) noexcept {
        return terms.attack_type_one_values[static_cast<std::size_t>(index)];
    };
    const auto type_two = [&terms](ActionTypeTwoDamageValue index) noexcept {
        return terms.attack_type_two_values[static_cast<std::size_t>(index)];
    };
    const auto multiply = [&damage](float additive_factor) noexcept {
        damage = damage * (additive_factor + 1.0f);
    };
    const std::int32_t action = terms.action_id_278;
    const bool has_shared_action_multiplier =
        (action >= 0x3a && action <= 0x3c) || action == 0x3f;

    if (terms.player_attack_type_144 == 1) {
        if (action == 0x2e) {
            multiply(type_one(ActionTypeOneDamageValue::action_2e_458));
        }
        if (action == 9 &&
                (terms.action_context_274 == 0x24 ||
                 terms.action_context_274 == 0x1e)) {
            multiply(type_one(
                ActionTypeOneDamageValue::context_24_or_1e_action_9_45c));
        }
        if (action >= 2 && action <= 4) {
            multiply(type_one(ActionTypeOneDamageValue::actions_2_to_4_46c));
        }
        if (action >= 5 && action <= 6) {
            multiply(type_one(ActionTypeOneDamageValue::actions_5_to_6_470));
        }
        if (action >= 7 && action <= 8) {
            multiply(type_one(ActionTypeOneDamageValue::actions_7_to_8_474));
        }
        if (action == 0x10) {
            multiply(type_one(ActionTypeOneDamageValue::action_10_hex_478));
        }
        if (action == 0x27) {
            multiply(type_one(ActionTypeOneDamageValue::action_27_hex_47c));
        }
        if (action == 10) {
            multiply(type_one(ActionTypeOneDamageValue::action_10_decimal_480));
        }
        if (action == 0x0f) {
            multiply(type_one(ActionTypeOneDamageValue::action_0f_484));
        }
        if (action >= 0x19 && action <= 0x1a) {
            multiply(type_one(ActionTypeOneDamageValue::actions_19_to_1a_488));
        }
        if (action == 0x13) {
            multiply(type_one(ActionTypeOneDamageValue::action_13_hex_48c));
        }
        if (action == 0x14) {
            multiply(type_one(ActionTypeOneDamageValue::action_14_hex_490));
        }
        if (action >= 0x0c && action <= 0x0e) {
            multiply(type_one(ActionTypeOneDamageValue::actions_0c_to_0e_494));
        }
        if (has_shared_action_multiplier) {
            multiply(type_one(
                ActionTypeOneDamageValue::actions_3a_to_3c_or_3f_49c));
        }
    } else if (terms.player_attack_type_144 == 2) {
        if (action == 0x2e) {
            multiply(type_two(ActionTypeTwoDamageValue::action_2e_4a4));
        }
        if (action >= 2 && action <= 5) {
            multiply(type_two(ActionTypeTwoDamageValue::actions_2_to_5_4a8));
        }
        if (action >= 6 && action <= 7) {
            multiply(type_two(ActionTypeTwoDamageValue::actions_6_to_7_4ac));
        }
        if (action == 8) {
            multiply(type_two(ActionTypeTwoDamageValue::action_8_4b0));
        }
        if (action >= 0x19 && action <= 0x1a) {
            multiply(type_two(ActionTypeTwoDamageValue::actions_19_to_1a_4b4));
        }
        if (action >= 0x35 && action <= 0x36) {
            multiply(type_two(ActionTypeTwoDamageValue::actions_35_to_36_4b8));
        }
        if (action == 0x38) {
            multiply(type_two(ActionTypeTwoDamageValue::action_38_4bc));
        }
        if (action == 0x13) {
            multiply(type_two(ActionTypeTwoDamageValue::action_13_hex_4c0));
        }
        if (action == 10) {
            multiply(type_two(ActionTypeTwoDamageValue::action_10_decimal_4c4));
        }
        if (action == 0x0d) {
            multiply(type_two(ActionTypeTwoDamageValue::action_0d_4c8));
        }
        if (action == 0x0e) {
            multiply(type_two(ActionTypeTwoDamageValue::action_0e_4cc));
        }
        if (action == 0x0f) {
            multiply(type_two(ActionTypeTwoDamageValue::action_0f_4d0));
        }
        if (action == 0x10) {
            multiply(type_two(ActionTypeTwoDamageValue::action_10_hex_4d4));
        }
        if (action == 0x27) {
            multiply(type_two(ActionTypeTwoDamageValue::action_27_hex_4d8));
        }
        if (action == 0x10) {
            multiply(type_two(ActionTypeTwoDamageValue::action_10_hex_4dc));
        }
        if (action == 8 &&
                (terms.action_context_274 == 0x24 ||
                 terms.action_context_274 == 0x1e)) {
            multiply(type_two(
                ActionTypeTwoDamageValue::context_24_or_1e_action_8_4e0));
        }
        if (has_shared_action_multiplier) {
            multiply(type_two(
                ActionTypeTwoDamageValue::actions_3a_to_3c_or_3f_4e8));
        }
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
