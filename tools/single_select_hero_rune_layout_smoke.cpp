#include <cassert>
#include <cmath>

#include "single_select_hero_rune_layout.h"

namespace {
bool close_enough(float lhs, float rhs) { return std::fabs(lhs - rhs) < 0.0001f; }
}

int main() {
    namespace layout = nevergone::single_select_hero_rune_layout;
    assert(layout::frame_index(1, false) == 0);
    assert(layout::frame_index(1, true) == 1);
    assert(layout::frame_index(5, true) == 9);
    assert(layout::frame_index(0, false) == -1);
    assert(layout::frame_index(6, false) == -1);
    for (int tag = 1; tag <= 5; ++tag) {
        assert(layout::enabled_tag(tag));
        assert(close_enough(layout::opacity(tag), 1.0f));
    }
    assert(!layout::enabled_tag(0));
    assert(!layout::enabled_tag(6));
    assert(close_enough(layout::center_y(1), 495.0f));
    assert(close_enough(layout::center_y(2), 405.0f));
    assert(close_enough(layout::center_y(3), 315.0f));
    assert(close_enough(layout::center_y(4), 225.0f));
    assert(close_enough(layout::center_y(5), 135.0f));

    layout::FrameGeometry career_one;
    career_one.width = 79; career_one.height = 99;
    career_one.source_width = 79; career_one.source_height = 99;
    auto quad = layout::quad_for_surface(career_one, 1, 1136, 640);
    assert(quad.valid);
    assert(close_enough(quad.x0, 528.5f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.x1, 607.5f * 2.0f / 1136.0f - 1.0f));

    const auto hit_one = layout::hit_rect_for_surface(career_one, 1, 1136, 640);
    assert(hit_one.valid);
    assert(close_enough(hit_one.left, 528.5f));
    assert(close_enough(hit_one.right, 607.5f));
    assert(close_enough(hit_one.top, 95.5f));
    assert(close_enough(hit_one.bottom, 194.5f));
    assert(layout::hit_test(career_one, 1, 1136, 640, 568.0f, 145.0f));
    assert(!layout::hit_test(career_one, 1, 1136, 640, 500.0f, 145.0f));

    layout::FrameGeometry career_two;
    career_two.width = 87; career_two.height = 83;
    career_two.top = 3; career_two.source_width = 87; career_two.source_height = 89;
    quad = layout::quad_for_surface(career_two, 2, 1136, 640);
    assert(quad.valid);
    assert(close_enough(quad.x0, 524.5f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.x1, 611.5f * 2.0f / 1136.0f - 1.0f));

    // The visible trimmed frame begins 3 px lower, but CCMenuItemSprite's hit
    // rectangle remains the full 87x89 source size centered at the menu item.
    const auto hit_two = layout::hit_rect_for_surface(career_two, 2, 1136, 640);
    assert(hit_two.valid);
    assert(close_enough(hit_two.left, 524.5f));
    assert(close_enough(hit_two.right, 611.5f));
    assert(close_enough(hit_two.top, 190.5f));
    assert(close_enough(hit_two.bottom, 279.5f));
    assert(layout::hit_test(career_two, 2, 1136, 640, 568.0f, 191.0f));

    const auto doubled = layout::quad_for_surface(career_one, 1, 2272, 1280);
    assert(doubled.valid);
    assert(close_enough(doubled.x0, 528.5f * 2.0f / 1136.0f - 1.0f));
    assert(layout::hit_test(career_one, 1, 2272, 1280, 1136.0f, 290.0f));

    layout::FrameGeometry invalid = career_one;
    invalid.width = 100;
    assert(!layout::quad_for_surface(invalid, 1, 1136, 640).valid);
    assert(!layout::hit_rect_for_surface(invalid, 1, 1136, 640).valid);
    assert(!layout::quad_for_surface(career_one, 0, 1136, 640).valid);
    return 0;
}
