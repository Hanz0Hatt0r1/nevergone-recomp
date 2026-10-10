#pragma once

#include <cstdint>

namespace nevergone::enemy_actions_frame_zero_bounds {

struct RectObservation {
    float origin_y = 0.0f;
    float height = 0.0f;
};

struct Result {
    bool applied = false;
    float field_16c = 0.0f;
    float field_170 = 0.0f;
};

// Reconstructs the frame-zero-only 0x2ac7e4..0x2ac824 slice.
// Native samples CCSprite::sprBoundingBox() and CCNode::boundingBox(), then
// consumes only spr.origin.y, node.origin.y and node.size.height.
inline Result apply(
        std::int32_t current_frame_294,
        const RectObservation& spr_bounds,
        const RectObservation& node_bounds) {
    Result out;
    if (current_frame_294 != 0) return out;

    // Preserve the native operation order:
    //   tmp = spr.origin.y - node.origin.y
    //   tmp = node.height * 0.5 - tmp      (VNMLS)
    //   tmp = tmp + tmp
    //   +0x170 = tmp
    //   +0x16c = spr.origin.y - tmp
    float tmp = spr_bounds.origin_y - node_bounds.origin_y;
    tmp = node_bounds.height * 0.5f - tmp;
    tmp = tmp + tmp;

    out.applied = true;
    out.field_170 = tmp;
    out.field_16c = spr_bounds.origin_y - tmp;
    return out;
}

}  // namespace nevergone::enemy_actions_frame_zero_bounds
