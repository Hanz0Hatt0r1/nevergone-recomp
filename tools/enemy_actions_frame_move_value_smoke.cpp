#include <cassert>
#include <cmath>
#include <cstdint>

#include "enemy_actions_frame_move_value.h"

namespace {

bool close_enough(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    using nevergone::enemy_actions_frame_move_value::BoundsObservation;
    using nevergone::enemy_actions_frame_move_value::State;
    using nevergone::enemy_actions_frame_move_value::apply;

    State state;
    state.field_17c = -1;
    state.field_180 = 100.0f;
    state.field_184 = 200.0f;

    // Frame zero seeds +0x298/+0x29c with the sprite bottom-center point.
    const auto seeded = apply(0, true, true, 3u, {10.0f, 20.0f, 8.0f}, state);
    assert(seeded.applied);
    assert(seeded.state.field_17c == 0);
    assert(close_enough(seeded.state.field_298, 14.0f));
    assert(close_enough(seeded.state.field_29c, 20.0f));
    assert(close_enough(seeded.state.field_180, 100.0f));
    assert(close_enough(seeded.state.field_184, 200.0f));

    // Nonzero frames accumulate point deltas, then store the delta itself in
    // +0x298/+0x29c rather than the new absolute point.
    const auto moved = apply(1, true, true, 3u, {14.0f, 23.0f, 8.0f}, seeded.state);
    assert(moved.applied);
    assert(moved.state.field_17c == 1);
    assert(close_enough(moved.state.field_180, 104.0f));
    assert(close_enough(moved.state.field_184, 203.0f));
    assert(close_enough(moved.state.field_298, 4.0f));
    assert(close_enough(moved.state.field_29c, 3.0f));

    // Repeating the same frame exits before touching any state.
    const auto repeated = apply(1, true, true, 3u, {100.0f, 100.0f, 50.0f}, moved.state);
    assert(!repeated.applied);
    assert(close_enough(repeated.state.field_180, moved.state.field_180));
    assert(close_enough(repeated.state.field_298, moved.state.field_298));

    // Missing EAD/primary-array and equality with CCArray::count() all return
    // without advancing +0x17c.
    const auto no_data = apply(2, false, true, 3u, {1.0f, 2.0f, 4.0f}, moved.state);
    assert(!no_data.applied);
    assert(no_data.state.field_17c == 1);

    const auto no_array = apply(2, true, false, 3u, {1.0f, 2.0f, 4.0f}, moved.state);
    assert(!no_array.applied);
    assert(no_array.state.field_17c == 1);

    const auto at_count = apply(3, true, true, 3u, {1.0f, 2.0f, 4.0f}, moved.state);
    assert(!at_count.applied);
    assert(at_count.state.field_17c == 1);

    // Native tests equality only; current-frame bits greater than count do not
    // trip the guard in this helper.
    const auto beyond_count = apply(4, true, true, 3u, {6.0f, 8.0f, 4.0f}, moved.state);
    assert(beyond_count.applied);
    assert(beyond_count.state.field_17c == 4);

    return 0;
}
