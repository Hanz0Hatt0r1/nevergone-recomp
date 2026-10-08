#pragma once

#include <cstddef>

namespace nevergone::choose_hero_foreground_cloud_timeline {

constexpr std::size_t kInstanceCount = 6;
constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kDimOpacity = 178.0f / 255.0f;

struct Pose {
    int frame_index = -1;
    float x = 0.0f;
    float y = 0.0f;
    float anchor_x = 1.0f;
    float anchor_y = 0.5f;
    float opacity = 1.0f;
};

// Reconstructs the six repeated foreground-cloud actions created by
// ChooseHeroBackground::BalckCloud(). source_width/source_height model the
// original CCSprite content size restored from the TexturePacker frame.
bool pose_for_instance(
    std::size_t instance_index,
    double elapsed_seconds,
    float source_width,
    float source_height,
    Pose* output);

}  // namespace nevergone::choose_hero_foreground_cloud_timeline
