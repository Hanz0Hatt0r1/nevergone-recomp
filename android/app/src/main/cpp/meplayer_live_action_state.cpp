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
        std::uint64_t tutorial_jump_request_count,
        std::uint32_t player_index) {
    if (spawn_generation == 0u) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_snapshot.ready && g_snapshot.spawn_generation == spawn_generation &&
        g_snapshot.player_index == player_index) return true;

    ++g_generation;
    g_snapshot = {};
    g_snapshot.ready = true;
    g_snapshot.generation = g_generation;
    g_snapshot.spawn_generation = spawn_generation;
    g_snapshot.player_index = player_index;
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

bool observe_movement_action_state(
        std::uint32_t movement_action_state,
        bool moving) {
    if (moving && movement_action_state != kWalkActionState &&
            movement_action_state != kRunActionState) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready) return false;
    const auto is_locomotion_or_idle = [](std::uint32_t state) {
        return state == kInitialActionState || state == kWalkActionState ||
            state == kRunActionState;
    };
    const bool completed_movement_attack = g_snapshot.current_action_done &&
        (g_snapshot.current_action_state == kWalkAttackActionState ||
         g_snapshot.current_action_state == kRunAttackActionState);
    const bool completed_attack_is_current_request = completed_movement_attack &&
        g_snapshot.desired_action_state == g_snapshot.current_action_state;
    if ((!is_locomotion_or_idle(g_snapshot.current_action_state) &&
         !completed_movement_attack) ||
            (!is_locomotion_or_idle(g_snapshot.desired_action_state) &&
             !completed_attack_is_current_request)) {
        return false;
    }

    const std::uint32_t target = moving
        ? movement_action_state : kInitialActionState;
    if (g_snapshot.desired_action_state == target) return true;
    return queue_desired_action_locked(target);
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
    const bool function_state_changed =
        function_state != g_snapshot.observed_function_state;
    if (function_state == 0u) {
        if (function_state_changed) g_snapshot.observed_function_state = 0u;
        if (g_snapshot.player_index == 1u &&
            g_snapshot.current_action_state == kDirectionalACombo1ActionState &&
            g_snapshot.desired_action_state == kDirectionalACombo1ActionState &&
            g_snapshot.current_action_done &&
            g_snapshot.loaded_revision == g_snapshot.desired_revision) {
            return queue_desired_action_locked(kInitialActionState);
        }
        if (!function_state_changed) return false;
        if (g_snapshot.desired_action_state == kHeldBFollowupActionState &&
            (g_snapshot.current_action_state == kBaselineBActionState ||
             g_snapshot.current_action_state == kHeldBFollowupActionState)) {
            return queue_desired_action_locked(kInitialActionState);
        }
        return false;
    }
    if (!function_state_changed) return false;

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
    const bool base_a_action_matches_current =
        (pending_meplayer_state == kBaseAActionState &&
         (g_snapshot.current_action_state == kInitialActionState ||
          g_snapshot.current_action_state == 0x1cu)) ||
        (pending_meplayer_state == 0x10u &&
         g_snapshot.current_action_state == kWalkActionState) ||
        (pending_meplayer_state == 0x14u &&
         g_snapshot.current_action_state == kRunActionState);
    if (function_state == 7u && base_a_action_matches_current &&
        !pending_has_additional_guard) {
        accepted_action = pending_meplayer_state;
    } else if (function_state == 8u &&
               pending_meplayer_state == kBaselineBActionState &&
               pending_has_additional_guard &&
               g_snapshot.current_action_state == kInitialActionState &&
               g_snapshot.desired_action_state == kInitialActionState) {
        accepted_action = kBaselineBActionState;
    } else if (function_state == 0x15u &&
               pending_meplayer_state == 0x0fu &&
               !pending_has_additional_guard &&
               g_snapshot.player_index == 1u &&
               g_snapshot.current_action_state == kInitialActionState &&
               g_snapshot.desired_action_state == kInitialActionState) {
        accepted_action = 0x0fu;
    } else if (function_state == 0x16u &&
               pending_meplayer_state == kDirectionalACombo1ActionState &&
               !pending_has_additional_guard &&
               g_snapshot.player_index == 1u &&
               g_snapshot.current_action_state == kInitialActionState &&
               g_snapshot.desired_action_state == kInitialActionState) {
        accepted_action = kDirectionalACombo1ActionState;
    } else if (function_state == 0x14u &&
               pending_meplayer_state == kDirectionalAComboActionState &&
               !pending_has_additional_guard &&
               g_snapshot.player_index == 1u &&
               g_snapshot.current_action_state == kInitialActionState &&
               g_snapshot.desired_action_state == kInitialActionState) {
        accepted_action = 0x0cu;
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

    if (!g_snapshot.ready || !action_done || g_snapshot.current_action_done ||
        action_generation == 0u ||
        action_generation != g_snapshot.generation ||
        loaded_revision == 0u || loaded_revision != g_snapshot.loaded_revision ||
        completed_action_state != g_snapshot.current_action_state ||
        g_snapshot.current_action_state != g_snapshot.desired_action_state ||
        g_snapshot.loaded_revision != g_snapshot.desired_revision) {
        ++g_snapshot.rejected_completion_transition_count;
        return false;
    }

    std::uint32_t target_state = 0u;
    if (completed_action_state == kWalkAttackActionState ||
        completed_action_state == kRunAttackActionState) {
        g_snapshot.current_action_done = true;
        g_snapshot.last_completed_action_state = completed_action_state;
        g_snapshot.last_completion_target_state = completed_action_state;
        return true;
    } else if (completed_action_state == kDirectionalACombo1ActionState &&
               g_snapshot.player_index == 1u) {
        g_snapshot.current_action_done = true;
        g_snapshot.last_completed_action_state = completed_action_state;
        g_snapshot.last_completion_target_state = completed_action_state;
        return true;
    } else if (completed_action_state == kBaseAActionState ||
        completed_action_state == kTutorialJumpActionState) {
        target_state = kInitialActionState;
    } else if (completed_action_state == kBaselineBActionState) {
        target_state = current_function_state == 8u
            ? kHeldBFollowupActionState
            : kInitialActionState;
    } else if (completed_action_state == 0x0fu &&
               g_snapshot.player_index == 1u) {
        target_state = kInitialActionState;
    } else if (completed_action_state == kDirectionalAComboActionState &&
               g_snapshot.player_index == 1u) {
        target_state = 0x0du;
    } else if (completed_action_state == 0x0du &&
               g_snapshot.player_index == 1u) {
        target_state = 0x0eu;
    } else if (completed_action_state == 0x0eu &&
               g_snapshot.player_index == 1u) {
        target_state = kInitialActionState;
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

bool commit_loaded_action(std::uint32_t action_state, bool looping) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.ready || action_state != g_snapshot.desired_action_state) return false;
    g_snapshot.current_action_state = action_state;
    g_snapshot.current_action_looping = looping;
    g_snapshot.current_action_done = false;
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
    out << "player index: " << state.player_index << "\n";
    out << "current action: 0x" << std::hex << state.current_action_state << std::dec << "\n";
    out << "current action looping: " << (state.current_action_looping ? "yes" : "no") << "\n";
    out << "current action done: " << (state.current_action_done ? "yes" : "no") << "\n";
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
