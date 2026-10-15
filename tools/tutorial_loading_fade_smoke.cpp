#include <cassert>
#include <cmath>

#include "offline_startup_flow.h"
#include "tutorial_loading_fade.h"

int main() {
    namespace fade = nevergone::tutorial_loading_fade;
    namespace flow = nevergone::offline_startup_flow;

    fade::State state;

    // No unrelated GameLevels entry may fabricate a LoadingLayer fade.
    assert(fade::alpha_for_route(&state, flow::Route::kGameLevels, 100u) == 0.0f);
    assert(!state.active);
    assert(state.start_count == 0u);

    // A visible tutorial-loading frame is fully opaque and arms the exact
    // tutorial-loading -> game-levels transition.
    assert(fade::alpha_for_route(&state, flow::Route::kTutorialLoading, 200u) == 1.0f);
    assert(state.loading_route_seen);
    assert(!state.active);

    // The first GameLevels frame starts hideLoadingLayer(0.2f) at full alpha.
    assert(fade::alpha_for_route(&state, flow::Route::kGameLevels, 218u) == 1.0f);
    assert(state.active);
    assert(state.start_tick == 218u);
    assert(state.start_count == 1u);

    // The recovered 35 Hz game clock makes 0.2 seconds exactly seven ticks.
    const float middle = fade::alpha_for_route(&state, flow::Route::kGameLevels, 221u);
    assert(std::fabs(middle - (4.0f / 7.0f)) < 0.0001f);
    const float last_visible = fade::alpha_for_route(&state, flow::Route::kGameLevels, 224u);
    assert(std::fabs(last_visible - (1.0f / 7.0f)) < 0.0001f);

    assert(fade::alpha_for_route(&state, flow::Route::kGameLevels, 225u) == 0.0f);
    assert(!state.active);
    assert(state.completion_count == 1u);

    // Completion is stable: the same GameLevels route cannot restart the fade.
    assert(fade::alpha_for_route(&state, flow::Route::kGameLevels, 226u) == 0.0f);
    assert(state.start_count == 1u);
    assert(state.completion_count == 1u);

    // Leaving the route family resets stale transition state.
    assert(fade::alpha_for_route(&state, flow::Route::kChooseRole, 300u) == 0.0f);
    assert(!state.loading_route_seen);
    assert(!state.active);
    assert(state.start_count == 0u);
    assert(state.completion_count == 0u);

    return 0;
}
