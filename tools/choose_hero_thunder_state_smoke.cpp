#include "choose_hero_thunder_state.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool near(float left, float right, float epsilon = 0.001f) {
    return std::fabs(left - right) <= epsilon;
}

}  // namespace

int main() {
    using nevergone::choose_hero_thunder_state::Machine;

    Machine machine;
    machine.reset(123);
    machine.start();

    auto state = machine.snapshot(0.0);
    assert(state.initialized);
    assert(state.rng_draw_count == 48);  // 6 sprites * (6 discarded + delay + fade)
    assert(state.thunder_begin_count == 0);
    assert(state.thunder_end_count == 0);
    for (std::size_t index = 0; index < state.thunder_running.size(); ++index) {
        assert(state.thunder_running[index]);
        assert(near(state.thunder_alpha[index], 0.0f));
        assert(!state.lightning_running[index]);
    }
    assert(!state.ground_light_running);

    // Fixed srand48-equivalent seed 123 gives the first visible thunder callback
    // just before 4 seconds. Its sound slot is intentionally silent (>6).
    machine.advance(4.0);
    state = machine.snapshot(4.0);
    assert(state.rng_draw_count == 49);
    assert(state.thunder_begin_count == 1);
    assert(state.thunder_end_count == 0);
    assert(state.sound_callback_count == 1);
    assert(state.last_sound_index == -1);
    assert(machine.take_sound_index() == -1);

    // The next callback (another sprite) lands before 4.5s and maps to sound
    // slot 4. No thunder fade has ended yet.
    machine.advance(4.5);
    state = machine.snapshot(4.5);
    assert(state.rng_draw_count == 50);
    assert(state.thunder_begin_count == 2);
    assert(state.thunder_end_count == 0);
    assert(state.sound_callback_count == 2);
    assert(state.last_sound_index == 4);
    assert(machine.take_sound_index() == 4);
    assert(machine.take_sound_index() == -1);

    // The first FuncThunderEnd occurs at ~4.67446s. The recovered [A,B,C,C,C,C]
    // lookup resolves to [3,1,3,3,3,3] for this stream: only lightning 3 and 1
    // are idle on their first encounter, so only two fade RNG draws are consumed.
    // Five reschedule draws + two lightning fades + one discarded ground draw +
    // one ground fade move the total from 50 to 59.
    machine.advance(4.68);
    state = machine.snapshot(4.68);
    assert(state.rng_draw_count == 59);
    assert(state.thunder_begin_count == 2);
    assert(state.thunder_end_count == 1);
    assert(state.lightning_schedule_count == 2);
    assert(state.ground_light_schedule_count == 1);
    assert(state.lightning_running[1]);
    assert(state.lightning_running[3]);
    assert(!state.lightning_running[0]);
    assert(!state.lightning_running[2]);
    assert(!state.lightning_running[4]);
    assert(!state.lightning_running[5]);
    assert(state.ground_light_running);
    assert(near(state.lightning_alpha[1], 0.0f));
    assert(near(state.lightning_alpha[3], 0.0f));
    assert(near(state.ground_light_alpha, 0.0f));

    // Rewinding is not a runtime operation, but snapshotting before a scheduled
    // flash must remain hidden and side-effect free.
    const auto earlier = machine.snapshot(4.0);
    assert(near(earlier.lightning_alpha[1], 0.0f));
    assert(earlier.rng_draw_count == state.rng_draw_count);

    std::cout << "choose hero thunder state smoke: ok\n";
    return 0;
}
