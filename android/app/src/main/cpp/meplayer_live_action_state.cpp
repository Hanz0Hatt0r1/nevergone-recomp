#include "meplayer_live_action_state.h"

#include <mutex>
#include <sstream>

namespace nevergone::meplayer_live_action_state {
namespace {

std::mutex g_mutex;
Snapshot g_snapshot{};
std::uint64_t g_generation = 0;

bool queue_desired_action_locked(std::uint32_t action_state) {
    if (g_snapshot.desired_action_state == action_state) return false;
    g_snapshot.desired_action_state = action_state;
    ++g_snapshot.desired_revision;
    return true;
}

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

    const bool queued = queue_desired_action_locked(kTutorialJumpActionState);
    if (queued) ++g_snapshot.accepted_tutorial_jump_count;
    if (!queued) ++g_snapshot.rejected_tutorial_jump_count;
    if (delta > 1u) g_snapshot.rejected_tutorial_jump_count += delta - 1u;
    return queued;
}

bool observe_base_function_state(
        std::uint64_t function_generation,
        std::uint32_t function_state,
        bool has_pending_base_action,
        std::uint32_t pending_meplayer_state,
        bool pending_has_additional_guard) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready || function_generation == 0u ||
        function_generation <= g_snapshot.observed_function_generation) {
        return false;
    }

    g_snapshot.observed_function_generation = function_generation;

    // Controls publishes a new runtime generation every frame. Only function
    // state edges correspond to a fresh setFunctionState request for this live
    // reconstruction; holding a button must not continuously reload its action.
    if (function_state == g_snapshot.observed_function_state) return false;

    // Release/neutral is always safe to observe, but the original
    // setFunctionState(0) does not unconditionally return to state 0x1b. That
    // depends on action completion/isActionDone(), which is not reconstructed.
    if (function_state == 0u) {
        g_snapshot.observed_function_state = 0u;
        return false;
    }

    // Do not allow a later function edge to replace an action document that has
    // not yet been resolved and committed. Keep the previous observed function
    // state so a held button is reconsidered on the next frame after commit.
    if (g_snapshot.current_action_state != g_snapshot.desired_action_state ||
        g_snapshot.loaded_revision != g_snapshot.desired_revision) {
        return false;
    }

    g_snapshot.observed_function_state = function_state;

    if (!has_pending_base_action) {
        ++g_snapshot.rejected_function_action_count;
        return false;
    }

    std::uint32_t accepted_action = 0u;
    if (function_state == 7u && pending_meplayer_state == kBaseAActionState &&
        !pending_has_additional_guard) {
        accepted_action = kBaseAActionState;
    } else if (function_state == 8u &&
               pending_meplayer_state == kBaselineBActionState &&
               pending_has_additional_guard &&
               g_snapshot.current_action_state == kInitialActionState &&
               g_snapshot.desired_action_state == kInitialActionState) {
        // B is accepted only for the exact currently reconstructed baseline.
        // Other B branches need the live +0x1f0/+0x1f8 guard fields.
        accepted_action = kBaselineBActionState;
    } else {
        // C remains driven by the existing tutorial-event boundary for now, and
        // guarded/unsupported function branches fail closed here.
        ++g_snapshot.rejected_function_action_count;
        return false;
    }

    if (!queue_desired_action_locked(accepted_action)) return false;
    ++g_snapshot.accepted_function_action_count;
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
    out << "observed function generation: " << state.observed_function_generation << "\n";
    out << "observed function state: 0x" << std::hex << state.observed_function_state
        << std::dec << "\n";
    out << "accepted function actions: " << state.accepted_function_action_count << "\n";
    out << "rejected function actions: " << state.rejected_function_action_count << "\n";
    return out.str();
}

}  // namespace nevergone::meplayer_live_action_state
