#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace nevergone::player_damage_core_contract {

struct Terms {
    float meat_attack = 0.0f;
    float supr_attack = 0.0f;
    float base_attack = 0.0f;
    float enemy_power_2ec = 0.0f;
    float enemy_power_scale_2f4 = 0.0f;
    float weapon_curve_origin_8c = 0.0f;
    float weapon_curve_offset_90 = 0.0f;
    float weapon_curve_scale_94 = 0.0f;
    int player_attack_type_144 = 0;
};

struct AttackValues {
    float meat_attack = 0.0f;
    float supe_attack = 0.0f;
    float supr_attack = 0.0f;
    float base_attack = 0.0f;
};

struct PropertyBonusTerms {
    bool weapon_attack_up_on_attr = false;
    float weapon_attack_up_on_attr_a8 = 0.0f;
    bool weapon_attack_up_on_not_attr = false;
    float weapon_attack_up_on_not_attr_9c = 0.0f;
    bool atk5_bonus_damage = false;
    float atk5_bonus_damage_1c = 0.0f;
    float random_value_25c = 0.0f;
    float property_94 = 0.0f;
    float property_98 = 0.0f;
    bool player_attack_attribute_is_two = false;
    bool enemy_is_low_health_percent = false;
    float property_a4 = 0.0f;
    bool app_parameter_mode_is_nine = false;
    float random_value_230 = 0.0f;
    float property_88 = 0.0f;
};

struct PrimaryDamageMultiplierTerms {
    std::array<float, 4> player_multipliers_420{{1.0f, 1.0f, 1.0f, 1.0f}};
    bool weapon_multiplier_300_enabled = false;
    std::int32_t weapon_field_e4 = 0;
};

// Reconstructs the weapon-versus-enemy power curve and weighted core damage.
// The attack-category inputs are already resolved through their native
// equipment, level, property, weakness, and attack-state paths.
float resolve(const Terms& terms) noexcept;

bool resolve_weapon_attack_up_on_attr(
    float bonus_a8, bool weapon_exists, std::uint32_t weapon_attribute_a4) noexcept;
bool resolve_weapon_attack_up_on_not_attr(
    float bonus_9c, bool weapon_exists, std::uint32_t weapon_attribute_a4) noexcept;
bool resolve_atk5_bonus_damage(std::uint64_t lrand48_value) noexcept;
bool resolve_enemy_is_low_health_percent(
    float current_health_164, float max_health_160) noexcept;

// Adds the property-manager terms to the attack categories before the damage
// core. RNG and manager outcomes are supplied as decoded per-hit inputs.
void apply_property_attack_bonuses(
    AttackValues* values, const PropertyBonusTerms& terms) noexcept;

// Applies MEPlayer +0x420..+0x42c multipliers in order, but only when each is
// below 1.0f, then the equipped weapon's +0xe4 multiplier when +0x300 is set.
float apply_primary_damage_multipliers(
    float damage, const PrimaryDamageMultiplierTerms& terms) noexcept;

// Mirrors MeActionsSystem::DamageFactor(): reads the per-frame value from the
// +0xcc table indexed by MEActionsSystem +0x1a4.
std::optional<float> resolve_action_damage_factor(
    const std::vector<float>& factors, std::int32_t frame_index) noexcept;

float apply_action_damage_factor(float damage, float action_factor) noexcept;

}  // namespace nevergone::player_damage_core_contract
