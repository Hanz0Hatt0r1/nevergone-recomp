#include <cassert>
#include <cstdint>

#include "enemy_actions_combo_consumer.h"

int main() {
    using nevergone::enemy_actions_combo_consumer::combo_hit_mode_for_frame;
    using nevergone::enemy_actions_combo_consumer::current_frame_power;
    using nevergone::enemy_actions_wbg_combo_section::Block;
    using nevergone::enemy_actions_wbg_combo_section::DerivedActionComboValue;
    using nevergone::enemy_actions_wbg_combo_section::kArray94Offset;
    using nevergone::enemy_actions_wbg_combo_section::kArray9cOffset;

    Block block;

    DerivedActionComboValue power0;
    power0.target_array_offset = kArray9cOffset;
    power0.source_tuple_index = 0u;
    power0.field_1c = 3;
    block.array_9c_values.push_back(power0);

    DerivedActionComboValue power1;
    power1.target_array_offset = kArray9cOffset;
    power1.source_tuple_index = 1u;
    power1.field_1c = -4;
    block.array_9c_values.push_back(power1);

    assert(current_frame_power(block, 0u) == 3);
    assert(current_frame_power(block, 1u) == -4);
    assert(current_frame_power(block, 2u) == 1);
    assert(current_frame_power(block, 999u) == 1);

    DerivedActionComboValue range0;
    range0.target_array_offset = kArray94Offset;
    range0.field_14 = 0;
    range0.field_18 = 2;
    range0.field_1c = 1;
    block.array_94_values.push_back(range0);

    // Native comboHit() refuses to evaluate when +0x94 contains <= 1 value.
    assert(!combo_hit_mode_for_frame(block, 0u, 1).has_value());

    DerivedActionComboValue range1;
    range1.target_array_offset = kArray94Offset;
    range1.field_14 = 3;
    range1.field_18 = 5;
    range1.field_1c = 2;
    block.array_94_values.push_back(range1);

    const auto first_start = combo_hit_mode_for_frame(block, 0u, 0);
    const auto first_end = combo_hit_mode_for_frame(block, 0u, 2);
    assert(first_start.has_value() && *first_start == 1);
    assert(first_end.has_value() && *first_end == 1);
    assert(!combo_hit_mode_for_frame(block, 0u, -1).has_value());
    assert(!combo_hit_mode_for_frame(block, 0u, 3).has_value());

    const auto second_start = combo_hit_mode_for_frame(block, 1u, 3);
    const auto second_end = combo_hit_mode_for_frame(block, 1u, 5);
    assert(second_start.has_value() && *second_start == 2);
    assert(second_end.has_value() && *second_end == 2);
    assert(!combo_hit_mode_for_frame(block, 1u, 2).has_value());
    assert(!combo_hit_mode_for_frame(block, 1u, 6).has_value());
    assert(!combo_hit_mode_for_frame(block, 2u, 4).has_value());

    return 0;
}
