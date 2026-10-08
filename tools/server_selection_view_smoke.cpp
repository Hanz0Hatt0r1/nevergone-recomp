#include <cassert>
#include <cmath>
#include <iostream>

#include "server_selection_view.h"

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

}  // namespace

int main() {
    using namespace nevergone::server_selection_view;

    auto mapping = mapping_for_surface(1136, 640);
    assert(mapping.valid);
    assert(near(mapping.scale, 1.0f));
    assert(near(mapping.offset_x, 0.0f));
    assert(near(mapping.offset_y, 0.0f));

    auto point = surface_to_design(1136, 640, 0.0f, 0.0f);
    assert(point.inside_design);
    assert(near(point.x, 0.0f));
    assert(near(point.y, 640.0f));

    point = surface_to_design(1136, 640, 1136.0f, 640.0f);
    assert(point.inside_design);
    assert(near(point.x, 1136.0f));
    assert(near(point.y, 0.0f));

    mapping = mapping_for_surface(1920, 1080);
    assert(mapping.valid);
    assert(near(mapping.scale, 1.6875f));
    assert(near(mapping.offset_x, 1.5f));
    assert(near(mapping.offset_y, 0.0f));

    point = surface_to_design(1920, 1080, 960.0f, 540.0f);
    assert(point.inside_design);
    assert(near(point.x, 568.0f));
    assert(near(point.y, 320.0f));

    mapping = mapping_for_surface(1080, 1920);
    assert(mapping.valid);
    assert(mapping.offset_y > 600.0f);
    point = surface_to_design(1080, 1920, 540.0f, 100.0f);
    assert(!point.inside_design);

    // Exact-design surface makes the recovered row hit easy to assert. Android
    // Y is top-down, so original design Y=320 maps to surface Y=320.
    assert(hit_test_surface(4, 1136, 640, 66.0f, 320.0f) == 0);
    assert(hit_test_surface(4, 1136, 640, 506.0f, 320.0f) == 0);
    assert(hit_test_surface(4, 1136, 640, 546.0f, 320.0f) == -1);

    const auto confirm = confirm_rect();
    assert(confirm.original_tag == 10002);
    assert(confirm_contains(
        confirm.left + confirm.width * 0.5f,
        confirm.bottom + confirm.height * 0.5f));
    assert(!confirm_contains(0.0f, 0.0f));

    std::cout << "server selection view smoke: ok\n";
    return 0;
}
