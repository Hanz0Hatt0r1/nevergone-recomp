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
// selected range. A value is returned only when the selected +0x94 range
// contains the supplied frame inclusively; the result is ActionComboValue
// field +0x1c.
std::optional<std::int32_t> combo_hit_mode_for_frame(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::size_t range_index,
        std::int32_t current_frame_index);

// Offset-named byte state used by the proven comboHit() path. Values are kept
// as bytes instead of semantic booleans because broader state meanings remain
// unresolved; native gating treats any nonzero +0x190/+0x290 value as active.
struct ComboHitState {
    std::uint8_t flag_190 = 0;
    std::uint8_t flag_191 = 0;
    std::uint8_t flag_290 = 0;
};

struct ComboHitTransition {
    ComboHitState state;
    bool returned_true = false;
};

// Reconstructs the proven state-dependent portion of EnemyActionsSystem::comboHit().
// Native behavior:
//   - require +0x94 count > 1;
//   - require system+0x190 == 0 and system+0x290 == 0;
//   - require current frame inside selected [field_14, field_18];
//   - on match write 1 to system+0x191;
//   - when field_1c == 2 also write 1 to system+0x290;
//   - return true only on that matched path.
// Invalid project-owned indices are handled safely as a no-op/false result.
ComboHitTransition apply_combo_hit_transition(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::size_t range_index,
        std::int32_t current_frame_index,
        ComboHitState state);

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

// Narrow offset-named state slice proven around the +0x94 path in waUpdate().
// This deliberately excludes timing, action completion and the independent
// +0x98 cursor so that unresolved meanings are not folded into one abstraction.
struct WaUpdateComboState {
    std::uint8_t flag_191 = 0;
    std::uint8_t flag_290 = 0;
    std::uint8_t flag_291 = 0;
    std::size_t boundary_index_2a0 = 0;
};

struct WaUpdateComboTransition {
    WaUpdateComboState state;
    BoundaryCursorUpdate boundary;
};

// Reconstructs the bounded combo-related step after waUpdate() advances its
// current frame: nonzero +0x290 writes byte 1 to +0x291; nonzero +0x191 enables
// the already-proven +0x94 boundary cursor check. A zero +0x191 leaves +0x94
// untouched. Empty/stale project-owned arrays remain safely unchanged.
WaUpdateComboTransition apply_wa_update_combo_transition(
        const std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue>& array_94_values,
        std::int32_t current_frame_index,
        WaUpdateComboState state);

// Later in waUpdate(), native code has a strict post-endpoint branch that runs
// only when the +0x94 path did not make a real transition to a next boundary.
// Names remain offset-based because the gameplay meanings of these fields are
// not yet proven.
struct WaUpdateCompletionState {
    std::int32_t field_18c = 0;
    std::uint8_t flag_190 = 0;
    std::uint8_t flag_169 = 0;
    std::uint8_t flag_1bc = 0;
    std::uint8_t flag_292 = 0;
    std::int32_t current_frame_294 = 0;
};

struct WaUpdateCompletionTransition {
    WaUpdateCompletionState state;
    bool reset_applied = false;
};

// Evidence-backed late waUpdate() transition:
//   - do nothing when a real +0x94 next-boundary transition occurred;
//   - do nothing while current_frame <= current field_18 endpoint;
//   - otherwise store endpoint-1 at +0x18c, write 1 to +0x190, reset +0x294
//     to 0, write 1 to +0x1bc, and clear +0x169 only when +0x292 is zero.
// The independent updateData() path and surrounding animation/timing behavior
// remain outside this helper.
WaUpdateCompletionTransition apply_wa_update_completion_transition(
        std::int32_t boundary_endpoint_18,
        bool advanced_to_next_boundary,
        std::int32_t current_frame_index,
        WaUpdateCompletionState state);

}  // namespace nevergone::enemy_actions_combo_consumer
