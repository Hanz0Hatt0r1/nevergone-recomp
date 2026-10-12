#include <cassert>
#include <cmath>

#include "scene_actions_play_state.h"
#include "scene_actions_wa_update.h"

namespace {

nevergone::trap_actions_document::Document document() {
    namespace combo = nevergone::trap_actions_combo_section;
    namespace final_table = nevergone::enemy_actions_wbg_final_table;

    nevergone::trap_actions_document::Document doc;
    doc.core.primary.records.resize(3u);
    doc.final_table.entries.resize(3u);
    doc.final_table.entries[0].reciprocal_value = 100.0f;
    doc.final_table.entries[1].reciprocal_value = 200.0f;
    doc.final_table.entries[2].reciprocal_value = 300.0f;

    combo::DerivedActionComboValue a94_0;
    a94_0.target_array_offset = combo::kArray94Offset;
    a94_0.field_18 = 2;
    combo::DerivedActionComboValue a94_1 = a94_0;
    a94_1.field_18 = 3;
    doc.combo.array_94_values = {a94_0, a94_1};

    combo::DerivedActionComboValue a98_0;
    a98_0.target_array_offset = combo::kArray98Offset;
    a98_0.field_18 = 0;
    combo::DerivedActionComboValue a98_1 = a98_0;
    a98_1.field_18 = 3;
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
    auto state_opt = play::build(doc, 0u);
    assert(state_opt.has_value());
    auto state = *state_opt;

    // The first native waUpdate clears +0x15c and accumulator; dt is not added.
    auto result = update::advance(doc, 10.0f, &state);
    assert(result.status == update::Status::kNoAdvance);
    assert(!state.first_update_15c);
    assert(near(state.accumulator_154, 0.0f));
    assert(state.frame_index_250 == 0u);

    // 100 ms crosses frame 0. +0x98 uses a strict next-frame > boundary rule.
    result = update::advance(doc, 0.1f, &state);
    assert(result.status == update::Status::kAdvanced);
    assert(result.frames_advanced == 1u);
    assert(result.update_data_requested);
    assert(state.frame_index_250 == 1u);
    assert(state.boundary_cursor_98_260 == 1u);
    assert(state.boundary_cursor_94_25c == 0u);
    assert(near(state.accumulator_154, 0.0f));
    assert(near(state.current_frame_value_150, 200.0f));

    // +0x94 remains parked until comboHit() has activated +0x185. Once active,
    // reaching its field_18 boundary advances the cursor. +0x24c also latches
    // +0x24d on frame advancement.
    state.combo_hit_active_185 = true;
    state.combo_type_24c = true;
    result = update::advance(doc, 0.2f, &state);
    assert(result.status == update::Status::kAdvanced);
    assert(state.frame_index_250 == 2u);
    assert(state.boundary_cursor_94_25c == 1u);
    assert(state.boundary_cursor_98_260 == 1u);
    assert(state.combo_latched_24d);
    assert(near(state.current_frame_value_150, 300.0f));

    // Crossing the final known frame reports the native completion edge
    // explicitly instead of fabricating an >=-size completion rule.
    result = update::advance(doc, 0.3f, &state);
    assert(result.status == update::Status::kTerminalBoundary);
    assert(result.terminal_boundary_reached);
    assert(state.frame_index_250 == 3u);

    // Invalid time/data fail closed.
    auto bad = *play::build(doc, 1u);
    assert(update::advance(doc, -0.1f, &bad).status == update::Status::kRejected);
    auto broken = doc;
    broken.final_table.entries[0].reciprocal_value = 0.0f;
    assert(update::advance(broken, 0.1f, &bad).status == update::Status::kRejected);

    return 0;
}
