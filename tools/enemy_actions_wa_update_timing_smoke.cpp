#include <cassert>
#include <cmath>
#include <limits>

#include "enemy_actions_runtime_state.h"

int main() {
    namespace runtime = nevergone::enemy_actions_runtime_state;
    namespace final_table = nevergone::enemy_actions_wbg_final_table;

    final_table::Table table;
    table.primary_record_count = 2;

    final_table::Entry frame0;
    frame0.serialized_value = 4;
    frame0.reciprocal_value = 0.25f;
    table.entries.push_back(frame0);

    final_table::Entry frame1;
    frame1.serialized_value = 2;
    frame1.reciprocal_value = 0.5f;
    table.entries.push_back(frame1);

    // +0x1bc is the top-level native gate: no timing fields are touched.
    runtime::State blocked;
    blocked.flag_1bc = 7u;
    blocked.flag_168 = 9u;
    blocked.flag_169 = 1u;
    blocked.field_158 = 11.0f;
    blocked.field_15c = 0.75f;
    blocked.field_160 = 22.0f;
    const auto blocked_result = runtime::apply_wa_update_timing(table, 0.5f, blocked);
    assert(blocked_result.blocked_by_flag_1bc);
    assert(!blocked_result.had_valid_action_frame);
    assert(!blocked_result.threshold_reached);
    assert(!blocked_result.frame_processing_allowed);
    assert(blocked_result.state.flag_168 == 9u);
    assert(blocked_result.state.field_158 == 11.0f);
    assert(blocked_result.state.field_160 == 22.0f);

    // Nonzero +0x168 clears itself and zeroes +0x160 instead of adding dt.
    runtime::State one_shot_reset;
    one_shot_reset.flag_168 = 3u;
    one_shot_reset.flag_169 = 1u;
    one_shot_reset.field_15c = 0.75f;
    one_shot_reset.field_160 = 500.0f;
    one_shot_reset.current_frame_294 = 0;
    const auto reset_result = runtime::apply_wa_update_timing(table, 10.0f, one_shot_reset);
    assert(reset_result.state.flag_168 == 0u);
    assert(reset_result.state.field_160 == 0.0f);
    assert(reset_result.had_valid_action_frame);
    assert(reset_result.action_frame_5c == 0.25f);
    assert(reset_result.state.field_158 == 1.0f);
    assert(!reset_result.threshold_reached);
    assert(!reset_result.frame_processing_allowed);

    // Normal path adds dt*1000, forms +0x158 from AFD+0x5c + +0x15c,
    // reaches the threshold, then subtracts it when +0x169 is nonzero.
    runtime::State ready;
    ready.flag_169 = 5u;
    ready.field_15c = 0.75f;
    ready.field_160 = 2.0f;
    ready.current_frame_294 = 0;
    const auto ready_result = runtime::apply_wa_update_timing(table, 0.001f, ready);
    assert(ready_result.had_valid_action_frame);
    assert(ready_result.action_frame_5c == 0.25f);
    assert(ready_result.state.field_158 == 1.0f);
    assert(ready_result.threshold_reached);
    assert(ready_result.frame_processing_allowed);
    assert(ready_result.state.field_160 == 2.0f);
    assert(ready_result.state.flag_169 == 5u);

    // Below threshold: accumulator is retained, no subtraction or processing.
    runtime::State below;
    below.flag_169 = 1u;
    below.field_15c = 2.0f;
    below.field_160 = 0.25f;
    below.current_frame_294 = 1;
    const auto below_result = runtime::apply_wa_update_timing(table, 0.001f, below);
    assert(below_result.state.field_160 == 1.25f);
    assert(below_result.state.field_158 == 2.5f);
    assert(!below_result.threshold_reached);
    assert(!below_result.frame_processing_allowed);

    // Threshold can be reached while +0x169 still gates processing; native
    // returns before subtracting the threshold in that case.
    runtime::State gated;
    gated.flag_169 = 0u;
    gated.field_15c = 0.5f;
    gated.field_160 = 1.0f;
    gated.current_frame_294 = 1;
    const auto gated_result = runtime::apply_wa_update_timing(table, 0.0f, gated);
    assert(gated_result.state.field_158 == 1.0f);
    assert(gated_result.threshold_reached);
    assert(!gated_result.frame_processing_allowed);
    assert(gated_result.state.field_160 == 1.0f);

    // Invalid project-owned frame state safely stops after the native-proven
    // accumulator update rather than attempting invalid CCArray access.
    runtime::State invalid;
    invalid.flag_169 = 1u;
    invalid.field_160 = 4.0f;
    invalid.current_frame_294 = 99;
    const auto invalid_result = runtime::apply_wa_update_timing(table, 0.002f, invalid);
    assert(!invalid_result.had_valid_action_frame);
    assert(invalid_result.state.field_160 == 6.0f);
    assert(!invalid_result.frame_processing_allowed);

    runtime::State negative = invalid;
    negative.current_frame_294 = -1;
    const auto negative_result = runtime::apply_wa_update_timing(table, 0.0f, negative);
    assert(!negative_result.had_valid_action_frame);

    // Native VCMPE + BLT treats unordered inputs as taking the early branch.
    runtime::State unordered;
    unordered.flag_169 = 1u;
    unordered.field_15c = 0.0f;
    unordered.field_160 = std::numeric_limits<float>::quiet_NaN();
    unordered.current_frame_294 = 0;
    const auto unordered_result = runtime::apply_wa_update_timing(table, 0.0f, unordered);
    assert(unordered_result.had_valid_action_frame);
    assert(std::isnan(unordered_result.state.field_160));
    assert(!unordered_result.threshold_reached);
    assert(!unordered_result.frame_processing_allowed);

    return 0;
}
