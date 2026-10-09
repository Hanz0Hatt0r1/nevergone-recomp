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

struct FrameGeometry {
    int width = 0;
    int height = 0;
    int left = 0;
    int top = 0;
    int source_width = 0;
    int source_height = 0;
};

struct SurfaceRect {
    bool valid = false;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
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
    return valid_tag(tag);
}

inline int frame_index(int tag, bool pressed) {
    if (!valid_tag(tag)) return -1;
    return (tag - 1) * 2 + (pressed ? 1 : 0);
}

inline float center_y(int tag) {
    return valid_tag(tag) ? kFirstCenterY - kStepY * static_cast<float>(tag - 1) : 0.0f;
}

inline float opacity(int tag) {
    return valid_tag(tag) ? 1.0f : 0.0f;
}

inline bool valid_frame(const FrameGeometry& frame) {
    return frame.width > 0 && frame.height > 0 &&
        frame.source_width > 0 && frame.source_height > 0 &&
        frame.left >= 0 && frame.top >= 0 &&
        frame.left + frame.width <= frame.source_width &&
        frame.top + frame.height <= frame.source_height;
}

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

    const float resolved_scale = std::min(
        static_cast<float>(surface_width) / kDesignWidth,
        static_cast<float>(surface_height) / kDesignHeight);
    if (!std::isfinite(resolved_scale) || resolved_scale <= 0.0f) return false;

    *scale = resolved_scale;
    *offset_x = (static_cast<float>(surface_width) - kDesignWidth * resolved_scale) * 0.5f;
    *offset_y = (static_cast<float>(surface_height) - kDesignHeight * resolved_scale) * 0.5f;
    return true;
}

// Visible TexturePacker crop used by the renderer. The sprite's recovered
// center refers to its untrimmed source size, so left/top place the trimmed
// pixels inside that source-sized content box.
inline SurfaceRect visible_rect_for_surface(
        const FrameGeometry& frame,
        int tag,
        int surface_width,
        int surface_height) {
    SurfaceRect result;
    if (!valid_tag(tag) || !valid_frame(frame)) return result;

    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    if (!surface_transform(
            surface_width, surface_height, &scale, &offset_x, &offset_y)) {
        return result;
    }

    const float center_y_design = center_y(tag);
    const float design_left = kCenterX -
        static_cast<float>(frame.source_width) * 0.5f + static_cast<float>(frame.left);
    const float design_top = kDesignHeight -
        (center_y_design + static_cast<float>(frame.source_height) * 0.5f) +
        static_cast<float>(frame.top);

    result.valid = true;
    result.left = offset_x + design_left * scale;
    result.top = offset_y + design_top * scale;
    result.right = result.left + static_cast<float>(frame.width) * scale;
    result.bottom = result.top + static_cast<float>(frame.height) * scale;
    return result;
}

// CCMenuItemSprite inherits the normal sprite content size, not only the
// TexturePacker-visible crop. Use the imported untrimmed source dimensions for
// native hit testing around the exact recovered item center.
inline SurfaceRect content_rect_for_surface(
        const FrameGeometry& frame,
        int tag,
        int surface_width,
        int surface_height) {
    SurfaceRect result;
    if (!valid_tag(tag) || !valid_frame(frame)) return result;

    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    if (!surface_transform(
            surface_width, surface_height, &scale, &offset_x, &offset_y)) {
        return result;
    }

    const float design_left = kCenterX - static_cast<float>(frame.source_width) * 0.5f;
    const float design_top = kDesignHeight -
        (center_y(tag) + static_cast<float>(frame.source_height) * 0.5f);

    result.valid = true;
    result.left = offset_x + design_left * scale;
    result.top = offset_y + design_top * scale;
    result.right = result.left + static_cast<float>(frame.source_width) * scale;
    result.bottom = result.top + static_cast<float>(frame.source_height) * scale;
    return result;
}

inline bool contains(const SurfaceRect& rect, float x, float y) {
    return rect.valid && std::isfinite(x) && std::isfinite(y) &&
        x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

inline Quad quad_for_surface(
        const FrameGeometry& frame,
        int tag,
        int surface_width,
        int surface_height) {
    Quad result;
    const SurfaceRect rect = visible_rect_for_surface(
        frame, tag, surface_width, surface_height);
    if (!rect.valid) return result;

    result.valid = true;
    result.x0 = rect.left * 2.0f / static_cast<float>(surface_width) - 1.0f;
    result.x1 = rect.right * 2.0f / static_cast<float>(surface_width) - 1.0f;
    result.y0 = 1.0f - rect.top * 2.0f / static_cast<float>(surface_height);
    result.y1 = 1.0f - rect.bottom * 2.0f / static_cast<float>(surface_height);
    return result;
}

}  // namespace nevergone::single_select_hero_rune_layout
