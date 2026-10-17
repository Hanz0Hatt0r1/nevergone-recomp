#pragma once

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

// Mirrors WeaponsItemData::getFinalAddBase_atk() around its three helper
// results. The initial literal at 0x003458d0 is 0.0f.
WeaponFinalAddBaseResult resolve_weapon_final_add_base_attack(
    const WeaponFinalAddBaseTerms& terms) noexcept;

// Mirrors MEPlayer::getAttack()'s left-to-right float sum. The weapon getter
// and PlayerStatusInfo values remain inputs until their source data is restored.
float resolve_attack(const State& player, const AttackTerms& terms) noexcept;

// MEPlayer::setEquipAddAttack() writes the argument to +0x114.
void set_equipment_add_attack(State* state, float value) noexcept;

// The setters below mirror the native fields and arithmetic. Effective max
// health and total damage absorption are computed by larger, unrecovered
// functions and are intentionally not derived here.
void set_equipment_add_health(State* state, float value) noexcept;
void add_equipment_add_health(State* state, float delta) noexcept;
void set_equipment_add_damage_absorb(State* state, float value) noexcept;
void set_tools_add_health(State* state, float value) noexcept;

// Native MEPlayer::setToolsDamageAbsorb() stores 1.0f - value at +0x430.
void set_tools_damage_absorb(State* state, float value) noexcept;

}  // namespace nevergone::player_combat_stat_contract
