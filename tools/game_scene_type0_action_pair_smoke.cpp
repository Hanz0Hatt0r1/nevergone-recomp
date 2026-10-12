#include <cassert>

#include "game_scene_type0_action_pair.h"

namespace pair = nevergone::game_scene_type0_action_pair;

int main() {
    const auto first = pair::build(0u);
    assert(first.has_value());
    assert(first->variant == 1);
    assert(first->born.action_file == "se01_born01.actData");
    assert(first->idle.action_file == "se01_i01.actData");
    assert(first->born.mode_24e == 0u);
    assert(first->idle.mode_24e == 1u);

    // The native float32 fixed-point conversion reaches variants 2 and 3 at
    // approximately one-third and two-thirds of the non-negative 31-bit range.
    const auto second = pair::build(715827882u);
    assert(second.has_value());
    assert(second->variant == 2);
    const auto third = pair::build(1431655765u);
    assert(third.has_value());
    assert(third->variant == 3);

    // VCVT.F32.S32 #31 rounds the highest 31-bit integers to exactly 1.0f.
    // Preserve the resulting rare variant 04 instead of silently clamping it.
    const auto upper_before_round = pair::build(2147483583u);
    assert(upper_before_round.has_value());
    assert(upper_before_round->variant == 3);
    const auto rounded_upper = pair::build(2147483584u);
    assert(rounded_upper.has_value());
    assert(rounded_upper->variant == 4);
    assert(rounded_upper->born.action_file == "se01_born04.actData");
    assert(rounded_upper->idle.action_file == "se01_i04.actData");
    const auto maximum = pair::build(0x7fffffffu);
    assert(maximum.has_value());
    assert(maximum->variant == 4);

    assert(!pair::build(0x80000000u).has_value());
    assert(!pair::variant_from_random31(0xffffffffu).has_value());
    return 0;
}
