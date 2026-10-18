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

struct WeaponFinalAddCategoryTerms {
    bool weapon_field_18_is_zero = false;
    bool management_hole_gate_nonzero = false;
    float add_temper_attack = 0.0f;
    float add_hole_attack = 0.0f;
    float extra_attack = 0.0f;
    float weapon_attack_field = 0.0f;
};

struct WeaponFinalAddCategoryResult {
    float value = 0.0f;
    bool extra_flag = false;
};

struct WeaponFinalAddSuprTerms {
    std::int32_t weapon_field_18 = 0;
    bool management_hole_gate_nonzero = false;
    bool weapon_field_d4_is_zero = false;
    bool data_manager_54_field_4c_is_zero = false;
    float add_hole_attack = 0.0f;
    float extra_attack = 0.0f;
    float add_temper_attack = 0.0f;
    float weapon_field_48 = 0.0f;
};

struct WeaponFinalAddSuprResult {
    float value = 0.0f;
    bool extra_flag_4c = false;
    bool special_flag_e2 = false;
};

struct NestedWeaponCategoryAttackModifier {
    std::uint32_t type = 0;
    float value_when_weapon_field_18_is_zero = 0.0f;
    float value_when_weapon_field_18_is_one = 0.0f;
};

struct WeaponCategoryAttackItem {
    std::uint32_t type = 0;
    bool weapon_name_matches = false;
    float regular_attack_addition = 0.0f;
    float matching_weapon_attack_addition = 0.0f;
    std::vector<NestedWeaponCategoryAttackModifier> nested_modifiers;
};

struct WeaponCategoryAttackTerms {
    std::int32_t weapon_field_18 = 0;
    float data_manager_54_field_50 = 0.0f;
    std::vector<WeaponCategoryAttackItem> weapon_items;
};

struct WeaponCategoryExtraAttackTerms {
    std::uint32_t category_type = 0;
    float weapon_add_attack = 0.0f;
    bool weapon_field_18_is_zero = false;
    bool use_management_level_effects = false;
    float parameter_attack_bonus = 0.0f;
    std::vector<ManagementAttackEffect> management_effects;
    float data_manager_54_field_38 = 0.0f;
    float equipment_property_bonus = 0.0f;
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

// Mirrors the shared getFinalAddMeat_atk()/getFinalAddSupe_atk() gates and
// output clamp. Category-specific helper results and weapon fields are inputs.
WeaponFinalAddCategoryResult resolve_weapon_final_add_category_attack(
    const WeaponFinalAddCategoryTerms& terms) noexcept;

// Mirrors getFinalAddSupr_atk(), including the DataManager and weapon-state
// gate which suppresses its returned value and clears its extra flag.
WeaponFinalAddSuprResult resolve_weapon_final_add_supr_attack(
    const WeaponFinalAddSuprTerms& terms) noexcept;

// Category temper helpers share the doubled signed +0xac formula. Meat and
// Supr pass their recovered byte gates; Supe always enables the helper.
float resolve_weapon_add_category_temper_attack(
    bool category_temper_gate_clear, std::int32_t weapon_field_ac,
    float category_scale) noexcept;

// Meat, Supe, and Supr hole helpers all use the getStarLevel-derived formula.
float resolve_weapon_add_category_hole_attack(
    std::int32_t star_level, float category_scale) noexcept;

// Mirrors the getAddMeat/Supe/Supr_atk() item-array scan. Category types and
// virtual getter results are decoded inputs; array and nested order is kept.
float resolve_weapon_add_category_attack(
    const WeaponCategoryAttackTerms& terms,
    std::uint32_t category_type) noexcept;

// Mirrors the shared getExtraMeat/Supe/Supr_atk() composition. Manager and
// getter outputs remain inputs; category gates and addition order are exact.
float resolve_weapon_extra_category_attack(
    const WeaponCategoryExtraAttackTerms& terms) noexcept;

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

struct EatAttributeCsvRow {
    std::int32_t tier = 0;
    std::int32_t type = 0;
    float column_3 = 0.0f;
    float column_4 = 0.0f;
    float column_5 = 0.0f;
    float column_6 = 0.0f;
};

struct EatPropertyTerms {
    std::int32_t property_type = 0;
    float addition = 0.0f;
    // Data rows only, in the source CSV order.
    std::vector<EatAttributeCsvRow> attribute_rows;
    bool management_gate_nonzero = false;
};

// Mirrors MRGetEatAttribute(): scan source rows in order for CSV column 9=tier
// and column 10=type, then return the requested value from columns 3 through 6.
float resolve_eat_attribute(
        std::int32_t type, std::int32_t tier,
        const std::vector<EatAttributeCsvRow>& rows) noexcept;

// Mirrors MRGetHeroEatAttribute() using rows parsed from the original CSV.
float resolve_hero_eat_attribute(
        std::int32_t type, std::uint32_t count,
        const std::vector<EatAttributeCsvRow>& rows) noexcept;

// Mirrors PlayerStatusInfo::add_EatProperty() with source-derived CSV values.
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
