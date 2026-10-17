#pragma once

#include <cstdint>
#include <optional>

#include "enemy_actions_wbg_document.h"

namespace nevergone::enemy_attack_value_contract {

// EnemyObject::loadEnemyObjectFile(): GameLevelsEnemyData+0x20 is converted
// with VCVT.U32.F32 and stored at EnemyObject+0x2bc. Fail closed outside the
// finite in-range values for which the native conversion is fully defined.
std::optional<std::uint32_t> try_initialize_value_2bc(
    float game_levels_value_20) noexcept;

// EnemyObject::inputAtkValue(): unsigned EnemyObject+0x2bc converted to float,
// multiplied by EnemyActionsSystem::DamageFactor() for the current frame.
float resolve(std::uint32_t value_2bc, float current_frame_damage_factor) noexcept;

// Section F writes tuple[i].float_values[0] to EnemyActionsData+0xd8+4*i.
// Resolve that recovered table entry for EnemyActionsSystem+0x294 and apply it.
std::optional<float> resolve_at_frame(
    std::uint32_t value_2bc,
    const enemy_actions_wbg_document::Document& action,
    std::int32_t current_frame) noexcept;

}  // namespace nevergone::enemy_attack_value_contract
