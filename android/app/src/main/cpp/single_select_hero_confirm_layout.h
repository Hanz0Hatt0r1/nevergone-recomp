#pragma once

#include <algorithm>
#include <cmath>

namespace nevergone::single_select_hero_confirm_layout {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;

// Recovered from SingleSelectHero::initUI(): btn_a parent at (836, 70),
// with btn_b and the btn_d/btn_e CCMenuItemSprite centered inside that parent.
constexpr float kCenterX = 836.0f;
constexpr float kCenterY = 70.0f;

// The normal btn_d.png sprite is TexturePacker-trimmed to 166x75, but its
// untrimmed source/content size is 170x75. CCMenuItemSprite hit testing uses
// that untrimmed content size.
constexpr float kContentWidth = 170.0f;
constexpr float kContentHeight = 75.0f;

struct FrameGeometry {
    int width = 0;
    int height = 0;
    int left = 0;
    int top = 0;
    int source_width = 0;
    int source_height = 0;
};

struct Quad {
    bool valid = false;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};

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
    if (!surface_transform(
            surface_width, surface_height, &scale, &offset_x, &offset_y)) {
        return result;
    }

    const float design_left = kCenterX - kContentWidth * 0.5f;
    const float design_right = kCenterX + kContentWidth * 0.5f;
    // Cocos positions use bottom-origin design coordinates while Android
    // surface input is top-origin.
    const float design_top = kDesignHeight - (kCenterY + kContentHeight * 0.5f);
    const float design_bottom = kDesignHeight - (kCenterY - kContentHeight * 0.5f);

    result.valid = true;
    result.left = offset_x + design_left * scale;
    result.top = offset_y + design_top * scale;
    result.right = offset_x + design_right * scale;
    result.bottom = offset_y + design_bottom * scale;
    return result;
}

inline bool hit_test(
        int surface_width,
        int surface_height,
        float surface_x,
        float surface_y) {
    if (!std::isfinite(surface_x) || !std::isfinite(surface_y)) return false;
    const SurfaceRect rect = hit_rect_for_surface(surface_width, surface_height);
    return rect.valid &&
        surface_x >= rect.left && surface_x <= rect.right &&
        surface_y >= rect.top && surface_y <= rect.bottom;
}

inline Quad quad_for_surface(
        const FrameGeometry& frame,
        int surface_width,
        int surface_height) {
    Quad result;
    if (frame.width <= 0 || frame.height <= 0 ||
            frame.source_width <= 0 || frame.source_height <= 0 ||
            frame.left < 0 || frame.top < 0 ||
            frame.left + frame.width > frame.source_width ||
            frame.top + frame.height > frame.source_height) {
        return result;
    }

    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    if (!surface_transform(
            surface_width, surface_height, &scale, &offset_x, &offset_y)) {
        return result;
    }

    const float design_left = kCenterX -
        static_cast<float>(frame.source_width) * 0.5f + static_cast<float>(frame.left);
    const float design_top = kDesignHeight -
        (kCenterY + static_cast<float>(frame.source_height) * 0.5f) +
        static_cast<float>(frame.top);
    const float design_right = design_left + static_cast<float>(frame.width);
    const float design_bottom = design_top + static_cast<float>(frame.height);

    const float surface_left = offset_x + design_left * scale;
    const float surface_right = offset_x + design_right * scale;
    const float surface_top = offset_y + design_top * scale;
    const float surface_bottom = offset_y + design_bottom * scale;

    result.valid = true;
    result.x0 = surface_left * 2.0f / static_cast<float>(surface_width) - 1.0f;
    result.x1 = surface_right * 2.0f / static_cast<float>(surface_width) - 1.0f;
    result.y0 = 1.0f - surface_top * 2.0f / static_cast<float>(surface_height);
    result.y1 = 1.0f - surface_bottom * 2.0f / static_cast<float>(surface_height);
    return result;
}

}  // namespace nevergone::single_select_hero_confirm_layout
