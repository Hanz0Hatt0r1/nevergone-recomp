#pragma once

#include <cstdint>

namespace nevergone::enemy_attack_value_contract {

// EnemyObject::inputAtkValue(): unsigned EnemyObject+0x2bc converted to float,
// multiplied by EnemyActionsSystem::DamageFactor() for the current frame.
float resolve(std::uint32_t value_2bc, float current_frame_damage_factor) noexcept;

}  // namespace nevergone::enemy_attack_value_contract
