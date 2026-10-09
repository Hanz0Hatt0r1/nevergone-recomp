#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nevergone::single_select_hero_table_layout {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kCenterX = 830.0f;
constexpr float kCenterY = 320.0f;

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

inline int frame_index(std::int64_t selected_career, std::int64_t existing_career) {
    if (selected_career != 1 && selected_career != 2) return -1;
    const bool blocked = existing_career != 0 && existing_career == selected_career;
    return static_cast<int>((selected_career - 1) * 2 + (blocked ? 1 : 0));
}

inline Quad quad_for_surface(const FrameGeometry& frame, int surface_width, int surface_height) {
    Quad result;
    if (surface_width <= 0 || surface_height <= 0 ||
            frame.width <= 0 || frame.height <= 0 ||
            frame.source_width <= 0 || frame.source_height <= 0 ||
            frame.left < 0 || frame.top < 0 ||
            frame.left + frame.width > frame.source_width ||
            frame.top + frame.height > frame.source_height) {
        return result;
    }

    const float scale = std::min(
        static_cast<float>(surface_width) / kDesignWidth,
        static_cast<float>(surface_height) / kDesignHeight);
    if (!std::isfinite(scale) || scale <= 0.0f) return result;

    const float viewport_width = kDesignWidth * scale;
    const float viewport_height = kDesignHeight * scale;
    const float offset_x = (static_cast<float>(surface_width) - viewport_width) * 0.5f;
    const float offset_y = (static_cast<float>(surface_height) - viewport_height) * 0.5f;

    // TexturePacker left/top locate the upright trimmed pixels inside the
    // untrimmed sprite source. Cocos positions that source rectangle by its
    // default center anchor at the recovered (830, 320) design coordinate.
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

}  // namespace nevergone::single_select_hero_table_layout
