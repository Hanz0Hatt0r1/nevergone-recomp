#include <array>
#include <cassert>

#include "controls_function_state_runtime.h"

namespace runtime = nevergone::controls_function_state_runtime;

int main() {
    const std::array<bool, 3> none{false, false, false};
    const std::array<bool, 3> a{true, false, false};
    const std::array<bool, 3> b{false, true, false};
    const std::array<bool, 3> c{false, false, true};
    const std::array<bool, 3> ac{true, false, true};

    runtime::reset();
    assert(runtime::advance_sample(false, c, 0u, 0u, 10u));
    auto state = runtime::snapshot();
    assert(state.function_state == 0u);
    assert(!state.has_pending_base_action);

    runtime::reset();
    assert(runtime::advance_sample(true, c, 0u, 0u, 10u));
    state = runtime::snapshot();
    assert(state.function_state == 9u);
    assert(state.has_pending_base_action);
    assert(state.pending_meplayer_state == 0x20u);
    assert(!state.pending_force);

    assert(runtime::advance_sample(true, none, 0u, 0u, 11u));
    state = runtime::snapshot();
    assert(state.function_state == 0u);
    assert(!state.has_pending_base_action);

    runtime::reset();
    assert(runtime::advance_sample(true, ac, 0u, 0u, 1u));
    state = runtime::snapshot();
    assert(state.function_state == 0x1cu);
    assert(!state.has_pending_base_action);

    // From initial action state 0x1b the native B baseline falls through to
    // action state 0x1d. B remains marked guarded because other live MEPlayer
    // histories can select the alternate 0x11/force=true path.
    runtime::reset();
    assert(runtime::advance_sample(true, b, 0u, 0u, 1u));
    state = runtime::snapshot();
    assert(state.function_state == 8u);
    assert(state.pending_meplayer_state == 0x1du);
    assert(!state.pending_force);
    assert(state.pending_has_additional_guard);

    // A + a fresh primary combo 10 inside the 0.4-second window synthesizes
    // function state 0x14 and consumes that exact combo event.
    runtime::reset();
    assert(runtime::advance_sample(true, a, 10u, 100u, 110u));
    state = runtime::snapshot();
    assert(state.function_state == 0x14u);
    assert(state.source_combo_consumed);
    assert(state.synthesized_direction_combo_state == 0u);
    assert(!state.has_pending_base_action);

    // The same retained source event stays consumed after the button releases.
    assert(runtime::advance_sample(true, none, 10u, 100u, 111u));
    state = runtime::snapshot();
    assert(state.function_state == 0u);
    assert(state.effective_direction_combo_state == 0u);
    assert(state.source_combo_consumed);

    // A new start tick is a new combo event and becomes visible again.
    assert(runtime::advance_sample(true, none, 10u, 112u, 112u));
    state = runtime::snapshot();
    assert(state.effective_direction_combo_state == 10u);
    assert(!state.source_combo_consumed);

    // The currently recovered horizontal joystick domain (14..19 family)
    // feeds the secondary timer; first A activation consumes it even when it
    // does not select a directional attack rewrite.
    runtime::reset();
    assert(runtime::advance_sample(true, a, 18u, 200u, 205u));
    state = runtime::snapshot();
    assert(state.function_state == 7u);
    assert(state.source_combo_consumed);
    assert(state.has_pending_base_action);
    assert(state.pending_meplayer_state == 0x02u);

    return 0;
}
