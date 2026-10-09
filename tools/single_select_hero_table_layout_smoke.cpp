#include <cassert>
#include <cmath>
#include <iostream>

#include "single_select_hero_table_layout.h"

namespace {

bool near(float actual, float expected, float epsilon = 0.0001f) {
    return std::fabs(actual - expected) <= epsilon;
}

}  // namespace

int main() {
    namespace layout = nevergone::single_select_hero_table_layout;

    assert(layout::frame_index(0, 0) == -1);
    assert(layout::frame_index(3, 0) == -1);
    assert(layout::frame_index(1, 0) == 0);
    assert(layout::frame_index(1, 1) == 1);
    assert(layout::frame_index(1, 2) == 0);
    assert(layout::frame_index(2, 0) == 2);
    assert(layout::frame_index(2, 1) == 2);
    assert(layout::frame_index(2, 2) == 3);

    // Recovered EN_HeroTable_01_a geometry from Singleselechero.plist:
    // source 264x443, upright trim 264x425, placement left=0/top=18.
    layout::FrameGeometry career_one;
    career_one.width = 264;
    career_one.height = 425;
    career_one.left = 0;
    career_one.top = 18;
    career_one.source_width = 264;
    career_one.source_height = 443;

    const auto design = layout::quad_for_surface(career_one, 1136, 640);
    assert(design.valid);
    // Untrimmed sprite is centered at the exact recovered Cocos position
    // (830,320). Its trimmed visible rect is x=[698,962], y(top)=[116.5,541.5].
    assert(near(design.x0, 698.0f * 2.0f / 1136.0f - 1.0f));
    assert(near(design.x1, 962.0f * 2.0f / 1136.0f - 1.0f));
    assert(near(design.y0, 1.0f - 116.5f * 2.0f / 640.0f));
    assert(near(design.y1, 1.0f - 541.5f * 2.0f / 640.0f));

    // Aspect-fit scaling preserves the same NDC quad at exactly 2x surface size.
    const auto doubled = layout::quad_for_surface(career_one, 2272, 1280);
    assert(doubled.valid);
    assert(near(doubled.x0, design.x0));
    assert(near(doubled.x1, design.x1));
    assert(near(doubled.y0, design.y0));
    assert(near(doubled.y1, design.y1));

    // Letterboxed 16:9 still uses the shared 1136x640 aspect-fit mapping.
    const auto widescreen = layout::quad_for_surface(career_one, 1920, 1080);
    assert(widescreen.valid);
    assert(widescreen.x0 > -1.0f && widescreen.x1 < 1.0f);
    assert(widescreen.y0 < 1.0f && widescreen.y1 > -1.0f);

    layout::FrameGeometry invalid = career_one;
    invalid.top = 100;
    assert(!layout::quad_for_surface(invalid, 1136, 640).valid);
    assert(!layout::quad_for_surface(career_one, 0, 640).valid);

    std::cout << "SingleSelectHero HeroTable layout smoke: ok\n";
    return 0;
}
