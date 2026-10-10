#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "enemy_actions_wbg_combo_section.h"

namespace nevergone::enemy_actions_combo_consumer {

std::int32_t current_frame_power(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::uint32_t current_frame_index);

std::optional<std::int32_t> combo_hit_mode_for_frame(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::size_t range_index,
        std::int32_t current_frame_index);

struct ComboHitState {
    std::uint8_t flag_190 = 0;
    std::uint8_t flag_191 = 0;
    std::uint8_t flag_290 = 0;
};

struct ComboHitTransition {
    ComboHitState state;
    bool returned_true = false;
};

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

BoundaryCursorUpdate advance_boundary_cursor(
        const std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue>& values,
        std::size_t current_index,
        std::int32_t current_frame_index);

// Offset-named waUpdate state for the two proven Section-F boundary streams.
// +0x94 is selected by system index +0x2a0 and gated by +0x191. +0x98 is
// selected by +0x2a4 and advances independently of that byte gate.
struct WaUpdateComboState {
    std::uint8_t flag_191 = 0;
    std::uint8_t flag_290 = 0;
    std::uint8_t flag_291 = 0;
    std::size_t boundary_index_2a0 = 0;
    std::size_t boundary_index_2a4 = 0;
};

struct WaUpdateComboTransition {
    WaUpdateComboState state;
    // Existing field retained for compatibility: this is the +0x94 / +0x2a0 path.
    BoundaryCursorUpdate boundary;
    // Independent +0x98 / +0x2a4 path.
    BoundaryCursorUpdate boundary_98;
};

// Backward-compatible +0x94-only slice. This leaves +0x98/+0x2a4 untouched.
WaUpdateComboTransition apply_wa_update_combo_transition(
        const std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue>& array_94_values,
        std::int32_t current_frame_index,
        WaUpdateComboState state);

// Complete currently proven dual-boundary slice after waUpdate() advances its
// current frame:
//   - nonzero +0x290 writes byte 1 to +0x291;
//   - nonzero +0x191 enables +0x94/+0x2a0 endpoint advance/clamp;
//   - +0x98/+0x2a4 endpoint advance/clamp runs independently of +0x191.
// Empty/stale project-owned arrays are handled as safe no-ops for their own
// cursor only.
WaUpdateComboTransition apply_wa_update_combo_transition(
        const std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue>& array_94_values,
        const std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue>& array_98_values,
        std::int32_t current_frame_index,
        WaUpdateComboState state);

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

WaUpdateCompletionTransition apply_wa_update_completion_transition(
        std::int32_t boundary_endpoint_18,
        bool advanced_to_next_boundary,
        std::int32_t current_frame_index,
        WaUpdateCompletionState state);

}  // namespace nevergone::enemy_actions_combo_consumer
