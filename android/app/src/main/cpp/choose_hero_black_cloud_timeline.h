#pragma once

#include <array>
#include <cstddef>

namespace nevergone::choose_hero_black_cloud_timeline {

constexpr std::size_t kCloudCount = 6;

struct CloudInput {
    float source_width = 0.0f;
    float source_height = 0.0f;
};

struct CloudPose {
    int asset_index = -1;
    float x = 0.0f;
    float y = 0.0f;
    float anchor_x = 1.0f;
    float anchor_y = 0.5f;
    float alpha = 1.0f;
};

// Recovered ChooseHeroBackground::BalckCloud contract. Inputs are the
// untrimmed sprite-frame source dimensions for qianjingyun01/02/03.
std::array<CloudPose, kCloudCount> sample(
    double elapsed_seconds,
    float design_width,
    float design_height,
    const CloudInput& qianjingyun01,
    const CloudInput& qianjingyun02,
    const CloudInput& qianjingyun03);

}  // namespace nevergone::choose_hero_black_cloud_timeline
