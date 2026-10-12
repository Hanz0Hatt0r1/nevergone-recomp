#include <cassert>
#include <string>

#include "meplayer_live_action_state.h"

namespace live = nevergone::meplayer_live_action_state;

int main() {
    live::reset();
    auto state = live::snapshot();
    assert(!state.ready);
    assert(!live::sync_spawn(0u, 0u));

    // Baseline B path.
    assert(live::sync_spawn(7u, 3u));
    state = live::snapshot();
    assert(state.ready);
    assert(state.current_action_state == live::kInitialActionState);
    assert(state.desired_revision == 1u);
    assert(live::commit_loaded_action(live::kInitialActionState));

    assert(live::observe_base_function_state(
        1u, 8u, true, live::kBaselineBActionState, true));
    assert(live::commit_loaded_action(live::kBaselineBActionState));
    state = live::snapshot();
    const std::uint64_t b_revision = state.loaded_revision;

    // MEPlayer::actionState(): completed 0x1d while function state 8 is still
    // held transitions to 0x1e rather than idle 0x1b.
    assert(live::observe_action_completion(
        state.generation,
        b_revision,
        live::kBaselineBActionState,
        true,
        8u));
    state = live::snapshot();
    assert(state.desired_action_state == live::kHeldBFollowupActionState);
    assert(state.accepted_completion_transition_count == 1u);
    assert(state.last_completed_action_state == live::kBaselineBActionState);
    assert(state.last_completion_target_state == live::kHeldBFollowupActionState);

    // Re-observing the same done action cannot queue a second transition because
    // current and desired no longer identify the same loaded action.
    const std::uint64_t held_b_revision = state.desired_revision;
    assert(!live::observe_action_completion(
        state.generation,
        b_revision,
        live::kBaselineBActionState,
        true,
        8u));
    assert(live::snapshot().desired_revision == held_b_revision);

    // A released B completion follows the other actionState branch to 0x1b.
    assert(live::sync_spawn(8u, 3u));
    assert(live::commit_loaded_action(live::kInitialActionState));
    assert(live::observe_base_function_state(
        10u, 8u, true, live::kBaselineBActionState, true));
    assert(live::commit_loaded_action(live::kBaselineBActionState));
    state = live::snapshot();
    assert(live::observe_action_completion(
        state.generation,
        state.loaded_revision,
        live::kBaselineBActionState,
        true,
        0u));
    assert(live::snapshot().desired_action_state == live::kInitialActionState);

    // Tutorial C/jump: current 0x20 follows the generic done path to 0x1b.
    assert(live::sync_spawn(9u, 3u));
    assert(live::commit_loaded_action(live::kInitialActionState));
    assert(live::observe_tutorial_event_count(4u));
    assert(live::commit_loaded_action(live::kTutorialJumpActionState));
    state = live::snapshot();
    assert(live::observe_action_completion(
        state.generation,
        state.loaded_revision,
        live::kTutorialJumpActionState,
        true,
        0u));
    state = live::snapshot();
    assert(state.desired_action_state == live::kInitialActionState);
    assert(state.last_completion_target_state == live::kInitialActionState);

    // Base A: 0x02/0x03 share the native actionState branch; current supported
    // 0x02 therefore returns to 0x1b on completion.
    assert(live::sync_spawn(10u, 4u));
    assert(live::commit_loaded_action(live::kInitialActionState));
    assert(live::observe_base_function_state(
        20u, 7u, true, live::kBaseAActionState, false));
    assert(live::commit_loaded_action(live::kBaseAActionState));
    state = live::snapshot();
    assert(live::observe_action_completion(
        state.generation,
        state.loaded_revision,
        live::kBaseAActionState,
        true,
        0u));
    state = live::snapshot();
    assert(state.desired_action_state == live::kInitialActionState);
    assert(state.accepted_completion_transition_count == 1u);

    // Wrong generation/revision, not-done callbacks and unsupported actions all
    // fail closed without moving desired state.
    const std::uint64_t desired_revision = state.desired_revision;
    assert(!live::observe_action_completion(
        state.generation + 1u,
        state.loaded_revision,
        live::kBaseAActionState,
        true,
        0u));
    assert(!live::observe_action_completion(
        state.generation,
        state.loaded_revision,
        live::kBaseAActionState,
        false,
        0u));
    assert(live::snapshot().desired_revision == desired_revision);

    const std::string report = live::status_report();
    assert(report.find("accepted completion transitions: 1") != std::string::npos);
    assert(report.find("last completion target: 0x1b") != std::string::npos);
    return 0;
}
