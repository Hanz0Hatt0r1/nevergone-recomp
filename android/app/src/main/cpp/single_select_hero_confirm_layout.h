#pragma once

#include <algorithm>
#include <cmath>

namespace nevergone::single_select_hero_confirm_layout {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kCenterX = 836.0f;
constexpr float kCenterY = 70.0f;
constexpr float kItemWidth = 170.0f;
constexpr float kItemHeight = 75.0f;

struct SurfaceRect {
    bool valid = false;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

inline bool surface_transform(
        int surface_width,
        int surface_height,
        float* scale,
        float* offset_x,
        float* offset_y) {
    if (surface_width <= 0 || surface_height <= 0 ||
            scale == nullptr || offset_x == nullptr || offset_y == nullptr) {
        return false;
    }
    const float value = std::min(
        static_cast<float>(surface_width) / kDesignWidth,
        static_cast<float>(surface_height) / kDesignHeight);
    if (!std::isfinite(value) || value <= 0.0f) return false;
    *scale = value;
    *offset_x = (static_cast<float>(surface_width) - kDesignWidth * value) * 0.5f;
    *offset_y = (static_cast<float>(surface_height) - kDesignHeight * value) * 0.5f;
    return true;
}

inline SurfaceRect hit_rect_for_surface(int surface_width, int surface_height) {
    SurfaceRect result;
    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    if (!surface_transform(surface_width, surface_height, &scale, &offset_x, &offset_y)) {
        return result;
    }

    const float left_design = kCenterX - kItemWidth * 0.5f;
    const float right_design = kCenterX + kItemWidth * 0.5f;
    // Cocos design coordinates are bottom-origin; MotionEvent surface Y is top-origin.
    const float top_design = kDesignHeight - (kCenterY + kItemHeight * 0.5f);
    const float bottom_design = kDesignHeight - (kCenterY - kItemHeight * 0.5f);

    result.valid = true;
    result.left = offset_x + left_design * scale;
    result.top = offset_y + top_design * scale;
    result.right = offset_x + right_design * scale;
    result.bottom = offset_y + bottom_design * scale;
    return result;
}

inline bool hit_test(
        int surface_width,
        int surface_height,
        float surface_x,
        float surface_y) {
    const SurfaceRect rect = hit_rect_for_surface(surface_width, surface_height);
    return rect.valid && std::isfinite(surface_x) && std::isfinite(surface_y) &&
        surface_x >= rect.left && surface_x <= rect.right &&
        surface_y >= rect.top && surface_y <= rect.bottom;
}

}  // namespace nevergone::single_select_hero_confirm_layout
