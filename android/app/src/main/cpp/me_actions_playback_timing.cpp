#include "me_actions_playback_timing.h"

#include <cmath>
#include <limits>

namespace nevergone::me_actions_playback_timing {
namespace {

constexpr float kMillisecondsPerSecond = 1000.0f;
using Boundary = enemy_actions_wbg_combo_section::DerivedActionComboValue;

void clamp_increment(std::uint32_t* index, std::size_t count) {
    if (index == nullptr || count == 0u) return;
    if (*index + 1u < count) {
        ++(*index);
    } else {
        *index = static_cast<std::uint32_t>(count - 1u);
    }
}

bool boundary_at(
        const std::vector<Boundary>& values,
        std::uint32_t index,
        const Boundary** out) {
    if (out == nullptr || index >= values.size()) return false;
    *out = &values[index];
    return true;
}

void mirror_update_data(Cursor* cursor, PlaybackResult* result) {
    if (cursor == nullptr || result == nullptr ||
        cursor->previous_frame_1a8 == cursor->current_frame_1a4) {
        return;
    }
    if (cursor->update_data_gate_1bf) ++result->update_data_count;
    cursor->previous_frame_1a8 = cursor->current_frame_1a4;
}

bool valid_boundaries(
        const std::vector<Boundary>& values,
        std::size_t frame_count) {
    for (const auto& value : values) {
        if (value.field_18 < 0 ||
            static_cast<std::size_t>(value.field_18) >= frame_count) {
            return false;
        }
    }
    return true;
}

}  // namespace

State initial_play_state() {
    State state;
    // MeActionsSystem::play(): +0x220=0, +0x1bc=1, +0x1b4=0, +0x1b0=0.
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
        const me_actions_combo_boundaries::Boundaries& boundaries,
        float delta_seconds,
        Cursor cursor) {
    PlaybackResult result;
    result.cursor = cursor;

    const std::size_t frame_count = document.primary_records.size();
    if (frame_count == 0u || frame_count != document.final_table.entries.size() ||
        frame_count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
        !valid_boundaries(boundaries.array_b0_values, frame_count) ||
        !valid_boundaries(boundaries.array_b4_values, frame_count)) {
        return result;
    }
    result.valid_projection = true;

    float next_delta = delta_seconds;
    while (!result.cursor.timing.done_220) {
        TimingResult timing = apply_wa_update_timing(
            document,
            result.cursor.current_frame_1a4,
            next_delta,
            result.cursor.timing);
        // Native loops back after frame processing without reapplying the caller
        // delta; only the already retained millisecond accumulator remains.
        next_delta = 0.0f;
        result.cursor.timing = timing.state;
        result.valid_frame = result.valid_frame || timing.valid_frame;
        if (!timing.frame_processing_ready) break;

        const Boundary* primary = nullptr;
        if (!boundaries.array_b0_values.empty() &&
            !boundary_at(
                boundaries.array_b0_values,
                result.cursor.boundary_index_1f8,
                &primary)) {
            result.valid_projection = false;
            break;
        }
        const Boundary* secondary = nullptr;
        if (!boundaries.array_b4_values.empty() &&
            !boundary_at(
                boundaries.array_b4_values,
                result.cursor.secondary_boundary_index_1fc,
                &secondary)) {
            result.valid_projection = false;
            break;
        }

        ++result.cursor.current_frame_1a4;
        ++result.frames_advanced;
        if (result.cursor.update_marker_218) result.cursor.update_marker_219 = true;

        if (primary != nullptr) {
            const std::int32_t primary_end = primary->field_18;
            bool has_next_primary_run = false;
            if (result.cursor.boundary_gate_201 &&
                result.cursor.current_frame_1a4 >= primary_end) {
                const std::uint32_t before = result.cursor.boundary_index_1f8;
                clamp_increment(
                    &result.cursor.boundary_index_1f8,
                    boundaries.array_b0_values.size());
                has_next_primary_run = result.cursor.boundary_index_1f8 != before;
            }

            // In the primary-boundary branch native advances +0x1fc on >=.
            if (secondary != nullptr &&
                result.cursor.current_frame_1a4 >= secondary->field_18) {
                clamp_increment(
                    &result.cursor.secondary_boundary_index_1fc,
                    boundaries.array_b4_values.size());
            }

            if (!has_next_primary_run &&
                result.cursor.current_frame_1a4 > primary_end) {
                result.cursor.last_frame_1f4 = primary_end - 1;
                result.cursor.finished_cycle_200 = true;
                result.cursor.current_frame_1a4 = 0;
                result.cursor.timing.done_220 = true;
                result.became_done = true;
            }
        } else {
            const std::int32_t count = static_cast<std::int32_t>(frame_count);
            // The fallback branch uses strict > for the secondary boundary.
            if (secondary != nullptr &&
                result.cursor.current_frame_1a4 > secondary->field_18) {
                clamp_increment(
                    &result.cursor.secondary_boundary_index_1fc,
                    boundaries.array_b4_values.size());
            }
            if (result.cursor.current_frame_1a4 > count) {
                result.cursor.last_frame_1f4 = count - 1;
                result.cursor.finished_cycle_200 = true;
                result.cursor.current_frame_1a4 = 0;
                result.cursor.timing.done_220 = true;
                result.became_done = true;
            }
        }

        mirror_update_data(&result.cursor, &result);
    }

    return result;
}

}  // namespace nevergone::me_actions_playback_timing
