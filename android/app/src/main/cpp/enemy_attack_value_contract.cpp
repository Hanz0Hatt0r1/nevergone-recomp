#include "enemy_attack_value_contract.h"

#include <cmath>

#include "enemy_actions_wbg_topology_evidence.h"

namespace nevergone::enemy_attack_value_contract {

std::optional<std::uint32_t> try_initialize_value_2bc(float game_levels_value_20) noexcept {
    if (!std::isfinite(game_levels_value_20) || game_levels_value_20 < 0.0f ||
        static_cast<double>(game_levels_value_20) >= 4294967296.0) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(game_levels_value_20);
}

float resolve(std::uint32_t value_2bc, float current_frame_damage_factor) noexcept {
    return static_cast<float>(value_2bc) * current_frame_damage_factor;
}

std::optional<float> resolve_at_frame(
        std::uint32_t value_2bc,
        const enemy_actions_wbg_document::Document& action,
        std::int32_t current_frame) noexcept {
    if (current_frame < 0) return std::nullopt;

    const std::size_t frame = static_cast<std::size_t>(current_frame);
    const std::size_t count = static_cast<std::size_t>(action.prefix.action_frame_count);
    if (frame >= count || action.primary_records.size() != count ||
        action.combo_block.primary_record_count != action.prefix.action_frame_count ||
        action.combo_block.tuples.size() != count ||
        action.final_table.entries.size() != count) {
        return std::nullopt;
    }

    const auto& tuple = action.combo_block.tuples[frame];
    if (tuple.first_float_target_offset !=
            enemy_actions_wbg_topology_evidence::combo_first_float_destination(frame)) {
        return std::nullopt;
    }
    return resolve(value_2bc, tuple.float_values[0]);
}

std::int32_t resolve_frame_power(
        const enemy_actions_wbg_document::Document& action,
        std::int32_t current_frame) noexcept {
    if (current_frame < 0) return 1;
    const std::size_t frame = static_cast<std::size_t>(current_frame);
    if (frame >= action.combo_block.array_9c_values.size()) return 1;

    const auto& value = action.combo_block.array_9c_values[frame];
    if (value.target_array_offset != enemy_actions_wbg_combo_section::kArray9cOffset ||
        value.source_tuple_index != frame) {
        return 1;
    }
    return value.field_1c;
}

std::optional<CombatFrameValues> resolve_combat_frame(
        std::uint32_t value_2bc,
        const enemy_actions_wbg_document::Document& action,
        std::int32_t current_frame) noexcept {
    const auto input_attack_value = resolve_at_frame(value_2bc, action, current_frame);
    if (!input_attack_value.has_value()) return std::nullopt;

    CombatFrameValues values;
    values.input_attack_value = *input_attack_value;
    values.frame_power = resolve_frame_power(action, current_frame);
    return values;
}

}  // namespace nevergone::enemy_attack_value_contract
