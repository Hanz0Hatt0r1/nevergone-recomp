#include <cassert>
#include <cstdint>

#include "me_actions_combo_boundaries.h"
#include "me_actions_playback_timing.h"

namespace boundaries = nevergone::me_actions_combo_boundaries;
namespace combo = nevergone::enemy_actions_wbg_combo_section;
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

combo::DerivedActionComboValue value(std::size_t target, std::size_t tuple, int state) {
    combo::DerivedActionComboValue result;
    result.target_array_offset = target;
    result.source_tuple_index = tuple;
    result.field_14 = tuple == 0 ? 0 : static_cast<std::int32_t>(tuple);
    result.field_18 = static_cast<std::int32_t>(tuple);
    result.field_1c = state;
    return result;
}

}  // namespace

int main() {
    // Projection is the player-specific view of the already parsed Section-F
    // run boundaries: enemy +0x94 -> MeActionsData +0xb0, +0x98 -> +0xb4.
    auto document = make_document(3);
    document.combo_block.array_94_values.push_back(value(combo::kArray94Offset, 2, 1));
    document.combo_block.array_98_values.push_back(value(combo::kArray98Offset, 1, 1));

    boundaries::Projection projected;
    assert(boundaries::project(document, &projected));
    assert(projected.primary_b0.size() == 1u);
    assert(projected.primary_b0[0].target_array_offset == boundaries::kPrimaryBoundaryArrayOffset);
    assert(projected.primary_b0[0].field_18 == 2);
    assert(projected.secondary_b4.size() == 1u);
    assert(projected.secondary_b4[0].target_array_offset == boundaries::kSecondaryBoundaryArrayOffset);
    assert(projected.secondary_b4[0].field_18 == 1);

    auto cursor = timing::initial_cursor();
    // First waUpdate after play clears +0x1bc and does not accumulate dt.
    auto step = timing::apply_wa_update_playback(document, projected, 1.0f, cursor);
    assert(step.valid_projection);
    assert(step.valid_frame);
    assert(step.frames_advanced == 0u);
    assert(step.cursor.current_frame_1a4 == 0);
    assert(!step.cursor.timing.reset_accumulator_1bc);

    // 10 ms reaches frame 0 threshold and advances to frame 1.
    step = timing::apply_wa_update_playback(document, projected, 0.010f, step.cursor);
    assert(step.frames_advanced == 1u);
    assert(step.cursor.current_frame_1a4 == 1);
    assert(step.cursor.previous_frame_1a8 == 1);
    assert(step.update_data_count == 1u);
    assert(!step.cursor.timing.done_220);

    // The primary boundary is inclusive. Reaching current==field_18 stays live.
    step = timing::apply_wa_update_playback(document, projected, 0.010f, step.cursor);
    assert(step.frames_advanced == 1u);
    assert(step.cursor.current_frame_1a4 == 2);
    assert(!step.cursor.timing.done_220);

    // One more frame crosses the final +0xb0 boundary and mirrors native done.
    step = timing::apply_wa_update_playback(document, projected, 0.010f, step.cursor);
    assert(step.frames_advanced == 1u);
    assert(step.became_done);
    assert(step.cursor.timing.done_220);
    assert(step.cursor.finished_cycle_200);
    assert(step.cursor.current_frame_1a4 == 0);
    assert(step.cursor.last_frame_1f4 == 1);

    // Multi-run primary boundaries advance +0x1f8 instead of completing when
    // the first run closes.
    auto segmented = make_document(4);
    segmented.combo_block.array_94_values.push_back(value(combo::kArray94Offset, 1, 1));
    segmented.combo_block.array_94_values.push_back(value(combo::kArray94Offset, 3, 2));
    segmented.combo_block.array_98_values.push_back(value(combo::kArray98Offset, 3, 1));
    boundaries::Projection segmented_projection;
    assert(boundaries::project(segmented, &segmented_projection));

    cursor = timing::initial_cursor();
    (void)timing::apply_wa_update_playback(segmented, segmented_projection, 0.0f, cursor);
    auto primed = timing::apply_wa_update_playback(segmented, segmented_projection, 0.0f, cursor);
    // Prime explicitly so later update accumulates rather than resets.
    cursor = timing::initial_cursor();
    cursor.timing.reset_accumulator_1bc = false;
    auto multi = timing::apply_wa_update_playback(segmented, segmented_projection, 0.010f, cursor);
    assert(multi.frames_advanced == 1u);
    assert(multi.cursor.current_frame_1a4 == 1);
    assert(multi.cursor.primary_boundary_index_1f8 == 1u);
    assert(!multi.cursor.timing.done_220);

    // Large dt is consumed in the native same-call loop across several frames.
    multi = timing::apply_wa_update_playback(segmented, segmented_projection, 0.030f, multi.cursor);
    assert(multi.frames_advanced == 3u);
    assert(multi.became_done);
    assert(multi.cursor.timing.done_220);

    // Malformed projections fail closed before touching the frame cursor.
    auto invalid = projected;
    invalid.primary_b0[0].field_18 = 99;
    cursor = timing::initial_cursor();
    cursor.timing.reset_accumulator_1bc = false;
    const auto rejected = timing::apply_wa_update_playback(document, invalid, 1.0f, cursor);
    assert(!rejected.valid_projection);
    assert(rejected.frames_advanced == 0u);
    assert(rejected.cursor.current_frame_1a4 == 0);
    return 0;
}
