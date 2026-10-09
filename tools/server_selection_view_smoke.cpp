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
    assert(hit_test_surface(4, 1136, 640, 495.0f, 320.0f) == 0);
    assert(hit_test_surface(4, 1136, 640, 520.0f, 320.0f) == -1);
    assert(hit_test_surface(4, 1136, 640, 546.0f, 320.0f) == 1);

    const auto fallbackConfirm = confirm_rect();
    assert(fallbackConfirm.original_tag == 10002);
    assert(near(fallbackConfirm.left + fallbackConfirm.width * 0.5f, 568.0f));
    assert(near(fallbackConfirm.bottom + fallbackConfirm.height * 0.5f, 100.0f));
    assert(confirm_contains(568.0f, 100.0f));
    assert(!confirm_contains(0.0f, 0.0f));

    // User-supplied expansion evidence: all three shipped type-1 standard
    // button states decode to 162x63. Hit geometry follows those runtime
    // dimensions while retaining the recovered center and original tag 10002.
    const auto originalConfirm = confirm_rect(162.0f, 63.0f);
    assert(originalConfirm.original_tag == 10002);
    assert(near(originalConfirm.left, 487.0f));
    assert(near(originalConfirm.bottom, 68.5f));
    assert(near(originalConfirm.width, 162.0f));
    assert(near(originalConfirm.height, 63.0f));
    assert(confirm_contains(568.0f, 100.0f, 162.0f, 63.0f));
    assert(!confirm_contains(486.9f, 100.0f, 162.0f, 63.0f));

    std::cout << "server selection view smoke: ok\n";
    return 0;
}
