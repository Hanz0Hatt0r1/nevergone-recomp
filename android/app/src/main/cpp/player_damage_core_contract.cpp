#include "player_damage_core_contract.h"

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

}  // namespace nevergone::player_damage_core_contract
