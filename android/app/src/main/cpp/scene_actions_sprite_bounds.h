#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "game_scene_construction_plan.h"
#include "scene_actions_spatial_trigger.h"

namespace nevergone::scene_actions_sprite_bounds {

constexpr float kPi = 3.14159265358979323846f;

// Reconstruct CCSprite::sprBoundingBox() for the project-owned untrimmed,
// centered type-0 sprite path. Native sprBoundingBox() applies the sprite's
// nodeToParentTransform() to its local sprite rect. For direct-file sprites the
// retained renderer uses the same centered local rect and Cocos transform order
// (skew -> scale -> -rotation -> translation).
//
// Trimmed sprite-frame local rect offsets remain a separate boundary and must
// not use this helper until their +0x13c/+0x140 values are recovered.
inline std::optional<scene_actions_spatial_trigger::Rect> build_untrimmed(
        const game_scene_construction_plan::Type0SpriteTransform& transform,
        float skew_x_degrees,
        int texture_width,
        int texture_height) {
    if (texture_width <= 0 || texture_height <= 0 ||
            !std::isfinite(transform.position_x) ||
            !std::isfinite(transform.position_y) ||
            !std::isfinite(transform.rotation) ||
            !std::isfinite(transform.scale_x) ||
            !std::isfinite(transform.scale_y) ||
            !std::isfinite(skew_x_degrees)) {
        return std::nullopt;
    }

    const float half_w = static_cast<float>(texture_width) * 0.5f;
    const float half_h = static_cast<float>(texture_height) * 0.5f;
    const std::array<float, 4> local_x{-half_w, -half_w, half_w, half_w};
    const std::array<float, 4> local_y{half_h, -half_h, half_h, -half_h};

    const float skew_radians = skew_x_degrees * kPi / 180.0f;
    const float skew_tangent = std::tan(skew_radians);
    if (!std::isfinite(skew_tangent)) return std::nullopt;

    // Same convention as game_scene_direct_geometry and Cocos2d-x
    // CCNode::nodeToParentTransform(): positive node rotation uses -pi/180.
    const float radians = -transform.rotation * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    for (std::size_t index = 0; index < local_x.size(); ++index) {
        const float skewed_x = local_x[index] + skew_tangent * local_y[index];
        const float skewed_y = local_y[index];
        const float scaled_x = skewed_x * transform.scale_x;
        const float scaled_y = skewed_y * transform.scale_y;
        const float x = transform.position_x + scaled_x * cosine - scaled_y * sine;
        const float y = transform.position_y + scaled_x * sine + scaled_y * cosine;
        if (!std::isfinite(x) || !std::isfinite(y)) return std::nullopt;
        if (index == 0u) {
            min_x = max_x = x;
            min_y = max_y = y;
        } else {
            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);
        }
    }

    scene_actions_spatial_trigger::Rect rect;
    rect.x = min_x;
    rect.y = min_y;
    rect.width = max_x - min_x;
    rect.height = max_y - min_y;
    if (!std::isfinite(rect.width) || !std::isfinite(rect.height) ||
            rect.width < 0.0f || rect.height < 0.0f) {
        return std::nullopt;
    }
    return rect;
}

}  // namespace nevergone::scene_actions_sprite_bounds
