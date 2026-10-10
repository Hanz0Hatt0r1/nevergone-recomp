#include <cassert>
#include <cmath>

#include "enemy_actions_frame_zero_bounds.h"

namespace {

bool close_enough(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    using nevergone::enemy_actions_frame_zero_bounds::RectObservation;
    using nevergone::enemy_actions_frame_zero_bounds::apply;

    const RectObservation spr{10.0f, 999.0f};
    const RectObservation node{4.0f, 8.0f};

    const auto frame_zero = apply(0, spr, node);
    assert(frame_zero.applied);
    // +0x170 = 2 * (4 + 8/2 - 10) = -4
    assert(close_enough(frame_zero.field_170, -4.0f));
    // +0x16c = 10 - (-4) = 14
    assert(close_enough(frame_zero.field_16c, 14.0f));

    const auto later_frame = apply(1, spr, node);
    assert(!later_frame.applied);
    assert(close_enough(later_frame.field_170, 0.0f));
    assert(close_enough(later_frame.field_16c, 0.0f));

    // Width and sprite height are not consumed by the recovered native slice.
    const RectObservation spr_other_height{10.0f, -1234.0f};
    const auto same = apply(0, spr_other_height, node);
    assert(close_enough(same.field_170, frame_zero.field_170));
    assert(close_enough(same.field_16c, frame_zero.field_16c));

    return 0;
}
