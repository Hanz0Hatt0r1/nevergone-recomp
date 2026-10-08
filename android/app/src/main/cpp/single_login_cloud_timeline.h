#pragma once

#include <array>

namespace nevergone::single_login_cloud_timeline {

constexpr int kCloudCount = 9;

struct CloudPose {
    float x = 0.0f;
    float y = 0.0f;
    float alpha = 0.0f;
    float rotation_degrees = 0.0f;
    int z = 0;
    int frame_index = 0;  // 0 = zjmyun01.png, 1 = zjmyun02.png
};

struct Sample {
    std::array<CloudPose, kCloudCount> clouds{};
};

// Reconstruct the nine cloud nodes created by SingleLoginLayer::InitUI().
// Coordinates are original Cocos visible-space coordinates (origin bottom-left).
// The original routes query the first cloud sprite's content size even while
// configuring some zjmyun02 nodes, so primary_width/height deliberately model
// that recovered behavior rather than substituting the second frame's size.
Sample sample(
    double scene_seconds,
    float visible_width,
    float visible_height,
    float primary_width,
    float primary_height);

}  // namespace nevergone::single_login_cloud_timeline
