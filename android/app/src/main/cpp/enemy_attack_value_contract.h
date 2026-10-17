#pragma once

#include <cstdint>
#include <optional>

namespace nevergone::enemy_attack_value_contract {

// EnemyObject::loadEnemyObjectFile(): GameLevelsEnemyData+0x20 is converted
// with VCVT.U32.F32 and stored at EnemyObject+0x2bc. Fail closed outside the
// finite in-range values for which the native conversion is fully defined.
std::optional<std::uint32_t> try_initialize_value_2bc(
    float game_levels_value_20) noexcept;

// EnemyObject::inputAtkValue(): unsigned EnemyObject+0x2bc converted to float,
// multiplied by EnemyActionsSystem::DamageFactor() for the current frame.
float resolve(std::uint32_t value_2bc, float current_frame_damage_factor) noexcept;

}  // namespace nevergone::enemy_attack_value_contract
