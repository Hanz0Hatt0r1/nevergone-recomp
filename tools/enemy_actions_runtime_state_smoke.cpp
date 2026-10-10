#include <cassert>

#include "enemy_actions_runtime_state.h"

int main() {
    namespace runtime = nevergone::enemy_actions_runtime_state;
    using nevergone::enemy_actions_wbg_combo_section::Block;
    using nevergone::enemy_actions_wbg_combo_section::DerivedActionComboValue;
    using nevergone::enemy_actions_wbg_combo_section::kArray94Offset;
    using nevergone::enemy_actions_wbg_combo_section::kArray98Offset;

    Block block;

    DerivedActionComboValue range0;
    range0.target_array_offset = kArray94Offset;
    range0.field_14 = 0;
    range0.field_18 = 2;
    range0.field_1c = 1;
    block.array_94_values.push_back(range0);

    DerivedActionComboValue range1;
    range1.target_array_offset = kArray94Offset;
    range1.field_14 = 3;
    range1.field_18 = 5;
    range1.field_1c = 2;
    block.array_94_values.push_back(range1);

    DerivedActionComboValue aux0;
    aux0.target_array_offset = kArray98Offset;
    aux0.field_18 = 1;
    block.array_98_values.push_back(aux0);

    DerivedActionComboValue aux1;
    aux1.target_array_offset = kArray98Offset;
    aux1.field_18 = 7;
    block.array_98_values.push_back(aux1);

    runtime::State state;
    state.flag_169 = 9u;
    state.boundary_index_2a0 = 1u;
    state.boundary_index_2a4 = 0u;
    state.current_frame_294 = 4;

    const auto hit = runtime::apply_combo_hit(block, state);
    assert(hit.returned_true);
    assert(hit.state.flag_191 == 1u);
    assert(hit.state.flag_290 == 1u);
    assert(hit.state.boundary_index_2a0 == 1u);
    assert(hit.state.current_frame_294 == 4);

    // Caller supplies the frame after the unresolved native timing/increment
    // logic. At endpoint 5, +0x291 propagates, +0x94 clamps, +0x98 has already
    // advanced to its second boundary, and strict post-endpoint reset is absent.
    auto at_endpoint_state = hit.state;
    at_endpoint_state.current_frame_294 = 5;
    const auto at_endpoint = runtime::apply_wa_update_after_frame_advance(
            block, at_endpoint_state);
    assert(at_endpoint.had_valid_boundary_94);
    assert(at_endpoint.boundary_94_endpoint_18 == 5);
    assert(at_endpoint.state.flag_291 == 1u);
    assert(at_endpoint.state.boundary_index_2a0 == 1u);
    assert(at_endpoint.boundary_94.reached_last_boundary);
    assert(!at_endpoint.boundary_94.advanced_to_next);
    assert(at_endpoint.state.boundary_index_2a4 == 1u);
    assert(at_endpoint.boundary_98.advanced_to_next);
    assert(!at_endpoint.reset_applied);
    assert(at_endpoint.state.current_frame_294 == 5);

    // One frame past the final +0x94 endpoint applies the proven late write set.
    auto past_endpoint_state = at_endpoint.state;
    past_endpoint_state.current_frame_294 = 6;
    const auto past_endpoint = runtime::apply_wa_update_after_frame_advance(
            block, past_endpoint_state);
    assert(past_endpoint.reset_applied);
    assert(past_endpoint.state.field_18c == 4);
    assert(past_endpoint.state.flag_190 == 1u);
    assert(past_endpoint.state.current_frame_294 == 0);
    assert(past_endpoint.state.flag_1bc == 1u);
    assert(past_endpoint.state.flag_169 == 0u);
    assert(past_endpoint.state.flag_291 == 1u);
    assert(past_endpoint.state.boundary_index_2a0 == 1u);
    assert(past_endpoint.state.boundary_index_2a4 == 1u);

    // The newly raised +0x190 gates a later comboHit even if frame/index match.
    auto gated_hit_state = past_endpoint.state;
    gated_hit_state.current_frame_294 = 4;
    const auto gated_hit = runtime::apply_combo_hit(block, gated_hit_state);
    assert(!gated_hit.returned_true);
    assert(gated_hit.state.flag_190 == 1u);

    // A real +0x94 transition suppresses the late reset while +0x98 remains
    // independently updateable.
    runtime::State advancing;
    advancing.flag_191 = 1u;
    advancing.flag_169 = 6u;
    advancing.boundary_index_2a0 = 0u;
    advancing.boundary_index_2a4 = 0u;
    advancing.current_frame_294 = 3;
    const auto advanced = runtime::apply_wa_update_after_frame_advance(block, advancing);
    assert(advanced.had_valid_boundary_94);
    assert(advanced.boundary_94_endpoint_18 == 2);
    assert(advanced.boundary_94.advanced_to_next);
    assert(advanced.state.boundary_index_2a0 == 1u);
    assert(advanced.boundary_98.advanced_to_next);
    assert(advanced.state.boundary_index_2a4 == 1u);
    assert(!advanced.reset_applied);
    assert(advanced.state.flag_190 == 0u);
    assert(advanced.state.flag_169 == 6u);
    assert(advanced.state.current_frame_294 == 3);

    // Invalid +0x94 reconstructed state safely skips completion, while a valid
    // +0x98 stream still advances independently.
    runtime::State invalid_94;
    invalid_94.flag_191 = 1u;
    invalid_94.boundary_index_2a0 = 99u;
    invalid_94.boundary_index_2a4 = 0u;
    invalid_94.current_frame_294 = 2;
    const auto safe_invalid = runtime::apply_wa_update_after_frame_advance(
            block, invalid_94);
    assert(!safe_invalid.had_valid_boundary_94);
    assert(!safe_invalid.reset_applied);
    assert(safe_invalid.state.boundary_index_2a0 == 99u);
    assert(safe_invalid.state.boundary_index_2a4 == 1u);
    assert(safe_invalid.boundary_98.advanced_to_next);
    assert(safe_invalid.state.current_frame_294 == 2);

    return 0;
}
