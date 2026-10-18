#pragma once

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

// Reconstructs the weapon-versus-enemy power curve and weighted core damage.
// The attack-category inputs are already resolved through their native
// equipment, level, property, weakness, and attack-state paths.
float resolve(const Terms& terms) noexcept;

}  // namespace nevergone::player_damage_core_contract
