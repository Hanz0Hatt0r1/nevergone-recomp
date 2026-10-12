#include <cassert>
#include <cstdint>

#include "me_actions_live_playback.h"

namespace live = nevergone::me_actions_live_playback;
namespace document_ns = nevergone::enemy_actions_wbg_document;

namespace {

document_ns::Document document3() {
    document_ns::Document document;
    document.prefix.action_frame_count = 3;
    document.primary_records.resize(3);
    document.final_table.primary_record_count = 3;
    document.final_table.entries.resize(3);
    document.combo_block.primary_record_count = 3;
    document.combo_block.tuples.resize(3);
    for (std::size_t i = 0; i < 3; ++i) {
        document.primary_records[i].third_string = "frame" + std::to_string(i);
        document.primary_records[i].third_string_length_i32 = 6;
        document.final_table.entries[i].reciprocal_value = 40.0f;
        document.combo_block.tuples[i].i32_values[1] = 0;
        document.combo_block.tuples[i].i32_values[3] = 0;
    }
    return document;
}

}  // namespace

int main() {
    const auto document = document3();
    live::reset();

    // Loading/committing an action creates the exact play() cursor at frame 0.
    assert(live::sync_and_tick(1, 1, 0x20, document, 100));
    auto state = live::snapshot();
    assert(state.ready);
    assert(state.cursor.current_frame_1a4 == 0);
    const auto initial_revision = state.frame_revision;

    // Multiple queue builds in the same logical tick are idempotent.
    assert(live::sync_and_tick(1, 1, 0x20, document, 100));
    assert(live::snapshot().frame_revision == initial_revision);

    // First later waUpdate clears the native +0x1bc reset flag and discards dt.
    assert(live::sync_and_tick(1, 1, 0x20, document, 101));
    assert(live::snapshot().cursor.current_frame_1a4 == 0);

    // 35 Hz fixed ticks accumulate against the 40 ms ActionFrameData duration.
    assert(live::sync_and_tick(1, 1, 0x20, document, 102));
    assert(live::snapshot().cursor.current_frame_1a4 == 0);
    assert(live::sync_and_tick(1, 1, 0x20, document, 103));
    state = live::snapshot();
    assert(state.cursor.current_frame_1a4 == 1);
    assert(state.frame_revision > initial_revision);
    const auto frame_one_revision = state.frame_revision;

    // Same tick cannot advance frame 1 again.
    assert(live::sync_and_tick(1, 1, 0x20, document, 103));
    assert(live::snapshot().frame_revision == frame_one_revision);

    // A new loaded action revision replays native play() initialization.
    assert(live::sync_and_tick(1, 2, 0x02, document, 104));
    state = live::snapshot();
    assert(state.loaded_revision == 2);
    assert(state.action_state == 0x02u);
    assert(state.cursor.current_frame_1a4 == 0);
    assert(state.cursor.reset_accumulator_1bc);

    // A GL/game-clock reset cannot underflow elapsed ticks or jump animation.
    assert(live::sync_and_tick(1, 2, 0x02, document, 1));
    assert(live::snapshot().cursor.current_frame_1a4 == 0);

    const auto runtime = live::runtime_state();
    assert(runtime.current_frame_1a4 == 0);
    assert(runtime.previous_frame_1a8 == -1);
    return 0;
}
