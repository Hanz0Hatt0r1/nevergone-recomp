#pragma once

#include <cstddef>
#include <cstdint>

#include "enemy_actions_wbg_document.h"

namespace nevergone::tutorial_enemy_action_animation {

enum class Phase : std::uint8_t {
    kAppearance = 0,
    kStateAction,
};

struct Sample {
    Phase phase = Phase::kAppearance;
    unsigned action_id = 0;
    std::size_t frame_index = 0;
    const enemy_actions_wbg_document::Document* document = nullptr;
};

// Selects an action frame from the original per-frame durations. The initial
// non-looping clip hands its remaining elapsed time to the actor's state clip.
Sample sample(
    const enemy_actions_wbg_document::Document& appearance,
    unsigned appearance_action_id,
    bool appearance_loops,
    const enemy_actions_wbg_document::Document& state_action,
    unsigned state_action_id,
    bool state_action_loops,
    double elapsed_seconds);

// Samples a newly selected state action from its own transition clock.
Sample sample_action(
    const enemy_actions_wbg_document::Document& action,
    unsigned action_id,
    bool loops,
    double elapsed_seconds);

}  // namespace nevergone::tutorial_enemy_action_animation
