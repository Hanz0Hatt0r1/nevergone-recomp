#include "enemy_actions_runtime_state.h"

#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace nevergone::enemy_actions_runtime_state {
namespace {

std::int32_t arm32_increment(std::int32_t value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    bits += 1u;
    std::int32_t result = 0;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

std::string format_resource_path(
        const char* prefix,
        std::int32_t id,
        const std::string& resource_name) {
    std::ostringstream stream;
    stream << prefix << std::setfill('0') << std::setw(2) << id
           << "/res/" << resource_name;
    return stream.str();
}

}  // namespace

ComboHitResult apply_combo_hit(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        State state) {
    enemy_actions_combo_consumer::ComboHitState hit_state;
    hit_state.flag_190 = state.flag_190;
    hit_state.flag_191 = state.flag_191;
    hit_state.flag_290 = state.flag_290;

    const auto hit = enemy_actions_combo_consumer::apply_combo_hit_transition(
            combo_block,
            state.boundary_index_2a0,
            state.current_frame_294,
            hit_state);

    state.flag_190 = hit.state.flag_190;
    state.flag_191 = hit.state.flag_191;
    state.flag_290 = hit.state.flag_290;

    ComboHitResult result;
    result.state = state;
    result.returned_true = hit.returned_true;
    return result;
}

WaUpdateTimingResult apply_wa_update_timing(
        const enemy_actions_wbg_final_table::Table& final_table,
        float delta_seconds,
        State state) {
    WaUpdateTimingResult result;
    result.state = state;

    if (state.flag_1bc != 0u) {
        result.blocked_by_flag_1bc = true;
        return result;
    }

    if (state.flag_168 != 0u) {
        result.state.flag_168 = 0u;
        result.state.field_160 = 0.0f;
    } else {
        result.state.field_160 = state.field_160 + delta_seconds * 1000.0f;
    }

    if (state.current_frame_294 < 0) return result;
    const std::size_t frame_index = static_cast<std::size_t>(state.current_frame_294);
    if (frame_index >= final_table.entries.size()) return result;

    result.had_valid_action_frame = true;
    result.action_frame_5c = final_table.entries[frame_index].reciprocal_value;
    result.state.field_158 = result.action_frame_5c + state.field_15c;

    if (std::isnan(result.state.field_160) || std::isnan(result.state.field_158) ||
        result.state.field_160 < result.state.field_158) {
        return result;
    }

    result.threshold_reached = true;
    if (state.flag_169 == 0u) return result;

    result.state.field_160 -= result.state.field_158;
    result.frame_processing_allowed = true;
    return result;
}

WaUpdateAfterFrameResult apply_wa_update_after_frame_advance(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        State state) {
    WaUpdateAfterFrameResult result;
    result.state = state;

    if (state.boundary_index_2a0 < combo_block.array_94_values.size()) {
        result.had_valid_boundary_94 = true;
        result.boundary_94_endpoint_18 =
                combo_block.array_94_values[state.boundary_index_2a0].field_18;
    }

    enemy_actions_combo_consumer::WaUpdateComboState combo_state;
    combo_state.flag_191 = state.flag_191;
    combo_state.flag_290 = state.flag_290;
    combo_state.flag_291 = state.flag_291;
    combo_state.boundary_index_2a0 = state.boundary_index_2a0;
    combo_state.boundary_index_2a4 = state.boundary_index_2a4;

    const auto combo = enemy_actions_combo_consumer::apply_wa_update_combo_transition(
            combo_block.array_94_values,
            combo_block.array_98_values,
            state.current_frame_294,
            combo_state);

    result.boundary_94 = combo.boundary;
    result.boundary_98 = combo.boundary_98;
    result.state.flag_191 = combo.state.flag_191;
    result.state.flag_290 = combo.state.flag_290;
    result.state.flag_291 = combo.state.flag_291;
    result.state.boundary_index_2a0 = combo.state.boundary_index_2a0;
    result.state.boundary_index_2a4 = combo.state.boundary_index_2a4;

    if (!result.had_valid_boundary_94) return result;

    enemy_actions_combo_consumer::WaUpdateCompletionState completion_state;
    completion_state.field_18c = result.state.field_18c;
    completion_state.flag_190 = result.state.flag_190;
    completion_state.flag_169 = result.state.flag_169;
    completion_state.flag_1bc = result.state.flag_1bc;
    completion_state.flag_292 = result.state.flag_292;
    completion_state.current_frame_294 = result.state.current_frame_294;

    const auto completion =
            enemy_actions_combo_consumer::apply_wa_update_completion_transition(
                    result.boundary_94_endpoint_18,
                    result.boundary_94.advanced_to_next,
                    result.state.current_frame_294,
                    completion_state);

    result.state.field_18c = completion.state.field_18c;
    result.state.flag_190 = completion.state.flag_190;
    result.state.flag_169 = completion.state.flag_169;
    result.state.flag_1bc = completion.state.flag_1bc;
    result.state.flag_292 = completion.state.flag_292;
    result.state.current_frame_294 = completion.state.current_frame_294;
    result.reset_applied = completion.reset_applied;
    return result;
}

WaUpdateFrameStepResult apply_wa_update_frame_step(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::uint32_t action_frame_count,
        State state) {
    WaUpdateFrameStepResult result;
    result.state = state;
    result.state.current_frame_294 = arm32_increment(state.current_frame_294);

    const bool valid_boundary_94 =
            state.boundary_index_2a0 < combo_block.array_94_values.size();

    if (valid_boundary_94) {
        const auto after = apply_wa_update_after_frame_advance(combo_block, result.state);
        result.state = after.state;
        result.boundary_94 = after.boundary_94;
        result.boundary_98 = after.boundary_98;
        result.had_valid_boundary_94 = after.had_valid_boundary_94;
        result.reset_applied = after.reset_applied;
    } else {
        result.used_no_boundary_94_fallback = true;

        result.boundary_98 = enemy_actions_combo_consumer::advance_boundary_cursor(
                combo_block.array_98_values,
                state.boundary_index_2a4,
                result.state.current_frame_294);
        result.state.boundary_index_2a4 = result.boundary_98.index;

        const std::int32_t count_i32 = static_cast<std::int32_t>(action_frame_count);
        if (result.state.current_frame_294 > count_i32) {
            result.state.field_18c = count_i32 - 1;
            result.state.flag_190 = 1u;
            result.state.current_frame_294 = 0;
            result.state.flag_1bc = 1u;
            if (result.state.flag_292 == 0u) result.state.flag_169 = 0u;
            result.reset_applied = true;
        }
    }

    const bool update_data_gate =
            !result.reset_applied || result.state.flag_292 != 0u;

    if (result.state.previous_frame_154 != result.state.current_frame_294) {
        result.previous_frame_changed = true;
        result.should_call_update_data = update_data_gate;
        result.state.previous_frame_154 = result.state.current_frame_294;
    }

    return result;
}

WaUpdateIterationResult apply_wa_update_iteration(
        const enemy_actions_wbg_document::Document& document,
        float delta_seconds,
        State state) {
    WaUpdateIterationResult result;
    result.timing = apply_wa_update_timing(document.final_table, delta_seconds, state);
    result.state = result.timing.state;

    if (!result.timing.frame_processing_allowed) return result;

    result.frame_step = apply_wa_update_frame_step(
            document.combo_block,
            document.prefix.action_frame_count,
            result.timing.state);
    result.state = result.frame_step.state;
    result.frame_step_applied = true;
    return result;
}

ShowActionLastFrameResult apply_show_action_last_frame(State state) {
    ShowActionLastFrameResult result;
    result.state = state;
    result.state.current_frame_294 = state.field_18c;
    return result;
}

UpdateDataEntryResult apply_update_data_entry(
        const enemy_actions_wbg_document::Document& document,
        State state) {
    UpdateDataEntryResult result;
    result.state = state;

    if (state.flag_26c == 0u) return result;
    result.readiness_gate_open = true;

    if (document.primary_records.empty()) return result;
    result.has_primary_frames = true;

    if (state.current_frame_294 < 0) return result;
    const std::size_t frame_index = static_cast<std::size_t>(state.current_frame_294);
    if (frame_index >= document.primary_records.size()) return result;

    result.current_frame_in_range = true;
    result.would_enter_frame_update = true;
    result.selected_primary_index = frame_index;
    return result;
}

UpdateDataResourceResult apply_update_data_resource_selection(
        const enemy_actions_wbg_document::Document& document,
        State state) {
    UpdateDataResourceResult result;
    result.state = state;
    result.entry = apply_update_data_entry(document, state);
    if (!result.entry.would_enter_frame_update) return result;

    const auto& frame = document.primary_records[result.entry.selected_primary_index];
    result.selected_frame_available = true;
    result.frame_resource_60 = frame.first_string;

    const char* prefix = nullptr;
    switch (state.field_250) {
        case 0:
            prefix = "enemy";
            break;
        case 1:
            prefix = "npc";
            break;
        case 2:
            prefix = "pet";
            break;
        default:
            break;
    }

    if (prefix != nullptr) {
        result.formatted_path_available = true;
        result.formatted_path =
                format_resource_path(prefix, state.field_268, result.frame_resource_60);
    }

    if (result.frame_resource_60 == "looping") {
        result.should_add_sprite_frames = true;
        result.sprite_frames_file = result.frame_resource_60;
        return result;
    }

    if ((state.field_250 == 1 || state.field_250 == 2) &&
        result.formatted_path_available) {
        result.should_add_sprite_frames = true;
        result.sprite_frames_file = result.formatted_path;
        result.should_record_enemy_object_res = true;
    }

    return result;
}

UpdateDataFrameLookupResult apply_update_data_frame_lookup(
        const enemy_actions_wbg_document::Document& document,
        State state) {
    UpdateDataFrameLookupResult result;
    result.state = state;
    result.entry = apply_update_data_entry(document, state);
    if (!result.entry.would_enter_frame_update) return result;

    const auto& frame = document.primary_records[result.entry.selected_primary_index];
    result.selected_frame_available = true;
    result.frame_name_68 = frame.third_string;
    result.frame_name_length_6c = frame.third_string_length_i32;
    result.should_lookup_sprite_frame = true;
    return result;
}

}  // namespace nevergone::enemy_actions_runtime_state
