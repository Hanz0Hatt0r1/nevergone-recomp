#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "enemy_actions_wbg_combo_section.h"

namespace nevergone::enemy_actions_combo_consumer {

// Evidence-backed data-dependent portion of EnemyActionsSystem::curFramePower().
// The original returns 1 when the current frame index is outside +0x9c.
std::int32_t current_frame_power(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::uint32_t current_frame_index);

// Evidence-backed data-dependent portion of EnemyActionsSystem::comboHit().
// The original requires +0x94 to contain more than one value before testing the
// selected range. External EnemyActionsSystem state flags remain outside this
// helper. A value is returned only when the selected +0x94 range contains the
// supplied frame inclusively; the result is ActionComboValue field +0x1c.
std::optional<std::int32_t> combo_hit_mode_for_frame(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::size_t range_index,
        std::int32_t current_frame_index);

}  // namespace nevergone::enemy_actions_combo_consumer
