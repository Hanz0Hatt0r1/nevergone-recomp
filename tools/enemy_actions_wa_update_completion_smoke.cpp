#include <cassert>
#include <cstdint>

#include "enemy_actions_combo_consumer.h"

int main() {
    using nevergone::enemy_actions_combo_consumer::WaUpdateCompletionState;
    using nevergone::enemy_actions_combo_consumer::apply_wa_update_completion_transition;

    WaUpdateCompletionState state;
    state.field_18c = 77;
    state.flag_190 = 9u;
    state.flag_169 = 5u;
    state.flag_1bc = 6u;
    state.flag_292 = 0u;
    state.current_frame_294 = 123;

    // A real +0x94 transition suppresses this later reset block.
    const auto advanced = apply_wa_update_completion_transition(10, true, 99, state);
    assert(!advanced.reset_applied);
    assert(advanced.state.field_18c == 77);
    assert(advanced.state.flag_190 == 9u);
    assert(advanced.state.flag_169 == 5u);
    assert(advanced.state.flag_1bc == 6u);
    assert(advanced.state.current_frame_294 == 123);

    // The native comparison is strict: frame == endpoint is still a no-op.
    const auto equal = apply_wa_update_completion_transition(10, false, 10, state);
    assert(!equal.reset_applied);
    const auto before = apply_wa_update_completion_transition(10, false, 9, state);
    assert(!before.reset_applied);

    // First post-endpoint frame applies the complete observed write set.
    const auto reset = apply_wa_update_completion_transition(10, false, 11, state);
    assert(reset.reset_applied);
    assert(reset.state.field_18c == 9);
    assert(reset.state.flag_190 == 1u);
    assert(reset.state.current_frame_294 == 0);
    assert(reset.state.flag_1bc == 1u);
    assert(reset.state.flag_292 == 0u);
    assert(reset.state.flag_169 == 0u);

    // Nonzero +0x292 preserves +0x169 while the other writes still occur.
    WaUpdateCompletionState gated_169 = state;
    gated_169.flag_292 = 3u;
    gated_169.flag_169 = 8u;
    const auto preserve_169 = apply_wa_update_completion_transition(
            20, false, 21, gated_169);
    assert(preserve_169.reset_applied);
    assert(preserve_169.state.field_18c == 19);
    assert(preserve_169.state.flag_190 == 1u);
    assert(preserve_169.state.current_frame_294 == 0);
    assert(preserve_169.state.flag_1bc == 1u);
    assert(preserve_169.state.flag_292 == 3u);
    assert(preserve_169.state.flag_169 == 8u);

    // Preserve ARM32 subtraction behavior at the signed minimum endpoint.
    const auto wrapped = apply_wa_update_completion_transition(
            static_cast<std::int32_t>(0x80000000u), false,
            static_cast<std::int32_t>(0x80000001u), WaUpdateCompletionState{});
    // Signed comparison makes 0x80000001 (-2147483647) greater than INT32_MIN.
    assert(wrapped.reset_applied);
    assert(static_cast<std::uint32_t>(wrapped.state.field_18c) == 0x7fffffffu);

    return 0;
}
