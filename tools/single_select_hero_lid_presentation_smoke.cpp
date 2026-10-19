#include <cassert>
#include <cmath>
#include <cstdint>

#include "single_select_hero_lid_presentation.h"

namespace {
bool near(float a, float b, float eps = 0.15f) {
    return std::fabs(a - b) <= eps;
}
}

int main() {
    namespace timeline = nevergone::single_select_hero_transition_timeline;
    namespace view = nevergone::single_select_hero_lid_presentation;
    namespace layout = nevergone::single_select_hero_coffin_layout;
    timeline::reset();

    const std::uint64_t start = 100;
    timeline::begin_initial(start, 7, 1);
    auto visual = timeline::visual_snapshot();
    assert(visual.phase == timeline::Phase::kInitialCarousel);
    assert(visual.initial_positions_verified);
    assert(visual.expected_finish_tick == start + 203);

    auto p = view::sample(visual, start, 7);
    assert(p.phase == view::Phase::kClosed && p.draw_lid);
    assert(p.lid_career == 1);
    assert(near(p.lid_local_x, 287) && near(p.reel_x, 0));
    assert(view::sample(visual, start + 83, 7).draw_lid);

    // At 84 ticks original FunOpenTheDoor unlocks the input but starts
    // a separate 3.39-second visual sequence. The lid must NOT vanish.
    assert(timeline::advance(start + 84, 7));
    assert(timeline::snapshot().phase == timeline::Phase::kInactive);
    visual = timeline::visual_snapshot();
    p = view::sample(visual, start + 84, 7);
    assert(p.phase == view::Phase::kOpeningNew && p.draw_lid);
    assert(near(p.lid_local_x, 287));
    p = view::sample(visual, start + 179, 7);
    assert(p.draw_lid && p.lid_local_x < 5.0f);
    p = view::sample(visual, start + 203, 7);
    assert(!p.draw_lid && p.phase == view::Phase::kFinished);
    assert(!view::sample(visual, start + 84, 8).draw_lid);
    assert(!view::sample(visual, start - 1, 7).draw_lid);

    // A reselect while the first door is still moving has conditional
    // Cocos starting positions: fail closed on covering the character.
    timeline::reset();
    timeline::begin_initial(100, 7, 1);
    assert(timeline::advance(184, 7));
    assert(timeline::begin_career_change(185, 7, 1, 2));
    visual = timeline::visual_snapshot();
    assert(!visual.initial_positions_verified);
    p = view::sample(visual, 185, 7);
    assert(!p.draw_lid && p.phase == view::Phase::kUnknownStart);

    // When the original opening action has fully finished, the next
    // old-lid closing and new-lid opening endpoints are established.
    timeline::reset();
    timeline::begin_initial(100, 7, 1);
    assert(timeline::advance(184, 7));
    assert(timeline::begin_career_change(303, 7, 1, 2));
    visual = timeline::visual_snapshot();
    assert(visual.initial_positions_verified);
    assert(visual.expected_finish_tick == 600);

    p = view::sample(visual, 303, 7);
    assert(p.phase == view::Phase::kClosingOld && p.draw_lid);
    assert(p.lid_career == 1 && near(p.lid_local_x, 0));
    p = view::sample(visual, 303 + 95, 7);
    assert(p.phase == view::Phase::kReel && p.draw_lid);
    assert(p.lid_career == 2);
    assert(p.reel_x < 0.0f && p.reel_x > -271.0f);
    p = view::sample(visual, 303 + 179, 7);
    assert(p.phase == view::Phase::kOpeningNew && p.draw_lid);
    assert(near(p.reel_x, -271));
    assert(near(p.lid_local_x, layout::initial_lid_center_x(1), 1));
    p = view::sample(visual, 303 + 297, 7);
    assert(p.phase == view::Phase::kFinished && !p.draw_lid);

    // Scene reset must not retain a stale door from another selector.
    timeline::reset();
    visual = timeline::visual_snapshot();
    assert(visual.phase == timeline::Phase::kInactive);
    assert(!view::sample(visual, 10000, 7).draw_lid);
    return 0;
}
