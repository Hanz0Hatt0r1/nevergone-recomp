#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "enemy_actions_secondary_flip_geometry.h"

namespace {

bool close_enough(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

std::uint32_t bits_of(float value) {
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

}  // namespace

int main() {
    using nevergone::enemy_actions_secondary_flip_geometry::apply;
    using nevergone::enemy_actions_secondary_flip_geometry::toggle_sign_bit;
    using nevergone::enemy_actions_secondary_sprite_update::Plan;
    using nevergone::enemy_actions_secondary_sprite_update::SystemInputs;
    using nevergone::enemy_actions_wbg_prefix::ActionFrameRecord;

    ActionFrameRecord primary;
    primary.float_values[6] = 10.0f;  // +0x1c pivot X
    primary.float_values[7] = 4.0f;   // +0x20 pivot Y

    Plan base;
    base.active = true;
    base.should_enter_flag_16a_geometry = true;
    base.position_x = 8.0f;
    base.position_y = 15.2f;
    base.field_1c8_x = 8.0f;
    base.field_1c8_y = 15.2f;
    base.anchor_x = 0.25f;
    base.anchor_y = 0.75f;
    base.rotation = 450.0f;

    SystemInputs system;
    system.field_194 = 5.0f;
    system.field_198 = 6.0f;

    const auto flipped = apply(primary, base, system);
    assert(flipped.applied);
    // Native order: pivot-current = (2,-11.2), negate Y, add pivot ->
    // (12,15.2), then add (+0x194,+0x198) -> (17,21.2), then overwrite
    // final Y/+0x1cc with the unchanged base Y.
    assert(close_enough(flipped.position_x, 17.0f));
    assert(close_enough(flipped.position_y, 15.2f));
    assert(close_enough(flipped.field_1c8_x, 17.0f));
    assert(close_enough(flipped.field_1c8_y, 15.2f));
    assert(close_enough(flipped.anchor_x, 0.75f));
    assert(close_enough(flipped.anchor_y, 0.75f));
    assert(close_enough(flipped.rotation, -450.0f));

    base.should_enter_flag_16a_geometry = false;
    const auto bypass = apply(primary, base, system);
    assert(!bypass.applied);
    assert(close_enough(bypass.position_x, 8.0f));
    assert(close_enough(bypass.position_y, 15.2f));
    assert(close_enough(bypass.anchor_x, 0.25f));
    assert(close_enough(bypass.rotation, 450.0f));

    // Native toggles the IEEE-754 sign bit exactly, including signed zero.
    const float negative_zero = toggle_sign_bit(0.0f);
    assert(bits_of(negative_zero) == 0x80000000u);
    const float positive_zero = toggle_sign_bit(negative_zero);
    assert(bits_of(positive_zero) == 0x00000000u);

    // NaN payload bits are preserved apart from the sign bit.
    std::uint32_t nan_bits = 0x7fc12345u;
    float nan_value = 0.0f;
    std::memcpy(&nan_value, &nan_bits, sizeof(nan_value));
    assert(bits_of(toggle_sign_bit(nan_value)) == 0xffc12345u);

    return 0;
}
