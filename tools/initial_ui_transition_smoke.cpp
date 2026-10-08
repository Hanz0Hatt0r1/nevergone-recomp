#include <cassert>
#include <string>

#include "initial_ui_transition.h"

int main() {
    using nevergone::initial_ui_transition::Phase;

    nevergone::initial_ui_transition::reset(4);
    auto state = nevergone::initial_ui_transition::snapshot();
    assert(state.phase == Phase::kWaitingForAppDelegate);
    assert(state.scene_generation == 4);
    assert(state.transition_count == 0);

    nevergone::initial_ui_transition::sync(false, 4);
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.phase == Phase::kWaitingForAppDelegate);

    nevergone::initial_ui_transition::sync(true, 4);
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.phase == Phase::kManagementLoginInitialized);
    assert(state.initial_ui_ready_seen);
    assert(state.transition_count == 2);

    nevergone::initial_ui_transition::sync(true, 4);
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.transition_count == 2);

    nevergone::initial_ui_transition::sync(false, 5);
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.phase == Phase::kWaitingForAppDelegate);
    assert(state.scene_generation == 5);
    assert(!state.initial_ui_ready_seen);

    const std::string report = nevergone::initial_ui_transition::status_report();
    assert(report.find("waiting-for-appdelegate") != std::string::npos);
    return 0;
}
