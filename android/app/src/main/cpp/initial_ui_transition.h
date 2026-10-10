#pragma once

#include <cstdint>
#include <string>

namespace nevergone::initial_ui_transition {

enum class Phase {
    kWaitingForAppDelegate,
    kHelloWorldCreateUi,
    kManagementLoginInitialized,
};

enum class ManagementRoute {
    kInactive,
    kLoginRoot,
    kAnnouncement,
    kServerSelection,
    kAwaitingRoleList,
    kRoleSelection,
    kRoleCreated,
    kEnteringGame,
};

struct Snapshot {
    Phase phase = Phase::kWaitingForAppDelegate;
    ManagementRoute management_route = ManagementRoute::kInactive;
    ManagementRoute pending_management_route = ManagementRoute::kInactive;
    std::uint64_t scene_generation = 0;
    std::uint64_t transition_count = 0;
    std::uint64_t management_route_transition_count = 0;
    bool initial_ui_ready_seen = false;
};

void reset(std::uint64_t scene_generation);
void sync(bool initial_ui_ready, std::uint64_t scene_generation);
void on_management_callback(const std::string& callback_name);
bool on_server_enter_dispatch_succeeded();
Snapshot snapshot();
const char* phase_name(Phase phase);
const char* management_route_name(ManagementRoute route);
std::string status_report();

}  // namespace nevergone::initial_ui_transition
