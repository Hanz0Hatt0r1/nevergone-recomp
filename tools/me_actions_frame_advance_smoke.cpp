#include <cassert>
#include <cstdint>

#include "me_actions_combo_boundaries.h"
#include "me_actions_playback_timing.h"

namespace boundaries = nevergone::me_actions_combo_boundaries;
namespace timing = nevergone::me_actions_playback_timing;
namespace document_ns = nevergone::enemy_actions_wbg_document;

namespace {

document_ns::Document make_document(std::size_t frame_count) {
    document_ns::Document document;
    document.prefix.action_frame_count = static_cast<std::uint32_t>(frame_count);
    document.primary_records.resize(frame_count);
    document.final_table.primary_record_count = static_cast<std::uint32_t>(frame_count);
    document.final_table.entries.resize(frame_count);
    document.combo_block.primary_record_count = static_cast<std::uint32_t>(frame_count);
    document.combo_block.tuples.resize(frame_count);
    for (auto& entry : document.final_table.entries) entry.reciprocal_value = 10.0f;
    return document;
}

}  // namespace

int main() {
    // One Section-F run: derive() must close it on the final tuple, making
    // +0xb0.field_18 the inclusive primary frame boundary.
    auto document = make_document(3);
    for (auto& tuple : document.combo_block.tuples) {
        tuple.i32_values[1] = 0;
        tuple.i32_values[3] = 0;
    }
    const auto one_run = boundaries::derive(document.combo_block);
    assert(one_run.array_b0_values.size() == 1u);
    assert(one_run.array_b0_values[0].field_18 == 2);

    auto cursor = timing::initial_cursor();
    auto step = timing::apply_wa_update_playback(document, one_run, 1.0f, cursor);
    assert(step.valid_projection);
    assert(step.valid_frame);
    assert(step.frames_advanced == 0u);
    assert(step.cursor.current_frame_1a4 == 0);
    assert(!step.cursor.timing.reset_accumulator_1bc);

    step = timing::apply_wa_update_playback(document, one_run, 0.010f, step.cursor);
    assert(step.frames_advanced == 1u);
    assert(step.cursor.current_frame_1a4 == 1);
    assert(step.cursor.previous_frame_1a8 == 1);
    assert(step.update_data_count == 1u);
    assert(!step.cursor.timing.done_220);

    step = timing::apply_wa_update_playback(document, one_run, 0.010f, step.cursor);
    assert(step.cursor.current_frame_1a4 == 2);
    assert(!step.cursor.timing.done_220);

    step = timing::apply_wa_update_playback(document, one_run, 0.010f, step.cursor);
    assert(step.became_done);
    assert(step.cursor.timing.done_220);
    assert(step.cursor.finished_cycle_200);
    assert(step.cursor.current_frame_1a4 == 0);
    // Native stores primary boundary - 1 on this completion path.
    assert(step.cursor.last_frame_1f4 == 1);

    // Two primary Section-F runs. int32[3]+1 changes at tuple 1 and the final
    // tuple closes the second run, producing boundaries [1,3].
    auto segmented_document = make_document(4);
    segmented_document.combo_block.tuples[0].i32_values[3] = 0;
    segmented_document.combo_block.tuples[1].i32_values[3] = 1;
    segmented_document.combo_block.tuples[2].i32_values[3] = 1;
    segmented_document.combo_block.tuples[3].i32_values[3] = 1;
    for (auto& tuple : segmented_document.combo_block.tuples) tuple.i32_values[1] = 0;
    const auto segmented = boundaries::derive(segmented_document.combo_block);
    assert(segmented.array_b0_values.size() == 2u);
    assert(segmented.array_b0_values[0].field_18 == 1);
    assert(segmented.array_b0_values[1].field_18 == 3);

    cursor = timing::initial_cursor();
    cursor.timing.reset_accumulator_1bc = false;
    auto multi = timing::apply_wa_update_playback(
        segmented_document, segmented, 0.010f, cursor);
    assert(multi.frames_advanced == 1u);
    assert(multi.cursor.current_frame_1a4 == 1);
    assert(multi.cursor.boundary_index_1f8 == 1u);
    assert(!multi.cursor.timing.done_220);

    // The retained 30 ms crosses frames 1,2,3 in the same native-style loop.
    multi = timing::apply_wa_update_playback(
        segmented_document, segmented, 0.030f, multi.cursor);
    assert(multi.frames_advanced == 3u);
    assert(multi.update_data_count == 3u);
    assert(multi.became_done);
    assert(multi.cursor.timing.done_220);

    // Fallback branch without +0xb0 uses the primary frame-array count and a
    // strict '>' comparison for +0xb4 cursor advancement.
    auto fallback_boundaries = segmented;
    fallback_boundaries.array_b0_values.clear();
    cursor = timing::initial_cursor();
    cursor.timing.reset_accumulator_1bc = false;
    auto fallback = timing::apply_wa_update_playback(
        segmented_document, fallback_boundaries, 0.010f, cursor);
    assert(fallback.frames_advanced == 1u);
    assert(!fallback.cursor.timing.done_220);

    // A boundary outside the primary frame array fails closed before mutation.
    auto invalid = one_run;
    invalid.array_b0_values[0].field_18 = 99;
    cursor = timing::initial_cursor();
    cursor.timing.reset_accumulator_1bc = false;
    const auto rejected = timing::apply_wa_update_playback(
        document, invalid, 1.0f, cursor);
    assert(!rejected.valid_projection);
    assert(rejected.frames_advanced == 0u);
    assert(rejected.cursor.current_frame_1a4 == 0);
    return 0;
}
