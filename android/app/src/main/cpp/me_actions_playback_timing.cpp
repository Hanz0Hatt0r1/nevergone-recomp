#include "me_actions_playback_timing.h"

#include <cmath>
#include <limits>

namespace nevergone::me_actions_playback_timing {
namespace {

constexpr float kMillisecondsPerSecond = 1000.0f;

void clamp_increment(std::uint32_t* index, std::size_t count) {
    if (index == nullptr || count == 0u) return;
    if (*index + 1u < count) {
        ++(*index);
    } else {
        *index = static_cast<std::uint32_t>(count - 1u);
    }
}

bool primary_boundary_at(
        const me_actions_combo_boundaries::Projection& boundaries,
        std::uint32_t index,
        const me_actions_combo_boundaries::Boundary** out) {
    if (out == nullptr || index >= boundaries.primary_b0.size()) return false;
    *out = &boundaries.primary_b0[index];
    return true;
}

bool secondary_boundary_at(
        const me_actions_combo_boundaries::Projection& boundaries,
        std::uint32_t index,
        const me_actions_combo_boundaries::Boundary** out) {
    if (out == nullptr || index >= boundaries.secondary_b4.size()) return false;
    *out = &boundaries.secondary_b4[index];
    return true;
}

void mirror_update_data_gate(Cursor* cursor, PlaybackResult* result) {
    if (cursor == nullptr || result == nullptr) return;
    if (cursor->previous_frame_1a8 == cursor->current_frame_1a4) return;
    if (cursor->update_data_gate_1bf) ++result->update_data_count;
    cursor->previous_frame_1a8 = cursor->current_frame_1a4;
}

}  // namespace

State initial_play_state() {
    State state;
    state.attach_duration_1b0 = 0.0f;
    state.accumulator_1b4 = 0.0f;
    state.reset_accumulator_1bc = true;
    state.done_220 = false;
    return state;
}

Cursor initial_cursor() {
    Cursor cursor;
    cursor.timing = initial_play_state();
    return cursor;
}

TimingResult apply_wa_update_timing(
        const enemy_actions_wbg_document::Document& document,
        std::int32_t current_frame_1a4,
        float delta_seconds,
        State state) {
    TimingResult result;
    result.state = state;

    if (state.done_220) return result;

    if (state.reset_accumulator_1bc) {
        result.state.reset_accumulator_1bc = false;
        result.state.accumulator_1b4 = 0.0f;
    } else {
        result.state.accumulator_1b4 += delta_seconds * kMillisecondsPerSecond;
    }

    if (current_frame_1a4 < 0) return result;
    const std::size_t frame_index = static_cast<std::size_t>(current_frame_1a4);
    if (frame_index >= document.primary_records.size() ||
        frame_index >= document.final_table.entries.size()) {
        return result;
    }

    result.valid_frame = true;
    result.frame_index = frame_index;
    result.action_frame_duration_5c =
        document.final_table.entries[frame_index].reciprocal_value;
    result.threshold_1ac =
        result.action_frame_duration_5c + result.state.attach_duration_1b0;

    if (std::isnan(result.state.accumulator_1b4) ||
        std::isnan(result.threshold_1ac) ||
        result.state.accumulator_1b4 < result.threshold_1ac) {
        return result;
    }

    result.state.accumulator_1b4 -= result.threshold_1ac;
    result.frame_processing_ready = true;
    return result;
}

PlaybackResult apply_wa_update_playback(
        const enemy_actions_wbg_document::Document& document,
        const me_actions_combo_boundaries::Projection& boundaries,
        float delta_seconds,
        Cursor cursor) {
    PlaybackResult result;
    result.cursor = cursor;

    if (document.primary_records.size() != document.final_table.entries.size() ||
        document.primary_records.size() >
            static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return result;
    }
    for (const auto& value : boundaries.primary_b0) {
        if (value.field_18 < 0 ||
            static_cast<std::size_t>(value.field_18) >= document.primary_records.size()) {
            return result;
        }
    }
    for (const auto& value : boundaries.secondary_b4) {
        if (value.field_18 < 0 ||
            static_cast<std::size_t>(value.field_18) >= document.primary_records.size()) {
            return result;
        }
    }
    result.valid_projection = true;

    float next_delta = delta_seconds;
    while (!result.cursor.timing.done_220) {
        TimingResult timing = apply_wa_update_timing(
            document,
            result.cursor.current_frame_1a4,
            next_delta,
            result.cursor.timing);
        next_delta = 0.0f;
        result.cursor.timing = timing.state;
        result.valid_frame = result.valid_frame || timing.valid_frame;
        if (!timing.frame_processing_ready) break;

        const bool had_primary_boundary = !boundaries.primary_b0.empty();
        const me_actions_combo_boundaries::Boundary* primary = nullptr;
        if (had_primary_boundary &&
            !primary_boundary_at(
                boundaries, result.cursor.primary_boundary_index_1f8, &primary)) {
            result.valid_projection = false;
            break;
        }
        const me_actions_combo_boundaries::Boundary* secondary = nullptr;
        if (!boundaries.secondary_b4.empty() &&
            !secondary_boundary_at(
                boundaries, result.cursor.secondary_boundary_index_1fc, &secondary)) {
            result.valid_projection = false;
            break;
        }

        ++result.cursor.current_frame_1a4;
        ++result.frames_advanced;
        if (result.cursor.update_marker_218) result.cursor.update_marker_219 = true;

        bool next_primary_segment = false;
        if (primary != nullptr) {
            const std::int32_t primary_end = primary->field_18;
            if (result.cursor.primary_boundary_gate_201 &&
                result.cursor.current_frame_1a4 >= primary_end) {
                const std::size_t count = boundaries.primary_b0.size();
                const std::uint32_t before = result.cursor.primary_boundary_index_1f8;
                clamp_increment(&result.cursor.primary_boundary_index_1f8, count);
                next_primary_segment =
                    result.cursor.primary_boundary_index_1f8 != before;
            }

            if (secondary != nullptr &&
                result.cursor.current_frame_1a4 >= secondary->field_18) {
                clamp_increment(
                    &result.cursor.secondary_boundary_index_1fc,
                    boundaries.secondary_b4.size());
            }

            if (!next_primary_segment && result.cursor.current_frame_1a4 > primary_end) {
                result.cursor.last_frame_1f4 = primary_end - 1;
                result.cursor.finished_cycle_200 = true;
                result.cursor.current_frame_1a4 = 0;
                result.cursor.timing.done_220 = true;
                result.became_done = true;
            }
        } else {
            const std::int32_t primary_count =
                static_cast<std::int32_t>(document.primary_records.size());
            if (secondary != nullptr &&
                result.cursor.current_frame_1a4 > secondary->field_18) {
                clamp_increment(
                    &result.cursor.secondary_boundary_index_1fc,
                    boundaries.secondary_b4.size());
            }
            if (result.cursor.current_frame_1a4 > primary_count) {
                result.cursor.last_frame_1f4 = primary_count - 1;
                result.cursor.finished_cycle_200 = true;
                result.cursor.current_frame_1a4 = 0;
                result.cursor.timing.done_220 = true;
                result.became_done = true;
            }
        }

        mirror_update_data_gate(&result.cursor, &result);
    }

    return result;
}

}  // namespace nevergone::me_actions_playback_timing
