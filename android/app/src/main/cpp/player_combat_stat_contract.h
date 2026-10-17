#pragma once

#include <cstdint>
#include <vector>

namespace nevergone::player_combat_stat_contract {

struct State {
    float equipment_add_attack_114 = 0.0f;
    float equipment_add_health_110 = 0.0f;
    std::int32_t effective_max_health_11c = 0;
    float equipment_add_damage_absorb_118 = 0.0f;
    float tools_add_health_120 = 0.0f;
    float tools_damage_absorb_factor_430 = 0.0f;
};

struct PlayerStatusInfoState {
    float cloth_add_health_138 = 0.0f;
    float cloth_add_strength_13c = 0.0f;
    float cloth_add_damage_absorb_140 = 0.0f;
    float cloth_add_attack_144 = 0.0f;
    float eat_property_148 = 0.0f;
    float eat_property_14c = 0.0f;
    float eat_property_150 = 0.0f;
    float eat_property_154 = 0.0f;
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

struct ManagementHealthEffect {
    std::uint32_t type = 0;
    std::uint32_t level = 0;
    float base_bonus = 0.0f;
    float growth_bonus = 0.0f;
};

struct AttributeHealthEffect {
    std::uint32_t type = 0;
    std::uint32_t level = 0;
    bool enabled = false;
    float per_level_bonus = 0.0f;
};

struct MaxHealthTerms {
    float player_level_health = 0.0f;
    bool use_management_level_effects = false;
    float parameter_bonus_304 = 0.0f;
    std::vector<ManagementHealthEffect> management_effects;
    std::vector<AttributeHealthEffect> attribute_effects;
    bool armband_equipped = false;
    float armband_health_multiplier_3c = 0.0f;
    std::int32_t armband_health_addition = 0;
    // Pre-resolved from the first type-6 entry when mercenaries are alive.
    bool has_living_type6_mercenary = false;
    std::int32_t type6_mercenary_health = 0;
    std::int32_t weapon_add_health = 0;
    float player_status_health_138 = 0.0f;
};

struct ManagementAttackEffect {
    std::uint32_t type = 0;
    std::uint32_t level = 0;
    float base_bonus = 0.0f;
    float growth_bonus = 0.0f;
};

struct AttributeAttackEffect {
    std::uint32_t type = 0;
    std::uint32_t level = 0;
    bool enabled = false;
    float per_level_bonus = 0.0f;
};

struct ExtraBaseAttackTerms {
    float weapon_add_base_attack = 0.0f;
    bool weapon_field_18_is_zero = false;
    bool use_management_level_effects = false;
    float parameter_bonus_2ec = 0.0f;
    std::vector<ManagementAttackEffect> management_effects;
    std::vector<AttributeAttackEffect> attribute_effects;
    bool armband_equipped = false;
    std::int32_t armband_attack_addition = 0;
    float armband_base_attack_38 = 0.0f;
    float equipment_property_bonus_30 = 0.0f;
};

struct NestedWeaponBaseAttackModifier {
    std::uint32_t type = 0;
    float value_when_weapon_field_18_is_zero = 0.0f;
    float value_when_weapon_field_18_is_one = 0.0f;
};

struct WeaponBaseAttackItem {
    std::uint32_t type = 0;
    bool weapon_name_matches = false;
    float regular_attack_addition = 0.0f;
    float matching_weapon_attack_addition = 0.0f;
    std::vector<NestedWeaponBaseAttackModifier> nested_modifiers;
};

struct WeaponBaseAttackTerms {
    std::int32_t weapon_field_18 = 0;
    float armband_effect_multiplier_50 = 0.0f;
    std::vector<WeaponBaseAttackItem> weapon_items;
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

// Mirrors MEPlayer::getMaxHealth(). Native manager lookups and the type-6
// mercenary map scan are supplied as decoded terms; the int cache write and
// returned float conversion match MEPlayer +0x11c.
float resolve_max_health(State* player, const MaxHealthTerms& terms) noexcept;

// Mirrors WeaponsItemData::getExtraBase_atk(); native manager and getter
// results are decoded inputs, while the mode and equipped-item gates are exact.
float resolve_extra_base_attack(const ExtraBaseAttackTerms& terms) noexcept;

// Mirrors WeaponsItemData::getAddBase_atk(); virtual item getters and string
// comparison outcomes are decoded per item, preserving the native iteration.
float resolve_weapon_add_base_attack(const WeaponBaseAttackTerms& terms) noexcept;

// Mirrors PlayerStatusInfo::add_C_AttackValue(); a nonzero management-layer
// gate suppresses the field +0x144 addition.
void add_cloth_attack_value(
        PlayerStatusInfoState* status, bool management_gate_nonzero,
        float value) noexcept;

void add_cloth_health_value(
        PlayerStatusInfoState* status, bool management_gate_nonzero,
        float value) noexcept;
void add_cloth_strength_value(
        PlayerStatusInfoState* status, bool management_gate_nonzero,
        float value) noexcept;
void add_cloth_damage_absorb_value(
        PlayerStatusInfoState* status, bool management_gate_nonzero,
        float value) noexcept;

// Mirrors resetPlayerStatusClothAddData() at +0x138, +0x13c, +0x140, +0x144.
void reset_cloth_add_values(PlayerStatusInfoState* status) noexcept;

struct EatAttributeRates {
    float per_unit = 0.0f;
    float per_thousand = 0.0f;
    float per_million = 0.0f;
};

struct HeroEatAttributeTerms {
    EatAttributeRates table_type_1;
    EatAttributeRates table_type_2;
    EatAttributeRates table_type_3;
    EatAttributeRates table_type_4;
};

struct EatPropertyTerms {
    std::int32_t property_type = 0;
    float addition = 0.0f;
    HeroEatAttributeTerms attribute_rates;
    bool management_gate_nonzero = false;
};

// Mirrors MRGetHeroEatAttribute(); MRGetEatAttribute() table results are
// supplied for each source type and tier.
float resolve_hero_eat_attribute(
        std::int32_t type, std::uint32_t count,
        const HeroEatAttributeTerms& terms) noexcept;

// Mirrors PlayerStatusInfo::add_EatProperty(); MRGetHeroEatAttribute remains
// data-derived through its table inputs, while the type, management, and cap
// gates are exact.
void add_eat_property(
        PlayerStatusInfoState* status, const EatPropertyTerms& terms) noexcept;

// MEPlayer::setEquipAddAttack() writes the argument to +0x114.
void set_equipment_add_attack(State* state, float value) noexcept;

// The setters below mirror the native fields and arithmetic. The max-health
// getter is reconstructed separately because it also reads manager data.
void set_equipment_add_health(State* state, float value) noexcept;
void add_equipment_add_health(State* state, float delta) noexcept;
void set_equipment_add_damage_absorb(State* state, float value) noexcept;
void set_tools_add_health(State* state, float value) noexcept;

// Native MEPlayer::setToolsDamageAbsorb() stores 1.0f - value at +0x430.
void set_tools_damage_absorb(State* state, float value) noexcept;

}  // namespace nevergone::player_combat_stat_contract
