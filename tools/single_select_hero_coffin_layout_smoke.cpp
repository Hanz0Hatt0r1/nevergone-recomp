#include <cassert>
#include <cmath>

#include "single_select_hero_coffin_layout.h"

namespace {
bool near(float a, float b) { return std::fabs(a-b) < 0.0001f; }
}

int main() {
    namespace coffin = nevergone::single_select_hero_coffin_layout;
    assert(coffin::kFrameCount == 2 && coffin::kLidCopies == 5);
    assert(near(coffin::kFixedCenterX, 287.0f));
    assert(near(coffin::kFixedCenterY, 320.0f));
    assert(near(coffin::kReelStepX, 271.0f));
    for (int i=0; i<coffin::kLidCopies; ++i) {
        assert(near(coffin::initial_lid_center_x(i), 287.0f + 271.0f * i));
        assert(near(coffin::settled_reel_offset_x(i+1), -271.0f * i));
        assert(near(coffin::initial_lid_center_x(i) +
            coffin::settled_reel_offset_x(i+1), 287.0f));
    }
    assert(coffin::initial_lid_center_x(-1) < 0);
    assert(coffin::initial_lid_center_x(5) < 0);
    assert(near(coffin::settled_reel_offset_x(0), 0));

    // Original rotated TexturePacker xrshiguan.png is untrimmed 274 x 514.
    coffin::FrameGeometry base{274, 514, 0, 0, 274, 514};
    const auto quad = coffin::base_quad_for_surface(base, 1136, 640);
    assert(quad.valid);
    assert(near(quad.x0, 150.0f * 2.0f / 1136.0f - 1.0f));
    assert(near(quad.x1, 424.0f * 2.0f / 1136.0f - 1.0f));
    assert(near(quad.y0, 1.0f - 63.0f * 2.0f / 640.0f));
    assert(near(quad.y1, 1.0f - 577.0f * 2.0f / 640.0f));
    const auto doubled = coffin::base_quad_for_surface(base, 2272, 1280);
    assert(doubled.valid);
    assert(near(doubled.x0, quad.x0) && near(doubled.y1, quad.y1));
    auto invalid = base; invalid.top = 1;
    assert(!coffin::base_quad_for_surface(invalid, 1136, 640).valid);
    return 0;
}
