#include <cassert>

#include "enemy_actions_runtime_state.h"

int main() {
    namespace runtime = nevergone::enemy_actions_runtime_state;
    namespace document_ns = nevergone::enemy_actions_wbg_document;
    using nevergone::enemy_actions_wbg_combo_section::DerivedActionComboValue;
    using nevergone::enemy_actions_wbg_combo_section::kArray94Offset;
    using nevergone::enemy_actions_wbg_combo_section::kArray98Offset;
    using nevergone::enemy_actions_wbg_final_table::Entry;

    document_ns::Document document;
    document.prefix.action_frame_count = 2u;

    Entry frame0;
    frame0.serialized_value = 2;
    frame0.reciprocal_value = 0.5f;
    document.final_table.entries.push_back(frame0);

    Entry frame1;
    frame1.serialized_value = 2;
    frame1.reciprocal_value = 0.5f;
    document.final_table.entries.push_back(frame1);

    DerivedActionComboValue range0;
    range0.target_array_offset = kArray94Offset;
    range0.field_18 = 1;
    document.combo_block.array_94_values.push_back(range0);

    DerivedActionComboValue range1;
    range1.target_array_offset = kArray94Offset;
    range1.field_18 = 3;
    document.combo_block.array_94_values.push_back(range1);

    DerivedActionComboValue aux0;
    aux0.target_array_offset = kArray98Offset;
    aux0.field_18 = 1;
    document.combo_block.array_98_values.push_back(aux0);

    DerivedActionComboValue aux1;
    aux1.target_array_offset = kArray98Offset;
    aux1.field_18 = 4;
    document.combo_block.array_98_values.push_back(aux1);

    // Below timing threshold: only the accumulator/threshold state changes.
    runtime::State below;
    below.flag_169 = 1u;
    below.field_15c = 1.0f;
    below.field_160 = 0.0f;
    below.current_frame_294 = 0;
    const auto below_result = runtime::apply_wa_update_iteration(
            document, 0.001f, below);
    assert(below_result.timing.had_valid_action_frame);
    assert(!below_result.timing.threshold_reached);
    assert(!below_result.timing.frame_processing_allowed);
    assert(!below_result.frame_step_applied);
    assert(below_result.state.current_frame_294 == 0);
    assert(below_result.state.field_158 == 1.5f);
    assert(below_result.state.field_160 == 1.0f);

    // Reaching threshold executes exactly one frame step. The timing helper
    // subtracts one threshold, then frame 0 -> 1 advances both boundary streams.
    runtime::State ready;
    ready.flag_169 = 1u;
    ready.flag_191 = 1u;
    ready.field_15c = 0.5f;
    ready.field_160 = 1.0f;
    ready.previous_frame_154 = 0;
    ready.current_frame_294 = 0;
    const auto ready_result = runtime::apply_wa_update_iteration(
            document, 0.0f, ready);
    assert(ready_result.timing.threshold_reached);
    assert(ready_result.timing.frame_processing_allowed);
    assert(ready_result.timing.state.field_160 == 0.0f);
    assert(ready_result.frame_step_applied);
    assert(ready_result.frame_step.had_valid_boundary_94);
    assert(ready_result.frame_step.boundary_94.advanced_to_next);
    assert(ready_result.frame_step.boundary_98.advanced_to_next);
    assert(ready_result.state.current_frame_294 == 1);
    assert(ready_result.state.boundary_index_2a0 == 1u);
    assert(ready_result.state.boundary_index_2a4 == 1u);
    assert(ready_result.frame_step.previous_frame_changed);
    assert(ready_result.frame_step.should_call_update_data);
    assert(ready_result.state.previous_frame_154 == 1);

    // +0x1bc top gate stops before both timing mutation and frame processing.
    runtime::State blocked = ready;
    blocked.flag_1bc = 1u;
    blocked.field_160 = 7.0f;
    const auto blocked_result = runtime::apply_wa_update_iteration(
            document, 1.0f, blocked);
    assert(blocked_result.timing.blocked_by_flag_1bc);
    assert(!blocked_result.frame_step_applied);
    assert(blocked_result.state.field_160 == 7.0f);
    assert(blocked_result.state.current_frame_294 == 0);

    // This helper intentionally performs one native processing iteration only.
    // A large accumulator retains excess time after one threshold subtraction;
    // it does not loop and consume a second frame in this call.
    runtime::State excess = ready;
    excess.field_160 = 3.0f;
    const auto excess_result = runtime::apply_wa_update_iteration(
            document, 0.0f, excess);
    assert(excess_result.frame_step_applied);
    assert(excess_result.timing.state.field_160 == 2.0f);
    assert(excess_result.state.current_frame_294 == 1);

    return 0;
}
