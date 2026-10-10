#include "enemy_actions_combo_consumer.h"

namespace nevergone::enemy_actions_combo_consumer {

std::int32_t current_frame_power(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::uint32_t current_frame_index) {
    const std::size_t index = static_cast<std::size_t>(current_frame_index);
    if (index >= combo_block.array_9c_values.size()) return 1;
    return combo_block.array_9c_values[index].field_1c;
}

std::optional<std::int32_t> combo_hit_mode_for_frame(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::size_t range_index,
        std::int32_t current_frame_index) {
    if (combo_block.array_94_values.size() <= 1u ||
        range_index >= combo_block.array_94_values.size()) {
        return std::nullopt;
    }

    const auto& range = combo_block.array_94_values[range_index];
    if (current_frame_index < range.field_14 ||
        current_frame_index > range.field_18) {
        return std::nullopt;
    }
    return range.field_1c;
}

ComboHitTransition apply_combo_hit_transition(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::size_t range_index,
        std::int32_t current_frame_index,
        ComboHitState state) {
    ComboHitTransition result;
    result.state = state;

    if (state.flag_190 != 0u || state.flag_290 != 0u) return result;

    const auto mode = combo_hit_mode_for_frame(
            combo_block, range_index, current_frame_index);
    if (!mode.has_value()) return result;

    result.state.flag_191 = 1u;
    if (*mode == 2) result.state.flag_290 = 1u;
    result.returned_true = true;
    return result;
}

BoundaryCursorUpdate advance_boundary_cursor(
        const std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue>& values,
        std::size_t current_index,
        std::int32_t current_frame_index) {
    BoundaryCursorUpdate result;
    result.index = current_index;

    // The original only looks up an object when count != 0. Project-owned code
    // also rejects a stale index rather than reproducing CCArray undefined use.
    if (values.empty() || current_index >= values.size()) return result;

    const auto& current = values[current_index];
    if (current_frame_index < current.field_18) return result;

    const std::size_t incremented = current_index + 1u;
    if (incremented == values.size()) {
        // waUpdate stores incremented first, observes index == count, then
        // subtracts one. The final externally visible index stays on the last
        // boundary and the +0x94 path treats this as not advancing to a new run.
        result.index = current_index;
        result.reached_last_boundary = true;
        return result;
    }

    result.index = incremented;
    result.advanced_to_next = true;
    return result;
}

WaUpdateComboTransition apply_wa_update_combo_transition(
        const std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue>& array_94_values,
        std::int32_t current_frame_index,
        WaUpdateComboState state) {
    WaUpdateComboTransition result;
    result.state = state;
    result.boundary.index = state.boundary_index_2a0;

    if (state.flag_290 != 0u) result.state.flag_291 = 1u;

    if (state.flag_191 == 0u) return result;

    result.boundary = advance_boundary_cursor(
            array_94_values, state.boundary_index_2a0, current_frame_index);
    result.state.boundary_index_2a0 = result.boundary.index;
    return result;
}

}  // namespace nevergone::enemy_actions_combo_consumer
