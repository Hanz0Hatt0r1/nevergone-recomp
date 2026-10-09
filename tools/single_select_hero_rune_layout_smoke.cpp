#include <cassert>
#include <cmath>

#include "single_select_hero_rune_layout.h"

namespace {
bool close_enough(float lhs, float rhs) { return std::fabs(lhs - rhs) < 0.0001f; }
void assert_rect(
        const nevergone::single_select_hero_rune_layout::SurfaceRect& rect,
        float left, float top, float right, float bottom) {
    assert(rect.valid);
    assert(close_enough(rect.left, left));
    assert(close_enough(rect.top, top));
    assert(close_enough(rect.right, right));
    assert(close_enough(rect.bottom, bottom));
}
}

int main() {
    namespace layout = nevergone::single_select_hero_rune_layout;
    layout::set_pressed_render_tag(0);
    assert(layout::pressed_render_tag() == 0);
    assert(layout::frame_index(1, false) == 0);
    assert(layout::frame_index(1, true) == 1);
    assert(layout::frame_index(5, true) == 9);
    assert(layout::frame_index(0, false) == -1);
    assert(layout::frame_index(6, false) == -1);

    // The touch router publishes a presentation-only pressed tag. Existing
    // compositor calls that request the normal frame then transparently select
    // the staged glow frame only for that rune.
    layout::set_pressed_render_tag(3);
    assert(layout::pressed_render_tag() == 3);
    assert(layout::frame_index(1, false) == 0);
    assert(layout::frame_index(3, false) == 5);
    assert(layout::frame_index(3, true) == 5);
    assert(layout::frame_index(4, false) == 6);
    layout::set_pressed_render_tag(99);
    assert(layout::pressed_render_tag() == 0);
    assert(layout::frame_index(3, false) == 4);

    for (int tag = 1; tag <= 5; ++tag) {
        assert(layout::enabled_tag(tag));
        assert(close_enough(layout::opacity(tag), 1.0f));
    }

    assert(close_enough(layout::center_y(1), 495.0f));
    assert(close_enough(layout::center_y(2), 405.0f));
    assert(close_enough(layout::center_y(3), 315.0f));
    assert(close_enough(layout::center_y(4), 225.0f));
    assert(close_enough(layout::center_y(5), 135.0f));

    // Recovered untrimmed source sizes from the shipped atlas. These are the
    // CCMenuItemSprite content sizes used for touch, not the trimmed visible quads.
    assert_rect(layout::hit_rect_for_surface(79, 99, 1, 1136, 640),
                528.5f, 95.5f, 607.5f, 194.5f);
    assert_rect(layout::hit_rect_for_surface(87, 89, 2, 1136, 640),
                524.5f, 190.5f, 611.5f, 279.5f);
    assert_rect(layout::hit_rect_for_surface(75, 95, 3, 1136, 640),
                530.5f, 277.5f, 605.5f, 372.5f);
    assert_rect(layout::hit_rect_for_surface(111, 110, 4, 1136, 640),
                512.5f, 360.0f, 623.5f, 470.0f);
    assert_rect(layout::hit_rect_for_surface(137, 131, 5, 1136, 640),
                499.5f, 439.5f, 636.5f, 570.5f);

    assert(layout::hit_test(79, 99, 1, 1136, 640, 568.0f, 145.0f));
    assert(layout::hit_test(87, 89, 2, 1136, 640, 524.5f, 190.5f));
    assert(!layout::hit_test(87, 89, 2, 1136, 640, 524.4f, 190.5f));
    assert(!layout::hit_test(137, 131, 5, 1136, 640, 568.0f, 571.0f));
    assert(!layout::hit_rect_for_surface(0, 99, 1, 1136, 640).valid);
    assert(!layout::hit_rect_for_surface(79, 99, 0, 1136, 640).valid);

    // Aspect-fit mapping preserves the content rect on a 2x surface.
    assert_rect(layout::hit_rect_for_surface(79, 99, 1, 2272, 1280),
                1057.0f, 191.0f, 1215.0f, 389.0f);

    // Visible drawing still honors TexturePacker trims independently of hit size.
    layout::FrameGeometry career_one;
    career_one.width = 79;
    career_one.height = 99;
    career_one.source_width = 79;
    career_one.source_height = 99;
    auto quad = layout::quad_for_surface(career_one, 1, 1136, 640);
    assert(quad.valid);
    assert(close_enough(quad.x0, 528.5f * 2.0f / 1136.0f - 1.0f));

    layout::FrameGeometry career_two;
    career_two.width = 87;
    career_two.height = 83;
    career_two.top = 3;
    career_two.source_width = 87;
    career_two.source_height = 89;
    quad = layout::quad_for_surface(career_two, 2, 1136, 640);
    assert(quad.valid);
    // Trimmed visible top is 193.5, while the touch rect begins at 190.5.
    assert(close_enough(quad.y0, 1.0f - 193.5f * 2.0f / 640.0f));

    layout::FrameGeometry invalid = career_one;
    invalid.width = 100;
    assert(!layout::quad_for_surface(invalid, 1, 1136, 640).valid);
    return 0;
}
