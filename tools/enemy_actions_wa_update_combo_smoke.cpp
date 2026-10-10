#include <cassert>

#include "enemy_actions_combo_consumer.h"

int main() {
    using nevergone::enemy_actions_combo_consumer::apply_wa_update_combo_transition;
    using nevergone::enemy_actions_combo_consumer::WaUpdateComboState;
    using nevergone::enemy_actions_wbg_combo_section::DerivedActionComboValue;
    using nevergone::enemy_actions_wbg_combo_section::kArray94Offset;

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

    // Nonzero +0x290 writes exact byte 1 to +0x291 independently of +0x191.
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

    // Zero +0x290 preserves +0x291 exactly.
    WaUpdateComboState no_propagate;
    no_propagate.flag_291 = 0x55u;
    const auto preserved = apply_wa_update_combo_transition(values, 1, no_propagate);
    assert(preserved.state.flag_291 == 0x55u);

    // Zero +0x191 gates the +0x94 cursor even after the endpoint is reached.
    WaUpdateComboState cursor_gated;
    cursor_gated.boundary_index_2a0 = 0u;
    const auto gated = apply_wa_update_combo_transition(values, 2, cursor_gated);
    assert(gated.state.boundary_index_2a0 == 0u);
    assert(gated.boundary.index == 0u);
    assert(!gated.boundary.advanced_to_next);
    assert(!gated.boundary.reached_last_boundary);

    // Nonzero +0x191 enables the proven endpoint check and normal advance.
    WaUpdateComboState cursor_enabled;
    cursor_enabled.flag_191 = 3u;
    cursor_enabled.boundary_index_2a0 = 0u;
    const auto advanced = apply_wa_update_combo_transition(values, 2, cursor_enabled);
    assert(advanced.state.flag_191 == 3u);
    assert(advanced.state.boundary_index_2a0 == 1u);
    assert(advanced.boundary.index == 1u);
    assert(advanced.boundary.advanced_to_next);
    assert(!advanced.boundary.reached_last_boundary);

    // Before the endpoint the enabled path still leaves the index unchanged.
    const auto before_endpoint = apply_wa_update_combo_transition(values, 1, cursor_enabled);
    assert(before_endpoint.state.boundary_index_2a0 == 0u);
    assert(!before_endpoint.boundary.advanced_to_next);
    assert(!before_endpoint.boundary.reached_last_boundary);

    // At the final endpoint native increment->count->decrement clamps to last.
    WaUpdateComboState final_state;
    final_state.flag_191 = 1u;
    final_state.boundary_index_2a0 = 1u;
    const auto final_clamp = apply_wa_update_combo_transition(values, 5, final_state);
    assert(final_clamp.state.boundary_index_2a0 == 1u);
    assert(final_clamp.boundary.index == 1u);
    assert(!final_clamp.boundary.advanced_to_next);
    assert(final_clamp.boundary.reached_last_boundary);

    // The two proven operations happen in the same bounded step.
    WaUpdateComboState combined;
    combined.flag_191 = 1u;
    combined.flag_290 = 1u;
    combined.boundary_index_2a0 = 0u;
    const auto both = apply_wa_update_combo_transition(values, 2, combined);
    assert(both.state.flag_291 == 1u);
    assert(both.state.boundary_index_2a0 == 1u);
    assert(both.boundary.advanced_to_next);

    // Empty/stale project-owned inputs do not reproduce invalid CCArray access.
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

    return 0;
}
