#include <cassert>
#include <cmath>
#include <iostream>

#include "character_name_layout.h"

namespace {

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

}  // namespace

int main() {
    using namespace nevergone::character_name_layout;

    const Layout layout = compute(
        1136.0f,
        640.0f,
        420.0f,
        120.0f,
        280.0f,
        48.0f,
        64.0f,
        64.0f,
        150.0f,
        64.0f,
        150.0f,
        64.0f);
    assert(layout.valid);

    assert(near(layout.background.center_x, 568.0f));
    assert(near(layout.background.center_y, 452.0f));
    assert(near(layout.title.center_x, 568.0f));
    assert(near(layout.title.center_y, 452.0f));

    // y = (640 - 188) - 120/2 - 10 - 48/2 = 358.
    assert(near(layout.name_plate.center_x, 568.0f));
    assert(near(layout.name_plate.center_y, 358.0f));
    assert(near(layout.edit_box.center_x, 568.0f));
    assert(near(layout.edit_box.center_y, 358.0f));
    assert(near(layout.edit_box.width, 280.0f));
    assert(near(layout.edit_box.height, 30.0f));

    assert(near(layout.random_button.center_x, 748.0f));
    assert(near(layout.random_button.center_y, 358.0f));

    assert(near(layout.confirm_button.center_x, 1001.0f));
    assert(near(layout.confirm_button.center_y, 52.0f));
    assert(near(layout.cancel_button.center_x, 135.0f));
    assert(near(layout.cancel_button.center_y, 52.0f));

    assert(near(layout.touch_blocker.center_x, 568.0f));
    assert(near(layout.touch_blocker.center_y, 320.0f));
    assert(near(layout.touch_blocker.width, 1136.0f));
    assert(near(layout.touch_blocker.height, 640.0f));

    assert(contains(layout.confirm_button, 1001.0f, 52.0f));
    assert(contains(layout.confirm_button, 1076.0f, 84.0f));
    assert(!contains(layout.confirm_button, 1076.1f, 84.0f));
    assert(contains(layout.cancel_button, 60.0f, 20.0f));
    assert(!contains(layout.cancel_button, 59.9f, 20.0f));
    assert(contains(layout.random_button, 748.0f, 358.0f));
    assert(contains(layout.touch_blocker, 0.0f, 0.0f));
    assert(contains(layout.touch_blocker, 1136.0f, 640.0f));

    assert(!compute(
        0.0f, 640.0f, 420.0f, 120.0f, 280.0f, 48.0f,
        64.0f, 64.0f, 150.0f, 64.0f, 150.0f, 64.0f).valid);
    assert(!compute(
        1136.0f, 640.0f, 420.0f, 120.0f, 0.0f, 48.0f,
        64.0f, 64.0f, 150.0f, 64.0f, 150.0f, 64.0f).valid);

    std::cout << "character name layout smoke: ok\n";
    return 0;
}
