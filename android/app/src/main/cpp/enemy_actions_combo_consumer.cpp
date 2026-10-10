#include "enemy_actions_combo_consumer.h"

namespace nevergone::enemy_actions_combo_consumer {

std::int32_t current_frame_power(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::uint32_t current_frame_index) {
    const std::size_t index = static_cast<std::size_t>(current_frame_index);
    if (index >= combo_block.array_9c_values.size()) return 1;
    return combo_block.array_9c_values[index].field_1c;
}

std::optional<std::int32_t> combo_hit_mode_for_frame(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::size_t range_index,
        std::int32_t current_frame_index) {
    if (combo_block.array_94_values.size() <= 1u ||
        range_index >= combo_block.array_94_values.size()) {
        return std::nullopt;
    }

    const auto& range = combo_block.array_94_values[range_index];
    if (current_frame_index < range.field_14 ||
        current_frame_index > range.field_18) {
        return std::nullopt;
    }
    return range.field_1c;
}

}  // namespace nevergone::enemy_actions_combo_consumer
