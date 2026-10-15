#pragma once

#include <algorithm>
#include <cstdint>

#include "game_clock.h"
#include "offline_startup_flow.h"
#include "tutorial_scene_bootstrap.h"

namespace nevergone::tutorial_loading_fade {

struct State {
    bool loading_route_seen = false;
    bool active = false;
    std::uint64_t start_tick = 0u;
    std::uint64_t start_count = 0u;
    std::uint64_t completion_count = 0u;
};

inline void reset(State* state) {
    if (state != nullptr) *state = State{};
}

// Reproduce GameSceneUI::hideLoadingLayer(0.2f) without a wall-clock timer.
// The caller feeds the already recovered fixed 35 Hz game clock. A fade may
// begin only after this state has actually observed tutorial-loading followed
// by game-levels; unrelated GameLevels entries therefore cannot synthesize a
// loading transition.
inline float alpha_for_route(
        State* state,
        offline_startup_flow::Route route,
        std::uint64_t game_tick) {
    if (state == nullptr) return 0.0f;

    if (route == offline_startup_flow::Route::kTutorialLoading) {
        state->loading_route_seen = true;
        state->active = false;
        state->start_tick = 0u;
        return 1.0f;
    }

    if (route != offline_startup_flow::Route::kGameLevels) {
        reset(state);
        return 0.0f;
    }

    if (!state->active) {
        if (!state->loading_route_seen) return 0.0f;
        state->loading_route_seen = false;
        state->active = true;
        state->start_tick = game_tick;
        ++state->start_count;
    }

    const std::uint64_t elapsed_ticks =
        game_tick >= state->start_tick ? game_tick - state->start_tick : 0u;
    const double elapsed_seconds =
        static_cast<double>(elapsed_ticks) * game_clock::kFixedStepSeconds;
    if (elapsed_seconds >= tutorial_scene_bootstrap::kOriginalHideFadeSeconds) {
        state->active = false;
        ++state->completion_count;
        return 0.0f;
    }

    const double alpha =
        1.0 - elapsed_seconds / tutorial_scene_bootstrap::kOriginalHideFadeSeconds;
    return static_cast<float>(std::clamp(alpha, 0.0, 1.0));
}

}  // namespace nevergone::tutorial_loading_fade
