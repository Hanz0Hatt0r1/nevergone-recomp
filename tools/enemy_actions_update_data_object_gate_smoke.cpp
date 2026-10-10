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
    assert(present.would_enter_object_update_block);
    assert(!present.would_bypass_object_update_block);

    const auto absent = runtime::apply_update_data_object_gate(doc, state, false);
    assert(absent.should_call_virtual_slot_cc);
    assert(!absent.virtual_slot_cc_result_nonnull);
    assert(!absent.would_enter_object_update_block);
    assert(absent.would_bypass_object_update_block);

    state.field_250 = 1;
    const auto mode1 = runtime::apply_update_data_object_gate(doc, state, true);
    assert(mode1.entry.would_enter_frame_update);
    assert(!mode1.should_call_virtual_slot_cc);
    assert(!mode1.virtual_slot_cc_result_nonnull);
    assert(!mode1.would_enter_object_update_block);
    assert(mode1.would_bypass_object_update_block);

    state.field_250 = 2;
    const auto mode2 = runtime::apply_update_data_object_gate(doc, state, true);
    assert(!mode2.should_call_virtual_slot_cc);
    assert(mode2.would_bypass_object_update_block);

    state.flag_26c = 0u;
    state.field_250 = 0;
    const auto closed = runtime::apply_update_data_object_gate(doc, state, true);
    assert(!closed.entry.would_enter_frame_update);
    assert(!closed.should_call_virtual_slot_cc);
    assert(!closed.would_bypass_object_update_block);

    return 0;
}
