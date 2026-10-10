#include <cassert>

#include "enemy_actions_runtime_state.h"

int main() {
    namespace runtime = nevergone::enemy_actions_runtime_state;
    namespace document = nevergone::enemy_actions_wbg_document;
    namespace prefix = nevergone::enemy_actions_wbg_prefix;

    document::Document doc;
    doc.primary_records.push_back(prefix::ActionFrameRecord{});

    runtime::State state;
    state.flag_26c = 1u;
    state.current_frame_294 = 0;
    state.field_250 = 0;

    const auto present = runtime::apply_update_data_object_gate(doc, state, true);
    assert(present.entry.would_enter_frame_update);
    assert(present.should_call_virtual_slot_cc);
    assert(present.virtual_slot_cc_result_nonnull);
    assert(present.cut_processing_gate_open);
    assert(!present.cut_processing_bypassed);
    assert(present.continues_to_common_downstream);

    const auto absent = runtime::apply_update_data_object_gate(doc, state, false);
    assert(absent.should_call_virtual_slot_cc);
    assert(!absent.virtual_slot_cc_result_nonnull);
    assert(!absent.cut_processing_gate_open);
    assert(absent.cut_processing_bypassed);
    // Null skips the cut slice but rejoins the common downstream transform path.
    assert(absent.continues_to_common_downstream);

    state.field_250 = 1;
    const auto mode1 = runtime::apply_update_data_object_gate(doc, state, true);
    assert(mode1.entry.would_enter_frame_update);
    assert(!mode1.should_call_virtual_slot_cc);
    assert(!mode1.virtual_slot_cc_result_nonnull);
    assert(!mode1.cut_processing_gate_open);
    assert(mode1.cut_processing_bypassed);
    assert(mode1.continues_to_common_downstream);

    state.field_250 = 2;
    const auto mode2 = runtime::apply_update_data_object_gate(doc, state, true);
    assert(!mode2.should_call_virtual_slot_cc);
    assert(mode2.cut_processing_bypassed);
    assert(mode2.continues_to_common_downstream);

    state.flag_26c = 0u;
    state.field_250 = 0;
    const auto closed = runtime::apply_update_data_object_gate(doc, state, true);
    assert(!closed.entry.would_enter_frame_update);
    assert(!closed.should_call_virtual_slot_cc);
    assert(!closed.cut_processing_gate_open);
    assert(!closed.cut_processing_bypassed);
    assert(!closed.continues_to_common_downstream);

    return 0;
}
