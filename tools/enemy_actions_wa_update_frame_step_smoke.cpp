#include <cassert>
#include <cstdint>
#include <limits>

#include "enemy_actions_runtime_state.h"

int main() {
    namespace runtime = nevergone::enemy_actions_runtime_state;
    using nevergone::enemy_actions_wbg_combo_section::Block;
    using nevergone::enemy_actions_wbg_combo_section::DerivedActionComboValue;
    using nevergone::enemy_actions_wbg_combo_section::kArray94Offset;
    using nevergone::enemy_actions_wbg_combo_section::kArray98Offset;

    Block block;

    DerivedActionComboValue range0;
    range0.target_array_offset = kArray94Offset;
    range0.field_18 = 2;
    block.array_94_values.push_back(range0);

    DerivedActionComboValue range1;
    range1.target_array_offset = kArray94Offset;
    range1.field_18 = 5;
    block.array_94_values.push_back(range1);

    DerivedActionComboValue aux0;
    aux0.target_array_offset = kArray98Offset;
    aux0.field_18 = 1;
    block.array_98_values.push_back(aux0);

    DerivedActionComboValue aux1;
    aux1.target_array_offset = kArray98Offset;
    aux1.field_18 = 7;
    block.array_98_values.push_back(aux1);

    // Valid +0x94 path: increment frame first, then both boundary streams.
    runtime::State advancing;
    advancing.flag_191 = 1u;
    advancing.flag_169 = 1u;
    advancing.previous_frame_154 = 1;
    advancing.current_frame_294 = 1;
    advancing.boundary_index_2a0 = 0u;
    advancing.boundary_index_2a4 = 0u;
    const auto advanced = runtime::apply_wa_update_frame_step(block, 6u, advancing);
    assert(advanced.had_valid_boundary_94);
    assert(!advanced.used_no_boundary_94_fallback);
    assert(advanced.state.current_frame_294 == 2);
    assert(advanced.boundary_94.advanced_to_next);
    assert(advanced.state.boundary_index_2a0 == 1u);
    assert(advanced.boundary_98.advanced_to_next);
    assert(advanced.state.boundary_index_2a4 == 1u);
    assert(!advanced.reset_applied);
    assert(advanced.previous_frame_changed);
    assert(advanced.should_call_update_data);
    assert(advanced.state.previous_frame_154 == 2);

    // If +0x154 already equals the post-increment frame, native skips both the
    // updateData call and the redundant +0x154 store path.
    runtime::State unchanged = advancing;
    unchanged.previous_frame_154 = 2;
    const auto unchanged_result = runtime::apply_wa_update_frame_step(block, 6u, unchanged);
    assert(unchanged_result.state.current_frame_294 == 2);
    assert(!unchanged_result.previous_frame_changed);
    assert(!unchanged_result.should_call_update_data);
    assert(unchanged_result.state.previous_frame_154 == 2);

    // Final +0x94 endpoint: increment to 6, clamp the boundary, then apply the
    // proven reset. With +0x292 == 0 native suppresses updateData after reset.
    runtime::State final_reset;
    final_reset.flag_191 = 1u;
    final_reset.flag_169 = 9u;
    final_reset.previous_frame_154 = 5;
    final_reset.current_frame_294 = 5;
    final_reset.boundary_index_2a0 = 1u;
    final_reset.boundary_index_2a4 = 1u;
    const auto reset = runtime::apply_wa_update_frame_step(block, 6u, final_reset);
    assert(reset.had_valid_boundary_94);
    assert(reset.boundary_94.reached_last_boundary);
    assert(reset.reset_applied);
    assert(reset.state.field_18c == 4);
    assert(reset.state.flag_190 == 1u);
    assert(reset.state.flag_1bc == 1u);
    assert(reset.state.flag_169 == 0u);
    assert(reset.state.current_frame_294 == 0);
    assert(reset.previous_frame_changed);
    assert(!reset.should_call_update_data);
    assert(reset.state.previous_frame_154 == 0);

    // The same reset with nonzero +0x292 preserves +0x169 and keeps the native
    // updateData gate enabled when +0x154 differs from the reset frame.
    runtime::State reset_with_292 = final_reset;
    reset_with_292.flag_292 = 3u;
    const auto reset_292 = runtime::apply_wa_update_frame_step(block, 6u, reset_with_292);
    assert(reset_292.reset_applied);
    assert(reset_292.state.flag_169 == 9u);
    assert(reset_292.previous_frame_changed);
    assert(reset_292.should_call_update_data);

    // No +0x94 object: native still increments the frame and updates +0x98,
    // then compares current frame against the +0x88 ActionFrameData count.
    Block no_94;
    no_94.array_98_values = block.array_98_values;
    runtime::State fallback;
    fallback.flag_169 = 1u;
    fallback.previous_frame_154 = 2;
    fallback.current_frame_294 = 2;
    fallback.boundary_index_2a4 = 0u;
    const auto fallback_ok = runtime::apply_wa_update_frame_step(no_94, 3u, fallback);
    assert(!fallback_ok.had_valid_boundary_94);
    assert(fallback_ok.used_no_boundary_94_fallback);
    assert(fallback_ok.state.current_frame_294 == 3);
    assert(fallback_ok.boundary_98.advanced_to_next);
    assert(fallback_ok.state.boundary_index_2a4 == 1u);
    assert(!fallback_ok.reset_applied);
    assert(fallback_ok.previous_frame_changed);
    assert(fallback_ok.should_call_update_data);
    assert(fallback_ok.state.previous_frame_154 == 3);

    // One past the frame count enters the fallback reset and stores count-1 at
    // +0x18c. Zero +0x292 again suppresses updateData after the reset.
    runtime::State fallback_past = fallback;
    fallback_past.previous_frame_154 = 3;
    fallback_past.current_frame_294 = 3;
    fallback_past.boundary_index_2a4 = 1u;
    const auto fallback_reset = runtime::apply_wa_update_frame_step(no_94, 3u, fallback_past);
    assert(fallback_reset.used_no_boundary_94_fallback);
    assert(fallback_reset.reset_applied);
    assert(fallback_reset.state.field_18c == 2);
    assert(fallback_reset.state.current_frame_294 == 0);
    assert(fallback_reset.state.flag_190 == 1u);
    assert(fallback_reset.state.flag_1bc == 1u);
    assert(fallback_reset.state.flag_169 == 0u);
    assert(fallback_reset.previous_frame_changed);
    assert(!fallback_reset.should_call_update_data);
    assert(fallback_reset.state.previous_frame_154 == 0);

    // Native ADDS wraps the signed frame storage as raw ARM32 bits.
    runtime::State wrapping;
    wrapping.previous_frame_154 = std::numeric_limits<std::int32_t>::max();
    wrapping.current_frame_294 = std::numeric_limits<std::int32_t>::max();
    const auto wrapped = runtime::apply_wa_update_frame_step(no_94, 3u, wrapping);
    assert(wrapped.state.current_frame_294 == std::numeric_limits<std::int32_t>::min());
    assert(wrapped.previous_frame_changed);
    assert(wrapped.should_call_update_data);

    // Empty +0x88 count follows the native count-1 fallback exactly.
    runtime::State empty_count;
    empty_count.flag_169 = 1u;
    empty_count.current_frame_294 = 0;
    const auto empty_reset = runtime::apply_wa_update_frame_step(no_94, 0u, empty_count);
    assert(empty_reset.reset_applied);
    assert(empty_reset.state.field_18c == -1);
    assert(empty_reset.state.current_frame_294 == 0);

    return 0;
}
