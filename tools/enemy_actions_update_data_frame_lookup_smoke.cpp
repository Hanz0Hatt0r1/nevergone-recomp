#include <cassert>

#include "enemy_actions_runtime_state.h"

int main() {
    namespace runtime = nevergone::enemy_actions_runtime_state;
    namespace document = nevergone::enemy_actions_wbg_document;
    namespace prefix = nevergone::enemy_actions_wbg_prefix;

    document::Document doc;
    prefix::ActionFrameRecord frame0;
    frame0.first_string = "T_E01_default.plist";
    frame0.first_string_length_i32 = 19;
    frame0.second_string = "a01";
    frame0.second_string_length_i32 = 3;
    frame0.third_string = "empty_a01_001.png";
    frame0.third_string_length_i32 = 17;
    doc.primary_records.push_back(frame0);

    runtime::State state;
    state.flag_26c = 1u;
    state.current_frame_294 = 0;

    const auto lookup = runtime::apply_update_data_frame_lookup(doc, state);
    assert(lookup.entry.would_enter_frame_update);
    assert(lookup.selected_frame_available);
    assert(lookup.should_lookup_sprite_frame);
    assert(lookup.frame_name_68 == "empty_a01_001.png");
    assert(lookup.frame_name_length_6c == 17);

    // The Section-A second string is not the value stored in AFD+0x68.
    assert(lookup.frame_name_68 != doc.primary_records[0].second_string);
    assert(lookup.frame_name_68 != doc.primary_records[0].first_string);

    // Native still issues spriteFrameByName for an empty +0x68 CCString once a
    // valid selected frame exists; the helper reports the request unchanged.
    doc.primary_records[0].third_string.clear();
    doc.primary_records[0].third_string_length_i32 = 0;
    const auto empty = runtime::apply_update_data_frame_lookup(doc, state);
    assert(empty.should_lookup_sprite_frame);
    assert(empty.frame_name_68.empty());
    assert(empty.frame_name_length_6c == 0);

    state.flag_26c = 0u;
    const auto closed = runtime::apply_update_data_frame_lookup(doc, state);
    assert(!closed.entry.readiness_gate_open);
    assert(!closed.selected_frame_available);
    assert(!closed.should_lookup_sprite_frame);

    state.flag_26c = 1u;
    state.current_frame_294 = 1;
    const auto out_of_range = runtime::apply_update_data_frame_lookup(doc, state);
    assert(out_of_range.entry.has_primary_frames);
    assert(!out_of_range.entry.current_frame_in_range);
    assert(!out_of_range.should_lookup_sprite_frame);

    return 0;
}
