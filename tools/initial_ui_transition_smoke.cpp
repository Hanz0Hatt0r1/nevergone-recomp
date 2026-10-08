#include <cassert>
#include <string>

#include "initial_ui_transition.h"

int main() {
    using nevergone::initial_ui_transition::ManagementRoute;
    using nevergone::initial_ui_transition::Phase;

    nevergone::initial_ui_transition::reset(4);
    auto state = nevergone::initial_ui_transition::snapshot();
    assert(state.phase == Phase::kWaitingForAppDelegate);
    assert(state.management_route == ManagementRoute::kInactive);
    assert(state.scene_generation == 4);
    assert(state.transition_count == 0);

    // A callback may arrive before HelloWorld/ManagementLayer is ready. It
    // must be retained as pending, not exposed as an active UI route yet.
    nevergone::initial_ui_transition::on_management_callback("cpp_OnGetServerList");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kInactive);
    assert(state.pending_management_route == ManagementRoute::kServerSelection);

    nevergone::initial_ui_transition::sync(false, 4);
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.phase == Phase::kWaitingForAppDelegate);

    nevergone::initial_ui_transition::sync(true, 4);
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.phase == Phase::kManagementLoginInitialized);
    assert(state.initial_ui_ready_seen);
    assert(state.transition_count == 2);
    assert(state.management_route == ManagementRoute::kServerSelection);
    assert(state.management_route_transition_count == 1);

    // Relevant callbacks drive the reconstructed ManagementLayer route in
    // arrival order once the verified init boundary has been reached.
    nevergone::initial_ui_transition::on_management_callback("cpp_OnGetRoleList");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kRoleSelection);

    nevergone::initial_ui_transition::on_management_callback("cpp_OnCreateTheRole");
    nevergone::initial_ui_transition::on_management_callback("cpp_OnEnterGame");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kEnteringGame);
    assert(state.management_route_transition_count == 4);

    // Non-routing callbacks remain diagnostic and must not disturb login UI.
    nevergone::initial_ui_transition::on_management_callback("cpp_OnUpdateData");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kEnteringGame);

    nevergone::initial_ui_transition::sync(true, 4);
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.transition_count == 2);

    // A new scene generation revokes the old UI route and pending callbacks.
    nevergone::initial_ui_transition::sync(false, 5);
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.phase == Phase::kWaitingForAppDelegate);
    assert(state.management_route == ManagementRoute::kInactive);
    assert(state.pending_management_route == ManagementRoute::kInactive);
    assert(state.scene_generation == 5);
    assert(!state.initial_ui_ready_seen);

    const std::string report = nevergone::initial_ui_transition::status_report();
    assert(report.find("waiting-for-appdelegate") != std::string::npos);
    assert(report.find("management route: inactive") != std::string::npos);
    return 0;
}
