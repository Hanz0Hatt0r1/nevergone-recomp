#include <cassert>
#include <cstdint>

#include "single_select_hero_transition_timeline.h"

int main() {
    namespace timeline = nevergone::single_select_hero_transition_timeline;

    timeline::reset();
    auto state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kInactive);

    timeline::begin_initial(100, 7, 1);
    state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kInitialCarousel);
    assert(state.selector_generation == 7);
    assert(state.old_career == 1 && state.new_career == 1);
    assert(state.begin_tick == 100);
    assert(state.unlock_tick == 184);
    assert(!timeline::advance(183, 7));
    assert(timeline::advance(184, 7));

    assert(timeline::career_change_unlock_ticks(1, 2) == 179);
    assert(timeline::career_change_unlock_ticks(2, 4) == 179);
    assert(timeline::begin_career_change(1000, 7, 2, 4));
    state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kCareerChange);
    assert(state.old_career == 2 && state.new_career == 4);
    assert(state.unlock_tick == 1179);
    assert(!timeline::advance(1178, 7));
    assert(timeline::advance(1179, 7));

    assert(timeline::career_change_unlock_ticks(1, 4) == 231);
    assert(timeline::career_change_unlock_ticks(1, 5) == 231);
    assert(timeline::career_change_unlock_ticks(5, 2) == 231);
    assert(timeline::begin_career_change(2000, 7, 5, 1));
    state = timeline::snapshot();
    assert(state.old_career == 5 && state.new_career == 1);
    assert(state.unlock_tick == 2231);
    assert(!timeline::advance(2230, 7));
    assert(timeline::advance(2231, 7));

    assert(timeline::career_change_unlock_ticks(3, 3) == 0);
    assert(timeline::career_change_unlock_ticks(0, 3) == 0);
    assert(!timeline::begin_career_change(3000, 7, 3, 3));
    assert(!timeline::begin_career_change(3000, 7, 0, 3));

    timeline::begin_initial(4000, 8, 2);
    assert(!timeline::advance(4084, 9));
    state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kInactive);
    assert(state.stale_generation_count == 1);
    assert(state.completion_count == 3);

    const std::uint64_t begin_count = state.begin_count;
    timeline::begin_initial(5000, 0, 1);
    timeline::begin_initial(5000, 8, 9);
    state = timeline::snapshot();
    assert(state.phase == timeline::Phase::kInactive);
    assert(state.begin_count == begin_count);

    return 0;
}
