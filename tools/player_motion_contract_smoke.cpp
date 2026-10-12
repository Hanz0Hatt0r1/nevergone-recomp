#include <cassert>
#include <cmath>
#include <limits>

#include "player_motion_contract.h"

namespace motion = nevergone::player_motion_contract;

namespace {

bool near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
}

}  // namespace

int main() {
    const motion::Vector direction{0.6f, -0.8f};

    const auto normal_one = motion::movement_value(1, motion::kNormalMoveActionState, direction);
    assert(normal_one.has_value());
    assert(near(normal_one->x, 4.8f));
    assert(near(normal_one->y, -3.2f));

    const auto normal_two = motion::movement_value(2, motion::kNormalMoveActionState, direction);
    const auto normal_three = motion::movement_value(3, motion::kNormalMoveActionState, direction);
    assert(normal_two.has_value() && normal_three.has_value());
    assert(near(normal_two->x, 4.8f) && near(normal_two->y, -3.2f));
    assert(near(normal_three->x, 4.8f) && near(normal_three->y, -3.2f));

    const auto fast_one = motion::movement_value(1, motion::kFastMoveActionState, direction);
    assert(fast_one.has_value());
    assert(near(fast_one->x, 12.0f));
    assert(near(fast_one->y, -8.0f));

    const auto fast_two = motion::movement_value(2, motion::kFastMoveActionState, direction);
    assert(fast_two.has_value());
    assert(near(fast_two->x, 9.6f));
    assert(near(fast_two->y, -6.4f));

    assert(!motion::movement_value(4, motion::kNormalMoveActionState, direction).has_value());
    assert(!motion::movement_value(1, 2, direction).has_value());
    assert(!motion::movement_value(
            1,
            motion::kNormalMoveActionState,
            {std::numeric_limits<float>::quiet_NaN(), 0.0f}).has_value());
    return 0;
}
