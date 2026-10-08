#include "choose_hero_black_cloud_timeline.h"

#include <cmath>

namespace nevergone::choose_hero_black_cloud_timeline {
namespace {

constexpr int kCloud01AssetIndex = 7;
constexpr int kCloud02AssetIndex = 8;
constexpr int kCloud03AssetIndex = 9;
constexpr float kDimAlpha = 178.0f / 255.0f;

float repeat_move(
        double elapsed_seconds,
        double delay_seconds,
        double move_seconds,
        float start_x,
        float end_x) {
    if (move_seconds <= 0.0) return end_x;
    if (elapsed_seconds < 0.0) elapsed_seconds = 0.0;

    const double period = delay_seconds + move_seconds;
    if (period <= 0.0) return start_x;
    double phase = std::fmod(elapsed_seconds, period);
    if (phase < 0.0) phase += period;
    if (phase < delay_seconds) return start_x;

    const double progress = (phase - delay_seconds) / move_seconds;
    if (progress <= 0.0) return start_x;
    if (progress >= 1.0) return end_x;
    return start_x + static_cast<float>(progress) * (end_x - start_x);
}

CloudPose pose(
        int asset_index,
        float x,
        float y,
        float alpha) {
    CloudPose result;
    result.asset_index = asset_index;
    result.x = x;
    result.y = y;
    result.anchor_x = 1.0f;
    result.anchor_y = 0.5f;
    result.alpha = alpha;
    return result;
}

}  // namespace

std::array<CloudPose, kCloudCount> sample(
        double elapsed_seconds,
        float design_width,
        float design_height,
        const CloudInput& qianjingyun01,
        const CloudInput& qianjingyun02,
        const CloudInput& qianjingyun03) {
    const float w01 = qianjingyun01.source_width;
    const float h01 = qianjingyun01.source_height;
    const float w02 = qianjingyun02.source_width;
    const float h02 = qianjingyun02.source_height;
    const float w03 = qianjingyun03.source_width;
    const float h03 = qianjingyun03.source_height;

    const float end02 = design_width + w02;
    const float end03 = design_width + w03;
    const float end01 = design_width + w01;

    return {{
        // qianjingyun02: both copies start at x=0. The second copy repeats
        // Delay(20) + MoveTo(40), so its full repeat period is 60 seconds.
        pose(
            kCloud02AssetIndex,
            repeat_move(elapsed_seconds, 0.0, 40.0, 0.0f, end02),
            design_height - h02 * 0.5f,
            1.0f),
        pose(
            kCloud02AssetIndex,
            repeat_move(elapsed_seconds, 20.0, 40.0, 0.0f, end02),
            design_height - h02 * 0.5f,
            1.0f),

        // qianjingyun03: lower cloud pair, opacity 178. The delayed copy
        // repeats Delay(30) + MoveTo(60), not a one-time phase offset.
        pose(
            kCloud03AssetIndex,
            repeat_move(elapsed_seconds, 0.0, 60.0, -w03, end03),
            100.0f + h03 * 0.5f,
            kDimAlpha),
        pose(
            kCloud03AssetIndex,
            repeat_move(elapsed_seconds, 30.0, 60.0, -2.0f * w03, end03),
            100.0f + h03 * 0.5f,
            kDimAlpha),

        // qianjingyun01: two slightly different vertical lanes, opacity 178.
        pose(
            kCloud01AssetIndex,
            repeat_move(elapsed_seconds, 0.0, 50.0, -w01, end01),
            350.0f + h01 * 0.5f,
            kDimAlpha),
        pose(
            kCloud01AssetIndex,
            repeat_move(elapsed_seconds, 25.0, 50.0, -2.0f * w01, end01),
            400.0f + h01 * 0.5f,
            kDimAlpha),
    }};
}

}  // namespace nevergone::choose_hero_black_cloud_timeline
