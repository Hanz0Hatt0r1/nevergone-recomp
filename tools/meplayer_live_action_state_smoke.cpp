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

    // Loading/committing the initial action catches the document state up with
    // the desired state without inventing a gameplay transition.
    assert(live::commit_loaded_action(0x1bu));
    state = live::snapshot();
    assert(state.current_action_state == 0x1bu);
    assert(state.loaded_revision == state.desired_revision);

    // The one semantic tutorial event currently emitted by action input is the
    // C tap that completes Jump. It requests the proven 0x1b -> 0x20 transition.
    assert(live::observe_tutorial_event_count(4u));
    state = live::snapshot();
    assert(state.current_action_state == 0x1bu);
    assert(state.desired_action_state == 0x20u);
    assert(state.desired_revision == 2u);
    assert(state.loaded_revision == 1u);
    assert(state.accepted_tutorial_jump_count == 1u);

    // The action state is not committed until the matching imported action33
    // document has actually been resolved and parsed.
    assert(!live::commit_loaded_action(0x02u));
    assert(live::snapshot().current_action_state == 0x1bu);
    assert(live::commit_loaded_action(0x20u));
    state = live::snapshot();
    assert(state.current_action_state == 0x20u);
    assert(state.loaded_revision == state.desired_revision);

    // Re-reading the same semantic count is idempotent. Later events do not
    // fabricate another transition after the player has left the initial state.
    assert(!live::observe_tutorial_event_count(4u));
    assert(!live::observe_tutorial_event_count(5u));
    state = live::snapshot();
    assert(state.desired_action_state == 0x20u);
    assert(state.rejected_tutorial_jump_count == 1u);

    // A new spawn resets to initial action state and takes the latest event count
    // as its baseline so an old Jump tap cannot replay on the new player.
    assert(live::sync_spawn(8u, 5u));
    state = live::snapshot();
    assert(state.spawn_generation == 8u);
    assert(state.current_action_state == 0x1bu);
    assert(state.desired_action_state == 0x1bu);
    assert(state.observed_tutorial_event_count == 5u);
    assert(state.accepted_tutorial_jump_count == 0u);

    const std::string report = live::status_report();
    assert(report.find("current action: 0x1b") != std::string::npos);
    assert(report.find("desired action: 0x1b") != std::string::npos);
    return 0;
}
