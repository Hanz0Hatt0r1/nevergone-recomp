#include "meplayer_live_action_state.h"

#include <mutex>
#include <sstream>

namespace nevergone::meplayer_live_action_state {
namespace {

std::mutex g_mutex;
Snapshot g_snapshot{};
std::uint64_t g_generation = 0;

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_generation;
    g_snapshot = {};
    g_snapshot.generation = g_generation;
}

bool sync_spawn(std::uint64_t spawn_generation, std::uint64_t tutorial_event_count) {
    if (spawn_generation == 0u) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_snapshot.ready && g_snapshot.spawn_generation == spawn_generation) return true;

    ++g_generation;
    g_snapshot = {};
    g_snapshot.ready = true;
    g_snapshot.generation = g_generation;
    g_snapshot.spawn_generation = spawn_generation;
    g_snapshot.current_action_state = kInitialActionState;
    g_snapshot.desired_action_state = kInitialActionState;
    g_snapshot.desired_revision = 1u;
    g_snapshot.loaded_revision = 0u;
    // Establish a baseline so an event from an older route/spawn cannot trigger
    // a fresh player immediately after spawn synchronization.
    g_snapshot.observed_tutorial_event_count = tutorial_event_count;
    return true;
}

bool observe_tutorial_event_count(std::uint64_t tutorial_event_count) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready) return false;
    if (tutorial_event_count <= g_snapshot.observed_tutorial_event_count) return false;

    const std::uint64_t delta = tutorial_event_count - g_snapshot.observed_tutorial_event_count;
    g_snapshot.observed_tutorial_event_count = tutorial_event_count;

    // The semantic layer currently publishes one tutorial event only for the
    // evidence-backed C tap that completes Jump. Do not generalize later lesson
    // events here: once current/desired leave initial state, extra events are
    // recorded as rejected rather than inventing additional action transitions.
    if (g_snapshot.current_action_state != kInitialActionState ||
        g_snapshot.desired_action_state != kInitialActionState) {
        g_snapshot.rejected_tutorial_jump_count += delta;
        return false;
    }

    g_snapshot.desired_action_state = kTutorialJumpActionState;
    ++g_snapshot.desired_revision;
    ++g_snapshot.accepted_tutorial_jump_count;
    if (delta > 1u) g_snapshot.rejected_tutorial_jump_count += delta - 1u;
    return true;
}

bool commit_loaded_action(std::uint32_t action_state) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready || action_state != g_snapshot.desired_action_state) return false;
    g_snapshot.current_action_state = action_state;
    g_snapshot.loaded_revision = g_snapshot.desired_revision;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_snapshot;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "MEPlayer live action state\n";
    out << "ready: " << (state.ready ? "yes" : "no") << "\n";
    out << "generation: " << state.generation << "\n";
    out << "spawn generation: " << state.spawn_generation << "\n";
    out << "current action: 0x" << std::hex << state.current_action_state << std::dec << "\n";
    out << "desired action: 0x" << std::hex << state.desired_action_state << std::dec << "\n";
    out << "desired revision: " << state.desired_revision << "\n";
    out << "loaded revision: " << state.loaded_revision << "\n";
    out << "tutorial input events observed: " << state.observed_tutorial_event_count << "\n";
    out << "accepted tutorial jumps: " << state.accepted_tutorial_jump_count << "\n";
    out << "rejected tutorial jumps: " << state.rejected_tutorial_jump_count << "\n";
    return out.str();
}

}  // namespace nevergone::meplayer_live_action_state
