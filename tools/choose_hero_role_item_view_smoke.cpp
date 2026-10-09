#include <cassert>
#include <cmath>

#include "choose_hero_role_item_view.h"

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

}  // namespace

int main() {
    using namespace nevergone::choose_hero_role_item_view;

    auto mapping = mapping_for_surface(1136, 640);
    assert(mapping.valid);
    assert(near(mapping.scale, 1.0f));
    assert(near(mapping.offset_x, 0.0f));
    assert(near(mapping.offset_y, 0.0f));

    auto point = surface_to_design(1136, 640, 100.0f, 100.0f);
    assert(point.inside);
    assert(near(point.x, 100.0f));
    assert(near(point.y, 540.0f));

    // 16:9 design fitted into a square surface produces vertical letterbox.
    mapping = mapping_for_surface(1000, 1000);
    assert(mapping.valid);
    assert(mapping.offset_y > 0.0f);
    point = surface_to_design(1000, 1000, 500.0f, 10.0f);
    assert(!point.inside);

    // Recovered role board is treated as a centered CCMenuItemToggle hit box.
    assert(contains_centered(100.0f, 540.0f, 398.0f, 116.0f, 100.0f, 540.0f));
    assert(contains_centered(100.0f, 540.0f, 398.0f, 116.0f, -99.0f, 482.0f));
    assert(!contains_centered(100.0f, 540.0f, 398.0f, 116.0f, -100.0f, 540.0f));
    assert(!contains_centered(100.0f, 540.0f, 0.0f, 116.0f, 100.0f, 540.0f));
    return 0;
}
