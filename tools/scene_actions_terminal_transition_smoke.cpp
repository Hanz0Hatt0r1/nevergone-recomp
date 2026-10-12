#include <cassert>

#include "scene_actions_terminal_transition.h"

namespace play = nevergone::scene_actions_play_state;
namespace terminal = nevergone::scene_actions_terminal_transition;

int main() {
    play::State born;
    born.active_15d = true;
    born.mode_24e = 0u;
    born.done_1a8 = false;
    born.end_184 = false;
    born.frame_index_250 = 9u;
    auto result = terminal::apply(9u, &born);
    assert(result.accepted);
    assert(result.completed);
    assert(!result.update_data_requested);
    assert(!result.looped_to_frame_zero);
    assert(born.terminal_last_frame_180 == 8);
    assert(born.end_184);
    assert(born.frame_index_250 == 0u);
    assert(born.done_1a8);

    play::State idle;
    idle.active_15d = true;
    idle.mode_24e = 1u;
    idle.done_1a8 = false;
    idle.frame_index_250 = 4u;
    result = terminal::apply(4u, &idle);
    assert(result.accepted);
    assert(!result.completed);
    assert(result.update_data_requested);
    assert(result.looped_to_frame_zero);
    assert(idle.terminal_last_frame_180 == 3);
    assert(idle.end_184);
    assert(idle.frame_index_250 == 0u);
    assert(!idle.done_1a8);

    assert(!terminal::apply(0u, &idle).accepted);
    play::State inactive;
    assert(!terminal::apply(1u, &inactive).accepted);
    return 0;
}
