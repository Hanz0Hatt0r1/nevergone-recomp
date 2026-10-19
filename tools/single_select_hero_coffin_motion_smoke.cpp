#include <cassert>
#include <cmath>

#include "single_select_hero_coffin_motion.h"

namespace {
bool near(float actual, float expected, float epsilon=0.002f) {
    return std::fabs(actual-expected) <= epsilon;
}
}

int main() {
    namespace m = nevergone::single_select_hero_coffin_motion;
    // Original calls: OpenTheDoor(false) = 2+.3+.39; true=.2+.5+2+.3+.39.
    assert(near(m::kCloseTotalSeconds,2.69f));
    assert(near(m::kOpenTotalSeconds,3.39f));
    assert(near(m::reel_total_seconds(1,2),2.4f));
    assert(near(m::reel_total_seconds(1,5),3.9f));

    // Change Dawn -> Higgs: 271-unit reel shift, +3 overshoot, settle.
    assert(near(m::reel_x(1,2,0),0));
    assert(near(m::reel_x(1,2,0.75f),-135.5f));
    assert(near(m::reel_x(1,2,1.5f),-271));
    assert(near(m::reel_x(1,2,1.8f),-268));
    assert(near(m::reel_x(1,2,2.4f),-271));
    // Reverse direction uses the negative overshoot.
    assert(near(m::reel_x(2,1,1.8f),-3));
    assert(near(m::reel_x(2,1,2.4f),0));
    // Initial Carousel calls itself with old == new and still bounces -3.
    assert(near(m::reel_x(1,1,1.8f),-3));
    assert(near(m::reel_x(1,1,2.4f),0));
    assert(near(m::reel_x(1,5,3.9f),-1084));
    assert(near(m::reel_x(5,1,3.9f),0));
    assert(near(m::reel_x(0,1,1),0));

    // Closed Dawn cover: X=287 in the original reel node.
    const float p = 287.0f;
    assert(near(m::closing_lid_x(p,0),0));
    assert(near(m::closing_lid_x(p,2.0f),p));
    assert(near(m::closing_lid_x(p,2.3f),p-3));
    assert(near(m::closing_lid_x(p,2.69f),p));
    assert(near(m::opening_lid_x(p,p,0),p));
    assert(near(m::opening_lid_x(p,p,.2f),p-8));
    assert(near(m::opening_lid_x(p,p,.7f),p-8));
    assert(near(m::opening_lid_x(p,p,2.7f),0));
    assert(near(m::opening_lid_x(p,p,3.0f),3));
    assert(near(m::opening_lid_x(p,p,3.39f),0));

    // The second open movement uses P-currentX, not an invented hard zero.
    assert(near(m::opening_lid_x(p,42.0f,3.39f),245));
    assert(near(m::seconds_from_ticks(140,0),4));
    assert(near(m::seconds_from_ticks(1,9),0));
    return 0;
}
