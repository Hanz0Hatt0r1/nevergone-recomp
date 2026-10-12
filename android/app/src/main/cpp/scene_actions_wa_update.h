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
    // Native waUpdate() has a separate one-past-frame completion branch. Keep
    // this edge explicit instead of silently replacing its signed `> count`
    // check with a project-side `>= size` rule.
    kTerminalBoundary,
};

struct Result {
    Status status = Status::kRejected;
    std::size_t frames_advanced = 0;
    bool update_data_requested = false;
    bool terminal_boundary_reached = false;
};

inline bool valid_document(const trap_actions_document::Document& document) {
    const std::size_t frame_count = document.core.primary.records.size();
    if (frame_count == 0u || document.final_table.entries.size() != frame_count) return false;
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
    if (state->frame_index_250 >= document.final_table.entries.size()) return result;
    if (state->boundary_cursor_94_25c >= document.combo.array_94_values.size() &&
        !document.combo.array_94_values.empty()) {
        return result;
    }
    if (state->boundary_cursor_98_260 >= document.combo.array_98_values.size() &&
        !document.combo.array_98_values.empty()) {
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

        if (!document.combo.array_94_values.empty()) {
            const auto& boundary = document.combo.array_94_values[state->boundary_cursor_94_25c];
            if (state->combo_type_24c) state->combo_latched_24d = true;
            // The native cursor advances only after comboHit() has set +0x185.
            if (state->combo_hit_active_185 &&
                next_frame >= static_cast<std::size_t>(static_cast<std::uint32_t>(boundary.field_18))) {
                if (state->boundary_cursor_94_25c + 1u < document.combo.array_94_values.size()) {
                    ++state->boundary_cursor_94_25c;
                }
            }
        }

        if (!document.combo.array_98_values.empty()) {
            const auto& boundary = document.combo.array_98_values[state->boundary_cursor_98_260];
            // Native uses a strict `frame > field_18` comparison for +0x98.
            if (next_frame > static_cast<std::size_t>(static_cast<std::uint32_t>(boundary.field_18))) {
                if (state->boundary_cursor_98_260 + 1u < document.combo.array_98_values.size()) {
                    ++state->boundary_cursor_98_260;
                }
            }
        }

        state->frame_index_250 = next_frame;
        ++result.frames_advanced;
        result.update_data_requested = true;

        if (state->frame_index_250 >= frame_count) {
            result.status = Status::kTerminalBoundary;
            result.terminal_boundary_reached = true;
            return result;
        }

        state->current_frame_value_150 =
                document.final_table.entries[state->frame_index_250].reciprocal_value;
    }

    return result;
}

}  // namespace nevergone::scene_actions_wa_update
