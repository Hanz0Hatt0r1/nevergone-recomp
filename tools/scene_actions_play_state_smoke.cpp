#include <cassert>
#include <limits>

#include "scene_actions_play_state.h"

namespace play = nevergone::scene_actions_play_state;
namespace trap = nevergone::trap_actions_document;

trap::Document make_document(float first_frame_value) {
    trap::Document document;
    document.core.primary.records.resize(1u);
    nevergone::enemy_actions_wbg_final_table::Entry entry;
    entry.serialized_value = 4;
    entry.reciprocal_value = first_frame_value;
    document.final_table.entries.push_back(entry);
    document.final_table.primary_record_count = 1u;
    return document;
}

int main() {
    const auto document = make_document(0.25f);

    const auto born = play::build(document, 0u);
    assert(born.has_value());
    assert(born->mode_24e == 0u);
    assert(born->active_15d);
    assert(born->first_update_15c);
    assert(!born->done_1a8);
    assert(!born->end_184);
    assert(born->last_frame_14c == -1);
    assert(born->secondary_last_170 == -1);
    assert(born->frame_index_250 == 0u);
    assert(born->boundary_cursor_94_25c == 0u);
    assert(born->boundary_cursor_98_260 == 0u);
    assert(born->accumulator_154 == 0.0f);
    assert(born->current_frame_value_150 == 0.25f);

    const auto idle = play::build(document, 1u);
    assert(idle.has_value());
    assert(idle->mode_24e == 1u);
    assert(idle->active_15d);
    assert(!idle->done_1a8);
    assert(idle->frame_index_250 == 0u);

    assert(!play::build(document, 2u).has_value());

    trap::Document empty;
    assert(!play::build(empty, 0u).has_value());

    auto mismatched = document;
    mismatched.final_table.entries.clear();
    assert(!play::build(mismatched, 0u).has_value());

    auto non_finite = make_document(std::numeric_limits<float>::quiet_NaN());
    assert(!play::build(non_finite, 0u).has_value());

    return 0;
}
