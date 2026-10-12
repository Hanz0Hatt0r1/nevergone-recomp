#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "scene_actions_play_state.h"
#include "trap_actions_document.h"

namespace nevergone::scene_actions_wa_update {

constexpr float kMillisecondsPerSecond = 1000.0f;

enum class Status {
    kRejected = 0,
    kNoAdvance,
    kAdvanced,
    kTerminalBoundary,
};

struct Result {
    Status status = Status::kRejected;
    std::size_t frames_advanced = 0;
    bool update_data_requested = false;
    bool terminal_boundary_reached = false;
    // Exact ActionComboValue+0x18 selected by the native +0x94 terminal path.
    std::size_t terminal_combo_boundary_frame = 0;
};

inline bool valid_boundary_array(
        const std::vector<trap_actions_combo_section::DerivedActionComboValue>& values,
        std::size_t frame_count) {
    if (values.empty() || frame_count == 0u) return false;
    std::size_t previous = 0u;
    bool first = true;
    for (const auto& value : values) {
        if (value.field_18 < 0) return false;
        const std::size_t boundary = static_cast<std::size_t>(value.field_18);
        if (boundary >= frame_count) return false;
        if (!first && boundary <= previous) return false;
        previous = boundary;
        first = false;
    }
    return previous + 1u == frame_count;
}

inline bool valid_document(const trap_actions_document::Document& document) {
    const std::size_t frame_count = document.core.primary.records.size();
    if (frame_count == 0u || document.final_table.entries.size() != frame_count) return false;
    if (static_cast<std::size_t>(document.core.primary.prefix.record_count) != frame_count ||
            static_cast<std::size_t>(document.combo.primary_record_count) != frame_count ||
            document.combo.tuples.size() != frame_count ||
            document.combo.array_90_values.size() != frame_count ||
            !valid_boundary_array(document.combo.array_94_values, frame_count) ||
            !valid_boundary_array(document.combo.array_98_values, frame_count)) {
        return false;
    }
    for (const auto& entry : document.final_table.entries) {
        if (!std::isfinite(entry.reciprocal_value) || entry.reciprocal_value <= 0.0f) return false;
    }
    return true;
}

inline Result advance(
        const trap_actions_document::Document& document,
        float dt_seconds,
        scene_actions_play_state::State* state) {
    Result result;
    if (state == nullptr || !state->active_15d || state->done_1a8 ||
        !std::isfinite(dt_seconds) || dt_seconds < 0.0f || !valid_document(document)) {
        return result;
    }
    if (state->frame_index_250 >= document.final_table.entries.size() ||
            state->boundary_cursor_94_25c >= document.combo.array_94_values.size() ||
            state->boundary_cursor_98_260 >= document.combo.array_98_values.size()) {
        return result;
    }

    if (state->first_update_15c) {
        state->first_update_15c = false;
        state->accumulator_154 = 0.0f;
    } else {
        state->accumulator_154 += dt_seconds * kMillisecondsPerSecond;
        if (!std::isfinite(state->accumulator_154)) return result;
    }

    const std::size_t frame_count = document.final_table.entries.size();
    while (state->frame_index_250 < frame_count) {
        const float threshold = document.final_table.entries[state->frame_index_250].reciprocal_value;
        state->current_frame_value_150 = threshold;
        if (state->accumulator_154 < threshold) {
            result.status = result.frames_advanced == 0u ? Status::kNoAdvance : Status::kAdvanced;
            return result;
        }

        state->accumulator_154 -= threshold;
        const std::size_t next_frame = state->frame_index_250 + 1u;

        // For every valid parsed TrapActions document +0x94 is non-empty and
        // its final field_18 is the final primary tuple index. Native waUpdate
        // therefore uses this current run boundary for the normal terminal path;
        // its CCArray::count() fallback is unreachable for non-empty parsed data.
        const auto& boundary_94 = document.combo.array_94_values[state->boundary_cursor_94_25c];
        const std::size_t boundary_94_frame = static_cast<std::size_t>(boundary_94.field_18);
        if (state->combo_type_24c) state->combo_latched_24d = true;

        // Native local r5 becomes 1 only when +0x94 advances to a *new* run.
        // At the final run it attempts to advance, clamps back to the final
        // element, leaves this flag zero, then applies `frame > field_18`.
        bool advanced_to_next_94_boundary = false;
        if (state->combo_hit_active_185 && next_frame >= boundary_94_frame) {
            if (state->boundary_cursor_94_25c + 1u < document.combo.array_94_values.size()) {
                ++state->boundary_cursor_94_25c;
                advanced_to_next_94_boundary = true;
            }
        }

        const auto& boundary_98 = document.combo.array_98_values[state->boundary_cursor_98_260];
        const std::size_t boundary_98_frame = static_cast<std::size_t>(boundary_98.field_18);
        // Native uses a strict `frame > field_18` comparison for +0x98.
        if (next_frame > boundary_98_frame) {
            if (state->boundary_cursor_98_260 + 1u < document.combo.array_98_values.size()) {
                ++state->boundary_cursor_98_260;
            }
        }

        state->frame_index_250 = next_frame;
        ++result.frames_advanced;

        if (!advanced_to_next_94_boundary && next_frame > boundary_94_frame) {
            result.status = Status::kTerminalBoundary;
            result.terminal_boundary_reached = true;
            result.terminal_combo_boundary_frame = boundary_94_frame;
            result.update_data_requested = false;
            return result;
        }

        // The structural validator proves the final +0x94 boundary is
        // frame_count-1, so any non-terminal frame remains a valid primary AFD.
        if (state->frame_index_250 >= frame_count) return Result{};

        result.update_data_requested = true;
        state->current_frame_value_150 =
                document.final_table.entries[state->frame_index_250].reciprocal_value;
    }

    return result;
}

}  // namespace nevergone::scene_actions_wa_update
