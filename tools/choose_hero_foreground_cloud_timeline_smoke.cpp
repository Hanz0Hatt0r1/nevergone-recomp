#include "choose_hero_foreground_cloud_timeline.h"

#include <cassert>
#include <cmath>

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

}  // namespace

int main() {
    using nevergone::choose_hero_foreground_cloud_timeline::Pose;
    using nevergone::choose_hero_foreground_cloud_timeline::kDimOpacity;
    using nevergone::choose_hero_foreground_cloud_timeline::pose_for_instance;

    constexpr float width = 200.0f;
    constexpr float height = 100.0f;
    Pose pose;

    // qianjingyun02: top band, immediate 40-second traverse.
    assert(pose_for_instance(0, 0.0, width, height, &pose));
    assert(pose.frame_index == 8);
    assert(near(pose.x, 0.0f));
    assert(near(pose.y, 590.0f));
    assert(near(pose.opacity, 1.0f));
    assert(pose_for_instance(0, 20.0, width, height, &pose));
    assert(near(pose.x, 668.0f));
    assert(pose_for_instance(0, 40.0, width, height, &pose));
    assert(near(pose.x, 0.0f));

    // Second qianjingyun02 copy repeats its 20-second delay each cycle.
    assert(pose_for_instance(1, 10.0, width, height, &pose));
    assert(near(pose.x, 0.0f));
    assert(pose_for_instance(1, 40.0, width, height, &pose));
    assert(near(pose.x, 668.0f));
    assert(pose_for_instance(1, 60.0, width, height, &pose));
    assert(near(pose.x, 0.0f));

    // qianjingyun03: lower band, opacity 178, with -width/-2*width resets.
    assert(pose_for_instance(2, 0.0, width, height, &pose));
    assert(pose.frame_index == 9);
    assert(near(pose.x, -200.0f));
    assert(near(pose.y, 150.0f));
    assert(near(pose.opacity, kDimOpacity));
    assert(pose_for_instance(2, 30.0, width, height, &pose));
    assert(near(pose.x, 568.0f));
    assert(pose_for_instance(3, 15.0, width, height, &pose));
    assert(near(pose.x, -400.0f));
    assert(pose_for_instance(3, 60.0, width, height, &pose));
    assert(near(pose.x, 368.0f));
    assert(pose_for_instance(3, 90.0, width, height, &pose));
    assert(near(pose.x, -400.0f));

    // qianjingyun01: distinct 350/400 baselines and 50/25+50 loops.
    assert(pose_for_instance(4, 0.0, width, height, &pose));
    assert(pose.frame_index == 7);
    assert(near(pose.x, -200.0f));
    assert(near(pose.y, 400.0f));
    assert(pose_for_instance(4, 25.0, width, height, &pose));
    assert(near(pose.x, 568.0f));
    assert(pose_for_instance(5, 0.0, width, height, &pose));
    assert(near(pose.x, -400.0f));
    assert(near(pose.y, 450.0f));
    assert(pose_for_instance(5, 25.0, width, height, &pose));
    assert(near(pose.x, -400.0f));
    assert(pose_for_instance(5, 50.0, width, height, &pose));
    assert(near(pose.x, 368.0f));
    assert(pose_for_instance(5, 75.0, width, height, &pose));
    assert(near(pose.x, -400.0f));

    assert(!pose_for_instance(6, 0.0, width, height, &pose));
    assert(!pose_for_instance(0, 0.0, 0.0f, height, &pose));
    return 0;
}
