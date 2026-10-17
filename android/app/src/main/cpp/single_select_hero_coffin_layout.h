#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nevergone::single_select_hero_coffin_layout {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kCoffinCenterX = 287.0f;
constexpr float kCoffinCenterY = 320.0f;
constexpr float kCoffinClipLeft = 150.0f;
constexpr float kCoffinClipTop = 63.0f;
constexpr float kCoffinClipWidth = 274.0f;
constexpr float kCoffinClipHeight = 514.0f;
constexpr float kCoffinDoorSourceWidth = 271.0f;
constexpr float kCoffinTrimSourceWidth = 310.0f;
constexpr float kCoffinTrimOffsetX =
    (kCoffinTrimSourceWidth - kCoffinDoorSourceWidth) * 0.5f;
constexpr float kDoorTravel = 300.0f;

constexpr std::uint64_t kDoorOpenDelayTicks = 18;
constexpr std::uint64_t kDoorOpenTravelTicks = 70;
constexpr std::uint64_t kDoorOpenSettleTicks = 25;
constexpr std::uint64_t kDoorOpenDurationTicks =
    kDoorOpenDelayTicks + kDoorOpenTravelTicks + kDoorOpenSettleTicks;
constexpr std::uint64_t kDoorCloseTravelTicks = 70;
constexpr std::uint64_t kDoorCloseDurationTicks = 95;

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

struct Scissor {
    bool valid = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

inline float clamp_unit(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

inline float ease_in_sine(float progress) {
    return 1.0f - std::cos(clamp_unit(progress) * 1.57079632679f);
}

inline float ease_out_sine(float progress) {
    return std::sin(clamp_unit(progress) * 1.57079632679f);
}

inline float opening_offset(std::uint64_t elapsed_ticks) {
    if (elapsed_ticks <= kDoorOpenDelayTicks) return 0.0f;
    const std::uint64_t travel_end = kDoorOpenDelayTicks + kDoorOpenTravelTicks;
    if (elapsed_ticks <= travel_end) {
        const float progress = static_cast<float>(elapsed_ticks - kDoorOpenDelayTicks) /
            static_cast<float>(kDoorOpenTravelTicks);
        return 0.9f * ease_in_sine(progress);
    }

    if (elapsed_ticks <= kDoorOpenDelayTicks + kDoorOpenTravelTicks + 10) {
        const float progress = static_cast<float>(elapsed_ticks - travel_end) / 10.0f;
        return 0.9f + 0.08f * ease_out_sine(progress);
    }

    const float progress = static_cast<float>(elapsed_ticks - travel_end - 10) /
        static_cast<float>(kDoorOpenSettleTicks - 10);
    return 0.98f + 0.02f * ease_in_sine(progress);
}

inline float closing_offset(std::uint64_t elapsed_ticks) {
    if (elapsed_ticks >= kDoorCloseDurationTicks) return 0.0f;
    if (elapsed_ticks <= kDoorCloseTravelTicks) {
        const float progress = static_cast<float>(elapsed_ticks) /
            static_cast<float>(kDoorCloseTravelTicks);
        return 1.0f - 0.9f * ease_in_sine(progress);
    }

    const float progress = static_cast<float>(elapsed_ticks - kDoorCloseTravelTicks) /
        static_cast<float>(kDoorCloseDurationTicks - kDoorCloseTravelTicks);
    return 0.1f * (1.0f - ease_out_sine(progress));
}

inline Quad quad_for_surface(
        const FrameGeometry& frame,
        float center_x,
        float center_y,
        int surface_width,
        int surface_height) {
    Quad result;
    if (surface_width <= 0 || surface_height <= 0 || frame.width <= 0 || frame.height <= 0 ||
            frame.source_width <= 0 || frame.source_height <= 0 || frame.left < 0 || frame.top < 0 ||
            frame.left + frame.width > frame.source_width ||
            frame.top + frame.height > frame.source_height) {
        return result;
    }

    const float scale = std::min(
        static_cast<float>(surface_width) / kDesignWidth,
        static_cast<float>(surface_height) / kDesignHeight);
    if (!std::isfinite(scale) || scale <= 0.0f) return result;

    const float offset_x = (static_cast<float>(surface_width) - kDesignWidth * scale) * 0.5f;
    const float offset_y = (static_cast<float>(surface_height) - kDesignHeight * scale) * 0.5f;
    const float design_left = center_x - static_cast<float>(frame.source_width) * 0.5f +
        static_cast<float>(frame.left);
    const float design_top = kDesignHeight -
        (center_y + static_cast<float>(frame.source_height) * 0.5f) +
        static_cast<float>(frame.top);
    const float surface_left = offset_x + design_left * scale;
    const float surface_right = surface_left + static_cast<float>(frame.width) * scale;
    const float surface_top = offset_y + design_top * scale;
    const float surface_bottom = surface_top + static_cast<float>(frame.height) * scale;

    result.valid = true;
    result.x0 = surface_left * 2.0f / static_cast<float>(surface_width) - 1.0f;
    result.x1 = surface_right * 2.0f / static_cast<float>(surface_width) - 1.0f;
    result.y0 = 1.0f - surface_top * 2.0f / static_cast<float>(surface_height);
    result.y1 = 1.0f - surface_bottom * 2.0f / static_cast<float>(surface_height);
    return result;
}

inline Scissor coffin_scissor_for_surface(int surface_width, int surface_height) {
    Scissor result;
    if (surface_width <= 0 || surface_height <= 0) return result;
    const float scale = std::min(
        static_cast<float>(surface_width) / kDesignWidth,
        static_cast<float>(surface_height) / kDesignHeight);
    if (!std::isfinite(scale) || scale <= 0.0f) return result;

    const float offset_x = (static_cast<float>(surface_width) - kDesignWidth * scale) * 0.5f;
    const float offset_y = (static_cast<float>(surface_height) - kDesignHeight * scale) * 0.5f;
    result.x = static_cast<int>(std::floor(offset_x + kCoffinClipLeft * scale));
    const int top = static_cast<int>(std::floor(offset_y + kCoffinClipTop * scale));
    result.width = static_cast<int>(std::ceil(kCoffinClipWidth * scale));
    result.height = static_cast<int>(std::ceil(kCoffinClipHeight * scale));
    result.y = surface_height - top - result.height;
    result.valid = result.width > 0 && result.height > 0;
    return result;
}

}  // namespace nevergone::single_select_hero_coffin_layout
