#include "enemy_actions_runtime_state.h"

#include <cmath>

namespace nevergone::enemy_actions_runtime_state {

ComboHitResult apply_combo_hit(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        State state) {
    enemy_actions_combo_consumer::ComboHitState hit_state;
    hit_state.flag_190 = state.flag_190;
    hit_state.flag_191 = state.flag_191;
    hit_state.flag_290 = state.flag_290;

    const auto hit = enemy_actions_combo_consumer::apply_combo_hit_transition(
            combo_block,
            state.boundary_index_2a0,
            state.current_frame_294,
            hit_state);

    state.flag_190 = hit.state.flag_190;
    state.flag_191 = hit.state.flag_191;
    state.flag_290 = hit.state.flag_290;

    ComboHitResult result;
    result.state = state;
    result.returned_true = hit.returned_true;
    return result;
}

WaUpdateTimingResult apply_wa_update_timing(
        const enemy_actions_wbg_final_table::Table& final_table,
        float delta_seconds,
        State state) {
    WaUpdateTimingResult result;
    result.state = state;

    // Native waUpdate() exits before touching the timing state when +0x1bc is
    // already nonzero.
    if (state.flag_1bc != 0u) {
        result.blocked_by_flag_1bc = true;
        return result;
    }

    if (state.flag_168 != 0u) {
        result.state.flag_168 = 0u;
        result.state.field_160 = 0.0f;
    } else {
        result.state.field_160 = state.field_160 + delta_seconds * 1000.0f;
    }

    if (state.current_frame_294 < 0) return result;
    const std::size_t frame_index = static_cast<std::size_t>(state.current_frame_294);
    if (frame_index >= final_table.entries.size()) return result;

    result.had_valid_action_frame = true;
    result.action_frame_5c = final_table.entries[frame_index].reciprocal_value;
    result.state.field_158 = result.action_frame_5c + state.field_15c;

    // ARM VCMPE followed by BLT also branches for unordered/NaN inputs. Mirror
    // that behavior explicitly instead of relying on C++ NaN comparison rules.
    if (std::isnan(result.state.field_160) || std::isnan(result.state.field_158) ||
        result.state.field_160 < result.state.field_158) {
        return result;
    }

    result.threshold_reached = true;
    if (state.flag_169 == 0u) return result;

    result.state.field_160 -= result.state.field_158;
    result.frame_processing_allowed = true;
    return result;
}

WaUpdateAfterFrameResult apply_wa_update_after_frame_advance(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        State state) {
    WaUpdateAfterFrameResult result;
    result.state = state;

    // Native later reuses the +0x94 object's pre-transition +0x18 endpoint.
    // Capture it before either cursor is advanced. Invalid reconstructed state
    // safely skips only the late completion transition.
    if (state.boundary_index_2a0 < combo_block.array_94_values.size()) {
        result.had_valid_boundary_94 = true;
        result.boundary_94_endpoint_18 =
                combo_block.array_94_values[state.boundary_index_2a0].field_18;
    }

    enemy_actions_combo_consumer::WaUpdateComboState combo_state;
    combo_state.flag_191 = state.flag_191;
    combo_state.flag_290 = state.flag_290;
    combo_state.flag_291 = state.flag_291;
    combo_state.boundary_index_2a0 = state.boundary_index_2a0;
    combo_state.boundary_index_2a4 = state.boundary_index_2a4;

    const auto combo = enemy_actions_combo_consumer::apply_wa_update_combo_transition(
            combo_block.array_94_values,
            combo_block.array_98_values,
            state.current_frame_294,
            combo_state);

    result.boundary_94 = combo.boundary;
    result.boundary_98 = combo.boundary_98;
    result.state.flag_191 = combo.state.flag_191;
    result.state.flag_290 = combo.state.flag_290;
    result.state.flag_291 = combo.state.flag_291;
    result.state.boundary_index_2a0 = combo.state.boundary_index_2a0;
    result.state.boundary_index_2a4 = combo.state.boundary_index_2a4;

    if (!result.had_valid_boundary_94) return result;

    enemy_actions_combo_consumer::WaUpdateCompletionState completion_state;
    completion_state.field_18c = result.state.field_18c;
    completion_state.flag_190 = result.state.flag_190;
    completion_state.flag_169 = result.state.flag_169;
    completion_state.flag_1bc = result.state.flag_1bc;
    completion_state.flag_292 = result.state.flag_292;
    completion_state.current_frame_294 = result.state.current_frame_294;

    const auto completion =
            enemy_actions_combo_consumer::apply_wa_update_completion_transition(
                    result.boundary_94_endpoint_18,
                    result.boundary_94.advanced_to_next,
                    result.state.current_frame_294,
                    completion_state);

    result.state.field_18c = completion.state.field_18c;
    result.state.flag_190 = completion.state.flag_190;
    result.state.flag_169 = completion.state.flag_169;
    result.state.flag_1bc = completion.state.flag_1bc;
    result.state.flag_292 = completion.state.flag_292;
    result.state.current_frame_294 = completion.state.current_frame_294;
    result.reset_applied = completion.reset_applied;
    return result;
}

}  // namespace nevergone::enemy_actions_runtime_state
