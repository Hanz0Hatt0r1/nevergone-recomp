#include <cassert>
#include <cmath>
#include <iostream>

#include "management_role_selection_view.h"

namespace {

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

}  // namespace

int main() {
    using namespace nevergone::management_role_selection_view;

    auto first = item_pose(0, 640.0f, 80.0f);
    assert(first.valid);
    assert(near(first.x, 100.0f));
    assert(near(first.y, 540.0f));

    auto second = item_pose(1, 640.0f, 80.0f);
    assert(second.valid);
    assert(near(second.x, 100.0f));
    assert(near(second.y, 445.0f));

    auto third = item_pose(2, 640.0f, 80.0f);
    assert(third.valid);
    assert(near(third.y, 350.0f));

    assert(!item_pose(0, 0.0f, 80.0f).valid);
    assert(!item_pose(0, 640.0f, 0.0f).valid);

    assert(hit_test(3, 640.0f, 160.0f, 80.0f, 100.0f, 540.0f) == 0);
    assert(hit_test(3, 640.0f, 160.0f, 80.0f, 100.0f, 445.0f) == 1);
    assert(hit_test(3, 640.0f, 160.0f, 80.0f, 100.0f, 350.0f) == 2);

    // Edge-inclusive board bounds match Cocos menu-item hit behavior.
    assert(hit_test(1, 640.0f, 160.0f, 80.0f, 20.0f, 500.0f) == 0);
    assert(hit_test(1, 640.0f, 160.0f, 80.0f, 180.0f, 580.0f) == 0);
    assert(hit_test(1, 640.0f, 160.0f, 80.0f, 19.9f, 540.0f) == -1);
    assert(hit_test(1, 640.0f, 160.0f, 80.0f, 100.0f, 580.1f) == -1);

    assert(hit_test(0, 640.0f, 160.0f, 80.0f, 100.0f, 540.0f) == -1);
    assert(hit_test(1, 640.0f, 0.0f, 80.0f, 100.0f, 540.0f) == -1);

    std::cout << "management role selection view smoke: ok\n";
    return 0;
}
