#include <cassert>
#include <cmath>
#include <iostream>

#include "choose_hero_black_cloud_timeline.h"

namespace {

bool near(float left, float right, float epsilon = 0.001f) {
    return std::fabs(left - right) <= epsilon;
}

}  // namespace

int main() {
    using namespace nevergone::choose_hero_black_cloud_timeline;

    constexpr float width = 1136.0f;
    constexpr float height = 640.0f;
    const CloudInput q01{300.0f, 120.0f};
    const CloudInput q02{400.0f, 160.0f};
    const CloudInput q03{200.0f, 100.0f};

    auto clouds = sample(0.0, width, height, q01, q02, q03);
    assert(clouds.size() == 6);
    assert(clouds[0].asset_index == 8);
    assert(clouds[1].asset_index == 8);
    assert(near(clouds[0].x, 0.0f));
    assert(near(clouds[1].x, 0.0f));
    assert(near(clouds[0].y, 560.0f));
    assert(near(clouds[0].alpha, 1.0f));

    // First qianjingyun02 copy is halfway through its 40-second move at t=20,
    // while the delayed copy has only just reached the start of its move.
    clouds = sample(20.0, width, height, q01, q02, q03);
    assert(near(clouds[0].x, (width + q02.source_width) * 0.5f));
    assert(near(clouds[1].x, 0.0f));
    clouds = sample(60.0, width, height, q01, q02, q03);
    assert(near(clouds[1].x, 0.0f));

    // qianjingyun03 second copy: Delay(30)+MoveTo(60), period 90.
    clouds = sample(0.0, width, height, q01, q02, q03);
    assert(near(clouds[2].x, -200.0f));
    assert(near(clouds[3].x, -400.0f));
    assert(near(clouds[2].y, 150.0f));
    assert(near(clouds[2].alpha, 178.0f / 255.0f));
    clouds = sample(30.0, width, height, q01, q02, q03);
    assert(near(clouds[3].x, -400.0f));
    clouds = sample(90.0, width, height, q01, q02, q03);
    assert(near(clouds[3].x, -400.0f));

    // qianjingyun01 uses two vertical lanes and the delayed copy has a
    // 25+50=75-second repeat period.
    clouds = sample(0.0, width, height, q01, q02, q03);
    assert(near(clouds[4].x, -300.0f));
    assert(near(clouds[5].x, -600.0f));
    assert(near(clouds[4].y, 410.0f));
    assert(near(clouds[5].y, 460.0f));
    clouds = sample(25.0, width, height, q01, q02, q03);
    assert(near(clouds[5].x, -600.0f));
    clouds = sample(75.0, width, height, q01, q02, q03);
    assert(near(clouds[5].x, -600.0f));

    // Negative elapsed time clamps to the recovered starting placements.
    clouds = sample(-5.0, width, height, q01, q02, q03);
    assert(near(clouds[0].x, 0.0f));
    assert(near(clouds[2].x, -200.0f));
    assert(near(clouds[4].x, -300.0f));

    std::cout << "choose hero black cloud timeline smoke: ok\n";
    return 0;
}
