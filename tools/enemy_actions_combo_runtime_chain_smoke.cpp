#include <cassert>
#include <cstdint>

#include "enemy_actions_combo_consumer.h"

int main() {
    namespace consumer = nevergone::enemy_actions_combo_consumer;
    using nevergone::enemy_actions_wbg_combo_section::Block;
    using nevergone::enemy_actions_wbg_combo_section::DerivedActionComboValue;
    using nevergone::enemy_actions_wbg_combo_section::kArray94Offset;

    Block block;

    DerivedActionComboValue first;
    first.target_array_offset = kArray94Offset;
    first.field_14 = 0;
    first.field_18 = 2;
    first.field_1c = 1;
    block.array_94_values.push_back(first);

    DerivedActionComboValue final;
    final.target_array_offset = kArray94Offset;
    final.field_14 = 3;
    final.field_18 = 5;
    final.field_1c = 2;
    block.array_94_values.push_back(final);

    // Proven chain: comboHit mode 2 raises +0x191/+0x290.
    const auto hit = consumer::apply_combo_hit_transition(
            block, 1u, 4, consumer::ComboHitState{});
    assert(hit.returned_true);
    assert(hit.state.flag_190 == 0u);
    assert(hit.state.flag_191 == 1u);
    assert(hit.state.flag_290 == 1u);

    // Carry only identical offset-named system bytes into the next recovered
    // waUpdate slice. At the final endpoint, +0x290 propagates to +0x291 and
    // +0x94 clamps on the final boundary instead of making a real advance.
    consumer::WaUpdateComboState update_state;
    update_state.flag_191 = hit.state.flag_191;
    update_state.flag_290 = hit.state.flag_290;
    update_state.boundary_index_2a0 = 1u;
    const auto at_final_endpoint = consumer::apply_wa_update_combo_transition(
            block.array_94_values, 5, update_state);
    assert(at_final_endpoint.state.flag_291 == 1u);
    assert(at_final_endpoint.state.boundary_index_2a0 == 1u);
    assert(!at_final_endpoint.boundary.advanced_to_next);
    assert(at_final_endpoint.boundary.reached_last_boundary);

    // One frame later, absence of a real +0x94 advance plus frame > endpoint
    // enters the proven late post-endpoint write set.
    consumer::WaUpdateCompletionState completion_state;
    completion_state.flag_190 = hit.state.flag_190;
    completion_state.flag_169 = 9u;
    completion_state.flag_1bc = 0u;
    completion_state.flag_292 = 0u;
    completion_state.current_frame_294 = 6;
    const auto completed = consumer::apply_wa_update_completion_transition(
            final.field_18,
            at_final_endpoint.boundary.advanced_to_next,
            6,
            completion_state);
    assert(completed.reset_applied);
    assert(completed.state.field_18c == 4);
    assert(completed.state.flag_190 == 1u);
    assert(completed.state.current_frame_294 == 0);
    assert(completed.state.flag_1bc == 1u);
    assert(completed.state.flag_169 == 0u);

    // A real transition from the first +0x94 boundary suppresses that late
    // reset block even though the frame is already past the old endpoint.
    consumer::WaUpdateComboState advancing_state;
    advancing_state.flag_191 = 1u;
    advancing_state.boundary_index_2a0 = 0u;
    const auto advanced = consumer::apply_wa_update_combo_transition(
            block.array_94_values, 3, advancing_state);
    assert(advanced.boundary.advanced_to_next);
    assert(!advanced.boundary.reached_last_boundary);
    assert(advanced.state.boundary_index_2a0 == 1u);

    consumer::WaUpdateCompletionState preserved;
    preserved.field_18c = 77;
    preserved.flag_190 = 8u;
    preserved.flag_169 = 7u;
    preserved.flag_1bc = 6u;
    preserved.current_frame_294 = 3;
    const auto suppressed = consumer::apply_wa_update_completion_transition(
            first.field_18,
            advanced.boundary.advanced_to_next,
            3,
            preserved);
    assert(!suppressed.reset_applied);
    assert(suppressed.state.field_18c == 77);
    assert(suppressed.state.flag_190 == 8u);
    assert(suppressed.state.flag_169 == 7u);
    assert(suppressed.state.flag_1bc == 6u);
    assert(suppressed.state.current_frame_294 == 3);

    return 0;
}
