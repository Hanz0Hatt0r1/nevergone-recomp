#include <cmath>
#include <iostream>

#include "single_login_cloud_timeline.h"

namespace {

bool nearly(float actual, float expected, float tolerance = 1.0e-4f) {
    return std::fabs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    using nevergone::single_login_cloud_timeline::sample;

    constexpr float kVisibleWidth = 1136.0f;
    constexpr float kVisibleHeight = 640.0f;
    constexpr float kPrimaryWidth = 721.0f;
    constexpr float kPrimaryHeight = 278.0f;

    const auto before = sample(-0.1, kVisibleWidth, kVisibleHeight, kPrimaryWidth, kPrimaryHeight);
    for (const auto& cloud : before.clouds) {
        if (cloud.alpha != 0.0f) {
            std::cerr << "negative-time cloud should be hidden\n";
            return 1;
        }
    }

    const auto start = sample(0.0, kVisibleWidth, kVisibleHeight, kPrimaryWidth, kPrimaryHeight);
    if (!nearly(start.clouds[0].x, -360.5f) ||
        !nearly(start.clouds[0].y, -300.0f) ||
        !nearly(start.clouds[0].alpha, 76.0f / 255.0f) ||
        !nearly(start.clouds[0].rotation_degrees, 30.0f) ||
        start.clouds[0].z != 3 || start.clouds[0].frame_index != 0) {
        std::cerr << "iteration-0 cloud start mismatch\n";
        return 1;
    }

    if (!nearly(start.clouds[3].x, 352.0f) ||
        !nearly(start.clouds[3].y, -35.0f) ||
        start.clouds[3].alpha != 0.0f ||
        start.clouds[3].z != 3 || start.clouds[3].frame_index != 0) {
        std::cerr << "faded cloud reset mismatch\n";
        return 1;
    }

    const float delta = 352.0f - 0.8f * kVisibleWidth;
    const float firstTarget = 352.0f + 0.2f * delta;
    const auto fadeHalf = sample(1.0, kVisibleWidth, kVisibleHeight, kPrimaryWidth, kPrimaryHeight);
    if (!nearly(fadeHalf.clouds[3].x, (352.0f + firstTarget) * 0.5f) ||
        !nearly(fadeHalf.clouds[3].alpha, (102.0f / 255.0f) * 0.5f)) {
        std::cerr << "cloud fade-in segment mismatch\n";
        return 1;
    }

    const auto midLinear = sample(8.0, kVisibleWidth, kVisibleHeight, kPrimaryWidth, kPrimaryHeight);
    const float cloud0EndX = 0.7f * kVisibleWidth;
    const float cloud0EndY = -kPrimaryHeight;
    if (!nearly(midLinear.clouds[0].x, (-360.5f + cloud0EndX) * 0.5f) ||
        !nearly(midLinear.clouds[0].y, (-300.0f + cloud0EndY) * 0.5f)) {
        std::cerr << "iteration-0 linear route mismatch\n";
        return 1;
    }

    const auto repeat16 = sample(16.0, kVisibleWidth, kVisibleHeight, kPrimaryWidth, kPrimaryHeight);
    if (!nearly(repeat16.clouds[0].x, start.clouds[0].x) ||
        !nearly(repeat16.clouds[0].y, start.clouds[0].y) ||
        !nearly(repeat16.clouds[3].x, start.clouds[3].x) ||
        !nearly(repeat16.clouds[3].alpha, 0.0f)) {
        std::cerr << "16-second cloud cycle mismatch\n";
        return 1;
    }

    const auto thirdStart = start.clouds[6];
    if (!nearly(thirdStart.x, -15.0f) ||
        !nearly(thirdStart.y, -139.0f) ||
        !nearly(thirdStart.rotation_degrees, -15.0f) ||
        thirdStart.z != 3 || thirdStart.frame_index != 0) {
        std::cerr << "iteration-2 cloud start mismatch\n";
        return 1;
    }

    const auto longSample = sample(1234.5, kVisibleWidth, kVisibleHeight, kPrimaryWidth, kPrimaryHeight);
    for (const auto& cloud : longSample.clouds) {
        if (!std::isfinite(cloud.x) || !std::isfinite(cloud.y) ||
            cloud.alpha < 0.0f || cloud.alpha > 1.0f) {
            std::cerr << "long-run cloud state invalid\n";
            return 1;
        }
    }

    std::cout << "SingleLogin cloud timeline smoke OK\n";
    return 0;
}
