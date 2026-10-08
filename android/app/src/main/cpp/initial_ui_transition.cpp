#include "initial_ui_transition.h"

#include <mutex>
#include <sstream>

namespace nevergone::initial_ui_transition {
namespace {

std::mutex g_mutex;
Snapshot g_state;

ManagementRoute route_for_callback(const std::string& name) {
    if (name == "cpp_OnGameAnnoucement") return ManagementRoute::kAnnouncement;
    if (name == "cpp_OnGetServerList") return ManagementRoute::kServerSelection;
    if (name == "cpp_OnGetRoleList") return ManagementRoute::kRoleSelection;
    if (name == "cpp_OnCreateTheRole") return ManagementRoute::kRoleCreated;
    if (name == "cpp_OnEnterGame") return ManagementRoute::kEnteringGame;
    return ManagementRoute::kInactive;
}

void set_management_route_locked(ManagementRoute route) {
    if (route == ManagementRoute::kInactive || g_state.management_route == route) return;
    g_state.management_route = route;
    ++g_state.management_route_transition_count;
}

}  // namespace

void reset(std::uint64_t scene_generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = Snapshot{};
    g_state.scene_generation = scene_generation;
}

void sync(bool initial_ui_ready, std::uint64_t scene_generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (scene_generation != g_state.scene_generation) {
        g_state = Snapshot{};
        g_state.scene_generation = scene_generation;
    }
    if (!initial_ui_ready || g_state.initial_ui_ready_seen) return;

    g_state.initial_ui_ready_seen = true;
    g_state.phase = Phase::kHelloWorldCreateUi;
    ++g_state.transition_count;

    // The recovered native flow calls ManagementLayer::initLoginLayer()
    // synchronously from HelloWorld::createUI(). Model that verified semantic
    // boundary without pretending to recreate either original C++ class ABI.
    g_state.phase = Phase::kManagementLoginInitialized;
    ++g_state.transition_count;

    if (g_state.pending_management_route != ManagementRoute::kInactive) {
        set_management_route_locked(g_state.pending_management_route);
    } else {
        set_management_route_locked(ManagementRoute::kLoginRoot);
    }
}

void on_management_callback(const std::string& callback_name) {
    const ManagementRoute route = route_for_callback(callback_name);
    if (route == ManagementRoute::kInactive) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.pending_management_route = route;
    if (g_state.phase == Phase::kManagementLoginInitialized) {
        set_management_route_locked(route);
    }
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* phase_name(Phase phase) {
    switch (phase) {
        case Phase::kWaitingForAppDelegate: return "waiting-for-appdelegate";
        case Phase::kHelloWorldCreateUi: return "hello-world-create-ui";
        case Phase::kManagementLoginInitialized: return "management-login-initialized";
    }
    return "unknown";
}

const char* management_route_name(ManagementRoute route) {
    switch (route) {
        case ManagementRoute::kInactive: return "inactive";
        case ManagementRoute::kLoginRoot: return "login-root";
        case ManagementRoute::kAnnouncement: return "announcement";
        case ManagementRoute::kServerSelection: return "server-selection";
        case ManagementRoute::kRoleSelection: return "role-selection";
        case ManagementRoute::kRoleCreated: return "role-created";
        case ManagementRoute::kEnteringGame: return "entering-game";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "initial UI transition\n";
    out << "phase: " << phase_name(state.phase) << "\n";
    out << "scene generation: " << state.scene_generation << "\n";
    out << "initial-ui-ready observed: " << (state.initial_ui_ready_seen ? "yes" : "no") << "\n";
    out << "transition count: " << state.transition_count << "\n";
    out << "management route: " << management_route_name(state.management_route) << "\n";
    out << "pending management route: " << management_route_name(state.pending_management_route) << "\n";
    out << "management route transitions: " << state.management_route_transition_count << "\n";
    return out.str();
}

}  // namespace nevergone::initial_ui_transition
