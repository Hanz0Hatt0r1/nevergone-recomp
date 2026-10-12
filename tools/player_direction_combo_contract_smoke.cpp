#include <cassert>
#include <cmath>

#include "player_direction_combo_contract.h"

namespace combo = nevergone::player_direction_combo_contract;

int main() {
    using Side = combo::HorizontalSide;

    assert(combo::horizontal_side(0.0f) == Side::kRight);
    assert(combo::horizontal_side(44.999f) == Side::kRight);
    assert(combo::horizontal_side(-44.999f) == Side::kRight);
    assert(combo::horizontal_side(45.0f) == Side::kNone);
    assert(combo::horizontal_side(-45.0f) == Side::kNone);
    assert(combo::horizontal_side(135.0f) == Side::kNone);
    assert(combo::horizontal_side(-135.0f) == Side::kNone);
    assert(combo::horizontal_side(135.001f) == Side::kLeft);
    assert(combo::horizontal_side(-135.001f) == Side::kLeft);
    assert(combo::horizontal_side(180.0f) == Side::kLeft);
    assert(combo::horizontal_side(-180.0f) == Side::kLeft);
    assert(combo::horizontal_side(std::nanf("")) == Side::kNone);

    assert(*combo::normal_state(Side::kLeft) == combo::kLeftState);
    assert(*combo::normal_state(Side::kRight) == combo::kRightState);
    assert(*combo::fast_state(Side::kLeft) == combo::kFastLeftState);
    assert(*combo::fast_state(Side::kRight) == combo::kFastRightState);

    // The native comparisons are strict: > 0.03 and < 0.40 seconds.
    assert(!combo::in_fast_reactivation_window(0.03f));
    assert(combo::in_fast_reactivation_window(0.031f));
    assert(combo::in_fast_reactivation_window(0.399f));
    assert(!combo::in_fast_reactivation_window(0.40f));

    assert(!combo::expires_on_deactivate(0.50f));
    assert(combo::expires_on_deactivate(0.501f));

    const auto fast_right = combo::fast_reactivation_state(
            combo::kRightState, Side::kRight, 0.20f);
    assert(fast_right.has_value() && *fast_right == combo::kFastRightState);
    const auto fast_left = combo::fast_reactivation_state(
            combo::kLeftState, Side::kLeft, 0.20f);
    assert(fast_left.has_value() && *fast_left == combo::kFastLeftState);

    // Opposite-sector reactivation is a different native combo branch and must
    // not be misclassified as the recovered fast-movement transition.
    assert(!combo::fast_reactivation_state(
            combo::kRightState, Side::kLeft, 0.20f).has_value());
    assert(!combo::fast_reactivation_state(
            combo::kLeftState, Side::kRight, 0.20f).has_value());
    assert(!combo::fast_reactivation_state(
            combo::kRightState, Side::kRight, 0.50f).has_value());

    assert(combo::is_fast_state(combo::kFastLeftState));
    assert(combo::is_fast_state(combo::kFastRightState));
    assert(!combo::is_fast_state(combo::kLeftState));
    return 0;
}
