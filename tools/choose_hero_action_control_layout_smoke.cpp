#include "choose_hero_action_control_layout.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    using nevergone::choose_hero_action_control_layout::compute;
    using nevergone::choose_hero_action_control_layout::contains;
    using nevergone::choose_hero_action_control_layout::type1_label_scale;

    const auto layout = compute(1136.0f, 160.0f, 54.0f, 160.0f, 54.0f);
    assert(layout.valid);
    assert(near(layout.play.center_x, 1022.4f));
    assert(near(layout.play.center_y, 59.0f));
    assert(near(layout.delete_hero.center_x, 852.4f));
    assert(near(layout.delete_hero.center_y, 59.0f));
    assert(near(layout.play.width, 160.0f));
    assert(near(layout.delete_hero.width, 160.0f));

    assert(contains(layout.play, 1022.4f, 59.0f));
    assert(contains(layout.play, 942.5f, 32.1f));
    assert(contains(layout.play, 1102.3f, 85.9f));
    assert(!contains(layout.play, 942.3f, 59.0f));
    assert(!contains(layout.play, 1022.4f, 86.1f));

    assert(near(type1_label_scale(80.0f), 1.0f));
    assert(near(type1_label_scale(115.0f), 1.0f));
    assert(near(type1_label_scale(230.0f), 0.5f));
    assert(type1_label_scale(0.0f) == 0.0f);

    assert(!compute(0.0f, 160.0f, 54.0f, 160.0f, 54.0f).valid);
    assert(!compute(1136.0f, 0.0f, 54.0f, 160.0f, 54.0f).valid);

    std::cout << "ChooseHero action-control layout smoke OK\n";
    return 0;
}
