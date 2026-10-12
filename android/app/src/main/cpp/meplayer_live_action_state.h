#pragma once

#include <cstdint>
#include <string>

namespace nevergone::meplayer_live_action_state {

constexpr std::uint32_t kInitialActionState = 0x1bu;
constexpr std::uint32_t kTutorialJumpActionState = 0x20u;

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

// Called only after PlayerActionRuntimeLoader has successfully resolved and
// parsed the action document corresponding to desired_action_state.
bool commit_loaded_action(std::uint32_t action_state);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::meplayer_live_action_state
