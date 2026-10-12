#include <cassert>

#include "game_scene_player_motion_state.h"

namespace motion = nevergone::game_scene_player_motion_state;
namespace contract = nevergone::player_motion_contract;

int main() {
    motion::reset();
    assert(!motion::snapshot().ready);

    assert(motion::sync_spawn(7u, 1u, 100u));
    auto state = motion::snapshot();
    assert(state.ready);
    assert(state.last_game_tick == 100u);
    assert(state.applied_steps == 0u);

    // Neutral time is consumed without generating later catch-up movement.
    assert(motion::advance_normal_sample(105u, false, 1.0f, 0.0f));
    state = motion::snapshot();
    assert(state.last_game_tick == 105u);
    assert(state.applied_steps == 0u);
    assert(state.offset_x == 0.0f);

    // Two elapsed normal-action ticks use the recovered 8/4 vector per tick.
    assert(motion::advance_normal_sample(107u, true, 1.0f, 0.5f));
    state = motion::snapshot();
    assert(state.applied_steps == 2u);
    assert(state.normal_steps == 2u);
    assert(state.fast_steps == 0u);
    assert(state.offset_x == 16.0f);
    assert(state.offset_y == 4.0f);

    // Player-index 1 action state 1 uses the separately proven 20/10 scale.
    assert(motion::advance_sample(
            108u,
            true,
            contract::kFastMoveActionState,
            1.0f,
            0.5f));
    state = motion::snapshot();
    assert(state.applied_steps == 3u);
    assert(state.normal_steps == 2u);
    assert(state.fast_steps == 1u);
    assert(state.last_action_state == contract::kFastMoveActionState);
    assert(state.offset_x == 36.0f);
    assert(state.offset_y == 9.0f);

    // Repeating the same game tick cannot double-apply movement.
    assert(motion::advance_sample(
            108u,
            true,
            contract::kFastMoveActionState,
            1.0f,
            1.0f));
    assert(motion::snapshot().applied_steps == 3u);

    // Player indices 2/3 use the recovered 16/8 fast-movement scale.
    assert(motion::sync_spawn(8u, 2u, 200u));
    assert(motion::advance_sample(
            201u,
            true,
            contract::kFastMoveActionState,
            -1.0f,
            0.5f));
    state = motion::snapshot();
    assert(state.fast_steps == 1u);
    assert(state.offset_x == -16.0f);
    assert(state.offset_y == 4.0f);

    // The same spawn preserves its live cursor; a new spawn generation resets it.
    assert(motion::sync_spawn(8u, 2u, 999u));
    assert(motion::snapshot().applied_steps == 1u);
    assert(motion::sync_spawn(9u, 1u, 300u));
    state = motion::snapshot();
    assert(state.applied_steps == 0u);
    assert(state.offset_x == 0.0f && state.offset_y == 0.0f);
    assert(state.last_game_tick == 300u);

    // Unproven playerMoveValue normal branches remain disabled.
    assert(!motion::sync_spawn(10u, 4u, 400u));
    assert(!motion::snapshot().ready);
    return 0;
}
