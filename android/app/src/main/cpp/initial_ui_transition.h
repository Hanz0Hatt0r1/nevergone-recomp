#pragma once

#include <cstdint>
#include <string>

namespace nevergone::initial_ui_transition {

enum class Phase {
    kWaitingForAppDelegate,
    kHelloWorldCreateUi,
    kManagementLoginInitialized,
};

struct Snapshot {
    Phase phase = Phase::kWaitingForAppDelegate;
    std::uint64_t scene_generation = 0;
    std::uint64_t transition_count = 0;
    bool initial_ui_ready_seen = false;
};

void reset(std::uint64_t scene_generation);
void sync(bool initial_ui_ready, std::uint64_t scene_generation);
Snapshot snapshot();
const char* phase_name(Phase phase);
std::string status_report();

}  // namespace nevergone::initial_ui_transition
