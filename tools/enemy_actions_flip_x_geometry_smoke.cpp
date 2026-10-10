#include <cassert>
#include <cmath>

#include "enemy_actions_base_sprite_update.h"
#include "enemy_actions_flip_x_geometry.h"

using nevergone::enemy_actions_base_sprite_update::Plan;
using nevergone::enemy_actions_flip_x_geometry::build;
using nevergone::enemy_actions_wbg_prefix::ActionFrameRecord;

namespace {

bool close_enough(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    ActionFrameRecord frame;
    frame.float_values[2] = 0.25f;  // AFD+0x24 anchor X
    frame.float_values[3] = 0.75f;  // AFD+0x28 anchor Y
    frame.float_values[6] = 10.0f;  // AFD+0x1c pivot X
    frame.float_values[7] = 4.0f;   // AFD+0x20 pivot Y

    Plan base;
    base.flip_x = true;
    base.position_x = 13.0f;
    base.position_y = 9.5f;
    base.rotation = 90.0f;
    base.scale_x = 1.25f;
    base.scale_y = 0.8f;

    const auto mirrored = build(frame, base);
    assert(mirrored.applied);
    assert(close_enough(mirrored.pivot_x, 10.0f));
    assert(close_enough(mirrored.pivot_y, 4.0f));
    assert(close_enough(mirrored.position_x, 7.0f));
    assert(close_enough(mirrored.position_y, 9.5f));
    assert(close_enough(mirrored.field_1c0_x, 7.0f));
    assert(close_enough(mirrored.field_1c0_y, 9.5f));
    assert(close_enough(mirrored.anchor_x, 0.75f));
    assert(close_enough(mirrored.anchor_y, 0.75f));
    assert(close_enough(mirrored.rotation, -90.0f));
    assert(close_enough(mirrored.scale_x, 1.25f));
    assert(close_enough(mirrored.scale_y, 0.8f));

    // Native bypasses this entire block when final flip-X is false.
    base.flip_x = false;
    const auto bypassed = build(frame, base);
    assert(!bypassed.applied);

    // Pivoting exactly on the current X leaves X unchanged and still mirrors
    // anchor/rotation as long as flip-X is active.
    base.flip_x = true;
    base.position_x = 10.0f;
    base.position_y = -3.0f;
    base.rotation = -15.0f;
    const auto centered = build(frame, base);
    assert(centered.applied);
    assert(close_enough(centered.position_x, 10.0f));
    assert(close_enough(centered.position_y, -3.0f));
    assert(close_enough(centered.rotation, 15.0f));

    // The native point arithmetic is ordinary float arithmetic; no clamp is
    // introduced by the reconstruction.
    base.position_x = 100.0f;
    const auto far = build(frame, base);
    assert(close_enough(far.position_x, -80.0f));

    return 0;
}
