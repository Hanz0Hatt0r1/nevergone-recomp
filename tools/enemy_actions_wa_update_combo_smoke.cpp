#include <cassert>

#include "enemy_actions_combo_consumer.h"

int main() {
    using nevergone::enemy_actions_combo_consumer::apply_wa_update_combo_transition;
    using nevergone::enemy_actions_combo_consumer::WaUpdateComboState;
    using nevergone::enemy_actions_wbg_combo_section::DerivedActionComboValue;
    using nevergone::enemy_actions_wbg_combo_section::kArray94Offset;
    using nevergone::enemy_actions_wbg_combo_section::kArray98Offset;

    std::vector<DerivedActionComboValue> values;

    DerivedActionComboValue first;
    first.target_array_offset = kArray94Offset;
    first.field_14 = 0;
    first.field_18 = 2;
    first.field_1c = 1;
    values.push_back(first);

    DerivedActionComboValue second;
    second.target_array_offset = kArray94Offset;
    second.field_14 = 3;
    second.field_18 = 5;
    second.field_1c = 2;
    values.push_back(second);

    WaUpdateComboState propagate;
    propagate.flag_290 = 7u;
    propagate.flag_291 = 9u;
    const auto propagated = apply_wa_update_combo_transition(values, 1, propagate);
    assert(propagated.state.flag_290 == 7u);
    assert(propagated.state.flag_291 == 1u);
    assert(propagated.state.boundary_index_2a0 == 0u);
    assert(propagated.boundary.index == 0u);
    assert(!propagated.boundary.advanced_to_next);
    assert(!propagated.boundary.reached_last_boundary);

    WaUpdateComboState no_propagate;
    no_propagate.flag_291 = 0x55u;
    const auto preserved = apply_wa_update_combo_transition(values, 1, no_propagate);
    assert(preserved.state.flag_291 == 0x55u);

    WaUpdateComboState cursor_gated;
    cursor_gated.boundary_index_2a0 = 0u;
    const auto gated = apply_wa_update_combo_transition(values, 2, cursor_gated);
    assert(gated.state.boundary_index_2a0 == 0u);
    assert(gated.boundary.index == 0u);
    assert(!gated.boundary.advanced_to_next);
    assert(!gated.boundary.reached_last_boundary);

    WaUpdateComboState cursor_enabled;
    cursor_enabled.flag_191 = 3u;
    cursor_enabled.boundary_index_2a0 = 0u;
    const auto advanced = apply_wa_update_combo_transition(values, 2, cursor_enabled);
    assert(advanced.state.flag_191 == 3u);
    assert(advanced.state.boundary_index_2a0 == 1u);
    assert(advanced.boundary.index == 1u);
    assert(advanced.boundary.advanced_to_next);
    assert(!advanced.boundary.reached_last_boundary);

    const auto before_endpoint = apply_wa_update_combo_transition(values, 1, cursor_enabled);
    assert(before_endpoint.state.boundary_index_2a0 == 0u);
    assert(!before_endpoint.boundary.advanced_to_next);
    assert(!before_endpoint.boundary.reached_last_boundary);

    WaUpdateComboState final_state;
    final_state.flag_191 = 1u;
    final_state.boundary_index_2a0 = 1u;
    const auto final_clamp = apply_wa_update_combo_transition(values, 5, final_state);
    assert(final_clamp.state.boundary_index_2a0 == 1u);
    assert(final_clamp.boundary.index == 1u);
    assert(!final_clamp.boundary.advanced_to_next);
    assert(final_clamp.boundary.reached_last_boundary);

    WaUpdateComboState combined;
    combined.flag_191 = 1u;
    combined.flag_290 = 1u;
    combined.boundary_index_2a0 = 0u;
    const auto both = apply_wa_update_combo_transition(values, 2, combined);
    assert(both.state.flag_291 == 1u);
    assert(both.state.boundary_index_2a0 == 1u);
    assert(both.boundary.advanced_to_next);

    WaUpdateComboState stale;
    stale.flag_191 = 1u;
    stale.boundary_index_2a0 = 9u;
    const auto stale_result = apply_wa_update_combo_transition(values, 99, stale);
    assert(stale_result.state.boundary_index_2a0 == 9u);
    assert(stale_result.boundary.index == 9u);
    assert(!stale_result.boundary.advanced_to_next);
    assert(!stale_result.boundary.reached_last_boundary);

    std::vector<DerivedActionComboValue> empty;
    const auto empty_result = apply_wa_update_combo_transition(empty, 99, cursor_enabled);
    assert(empty_result.state.boundary_index_2a0 == 0u);
    assert(empty_result.boundary.index == 0u);

    // +0x98 is an independent boundary stream selected by +0x2a4. It uses
    // the same endpoint/advance/clamp operation but is not gated by +0x191.
    std::vector<DerivedActionComboValue> values_98;
    DerivedActionComboValue first_98;
    first_98.target_array_offset = kArray98Offset;
    first_98.field_18 = 1;
    values_98.push_back(first_98);

    DerivedActionComboValue second_98;
    second_98.target_array_offset = kArray98Offset;
    second_98.field_18 = 4;
    values_98.push_back(second_98);

    WaUpdateComboState independent_98;
    independent_98.boundary_index_2a0 = 0u;
    independent_98.boundary_index_2a4 = 0u;
    const auto advanced_98 = apply_wa_update_combo_transition(
            values, values_98, 1, independent_98);
    assert(advanced_98.state.boundary_index_2a0 == 0u);
    assert(!advanced_98.boundary.advanced_to_next);
    assert(advanced_98.state.boundary_index_2a4 == 1u);
    assert(advanced_98.boundary_98.index == 1u);
    assert(advanced_98.boundary_98.advanced_to_next);
    assert(!advanced_98.boundary_98.reached_last_boundary);

    WaUpdateComboState final_98;
    final_98.boundary_index_2a4 = 1u;
    const auto clamp_98 = apply_wa_update_combo_transition(
            values, values_98, 4, final_98);
    assert(clamp_98.state.boundary_index_2a4 == 1u);
    assert(clamp_98.boundary_98.index == 1u);
    assert(!clamp_98.boundary_98.advanced_to_next);
    assert(clamp_98.boundary_98.reached_last_boundary);

    // Both streams may advance during the same update when +0x191 enables
    // +0x94 and the frame has reached both current endpoints.
    WaUpdateComboState dual;
    dual.flag_191 = 1u;
    dual.boundary_index_2a0 = 0u;
    dual.boundary_index_2a4 = 0u;
    const auto dual_advance = apply_wa_update_combo_transition(
            values, values_98, 2, dual);
    assert(dual_advance.state.boundary_index_2a0 == 1u);
    assert(dual_advance.boundary.advanced_to_next);
    assert(dual_advance.state.boundary_index_2a4 == 1u);
    assert(dual_advance.boundary_98.advanced_to_next);

    // Stale +0x98 state is isolated: it remains unchanged without affecting a
    // valid +0x94 transition in the same call.
    WaUpdateComboState stale_98;
    stale_98.flag_191 = 1u;
    stale_98.boundary_index_2a0 = 0u;
    stale_98.boundary_index_2a4 = 9u;
    const auto isolated_stale_98 = apply_wa_update_combo_transition(
            values, values_98, 2, stale_98);
    assert(isolated_stale_98.state.boundary_index_2a0 == 1u);
    assert(isolated_stale_98.boundary.advanced_to_next);
    assert(isolated_stale_98.state.boundary_index_2a4 == 9u);
    assert(isolated_stale_98.boundary_98.index == 9u);
    assert(!isolated_stale_98.boundary_98.advanced_to_next);
    assert(!isolated_stale_98.boundary_98.reached_last_boundary);

    return 0;
}
