#include <cassert>
#include <cmath>

#include "single_select_hero_coffin_layout.h"

int main() {
    namespace coffin = nevergone::single_select_hero_coffin_layout;

    assert(coffin::opening_offset(0) == 0.0f);
    assert(coffin::opening_offset(coffin::kDoorOpenDelayTicks) == 0.0f);
    assert(coffin::opening_offset(
        coffin::kDoorOpenDelayTicks + coffin::kDoorOpenTravelTicks) > 0.89f);
    assert(std::abs(coffin::opening_offset(coffin::kDoorOpenDurationTicks) - 1.0f) < 0.001f);
    assert(coffin::closing_offset(0) == 1.0f);
    assert(coffin::closing_offset(coffin::kDoorCloseTravelTicks) < 0.11f);
    assert(coffin::closing_offset(coffin::kDoorCloseDurationTicks) == 0.0f);

    const auto clip = coffin::coffin_scissor_for_surface(2340, 1080);
    assert(clip.valid);
    assert(clip.width > 400 && clip.width < 500);
    assert(clip.height > 800 && clip.height < 900);
    assert(clip.x > 400 && clip.x < 500);

    coffin::FrameGeometry frame{218, 465, 0, 0, 218, 465};
    const auto hero = coffin::quad_for_surface(frame, 287.0f, 320.0f, 2340, 1080);
    assert(hero.valid);
    assert(hero.x0 < hero.x1 && hero.y0 > hero.y1);
    assert(!coffin::quad_for_surface(frame, 287.0f, 320.0f, 0, 1080).valid);
    return 0;
}
