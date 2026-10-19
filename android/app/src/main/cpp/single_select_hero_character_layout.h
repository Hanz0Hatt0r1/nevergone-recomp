#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "single_select_hero_table_layout.h"

namespace nevergone::single_select_hero_character_layout {

// The source frames and their untrimmed sizes are verified against the
// shipped Singleselechero atlas. The pose is a provisional placement in the
// left character-preview region until the original sprite-node transform is
// resolved from SingleSelectHero::Carousel. Never treat it as native evidence.
constexpr float kPreviewCenterX = 320.0f;
constexpr float kPreviewCenterY = 320.0f;
constexpr int kFrameCount = 4;

// Atlas upload order: Dawn normal, Dawn grey, Higgs normal, Higgs grey.
inline int frame_index(std::int64_t selected_career, std::int64_t existing_career) {
    if (selected_career != 1 && selected_career != 2) return -1;
    const bool occupied = existing_career == selected_career;
    return static_cast<int>((selected_career - 1) * 2 + (occupied ? 1 : 0));
}

using FrameGeometry = single_select_hero_table_layout::FrameGeometry;
using Quad = single_select_hero_table_layout::Quad;

inline Quad quad_for_surface(
        const FrameGeometry& frame, int surface_width, int surface_height) {
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
        static_cast<float>(surface_width) /
            single_select_hero_table_layout::kDesignWidth,
        static_cast<float>(surface_height) /
            single_select_hero_table_layout::kDesignHeight);
    if (!std::isfinite(scale) || scale <= 0.0f) return result;

    const float design_w = single_select_hero_table_layout::kDesignWidth;
    const float design_h = single_select_hero_table_layout::kDesignHeight;
    const float offset_x = (surface_width - design_w * scale) * 0.5f;
    const float offset_y = (surface_height - design_h * scale) * 0.5f;
    // TexturePacker stores the visible pixels within the untrimmed sprite
    // source; the correct Cocos center anchor applies to the SOURCE first.
    const float left = kPreviewCenterX - frame.source_width * 0.5f + frame.left;
    const float top = design_h - kPreviewCenterY -
        frame.source_height * 0.5f + frame.top;
    const float right = left + frame.width;
    const float bottom = top + frame.height;

    result.valid = true;
    result.x0 = (offset_x + left * scale) * 2.0f / surface_width - 1.0f;
    result.x1 = (offset_x + right * scale) * 2.0f / surface_width - 1.0f;
    result.y0 = 1.0f - (offset_y + top * scale) * 2.0f / surface_height;
    result.y1 = 1.0f - (offset_y + bottom * scale) * 2.0f / surface_height;
    return result;
}

}  // namespace nevergone::single_select_hero_character_layout
