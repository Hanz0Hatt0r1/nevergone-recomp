#include <cassert>
#include <string>

#include "meplayer_live_action_state.h"

namespace live = nevergone::meplayer_live_action_state;

int main() {
    live::reset();
    auto state = live::snapshot();
    assert(!state.ready);
    assert(!live::sync_spawn(0u, 0u));

    // A fresh player spawn starts at the already-recovered initial state 0x1b
    // and establishes an event-count baseline.
    assert(live::sync_spawn(7u, 3u));
    state = live::snapshot();
    assert(state.ready);
    assert(state.spawn_generation == 7u);
    assert(state.current_action_state == 0x1bu);
    assert(state.desired_action_state == 0x1bu);
    assert(state.desired_revision == 1u);
    assert(state.loaded_revision == 0u);
    assert(state.observed_tutorial_event_count == 3u);

    assert(live::commit_loaded_action(0x1bu));

    // B is live only for the exact baseline currently proven by the native
    // setFunctionState branch: initial state 0x1b -> 0x1d/action30.
    assert(live::observe_base_function_state(1u, 8u, true, 0x1du, true));
    state = live::snapshot();
    assert(state.desired_action_state == 0x1du);
    assert(state.desired_revision == 2u);
    assert(state.accepted_function_action_count == 1u);
    assert(live::commit_loaded_action(0x1du));

    // Holding B produces fresh runtime samples but no new function-state edge.
    assert(!live::observe_base_function_state(2u, 8u, true, 0x1du, true));
    assert(live::snapshot().desired_revision == 2u);

    // Release is observed but does not fabricate the original isActionDone()
    // return-to-idle behavior.
    assert(!live::observe_base_function_state(3u, 0u, false, 0u, false));
    state = live::snapshot();
    assert(state.current_action_state == 0x1du);
    assert(state.desired_action_state == 0x1du);

    // B is deliberately rejected outside the recovered initial baseline until
    // the +0x1f0/+0x1f8 guarded live fields are reconstructed.
    assert(!live::observe_base_function_state(4u, 8u, true, 0x1du, true));
    assert(live::snapshot().rejected_function_action_count == 1u);

    // New spawn: preserve the existing tutorial C/jump path.
    assert(live::sync_spawn(8u, 3u));
    assert(live::commit_loaded_action(0x1bu));
    assert(live::observe_tutorial_event_count(4u));
    state = live::snapshot();
    assert(state.desired_action_state == 0x20u);
    assert(state.accepted_tutorial_jump_count == 1u);

    // A pressed while action33 is still pending must not replace that document.
    // The edge remains reconsiderable because observed_function_state is not
    // advanced until the previous desired revision has committed.
    assert(!live::observe_base_function_state(9u, 7u, true, 0x02u, false));
    state = live::snapshot();
    assert(state.desired_action_state == 0x20u);
    assert(state.observed_function_state == 0u);

    assert(live::commit_loaded_action(0x20u));

    // Base A is a proven direct function branch and can replace the loaded jump
    // action without relying on the still-unmodeled B guard fields.
    assert(live::observe_base_function_state(10u, 7u, true, 0x02u, false));
    state = live::snapshot();
    assert(state.current_action_state == 0x20u);
    assert(state.desired_action_state == 0x02u);
    assert(state.accepted_function_action_count == 1u);
    assert(live::commit_loaded_action(0x02u));

    // Held A is idempotent; releasing it does not force state 0x1b.
    assert(!live::observe_base_function_state(11u, 7u, true, 0x02u, false));
    assert(!live::observe_base_function_state(12u, 0u, false, 0u, false));
    state = live::snapshot();
    assert(state.current_action_state == 0x02u);
    assert(state.desired_action_state == 0x02u);

    // Unsupported combo/function state fails closed.
    assert(!live::observe_base_function_state(13u, 0x14u, false, 0u, false));
    assert(live::snapshot().rejected_function_action_count == 1u);

    const std::string report = live::status_report();
    assert(report.find("current action: 0x2") != std::string::npos);
    assert(report.find("accepted function actions: 1") != std::string::npos);
    return 0;
}
