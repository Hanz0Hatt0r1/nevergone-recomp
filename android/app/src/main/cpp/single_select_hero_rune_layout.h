#pragma once

#include <algorithm>
#include <cmath>

namespace nevergone::single_select_hero_rune_layout {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kCenterX = 568.0f;
constexpr float kFirstCenterY = 495.0f;
constexpr float kStepY = 90.0f;
constexpr int kRuneCount = 5;
constexpr int kFrameCount = kRuneCount * 2;
constexpr float kDisabledOpacity = 120.0f / 255.0f;

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

inline bool valid_tag(int tag) {
    return tag >= 1 && tag <= kRuneCount;
}

inline bool enabled_tag(int tag) {
    return tag == 1 || tag == 2;
}

inline int frame_index(int tag, bool pressed) {
    if (!valid_tag(tag)) return -1;
    return (tag - 1) * 2 + (pressed ? 1 : 0);
}

inline float center_y(int tag) {
    return valid_tag(tag) ? kFirstCenterY - kStepY * static_cast<float>(tag - 1) : 0.0f;
}

inline float opacity(int tag) {
    return enabled_tag(tag) ? 1.0f : (valid_tag(tag) ? kDisabledOpacity : 0.0f);
}

inline Quad quad_for_surface(
        const FrameGeometry& frame,
        int tag,
        int surface_width,
        int surface_height) {
    Quad result;
    if (!valid_tag(tag) || surface_width <= 0 || surface_height <= 0 ||
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

    const float center_y_design = center_y(tag);
    const float design_left = kCenterX -
        static_cast<float>(frame.source_width) * 0.5f + static_cast<float>(frame.left);
    const float design_top = kDesignHeight -
        (center_y_design + static_cast<float>(frame.source_height) * 0.5f) +
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

}  // namespace nevergone::single_select_hero_rune_layout
