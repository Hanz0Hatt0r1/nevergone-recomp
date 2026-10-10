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
    assert(!nevergone::initial_ui_transition::on_server_enter_dispatch_succeeded());
    assert(!nevergone::initial_ui_transition::on_role_enter_dispatch_succeeded());

    // A callback may arrive before HelloWorld/ManagementLayer is ready. It
    // must be retained as pending, not exposed as an active UI route yet.
    nevergone::initial_ui_transition::on_management_callback("cpp_OnGetServerList");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kInactive);
    assert(state.pending_management_route == ManagementRoute::kServerSelection);
    assert(!nevergone::initial_ui_transition::on_server_enter_dispatch_succeeded());

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

    // A successful EnterGameLogicServer dispatch advances the visible route
    // into an explicit wait state. It must not fabricate cpp_OnGetRoleList;
    // only the real callback may expose role-selection.
    assert(nevergone::initial_ui_transition::on_server_enter_dispatch_succeeded());
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kAwaitingRoleList);
    assert(state.pending_management_route == ManagementRoute::kAwaitingRoleList);
    assert(state.management_route_transition_count == 2);
    assert(!nevergone::initial_ui_transition::on_server_enter_dispatch_succeeded());
    assert(nevergone::initial_ui_transition::snapshot().management_route_transition_count == 2);

    // The real role-list callback owns the transition into role-selection.
    nevergone::initial_ui_transition::on_management_callback("cpp_OnGetRoleList");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kRoleSelection);
    assert(state.pending_management_route == ManagementRoute::kRoleSelection);
    assert(state.management_route_transition_count == 3);

    // EnterGameWithCid success is observable immediately but does not invent
    // cpp_OnEnterGame. Duplicate success notifications are rejected.
    assert(nevergone::initial_ui_transition::on_role_enter_dispatch_succeeded());
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kAwaitingEnterGame);
    assert(state.pending_management_route == ManagementRoute::kAwaitingEnterGame);
    assert(state.management_route_transition_count == 4);
    assert(!nevergone::initial_ui_transition::on_role_enter_dispatch_succeeded());
    assert(nevergone::initial_ui_transition::snapshot().management_route_transition_count == 4);

    nevergone::initial_ui_transition::on_management_callback("cpp_OnEnterGame");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kEnteringGame);
    assert(state.management_route_transition_count == 5);

    // Created-role auto-enter uses the same waiting boundary, but only after
    // cpp_OnCreateTheRole has made role-created the active route.
    nevergone::initial_ui_transition::reset(5);
    nevergone::initial_ui_transition::sync(true, 5);
    nevergone::initial_ui_transition::on_management_callback("cpp_OnCreateTheRole");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kRoleCreated);
    assert(nevergone::initial_ui_transition::on_role_enter_dispatch_succeeded());
    assert(nevergone::initial_ui_transition::snapshot().management_route ==
        ManagementRoute::kAwaitingEnterGame);
    nevergone::initial_ui_transition::on_management_callback("cpp_OnEnterGame");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kEnteringGame);

    // Non-routing callbacks remain diagnostic and must not disturb login UI.
    nevergone::initial_ui_transition::on_management_callback("cpp_OnUpdateData");
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.management_route == ManagementRoute::kEnteringGame);

    // A new scene generation revokes the old UI route and pending callbacks.
    nevergone::initial_ui_transition::sync(false, 6);
    state = nevergone::initial_ui_transition::snapshot();
    assert(state.phase == Phase::kWaitingForAppDelegate);
    assert(state.management_route == ManagementRoute::kInactive);
    assert(state.pending_management_route == ManagementRoute::kInactive);
    assert(state.scene_generation == 6);
    assert(!state.initial_ui_ready_seen);

    const std::string report = nevergone::initial_ui_transition::status_report();
    assert(report.find("waiting-for-appdelegate") != std::string::npos);
    assert(report.find("management route: inactive") != std::string::npos);
    return 0;
}
