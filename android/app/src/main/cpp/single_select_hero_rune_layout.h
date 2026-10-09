#pragma once

#include <algorithm>
#include <atomic>
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

struct RuntimeRuneMetrics {
    std::atomic<int> source_width{0};
    std::atomic<int> source_height{0};
};

inline std::atomic<int> g_runtime_surface_width{0};
inline std::atomic<int> g_runtime_surface_height{0};
inline std::atomic<int> g_runtime_pressed_tag{0};
inline RuntimeRuneMetrics g_runtime_metrics[kRuneCount];

inline bool valid_tag(int tag) {
    return tag >= 1 && tag <= kRuneCount;
}

inline bool enabled_tag(int tag) {
    return valid_tag(tag);
}

inline void set_runtime_pressed_tag(int tag) {
    g_runtime_pressed_tag.store(valid_tag(tag) ? tag : 0, std::memory_order_relaxed);
}

inline int runtime_pressed_tag() {
    return g_runtime_pressed_tag.load(std::memory_order_relaxed);
}

inline int runtime_surface_width() {
    return g_runtime_surface_width.load(std::memory_order_relaxed);
}

inline int runtime_surface_height() {
    return g_runtime_surface_height.load(std::memory_order_relaxed);
}

inline int runtime_source_width(int tag) {
    return valid_tag(tag)
        ? g_runtime_metrics[tag - 1].source_width.load(std::memory_order_relaxed)
        : 0;
}

inline int runtime_source_height(int tag) {
    return valid_tag(tag)
        ? g_runtime_metrics[tag - 1].source_height.load(std::memory_order_relaxed)
        : 0;
}

inline void reset_runtime_input_metrics() {
    g_runtime_surface_width.store(0, std::memory_order_relaxed);
    g_runtime_surface_height.store(0, std::memory_order_relaxed);
    g_runtime_pressed_tag.store(0, std::memory_order_relaxed);
    for (int index = 0; index < kRuneCount; ++index) {
        g_runtime_metrics[index].source_width.store(0, std::memory_order_relaxed);
        g_runtime_metrics[index].source_height.store(0, std::memory_order_relaxed);
    }
}

inline int frame_index(int tag, bool pressed) {
    if (!valid_tag(tag)) return -1;
    const bool effective_pressed = pressed || runtime_pressed_tag() == tag;
    return (tag - 1) * 2 + (effective_pressed ? 1 : 0);
}

inline float center_y(int tag) {
    return valid_tag(tag) ? kFirstCenterY - kStepY * static_cast<float>(tag - 1) : 0.0f;
}

inline float opacity(int tag) {
    return valid_tag(tag) ? 1.0f : 0.0f;
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
    const float value = std::min(
        static_cast<float>(surface_width) / kDesignWidth,
        static_cast<float>(surface_height) / kDesignHeight);
    if (!std::isfinite(value) || value <= 0.0f) return false;
    *scale = value;
    *offset_x = (static_cast<float>(surface_width) - kDesignWidth * value) * 0.5f;
    *offset_y = (static_cast<float>(surface_height) - kDesignHeight * value) * 0.5f;
    return true;
}

inline SurfaceRect hit_rect_for_surface(
        int source_width,
        int source_height,
        int tag,
        int surface_width,
        int surface_height) {
    SurfaceRect result;
    if (!valid_tag(tag) || source_width <= 0 || source_height <= 0) return result;

    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    if (!surface_transform(
            surface_width, surface_height, &scale, &offset_x, &offset_y)) {
        return result;
    }

    const float half_width = static_cast<float>(source_width) * 0.5f;
    const float half_height = static_cast<float>(source_height) * 0.5f;
    const float design_left = kCenterX - half_width;
    const float design_right = kCenterX + half_width;
    const float cy = center_y(tag);
    const float design_top = kDesignHeight - (cy + half_height);
    const float design_bottom = kDesignHeight - (cy - half_height);

    result.valid = true;
    result.left = offset_x + design_left * scale;
    result.top = offset_y + design_top * scale;
    result.right = offset_x + design_right * scale;
    result.bottom = offset_y + design_bottom * scale;
    return result;
}

inline bool hit_test(
        int source_width,
        int source_height,
        int tag,
        int surface_width,
        int surface_height,
        float surface_x,
        float surface_y) {
    const SurfaceRect rect = hit_rect_for_surface(
        source_width, source_height, tag, surface_width, surface_height);
    return rect.valid &&
        std::isfinite(surface_x) && std::isfinite(surface_y) &&
        surface_x >= rect.left && surface_x <= rect.right &&
        surface_y >= rect.top && surface_y <= rect.bottom;
}

inline bool hit_test_runtime(int tag, float surface_x, float surface_y) {
    return hit_test(
        runtime_source_width(tag),
        runtime_source_height(tag),
        tag,
        runtime_surface_width(),
        runtime_surface_height(),
        surface_x,
        surface_y);
}

inline Quad quad_for_surface(
        const FrameGeometry& frame,
        int tag,
        int surface_width,
        int surface_height) {
    Quad result;
    if (!valid_tag(tag) ||
            frame.width <= 0 || frame.height <= 0 ||
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

    g_runtime_surface_width.store(surface_width, std::memory_order_relaxed);
    g_runtime_surface_height.store(surface_height, std::memory_order_relaxed);
    if ((frame_index(tag, false) & 1) == 0) {
        g_runtime_metrics[tag - 1].source_width.store(
            frame.source_width, std::memory_order_relaxed);
        g_runtime_metrics[tag - 1].source_height.store(
            frame.source_height, std::memory_order_relaxed);
    }

    const float cy = center_y(tag);
    const float design_left = kCenterX -
        static_cast<float>(frame.source_width) * 0.5f + static_cast<float>(frame.left);
    const float design_top = kDesignHeight -
        (cy + static_cast<float>(frame.source_height) * 0.5f) +
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
