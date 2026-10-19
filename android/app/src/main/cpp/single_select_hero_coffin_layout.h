#pragma once

#include <cstdint>

#include "single_select_hero_character_layout.h"

namespace nevergone::single_select_hero_coffin_layout {

// Shipped initUI(): xrshiguan.png is anchored at x = 150 + 274 / 2,
// y = visibleHeight / 2. Its original z-order is behind the hero.
constexpr int kFrameCount = 2;
constexpr int kBaseFrame = 0;
constexpr int kLidFrame = 1;
constexpr int kLidCopies = 5;
constexpr float kFixedCenterX = single_select_hero_character_layout::kPreviewCenterX;
constexpr float kFixedCenterY = single_select_hero_character_layout::kPreviewCenterY;
constexpr float kReelStepX = single_select_hero_character_layout::kCarouselSpacing;

// The five xrguangai.png copies are children of the movable reel, laid
// out from the shiguan center in 271-unit increments. The actual door-opening
// timeline must be recovered before drawing the lids over the player.
inline float initial_lid_center_x(int zero_based_index) {
    if (zero_based_index < 0 || zero_based_index >= kLidCopies) return -1.0f;
    return kFixedCenterX + kReelStepX * static_cast<float>(zero_based_index);
}

inline float settled_reel_offset_x(std::int64_t selected_career) {
    if (selected_career < 1 || selected_career > kLidCopies) return 0.0f;
    return -kReelStepX * static_cast<float>(selected_career - 1);
}

using FrameGeometry = single_select_hero_character_layout::FrameGeometry;
using Quad = single_select_hero_character_layout::Quad;

inline Quad base_quad_for_surface(const FrameGeometry& frame, int width, int height) {
    return single_select_hero_character_layout::quad_for_surface(frame, width, height);
}

}  // namespace nevergone::single_select_hero_coffin_layout
