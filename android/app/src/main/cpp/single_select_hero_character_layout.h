#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nevergone::single_select_hero_character_layout {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kCareerOneCenterX = 287.0f;
constexpr float kCareerTwoCenterX = 558.0f;
constexpr float kCenterY = 320.0f;
constexpr float kCarouselStepX = 271.0f;

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

inline bool valid_career(std::int64_t career) {
    return career == 1 || career == 2;
}

inline float center_x(std::int64_t career) {
    if (career == 1) return kCareerOneCenterX;
    if (career == 2) return kCareerTwoCenterX;
    return 0.0f;
}

// Staging order used by the Java atlas extractor:
// career 1 normal/grey/overlay, then career 2 normal/grey/overlay.
inline int frame_index(std::int64_t career, int layer_index) {
    if (!valid_career(career) || layer_index < 0 || layer_index > 2) return -1;
    return static_cast<int>((career - 1) * 3 + layer_index);
}

inline bool composite_visible(std::int64_t selected_career, std::int64_t existing_career) {
    return valid_career(selected_career) &&
        !(existing_career != 0 && existing_career == selected_career);
}

inline Quad quad_for_surface(
        const FrameGeometry& frame,
        std::int64_t career,
        int surface_width,
        int surface_height) {
    Quad result;
    if (!valid_career(career) || surface_width <= 0 || surface_height <= 0 ||
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

    const float center = center_x(career);
    const float design_left = center -
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

}  // namespace nevergone::single_select_hero_character_layout
