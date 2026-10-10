#include <cassert>

#include "enemy_actions_runtime_state.h"

int main() {
    namespace runtime = nevergone::enemy_actions_runtime_state;
    namespace document = nevergone::enemy_actions_wbg_document;
    namespace prefix = nevergone::enemy_actions_wbg_prefix;

    document::Document doc;
    prefix::ActionFrameRecord frame0;
    prefix::ActionFrameRecord frame1;
    doc.primary_records.push_back(frame0);
    doc.primary_records.push_back(frame1);

    runtime::State closed;
    closed.flag_26c = 0u;
    closed.current_frame_294 = 1;
    closed.flag_169 = 7u;
    const auto closed_result = runtime::apply_update_data_entry(doc, closed);
    assert(!closed_result.readiness_gate_open);
    assert(!closed_result.has_primary_frames);
    assert(!closed_result.current_frame_in_range);
    assert(!closed_result.would_enter_frame_update);
    assert(closed_result.state.flag_169 == 7u);
    assert(closed_result.state.current_frame_294 == 1);

    runtime::State ready;
    ready.flag_26c = 1u;
    ready.current_frame_294 = 1;
    ready.previous_frame_154 = 44;
    ready.field_18c = -3;
    const auto valid = runtime::apply_update_data_entry(doc, ready);
    assert(valid.readiness_gate_open);
    assert(valid.has_primary_frames);
    assert(valid.current_frame_in_range);
    assert(valid.would_enter_frame_update);
    assert(valid.selected_primary_index == 1u);
    assert(valid.state.flag_26c == 1u);
    assert(valid.state.current_frame_294 == 1);
    assert(valid.state.previous_frame_154 == 44);
    assert(valid.state.field_18c == -3);

    document::Document empty;
    const auto no_frames = runtime::apply_update_data_entry(empty, ready);
    assert(no_frames.readiness_gate_open);
    assert(!no_frames.has_primary_frames);
    assert(!no_frames.current_frame_in_range);
    assert(!no_frames.would_enter_frame_update);

    auto negative_state = ready;
    negative_state.current_frame_294 = -1;
    const auto negative = runtime::apply_update_data_entry(doc, negative_state);
    assert(negative.readiness_gate_open);
    assert(negative.has_primary_frames);
    assert(!negative.current_frame_in_range);
    assert(!negative.would_enter_frame_update);

    auto past_end_state = ready;
    past_end_state.current_frame_294 = 2;
    const auto past_end = runtime::apply_update_data_entry(doc, past_end_state);
    assert(past_end.readiness_gate_open);
    assert(past_end.has_primary_frames);
    assert(!past_end.current_frame_in_range);
    assert(!past_end.would_enter_frame_update);

    // showActionLastFrame() can signal updateData(), but the readiness byte
    // remains the independent native entry gate.
    runtime::State show_state;
    show_state.flag_26c = 0u;
    show_state.field_18c = 1;
    const auto shown = runtime::apply_show_action_last_frame(show_state);
    assert(shown.should_call_update_data);
    assert(shown.state.current_frame_294 == 1);
    const auto shown_entry = runtime::apply_update_data_entry(doc, shown.state);
    assert(!shown_entry.would_enter_frame_update);

    return 0;
}
