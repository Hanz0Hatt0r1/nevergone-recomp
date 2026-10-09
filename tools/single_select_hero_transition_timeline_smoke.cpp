#include <cassert>
#include <cstdint>

#include "single_select_hero_transition_timeline.h"

int main() {
    namespace timeline = nevergone::single_select_hero_transition_timeline;

    timeline::reset();
    auto state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kInactive);

    // Initial initUI() Carousel reaches FunOpenTheDoor after exactly 84 fixed
    // 35 Hz ticks (2.4 seconds).
    timeline::begin_initial(100, 7);
    state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kInitialCarousel);
    assert(state.selector_generation == 7);
    assert(state.begin_tick == 100);
    assert(state.unlock_tick == 184);
    assert(state.begin_count == 1);
    assert(!timeline::advance(183, 7));
    assert(timeline::advance(184, 7));
    assert(!timeline::advance(185, 7));
    state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kInactive);
    assert(state.completion_count == 1);

    // A changed career locks through the 2.69 s close sequence followed by the
    // same 2.4 s Carousel. ceil(5.09 * 35) = 179 fixed ticks.
    timeline::begin_career_change(1000, 7);
    state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kCareerChange);
    assert(state.unlock_tick == 1179);
    assert(state.begin_count == 2);
    assert(!timeline::advance(1178, 7));
    assert(timeline::advance(1179, 7));
    state = timeline::snapshot();
    assert(state.completion_count == 2);

    // A fresh selector generation invalidates an old callback deadline.
    timeline::begin_initial(2000, 8);
    assert(!timeline::advance(2084, 9));
    state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kInactive);
    assert(state.stale_generation_count == 1);
    assert(state.completion_count == 2);

    // Invalid generation zero never starts a transition.
    timeline::begin_initial(3000, 0);
    state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kInactive);
    assert(state.begin_count == 3);

    return 0;
}
