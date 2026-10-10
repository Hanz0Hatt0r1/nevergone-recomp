#include <cassert>
#include <cmath>
#include <limits>

#include "enemy_actions_cut_rect.h"

int main() {
    namespace rect = nevergone::enemy_actions_cut_rect;

    // H-point below system+0x280: native applies the positive difference,
    // subtracts it from the original rect height, and stores half at +0x18.
    auto adjusted = rect::adjust_enemy_cut(100.0f, 640.0f, 600.0f);
    assert(adjusted.should_set_rect);
    assert(adjusted.delta_applied);
    assert(adjusted.delta == 40.0f);
    assert(adjusted.cut_field_1c == 100.0f);
    assert(adjusted.patched_rect_height == 60.0f);
    assert(adjusted.cut_field_18 == 20.0f);

    // Equality and greater-than do not take the native MI branch.
    adjusted = rect::adjust_enemy_cut(100.0f, 640.0f, 640.0f);
    assert(!adjusted.delta_applied);
    assert(adjusted.delta == 0.0f);
    assert(adjusted.patched_rect_height == 100.0f);
    assert(adjusted.cut_field_18 == 0.0f);

    adjusted = rect::adjust_enemy_cut(100.0f, 640.0f, 700.0f);
    assert(!adjusted.delta_applied);
    assert(adjusted.patched_rect_height == 100.0f);

    // No native clamp was observed after subtracting delta from rect height.
    adjusted = rect::adjust_enemy_cut(10.0f, 640.0f, 600.0f);
    assert(adjusted.delta == 40.0f);
    assert(adjusted.patched_rect_height == -30.0f);
    assert(adjusted.cut_field_18 == 20.0f);

    // VCMPE unordered does not take MI, so NaN comparisons leave delta zero.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    adjusted = rect::adjust_enemy_cut(50.0f, nan, 1.0f);
    assert(!adjusted.delta_applied);
    assert(adjusted.delta == 0.0f);
    assert(adjusted.patched_rect_height == 50.0f);

    adjusted = rect::adjust_enemy_cut(50.0f, 640.0f, nan);
    assert(!adjusted.delta_applied);
    assert(adjusted.delta == 0.0f);
    assert(adjusted.patched_rect_height == 50.0f);

    // The system-default matched path simply replaces the fourth rect float
    // with ActionsCut+0x1c.
    const auto default_patch = rect::patch_default_cut(72.5f);
    assert(default_patch.should_set_rect);
    assert(default_patch.cut_field_1c == 72.5f);
    assert(default_patch.patched_rect_height == 72.5f);

    return 0;
}
