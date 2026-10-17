#pragma once

#include <cstdint>
#include <vector>

namespace nevergone::player_combat_stat_contract {

struct State {
    float equipment_add_attack_114 = 0.0f;
    float equipment_add_health_110 = 0.0f;
    float equipment_add_damage_absorb_118 = 0.0f;
    float tools_add_health_120 = 0.0f;
    float tools_damage_absorb_factor_430 = 0.0f;
};

struct AttackTerms {
    float weapon_base_attack_38 = 0.0f;
    float weapon_final_add_base_attack = 0.0f;
    float player_level_attack_110 = 0.0f;
    float player_status_value_144 = 0.0f;
};

struct EquipmentAbsorbEffect {
    std::uint32_t type = 0;
    float value = 0.0f;
};

struct ManagementAbsorbEffect {
    std::uint32_t type = 0;
    std::uint32_t level = 0;
    float base_bonus = 0.0f;
    float growth_bonus = 0.0f;
};

struct AttributeAbsorbEffect {
    std::uint32_t type = 0;
    std::uint32_t level = 0;
    bool enabled = false;
    float per_level_bonus = 0.0f;
};

struct DamageAbsorbTerms {
    float player_level_absorb = 0.0f;
    float player_status_absorb_140 = 0.0f;
    bool use_management_level_effects = false;
    float parameter_multiplier_bonus_308 = 0.0f;
    bool armband_equipped = false;
    float armband_multiplier_40 = 0.0f;
    float armband_absorb_addition = 0.0f;
    std::vector<EquipmentAbsorbEffect> equipment_effects;
    std::vector<ManagementAbsorbEffect> management_effects;
    std::vector<AttributeAbsorbEffect> attribute_effects;
};

struct WeaponFinalAddBaseTerms {
    bool management_hole_gate_nonzero = false;
    float add_hole_base_attack = 0.0f;
    bool weapon_field_18_is_zero = false;
    float add_temper_base_attack = 0.0f;
    float extra_base_attack = 0.0f;
    float weapon_field_60 = 0.0f;
};

struct WeaponFinalAddBaseResult {
    float value = 0.0f;
    bool extra_flag_64 = false;
};

// WeaponsItemData::getStarLevel() returns the +0x98 CCArray count minus one,
// or zero for an empty array.
std::int32_t resolve_weapon_star_level(std::uint32_t hole_array_count) noexcept;

// Mirrors getAddHoleBase_atk(); star_level comes from the accessor above and
// weapon_field_7c is WeaponsItemData +0x7c.
float resolve_weapon_add_hole_base_attack(
    std::int32_t star_level, float weapon_field_7c) noexcept;

// Mirrors getAddTemperBase_atk() from WeaponsItemData +0xac and +0x7c.
float resolve_weapon_add_temper_base_attack(
    std::int32_t weapon_field_ac, float weapon_field_7c) noexcept;

// Mirrors WeaponsItemData::getFinalAddBase_atk() around its three helper
// results. The initial literal at 0x003458d0 is 0.0f.
WeaponFinalAddBaseResult resolve_weapon_final_add_base_attack(
    const WeaponFinalAddBaseTerms& terms) noexcept;

// Mirrors MEPlayer::getAttack()'s left-to-right float sum. The weapon getter
// and PlayerStatusInfo values remain inputs until their source data is restored.
float resolve_attack(const State& player, const AttackTerms& terms) noexcept;

// Mirrors MEPlayer::getAllDamageAbsorb(). Data-manager and status objects are
// supplied as decoded values; the aggregation and final multiplier are exact.
float resolve_damage_absorb(
        const State& player, const DamageAbsorbTerms& terms) noexcept;

// MEPlayer::setEquipAddAttack() writes the argument to +0x114.
void set_equipment_add_attack(State* state, float value) noexcept;

// The setters below mirror the native fields and arithmetic. Effective max
// health remains a separate, unrecovered calculation.
void set_equipment_add_health(State* state, float value) noexcept;
void add_equipment_add_health(State* state, float delta) noexcept;
void set_equipment_add_damage_absorb(State* state, float value) noexcept;
void set_tools_add_health(State* state, float value) noexcept;

// Native MEPlayer::setToolsDamageAbsorb() stores 1.0f - value at +0x430.
void set_tools_damage_absorb(State* state, float value) noexcept;

}  // namespace nevergone::player_combat_stat_contract
