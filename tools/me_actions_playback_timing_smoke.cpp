#include <cassert>
#include <cmath>
#include <limits>

#include "me_actions_playback_timing.h"

namespace timing = nevergone::me_actions_playback_timing;
namespace document = nevergone::enemy_actions_wbg_document;

int main() {
    document::Document action;
    action.primary_records.resize(2);
    action.final_table.primary_record_count = 2;
    action.final_table.entries.resize(2);
    action.final_table.entries[0].reciprocal_value = 40.0f;
    action.final_table.entries[1].reciprocal_value = 25.0f;

    auto state = timing::initial_play_state();
    assert(!timing::is_done(state));
    assert(state.reset_accumulator_1bc);
    assert(state.accumulator_1b4 == 0.0f);
    assert(state.attach_duration_1b0 == 0.0f);

    // play() leaves +0x1bc set. The first waUpdate clears that byte and resets
    // +0x1b4 instead of accumulating the supplied delta.
    auto result = timing::apply_wa_update_timing(action, 0, 0.5f, state);
    assert(result.valid_frame);
    assert(result.frame_index == 0u);
    assert(result.action_frame_duration_5c == 40.0f);
    assert(result.threshold_1ac == 40.0f);
    assert(!result.frame_processing_ready);
    assert(!result.state.reset_accumulator_1bc);
    assert(result.state.accumulator_1b4 == 0.0f);

    // Subsequent calls accumulate milliseconds and use strict BLT semantics.
    state = result.state;
    result = timing::apply_wa_update_timing(action, 0, 0.03f, state);
    assert(!result.frame_processing_ready);
    assert(std::fabs(result.state.accumulator_1b4 - 30.0f) < 0.001f);

    state = result.state;
    result = timing::apply_wa_update_timing(action, 0, 0.01f, state);
    assert(result.frame_processing_ready);  // equal threshold proceeds
    assert(std::fabs(result.state.accumulator_1b4) < 0.001f);

    // +0x1b0 is added to each selected AFD+0x5c threshold.
    state = result.state;
    state.attach_duration_1b0 = 5.0f;
    result = timing::apply_wa_update_timing(action, 1, 0.02f, state);
    assert(result.valid_frame);
    assert(result.action_frame_duration_5c == 25.0f);
    assert(result.threshold_1ac == 30.0f);
    assert(!result.frame_processing_ready);
    state = result.state;
    result = timing::apply_wa_update_timing(action, 1, 0.01f, state);
    assert(result.frame_processing_ready);
    assert(std::fabs(result.state.accumulator_1b4) < 0.001f);

    // MeActionsSystem::isDone() is byte +0x220 and waUpdate exits before timing.
    state = result.state;
    state.done_220 = true;
    state.accumulator_1b4 = 7.0f;
    result = timing::apply_wa_update_timing(action, 0, 1.0f, state);
    assert(timing::is_done(result.state));
    assert(result.state.accumulator_1b4 == 7.0f);
    assert(!result.frame_processing_ready);

    // Project bounds checks fail closed after the native-equivalent accumulator
    // prefix instead of reproducing an invalid CCArray::objectAtIndex access.
    state = timing::initial_play_state();
    state.reset_accumulator_1bc = false;
    result = timing::apply_wa_update_timing(action, 7, 0.01f, state);
    assert(!result.valid_frame);
    assert(std::fabs(result.state.accumulator_1b4 - 10.0f) < 0.001f);

    // ARM VCMPE + BLT treats unordered comparison as the early-return path.
    state = timing::initial_play_state();
    state.reset_accumulator_1bc = false;
    result = timing::apply_wa_update_timing(
        action, 0, std::numeric_limits<float>::quiet_NaN(), state);
    assert(result.valid_frame);
    assert(!result.frame_processing_ready);
    return 0;
}
