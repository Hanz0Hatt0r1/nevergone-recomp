#include <cassert>
#include <cmath>

#include "scene_actions_play_state.h"
#include "scene_actions_wa_update.h"

namespace {

nevergone::trap_actions_document::Document document() {
    namespace combo = nevergone::trap_actions_combo_section;

    nevergone::trap_actions_document::Document doc;
    doc.core.primary.prefix.record_count = 3u;
    doc.core.primary.records.resize(3u);
    doc.final_table.primary_record_count = 3u;
    doc.final_table.entries.resize(3u);
    doc.final_table.entries[0].reciprocal_value = 100.0f;
    doc.final_table.entries[1].reciprocal_value = 200.0f;
    doc.final_table.entries[2].reciprocal_value = 300.0f;

    doc.combo.primary_record_count = 3u;
    doc.combo.tuples.resize(3u);
    doc.combo.array_90_values.resize(3u);

    combo::DerivedActionComboValue a94_0;
    a94_0.target_array_offset = combo::kArray94Offset;
    a94_0.field_18 = 1;
    combo::DerivedActionComboValue a94_1 = a94_0;
    a94_1.field_18 = 2;
    doc.combo.array_94_values = {a94_0, a94_1};

    combo::DerivedActionComboValue a98_0;
    a98_0.target_array_offset = combo::kArray98Offset;
    a98_0.field_18 = 0;
    combo::DerivedActionComboValue a98_1 = a98_0;
    a98_1.field_18 = 2;
    doc.combo.array_98_values = {a98_0, a98_1};
    return doc;
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    namespace play = nevergone::scene_actions_play_state;
    namespace update = nevergone::scene_actions_wa_update;

    const auto doc = document();
    assert(update::valid_document(doc));
    auto state_opt = play::build(doc, 0u);
    assert(state_opt.has_value());
    auto state = *state_opt;
    assert(state.combo_hit_active_185);  // play() writes +0x185 = 1.

    // The first native waUpdate clears +0x15c and accumulator; dt is not added.
    auto result = update::advance(doc, 10.0f, &state);
    assert(result.status == update::Status::kNoAdvance);
    assert(!state.first_update_15c);
    assert(near(state.accumulator_154, 0.0f));
    assert(state.frame_index_250 == 0u);

    // 100 ms crosses frame 0. Reaching +0x94 field_18=1 advances to the next
    // run; +0x98 uses strict next-frame > field_18 and also advances here.
    result = update::advance(doc, 0.1f, &state);
    assert(result.status == update::Status::kAdvanced);
    assert(result.frames_advanced == 1u);
    assert(result.update_data_requested);
    assert(state.frame_index_250 == 1u);
    assert(state.boundary_cursor_94_25c == 1u);
    assert(state.boundary_cursor_98_260 == 1u);
    assert(near(state.accumulator_154, 0.0f));
    assert(near(state.current_frame_value_150, 200.0f));

    // At the final +0x94 run boundary, native cannot advance to another run.
    // Equality is still renderable: terminal uses strict `frame > field_18`.
    state.combo_type_24c = true;
    result = update::advance(doc, 0.2f, &state);
    assert(result.status == update::Status::kAdvanced);
    assert(result.update_data_requested);
    assert(state.frame_index_250 == 2u);
    assert(state.boundary_cursor_94_25c == 1u);
    assert(state.boundary_cursor_98_260 == 1u);
    assert(state.combo_latched_24d);
    assert(near(state.current_frame_value_150, 300.0f));

    // Crossing past final field_18=2 selects the native terminal branch before
    // another updateData. The recovered exclusive terminal limit is therefore 3.
    result = update::advance(doc, 0.3f, &state);
    assert(result.status == update::Status::kTerminalBoundary);
    assert(result.terminal_boundary_reached);
    assert(result.terminal_exclusive_frame_limit == 3u);
    assert(!result.update_data_requested);
    assert(state.frame_index_250 == 3u);

    // Invalid time/data and impossible parser topology fail closed.
    auto bad = *play::build(doc, 1u);
    assert(update::advance(doc, -0.1f, &bad).status == update::Status::kRejected);
    auto broken = doc;
    broken.final_table.entries[0].reciprocal_value = 0.0f;
    assert(update::advance(broken, 0.1f, &bad).status == update::Status::kRejected);

    broken = doc;
    broken.combo.array_94_values.back().field_18 = 1;
    assert(!update::valid_document(broken));
    assert(update::advance(broken, 0.1f, &bad).status == update::Status::kRejected);

    broken = doc;
    broken.combo.array_94_values.clear();
    assert(!update::valid_document(broken));

    return 0;
}
