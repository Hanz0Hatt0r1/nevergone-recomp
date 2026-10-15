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

bool sync_spawn(
        std::uint64_t spawn_generation,
        std::uint64_t tutorial_jump_request_count) {
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
    g_snapshot.observed_tutorial_jump_request_count = tutorial_jump_request_count;
    return true;
}

bool observe_tutorial_jump_request_count(
        std::uint64_t tutorial_jump_request_count) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready) return false;
    if (tutorial_jump_request_count <=
            g_snapshot.observed_tutorial_jump_request_count) {
        return false;
    }

    const std::uint64_t delta = tutorial_jump_request_count -
        g_snapshot.observed_tutorial_jump_request_count;
    g_snapshot.observed_tutorial_jump_request_count = tutorial_jump_request_count;

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
    if (function_state == g_snapshot.observed_function_state) return false;

    if (function_state == 0u) {
        g_snapshot.observed_function_state = 0u;
        return false;
    }

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
        accepted_action = kBaselineBActionState;
    } else {
        ++g_snapshot.rejected_function_action_count;
        return false;
    }

    if (!queue_desired_action_locked(accepted_action)) return false;
    ++g_snapshot.accepted_function_action_count;
    return true;
}

bool observe_action_completion(
        std::uint64_t action_generation,
        std::uint64_t loaded_revision,
        std::uint32_t completed_action_state,
        bool action_done,
        std::uint32_t current_function_state) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_snapshot.ready || !action_done || action_generation == 0u ||
        action_generation != g_snapshot.generation ||
        loaded_revision == 0u || loaded_revision != g_snapshot.loaded_revision ||
        completed_action_state != g_snapshot.current_action_state ||
        g_snapshot.current_action_state != g_snapshot.desired_action_state ||
        g_snapshot.loaded_revision != g_snapshot.desired_revision) {
        ++g_snapshot.rejected_completion_transition_count;
        return false;
    }

    std::uint32_t target_state = 0u;
    if (completed_action_state == kBaseAActionState ||
        completed_action_state == kTutorialJumpActionState) {
        target_state = kInitialActionState;
    } else if (completed_action_state == kBaselineBActionState) {
        target_state = current_function_state == 8u
            ? kHeldBFollowupActionState
            : kInitialActionState;
    } else {
        ++g_snapshot.rejected_completion_transition_count;
        return false;
    }

    g_snapshot.last_completed_action_state = completed_action_state;
    g_snapshot.last_completion_target_state = target_state;
    if (!queue_desired_action_locked(target_state)) {
        ++g_snapshot.rejected_completion_transition_count;
        return false;
    }
    ++g_snapshot.accepted_completion_transition_count;
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
    out << "tutorial jump requests observed: "
        << state.observed_tutorial_jump_request_count << "\n";
    out << "accepted tutorial jumps: " << state.accepted_tutorial_jump_count << "\n";
    out << "rejected tutorial jumps: " << state.rejected_tutorial_jump_count << "\n";
    out << "observed function generation: " << state.observed_function_generation << "\n";
    out << "observed function state: 0x" << std::hex << state.observed_function_state
        << std::dec << "\n";
    out << "accepted function actions: " << state.accepted_function_action_count << "\n";
    out << "rejected function actions: " << state.rejected_function_action_count << "\n";
    out << "accepted completion transitions: "
        << state.accepted_completion_transition_count << "\n";
    out << "rejected completion transitions: "
        << state.rejected_completion_transition_count << "\n";
    out << "last completed action: 0x" << std::hex
        << state.last_completed_action_state << std::dec << "\n";
    out << "last completion target: 0x" << std::hex
        << state.last_completion_target_state << std::dec << "\n";
    return out.str();
}

}  // namespace nevergone::meplayer_live_action_state
