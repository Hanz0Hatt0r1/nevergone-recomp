#include <cassert>

#include "enemy_actions_runtime_state.h"

int main() {
    namespace runtime = nevergone::enemy_actions_runtime_state;
    namespace document = nevergone::enemy_actions_wbg_document;
    namespace prefix = nevergone::enemy_actions_wbg_prefix;

    document::Document doc;
    prefix::ActionFrameRecord frame;
    frame.first_string = "Se04_e01_default.plist";
    doc.primary_records.push_back(frame);

    runtime::State state;
    state.flag_26c = 1u;
    state.current_frame_294 = 0;
    state.field_268 = 7;

    state.field_250 = 0;
    const auto enemy = runtime::apply_update_data_resource_selection(doc, state);
    assert(enemy.selected_frame_available);
    assert(enemy.frame_resource_60 == "Se04_e01_default.plist");
    assert(enemy.formatted_path_available);
    assert(enemy.formatted_path == "enemy07/res/Se04_e01_default.plist");
    assert(!enemy.should_add_sprite_frames);
    assert(!enemy.should_record_enemy_object_res);

    state.field_250 = 1;
    const auto npc = runtime::apply_update_data_resource_selection(doc, state);
    assert(npc.formatted_path == "npc07/res/Se04_e01_default.plist");
    assert(npc.should_add_sprite_frames);
    assert(npc.sprite_frames_file == npc.formatted_path);
    assert(npc.should_record_enemy_object_res);

    state.field_250 = 2;
    state.field_268 = 12;
    const auto pet = runtime::apply_update_data_resource_selection(doc, state);
    assert(pet.formatted_path == "pet12/res/Se04_e01_default.plist");
    assert(pet.should_add_sprite_frames);
    assert(pet.should_record_enemy_object_res);

    state.field_250 = 9;
    const auto unknown = runtime::apply_update_data_resource_selection(doc, state);
    assert(unknown.selected_frame_available);
    assert(!unknown.formatted_path_available);
    assert(unknown.formatted_path.empty());
    assert(!unknown.should_add_sprite_frames);
    assert(!unknown.should_record_enemy_object_res);

    // "looping" is a native special case: the +0x60 string is passed directly
    // to addSpriteFramesWithFile, independent of the +0x250 path mode.
    doc.primary_records[0].first_string = "looping";
    state.field_250 = 9;
    const auto looping = runtime::apply_update_data_resource_selection(doc, state);
    assert(looping.selected_frame_available);
    assert(!looping.formatted_path_available);
    assert(looping.should_add_sprite_frames);
    assert(looping.sprite_frames_file == "looping");
    assert(!looping.should_record_enemy_object_res);

    // Closed readiness still blocks this later slice.
    state.flag_26c = 0u;
    const auto closed = runtime::apply_update_data_resource_selection(doc, state);
    assert(!closed.entry.readiness_gate_open);
    assert(!closed.selected_frame_available);
    assert(!closed.should_add_sprite_frames);

    // `%02d` is a minimum width, not a truncation rule.
    doc.primary_records[0].first_string = "x.plist";
    state.flag_26c = 1u;
    state.field_250 = 1;
    state.field_268 = 123;
    const auto wide = runtime::apply_update_data_resource_selection(doc, state);
    assert(wide.formatted_path == "npc123/res/x.plist");

    return 0;
}
