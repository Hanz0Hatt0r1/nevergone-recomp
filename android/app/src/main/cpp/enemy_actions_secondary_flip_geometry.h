#pragma once

#include <cstdint>
#include <cstring>

#include "enemy_actions_secondary_sprite_update.h"
#include "enemy_actions_wbg_prefix.h"

namespace nevergone::enemy_actions_secondary_flip_geometry {

struct Plan {
    bool applied = false;
    float position_x = 0.0f;
    float position_y = 0.0f;
    float field_1c8_x = 0.0f;
    float field_1c8_y = 0.0f;
    float anchor_x = 0.0f;
    float anchor_y = 0.0f;
    float rotation = 0.0f;
};

inline float toggle_sign_bit(float value) {
    std::uint32_t bits = 0u;
    static_assert(sizeof(bits) == sizeof(value), "float/u32 size mismatch");
    std::memcpy(&bits, &value, sizeof(bits));
    bits ^= 0x80000000u;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// Reconstructs the +0x16a-gated secondary geometry slice at
// 0x2ac986..0x2aca6a. The incoming base plan is the immediately preceding
// secondary-sprite state applied to system sprite +0x114.
inline Plan apply(
        const enemy_actions_wbg_prefix::ActionFrameRecord& primary_frame,
        const enemy_actions_secondary_sprite_update::Plan& base,
        const enemy_actions_secondary_sprite_update::SystemInputs& system) {
    Plan out;
    out.position_x = base.position_x;
    out.position_y = base.position_y;
    out.field_1c8_x = base.field_1c8_x;
    out.field_1c8_y = base.field_1c8_y;
    out.anchor_x = base.anchor_x;
    out.anchor_y = base.anchor_y;
    out.rotation = base.rotation;

    if (!base.active || !base.should_enter_flag_16a_geometry) return out;

    // Proven Section-A mapping used by the native branch:
    // f6 -> AFD+0x1c, f7 -> AFD+0x20.
    const float pivot_x = primary_frame.float_values[6];
    const float pivot_y = primary_frame.float_values[7];

    // Native builds pivot-currentPosition, negates only the Y component, then
    // adds the pivot back. Preserve that operation order rather than reducing
    // the expression algebraically.
    const float delta_x = pivot_x - base.position_x;
    float delta_y = pivot_y - base.position_y;
    delta_y = -delta_y;

    float mirrored_x = pivot_x + delta_x;
    float mirrored_y = pivot_y + delta_y;

    // It next adds system (+0x194,+0x198) and stores that point into +0x1c8.
    mirrored_x = mirrored_x + system.field_194;
    mirrored_y = mirrored_y + system.field_198;

    out.field_1c8_x = mirrored_x;
    out.field_1c8_y = mirrored_y;

    // Immediately before setPosition(), native recomputes exactly the same Y
    // expression as the base slice and overwrites +0x1cc. Therefore final Y is
    // the base Y, while X retains the mirrored/add-offset value above.
    out.position_x = mirrored_x;
    out.position_y = base.position_y;
    out.field_1c8_y = base.position_y;

    // Anchor X is computed as 0.5 - (AFD+0x24 - 0.5), preserving the native
    // arithmetic order; anchor Y remains AFD+0x28.
    constexpr float kHalf = 0.5f;
    out.anchor_x = kHalf - (base.anchor_x - kHalf);
    out.anchor_y = base.anchor_y;

    // Native uses EOR #0x80000000 on the raw AFD+0x34 word, not a floating
    // arithmetic negate. This matters for signed zero and NaN payloads.
    out.rotation = toggle_sign_bit(base.rotation);
    out.applied = true;
    return out;
}

}  // namespace nevergone::enemy_actions_secondary_flip_geometry
