#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

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

struct BoundaryCursorUpdate {
    std::size_t index = 0;
    bool advanced_to_next = false;
    bool reached_last_boundary = false;
};

// Data-dependent boundary-index update used by EnemyActionsSystem::waUpdate().
// For a valid current index, field +0x18 is the inclusive boundary endpoint.
// When current_frame_index reaches/passes it, the index increments. If that
// increment equals count, native code immediately decrements back to the last
// valid element instead of moving beyond the array.
BoundaryCursorUpdate advance_boundary_cursor(
        const std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue>& values,
        std::size_t current_index,
        std::int32_t current_frame_index);

}  // namespace nevergone::enemy_actions_combo_consumer
