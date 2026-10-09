#include <cassert>
#include <cmath>

#include "single_select_hero_rune_layout.h"

namespace {

bool close_enough(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
}

}  // namespace

int main() {
    namespace layout = nevergone::single_select_hero_rune_layout;

    assert(layout::frame_index(1, false) == 0);
    assert(layout::frame_index(1, true) == 1);
    assert(layout::frame_index(2, false) == 2);
    assert(layout::frame_index(5, true) == 9);
    assert(layout::frame_index(0, false) == -1);
    assert(layout::frame_index(6, false) == -1);

    assert(layout::enabled_tag(1));
    assert(layout::enabled_tag(2));
    assert(!layout::enabled_tag(3));
    assert(close_enough(layout::opacity(1), 1.0f));
    assert(close_enough(layout::opacity(3), 120.0f / 255.0f));

    assert(close_enough(layout::center_y(1), 495.0f));
    assert(close_enough(layout::center_y(2), 405.0f));
    assert(close_enough(layout::center_y(3), 315.0f));
    assert(close_enough(layout::center_y(4), 225.0f));
    assert(close_enough(layout::center_y(5), 135.0f));

    // xrfuwen01.png: untrimmed 79x99 at recovered center (568,495).
    layout::FrameGeometry career_one;
    career_one.width = 79;
    career_one.height = 99;
    career_one.source_width = 79;
    career_one.source_height = 99;
    auto quad = layout::quad_for_surface(career_one, 1, 1136, 640);
    assert(quad.valid);
    assert(close_enough(quad.x0, 528.5f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.x1, 607.5f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.y0, 1.0f - 95.5f * 2.0f / 640.0f));
    assert(close_enough(quad.y1, 1.0f - 194.5f * 2.0f / 640.0f));

    // xrfuwen02.png has a 3 px top trim inside an 87x89 source.
    layout::FrameGeometry career_two;
    career_two.width = 87;
    career_two.height = 83;
    career_two.left = 0;
    career_two.top = 3;
    career_two.source_width = 87;
    career_two.source_height = 89;
    quad = layout::quad_for_surface(career_two, 2, 1136, 640);
    assert(quad.valid);
    assert(close_enough(quad.x0, 524.5f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.x1, 611.5f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.y0, 1.0f - 193.5f * 2.0f / 640.0f));
    assert(close_enough(quad.y1, 1.0f - 276.5f * 2.0f / 640.0f));

    // Aspect-fit mapping is centered and stable on a larger 16:9 surface.
    const auto doubled = layout::quad_for_surface(career_one, 1, 2272, 1280);
    assert(doubled.valid);
    assert(close_enough(doubled.x0, quad.x0) == false); // different career/frame above
    const auto doubled_one = layout::quad_for_surface(career_one, 1, 2272, 1280);
    assert(close_enough(doubled_one.x0, 528.5f * 2.0f / 1136.0f - 1.0f));

    layout::FrameGeometry invalid = career_one;
    invalid.width = 100;
    assert(!layout::quad_for_surface(invalid, 1, 1136, 640).valid);
    assert(!layout::quad_for_surface(career_one, 0, 1136, 640).valid);
    return 0;
}
