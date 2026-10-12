#pragma once

#include <cstdint>
#include <string>

namespace nevergone::meplayer_live_action_state {

constexpr std::uint32_t kInitialActionState = 0x1bu;
constexpr std::uint32_t kTutorialJumpActionState = 0x20u;
constexpr std::uint32_t kBaseAActionState = 0x02u;
constexpr std::uint32_t kBaselineBActionState = 0x1du;

struct Snapshot {
    bool ready = false;
    std::uint64_t generation = 0;
    std::uint64_t spawn_generation = 0;
    std::uint32_t current_action_state = kInitialActionState;
    std::uint32_t desired_action_state = kInitialActionState;
    std::uint64_t desired_revision = 0;
    std::uint64_t loaded_revision = 0;
    std::uint64_t observed_tutorial_event_count = 0;
    std::uint64_t accepted_tutorial_jump_count = 0;
    std::uint64_t rejected_tutorial_jump_count = 0;
    std::uint64_t observed_function_generation = 0;
    std::uint32_t observed_function_state = 0;
    std::uint64_t accepted_function_action_count = 0;
    std::uint64_t rejected_function_action_count = 0;
};

void reset();

// Starts/restarts the reconstructed MEPlayer action boundary for one concrete
// player spawn. The already recovered initial player action is state 0x1b
// (action28 in pNN_actions.plist).
bool sync_spawn(std::uint64_t spawn_generation, std::uint64_t tutorial_event_count);

// Observes semantic action-input progress. tutorial_event_count is incremented
// by game_scene_action_input only when the interactive C/jump tap actually
// advances the recovered tutorial Jump step, so this survives a fast tap that
// begins and ends between rendered frames.
bool observe_tutorial_event_count(std::uint64_t tutorial_event_count);

// Consumes the edge of the reconstructed Controls +0x118 function-state stream.
// Base A is accepted using its proven direct 0x02 action. Base B is accepted only
// while current/desired are the recovered initial 0x1b baseline; richer B guard
// state (+0x1f0/+0x1f8) remains outside this live boundary. State 0 records the
// release edge but deliberately does not synthesize return-to-idle: the original
// depends on MEPlayer action completion/isActionDone() for that transition.
bool observe_base_function_state(
    std::uint64_t function_generation,
    std::uint32_t function_state,
    bool has_pending_base_action,
    std::uint32_t pending_meplayer_state,
    bool pending_has_additional_guard);

// Called only after PlayerActionRuntimeLoader has successfully resolved and
// parsed the action document corresponding to desired_action_state.
bool commit_loaded_action(std::uint32_t action_state);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::meplayer_live_action_state
