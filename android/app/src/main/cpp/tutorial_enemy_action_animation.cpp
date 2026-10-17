#include "tutorial_enemy_action_animation.h"

#include <cmath>

namespace nevergone::tutorial_enemy_action_animation {
namespace {

bool frame_at_time(
        const enemy_actions_wbg_document::Document& document,
        double elapsed_seconds,
        bool looping,
        std::size_t* out_index,
        double* out_remainder) {
    if (out_index == nullptr || out_remainder == nullptr ||
        document.primary_records.empty() ||
        document.primary_records.size() != document.final_table.entries.size() ||
        !std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0) {
        return false;
    }

    double total_duration = 0.0;
    for (const auto& entry : document.final_table.entries) {
        const double duration = static_cast<double>(entry.reciprocal_value);
        if (!std::isfinite(duration) || duration <= 0.0) return false;
        total_duration += duration;
    }
    if (!std::isfinite(total_duration) || total_duration <= 0.0) return false;
    if (looping) elapsed_seconds = std::fmod(elapsed_seconds, total_duration);
    if (!looping && elapsed_seconds >= total_duration) return false;

    for (std::size_t index = 0; index < document.final_table.entries.size(); ++index) {
        const double duration = static_cast<double>(
                document.final_table.entries[index].reciprocal_value);
        if (elapsed_seconds < duration) {
            *out_index = index;
            *out_remainder = elapsed_seconds;
            return true;
        }
        elapsed_seconds -= duration;
    }

    if (!looping) return false;
    *out_index = 0u;
    *out_remainder = 0.0;
    return true;
}

}  // namespace

Sample sample(
        const enemy_actions_wbg_document::Document& appearance,
        unsigned appearance_action_id,
        bool appearance_loops,
        const enemy_actions_wbg_document::Document& state_action,
        unsigned state_action_id,
        bool state_action_loops,
        double elapsed_seconds) {
    Sample result;
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0) return result;

    std::size_t frame_index = 0;
    double remainder = 0.0;
    if (frame_at_time(
                appearance, elapsed_seconds, appearance_loops, &frame_index, &remainder)) {
        result.phase = Phase::kAppearance;
        result.action_id = appearance_action_id;
        result.frame_index = frame_index;
        result.document = &appearance;
        return result;
    }

    double appearance_duration = 0.0;
    for (const auto& entry : appearance.final_table.entries) {
        const double duration = static_cast<double>(entry.reciprocal_value);
        if (!std::isfinite(duration) || duration <= 0.0) return result;
        appearance_duration += duration;
    }
    if (!std::isfinite(appearance_duration) || elapsed_seconds < appearance_duration) {
        return result;
    }

    const double state_elapsed = elapsed_seconds - appearance_duration;
    if (!frame_at_time(
                state_action, state_elapsed, state_action_loops, &frame_index, &remainder)) {
        return result;
    }
    result.phase = Phase::kStateAction;
    result.action_id = state_action_id;
    result.frame_index = frame_index;
    result.document = &state_action;
    return result;
}

Sample sample_action(
        const enemy_actions_wbg_document::Document& action,
        unsigned action_id,
        bool loops,
        double elapsed_seconds) {
    Sample result;
    std::size_t frame_index = 0;
    double remainder = 0.0;
    if (!frame_at_time(
                action, elapsed_seconds, loops, &frame_index, &remainder)) {
        return result;
    }
    result.phase = Phase::kStateAction;
    result.action_id = action_id;
    result.frame_index = frame_index;
    result.document = &action;
    return result;
}

}  // namespace nevergone::tutorial_enemy_action_animation
