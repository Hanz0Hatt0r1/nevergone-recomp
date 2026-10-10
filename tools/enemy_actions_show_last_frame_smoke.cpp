#include <cassert>
#include <cstdint>
#include <limits>

#include "enemy_actions_runtime_state.h"

int main() {
    namespace runtime = nevergone::enemy_actions_runtime_state;

    runtime::State state;
    state.previous_frame_154 = 7;
    state.field_18c = 42;
    state.current_frame_294 = 3;
    state.flag_169 = 9u;
    state.flag_190 = 5u;
    state.flag_1bc = 4u;

    const auto result = runtime::apply_show_action_last_frame(state);
    assert(result.state.current_frame_294 == 42);
    assert(result.should_call_update_data);

    // showActionLastFrame only writes +0x294 before calling updateData(). It
    // does not perform the waUpdate +0x154 bookkeeping first.
    assert(result.state.previous_frame_154 == 7);
    assert(result.state.field_18c == 42);
    assert(result.state.flag_169 == 9u);
    assert(result.state.flag_190 == 5u);
    assert(result.state.flag_1bc == 4u);

    // The copy is bit-exact for signed ARM32 frame values.
    runtime::State negative;
    negative.field_18c = -1;
    negative.current_frame_294 = 123;
    const auto negative_result = runtime::apply_show_action_last_frame(negative);
    assert(negative_result.state.current_frame_294 == -1);
    assert(negative_result.should_call_update_data);

    runtime::State minimum;
    minimum.field_18c = std::numeric_limits<std::int32_t>::min();
    const auto minimum_result = runtime::apply_show_action_last_frame(minimum);
    assert(minimum_result.state.current_frame_294 ==
           std::numeric_limits<std::int32_t>::min());
    assert(minimum_result.should_call_update_data);

    return 0;
}
