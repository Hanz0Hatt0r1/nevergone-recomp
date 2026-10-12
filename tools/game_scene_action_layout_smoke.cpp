#include <cassert>
#include <cmath>

#include "game_scene_action_layout.h"

int main() {
    using namespace nevergone;
    namespace layout = game_scene_action_layout;
    using game_scene_action_input::Control;

    const auto& descriptors = layout::descriptors();
    assert(descriptors.size() == 3u);
    assert(descriptors[0].control == Control::kButtonA);
    assert(descriptors[0].original_tag == 1);
    assert(std::string(descriptors[0].normal_resource) == "ab.png");
    assert(std::string(descriptors[0].selected_resource) == "ab_s.png");
    assert(descriptors[1].control == Control::kButtonB);
    assert(descriptors[1].original_tag == 2);
    assert(std::string(descriptors[2].normal_resource) == "jb.png");
    assert(descriptors[2].control == Control::kButtonC);
    assert(descriptors[2].original_tag == 3);

    constexpr float width = 1136.0f;
    const auto a = layout::bounds_for(Control::kButtonA, width, {100.0f, 80.0f});
    assert(a.has_value());
    assert(std::fabs(a->left - 848.0f) < 0.001f);
    assert(std::fabs(a->right - 948.0f) < 0.001f);
    assert(std::fabs(a->bottom - 50.0f) < 0.001f);
    assert(std::fabs(a->top - 130.0f) < 0.001f);

    const std::array<layout::SpriteBounds, layout::kButtonCount> bounds{{
        {100.0f, 80.0f},
        {100.0f, 80.0f},
        {100.0f, 80.0f},
    }};

    assert(layout::hit_test(width, 898.0f, 90.0f, bounds) == Control::kButtonA);
    assert(layout::hit_test(width, 1040.0f, 90.0f, bounds) == Control::kButtonB);
    assert(layout::hit_test(width, 1000.0f, 224.0f, bounds) == Control::kButtonC);

    // Original ControlsButton rejects the left half of the visible area before
    // evaluating the sprite bounding box.
    assert(layout::hit_test(width, width * 0.5f, 90.0f, bounds) == Control::kNone);

    // Missing/invalid decoded dimensions cannot manufacture a hit region.
    auto invalid = bounds;
    invalid[0] = {0.0f, 0.0f};
    assert(layout::hit_test(width, 898.0f, 90.0f, invalid) == Control::kNone);

    return 0;
}
