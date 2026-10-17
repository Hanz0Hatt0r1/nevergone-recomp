#pragma once

#include <cstdint>
#include <string>

namespace nevergone::meplayer_live_action_state {

constexpr std::uint32_t kInitialActionState = 0x1bu;
constexpr std::uint32_t kWalkActionState = 0x00u;
constexpr std::uint32_t kRunActionState = 0x01u;
constexpr std::uint32_t kTutorialJumpActionState = 0x20u;
constexpr std::uint32_t kBaseAActionState = 0x02u;
constexpr std::uint32_t kBaselineBActionState = 0x1du;
constexpr std::uint32_t kHeldBFollowupActionState = 0x1eu;

struct Snapshot {
    bool ready = false;
    std::uint64_t generation = 0;
    std::uint64_t spawn_generation = 0;
    std::uint32_t current_action_state = kInitialActionState;
    bool current_action_looping = false;
    std::uint32_t desired_action_state = kInitialActionState;
    std::uint64_t desired_revision = 0;
    std::uint64_t loaded_revision = 0;
    std::uint64_t observed_tutorial_jump_request_count = 0;
    std::uint64_t accepted_tutorial_jump_count = 0;
    std::uint64_t rejected_tutorial_jump_count = 0;
    std::uint64_t observed_function_generation = 0;
    std::uint32_t observed_function_state = 0;
    std::uint64_t accepted_function_action_count = 0;
    std::uint64_t rejected_function_action_count = 0;
    std::uint64_t accepted_completion_transition_count = 0;
    std::uint64_t rejected_completion_transition_count = 0;
    std::uint32_t last_completed_action_state = 0;
    std::uint32_t last_completion_target_state = 0;
};

void reset();

// Starts/restarts the reconstructed MEPlayer action boundary for one concrete
// player spawn. The already recovered initial player action is state 0x1b
// (action28 in pNN_actions.plist). The request count is seeded so stale C taps
// from a previous spawn cannot replay into the new player instance.
bool sync_spawn(
    std::uint64_t spawn_generation,
    std::uint64_t tutorial_jump_request_count);

// Consume accepted C/jump requests from game_scene_action_input. A new request
// queues the recovered tutorial jump MEPLAYERSTATE 0x20 only while the currently
// loaded action is the initial 0x1b state.
bool observe_tutorial_jump_request_count(
    std::uint64_t tutorial_jump_request_count);

// Maps horizontal Controls movement to action01 walk/action02 run, then returns
// to idle on release. Other active actions retain control until completion.
bool observe_movement_action_state(
    std::uint32_t movement_action_state,
    bool moving);

// Consumes the edge of the reconstructed Controls +0x118 function-state stream.
bool observe_base_function_state(
    std::uint64_t function_generation,
    std::uint32_t function_state,
    bool has_pending_base_action,
    std::uint32_t pending_meplayer_state,
    bool pending_has_additional_guard);

// Mirrors the recovered MEPlayer::actionState() completion transitions for the
// currently supported live actions. The generation/revision/state tuple binds a
// completion to the exact loaded MeActionsSystem instance that became done.
// A(0x02) and tutorial jump(0x20) queue 0x1b. B(0x1d) queues 0x1e while
// function state 8 remains held, otherwise 0x1b. Unsupported states fail closed.
bool observe_action_completion(
    std::uint64_t action_generation,
    std::uint64_t loaded_revision,
    std::uint32_t completed_action_state,
    bool action_done,
    std::uint32_t current_function_state);

// Called only after PlayerActionRuntimeLoader has successfully resolved and
// parsed the action document corresponding to desired_action_state.
bool commit_loaded_action(std::uint32_t action_state, bool looping = false);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::meplayer_live_action_state
