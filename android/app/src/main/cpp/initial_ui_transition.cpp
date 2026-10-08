#include "initial_ui_transition.h"

#include <mutex>
#include <sstream>

namespace nevergone::initial_ui_transition {
namespace {

std::mutex g_mutex;
Snapshot g_state;

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

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "initial UI transition\n";
    out << "phase: " << phase_name(state.phase) << "\n";
    out << "scene generation: " << state.scene_generation << "\n";
    out << "initial-ui-ready observed: " << (state.initial_ui_ready_seen ? "yes" : "no") << "\n";
    out << "transition count: " << state.transition_count << "\n";
    return out.str();
}

}  // namespace nevergone::initial_ui_transition
