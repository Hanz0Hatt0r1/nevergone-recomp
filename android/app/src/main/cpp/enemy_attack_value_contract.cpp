#include "enemy_attack_value_contract.h"

namespace nevergone::enemy_attack_value_contract {

float resolve(std::uint32_t value_2bc, float current_frame_damage_factor) noexcept {
    return static_cast<float>(value_2bc) * current_frame_damage_factor;
}

}  // namespace nevergone::enemy_attack_value_contract
