#include <cassert>

#include "game_scene_player_motion_state.h"

namespace motion = nevergone::game_scene_player_motion_state;

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
    assert(state.offset_x == 16.0f);
    assert(state.offset_y == 4.0f);

    // Repeating the same game tick cannot double-apply movement.
    assert(motion::advance_normal_sample(107u, true, 1.0f, 1.0f));
    assert(motion::snapshot().applied_steps == 2u);

    // The same spawn preserves its live cursor; a new spawn generation resets it.
    assert(motion::sync_spawn(7u, 1u, 999u));
    assert(motion::snapshot().applied_steps == 2u);
    assert(motion::sync_spawn(8u, 1u, 200u));
    state = motion::snapshot();
    assert(state.applied_steps == 0u);
    assert(state.offset_x == 0.0f && state.offset_y == 0.0f);
    assert(state.last_game_tick == 200u);

    // Unproven playerMoveValue normal branches remain disabled.
    assert(!motion::sync_spawn(9u, 4u, 300u));
    assert(!motion::snapshot().ready);
    return 0;
}
